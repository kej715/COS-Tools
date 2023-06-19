/*--------------------------------------------------------------------------
**
**  Copyright 2022 Kevin E. Jordan
**
**  Name: ldr.c
**
**  Description:
**      This file is the main module of the COS loader.
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**      http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
**--------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "cosdataset.h"
#include "cosldr.h"
#include "ldrconst.h"
#include "ldrproto.h"
#include "ldrtypes.h"
#include "services.h"

static void addLibraryModule(char *libraryPath, u8 *module);
static Symbol *addSymbol(u8 *id, Block *block, u64 value, bool isParcelAddress);
static void addSuffix(char *inPath, char *suffix, char *outPath);
static void adjustEntryPoints(Symbol *symbol);
static char currentDate[9];
static char currentTime[9];
static Block *findBlock(Module *module, int blockIndex);
static Symbol *findSymbol(u8 *id);
static u64 getWord(u8 *bytes);
static int hasDirectory(Dataset *ds, int pass, char *sourcePath);
static int loadLibraryModule(Dataset *ds, u8 *module, int pass);
static u64 loadModules(Dataset *ds, int pass);
static int locateDirectory(Dataset *ds, u64 *hdr);
static int parseOptions(int argc, char *argv[]);
static void printAddress(u32 address, bool isParcelAddress);
static void printLoadMap(void);
static void printModuleSummary(Module *module);
static void printSymbol(Symbol *symbol, bool doDisplayModule);
static void printSymbols(Module *module, Symbol *symbol);
static int processBRT(Dataset *ds, u64 hdr, int tableLength);
static int processPDT(Dataset *ds, u64 hdr, u8 *table, int tableLength);
static int processTXT(Dataset *ds, u64 hdr, int tableLength);
static int processXRT(Dataset *ds, u64 hdr, int tableLength);
static int readWord(Dataset *ds, u64 *word);
static int resolveExternal(u8 *id);
static int resolveExternals(void);
static int searchLibrary(Dataset *ds, u8 *id, u64 hdr, u8 *module);
static int skipBytes(Dataset *ds, int count);
static void usage(void);
static int writeExecutable(Dataset *ds);
static int writeName(char *name, Dataset *ds);
static int writePDT(Dataset *ds);
static int writeString(char *s, Dataset *ds);
static int writeTXT(Dataset *ds);

static u32           blockLimit = 0200;
static Module        *currentModule = NULL;
static int           errorCount = 0;
static LibraryModule *firstLibraryModule = NULL;
static Module        *firstModule = NULL;
static bool          hasErrorFlag = 0;
static u8            *image = NULL;
static int           imageSize = 0;
static LibraryModule *lastLibraryModule = NULL;
static Module        *lastModule = NULL;
static char          *ldrName = "xLDR";
static char          *ldrVersion = "0.1";
static int           libraryCount = 0;
static char          *libraryPaths[MAX_LIBRARIES];
static FILE          *loadMap = NULL;
static char          *mFile = NULL;
static char          *oFile = NULL;
static char          *osDate = "02/28/89";
static char          *osName = "COS 1.17";
static Symbol        *startSymbol = NULL;
static Symbol        *symbolTable = NULL;

int main(int argc, char *argv[]) {
    time_t clock;
    char *cp;
    Dataset *ds;
    int firstFileIndex;
    int fileIndex;
    LibraryModule *lm;
    char objectPath[MAX_FILE_PATH_LENGTH+1];
    int pass;
    char sourcePath[MAX_FILE_PATH_LENGTH+1];
    char *sp;
    int status;
    struct tm *tmp;

    clock = time(NULL);
    tmp = localtime(&clock);
    sprintf(currentDate, "%02d/%02d/%02d", tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_year - 100);
    sprintf(currentTime, "%02d:%02d:%02d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

    firstFileIndex = parseOptions(argc, argv);

    //
    //  Execute the load in two passes. In pass one, process PDT's
    //  to build a module chain, create symbol table of entry points,
    //  and calculate total image size. In pass two, process TXT's
    //  to load code and data into the image, process BRT's to perform
    //  relocation, and process XRT's to resolve external references.
    //  
    for (pass = 1; pass <= 2; pass++) {
        currentModule = NULL;
        fileIndex = firstFileIndex;
        while (fileIndex < argc) {
            addSuffix(argv[fileIndex], ".obj", sourcePath);
            ds = cosDsOpen(sourcePath);
            if (ds == NULL) {
                fprintf(stderr, "Failed to open %s\n", sourcePath);
                exit(1);
            }
            status = hasDirectory(ds, pass, sourcePath);
            if (status == -1) {
                fprintf(stderr, "Failed to read %s\n", sourcePath);
                exit(1);
            }
            else if (status == 0) {
                if (loadModules(ds, pass) == -1) {
                    fprintf(stderr, "Failed to load modules from %s\n", sourcePath);
                    exit(1);
                }
            }
            cosDsClose(ds);
            fileIndex += 1;
        }
        if (pass == 1) {
            if (resolveExternals() == -1) {
                fputs("Failed to resolve external references\n", stderr);
                exit(1);
            }
            imageSize *= 8;
            image = (u8 *)allocate(imageSize);
            adjustEntryPoints(symbolTable);
        }
        else { // pass 2
            for (lm = firstLibraryModule; lm != NULL; lm = lm->next) {
                ds = cosDsOpen(lm->libraryPath);
                if (ds == NULL) {
                    fprintf(stderr, "Failed to open %s\n", lm->libraryPath);
                    exit(1);
                }
                status = loadLibraryModule(ds, lm->id, pass);
                cosDsClose(ds);
                if (status != 1) {
                    fprintf(stderr, "Failed to load %.8s from %s\n", lm->id, lm->libraryPath);
                    exit(1);
                }
            }
        }
    }
    if (oFile != NULL) {
        addSuffix(oFile, ".abs", objectPath);
    }
    else {
        strcpy(objectPath, argv[firstFileIndex]);
        cp = strrchr(objectPath, '.');
        sp = strrchr(objectPath, '/');
        if (cp == NULL || sp > cp) {
            addSuffix(argv[firstFileIndex], ".abs", objectPath);
        }
        else {
            strcpy(cp, ".abs");
        }
    }
    ds = cosDsCreate(objectPath);
    if (ds == NULL) {
        fprintf(stderr, "Failed to create %s\n", objectPath);
        exit(1);
    }
    status = writeExecutable(ds);
    cosDsClose(ds);
    if (status == -1) unlink(objectPath);
    if (loadMap != NULL) {
        printLoadMap();
        fclose(loadMap);
    }
    if (hasErrorFlag || errorCount > 0) {
        if (hasErrorFlag)
            fputs("One or more source modules have error flags set\n", stderr);
        if (errorCount > 0)
            fprintf(stderr, "%d linkage errors detected\n", errorCount);
        exit(1);
    }
}

static void addLibraryModule(char *libraryPath, u8 *module) {
    LibraryModule *lm;

    for (lm = firstLibraryModule; lm != NULL; lm = lm->next) {
        if (strcmp(libraryPath, lm->libraryPath) == 0 && memcmp(module, lm->id, 8) == 0) return;
    }
    lm = (LibraryModule *)allocate(sizeof(LibraryModule));
    lm->libraryPath = libraryPath;
    memcpy(lm->id, module, 8);
    if (firstLibraryModule == NULL) {
        firstLibraryModule = lm;
    }
    else {
        lastLibraryModule->next = lm;
    }
    lastLibraryModule = lm;
}

static void addSuffix(char *inPath, char *suffix, char *outPath) {
    char *ip;
    char *dp;
    char *limit;
    char *op;

    op = outPath;
    limit = outPath + MAX_FILE_PATH_LENGTH;
    dp = NULL;
    for (ip = inPath; *ip != '\0'; ip++) {
        if (*ip == '/' || *ip == '\\')
            dp = NULL;
        else if (*ip == '.')
            dp = ip;
        if (op >= limit) {
            fprintf(stderr, "Path too long: %s\n", inPath);
            exit(1);
        }
        *op++ = *ip;
    }
    if (dp == NULL) {
        ip = suffix;
        while (*ip != '\0' && op < limit) {
            *op++ = *ip++;
        }
        if (op >= limit) {
            fprintf(stderr, "Path too long: %s\n", inPath);
            exit(1);
        }
    }
    *op = '\0';
}

static Symbol *addSymbol(u8 *id, Block *block, u64 value, bool isParcelAddress) {
    Symbol *current;
    Symbol *new;
    int valence;

    new = (Symbol *)allocate(sizeof(Symbol));
    memcpy(new->id, id, 8);
    new->block = block;
    new->value = value;
    new->isParcelAddress = isParcelAddress;
    if (symbolTable == NULL) {
        symbolTable = new;
        return new;
    }
    current = symbolTable;
    while (current != NULL) {
        valence = memcmp(current->id, new->id, 8);
        if (valence > 0) {
            if (current->left != NULL) {
                current = current->left;
            }
            else {
                current->left = new;
                break;
            }
        }
        else if (valence < 0) {
            if (current->right != NULL) {
                current = current->right;
            }
            else {
                current->right = new;
                break;
            }
        }
        else {
            fprintf(stderr, "Duplicate entry point %s defined in module %s, previously defined in module %s\n",
                current->id, block->module->id, current->block->module->id);
            errorCount += 1;
            free(new);
            new = NULL;
            break;
        }
    }
    return new;
}

static void adjustEntryPoints(Symbol *symbol) {
    if (symbol == NULL) return;
    adjustEntryPoints(symbol->left);
    if (symbol->isParcelAddress)
        symbol->value += symbol->block->baseAddress << 2;
    else
        symbol->value += symbol->block->baseAddress;
    adjustEntryPoints(symbol->right);
}

static Block *findBlock(Module *module, int blockIndex) {
    Block *block;

    block = module->firstBlock;
    while (blockIndex-- > 0 && block != NULL) block = block->next;
    return block;
}

static Symbol *findSymbol(u8 *id) {
    Symbol *current;
    int valence;

    current = symbolTable;
    while (current != NULL) {
        valence = memcmp(current->id, id, 8);
        if (valence > 0)
            current = current->left;
        else if (valence < 0)
            current = current->right;
        else
            break;
    }
    return current;
}

static u64 getWord(u8 *bytes) {
    int i;
    u64 word;

    word = 0;
    for (i = 0; i < 8; i++)
        word = (word << 8) | *bytes++;
    return word;
}

static int hasDirectory(Dataset *ds, int pass, char *sourcePath) {
    u64 hdr;
    int i;
    int status;
    int tableLength;
    u64 wc;

    status = 0;
    if (pass == 1) {
        status = locateDirectory(ds, &hdr);
        if (status == 1) {
            if (libraryCount >= MAX_LIBRARIES) {
                fprintf(stderr, "Too many libraries specified, max is %d\n", MAX_LIBRARIES);
                exit(1);
            }
            libraryPaths[libraryCount] = (char *)allocate(strlen(sourcePath) + 1);
            strcpy(libraryPaths[libraryCount++], sourcePath);
        }
    }
    else {
        for (i = 0; i < libraryCount; i++) {
            if (strcmp(libraryPaths[i], sourcePath) == 0) return 1;
        }
    }
    return status;
}

static int loadLibraryModule(Dataset *ds, u8 *module, int pass) {
    int blockWordCount;
    u8 buf[8];
    u64 cw;
    u64 hdr;
    int hdrLen;
    int n;
    int offset;
    u8 *table;
    int tableLength;
    u8 tableType;
    u64 wc;

    cosDsRewind(ds);
    //
    //  Find PDT with specified module name
    //
    while (TRUE) {
        n = cosDsRead(ds, buf, 8);
        if (n == -1) return -1;
        if (n == 0) {
            cw = cosDsReadCW(ds);
            if (cosDsIsEOF(cw) || cosDsIsEOD(cw)) return 0;
            continue; // EOR
        }
        hdr = getWord(buf);
        tableType = hdr >> 60;
        wc = (hdr >> 36) & 0xffffff; // word count for most table types
        tableLength = (wc - 1) * 8;
        if (tableType == LDR_TT_PDT) {
            blockWordCount = hdr & 0xff;
            table = (u8 *)allocate(tableLength);
            n = cosDsRead(ds, table, tableLength);
            if (n != tableLength) {
                free(table);
                return -1;
            }
            hdrLen = getWord(table) & 0x3fff;
            offset = hdrLen * 8;
            if (blockWordCount > 0) {
                //
                //  Normally, a module has at least one block, the first block
                //  is a program block, and its name is also the module name.
                //
                if (memcmp(module, table + offset, 8) == 0) break;
                free(table);
                continue;
            }
        }
        else if (tableType == LDR_TT_DFT) {
            wc = (hdr >> 24) & 0xffffff;
            tableLength = (wc - 1) * 8;
        }
        if (skipBytes(ds, tableLength) == -1) return -1;
    }
    if (pass == 1) {
        if (processPDT(ds, hdr, table, tableLength) == -1) {
            free(table);
            return -1;
        }
        free(table);
    }
    else { // pass 2
        free(table);
        while (TRUE) {
            n = cosDsRead(ds, buf, 8);
            if (n == -1) return -1;
            if (n == 0) break;
            hdr = getWord(buf);
            tableType = hdr >> 60;
            wc = (hdr >> 36) & 0xffffff; // word count for most table types
            tableLength = (wc - 1) * 8;
            switch (tableType) {
            case LDR_TT_XRT:
                if (processXRT(ds, hdr, tableLength) == -1) return -1;
                break;
            case LDR_TT_BRT:
                if (processBRT(ds, hdr, tableLength) == -1) return -1;
                break;
            case LDR_TT_TXT:
                if (processTXT(ds, hdr, tableLength) == -1) return -1;
                break;
            case LDR_TT_DFT:
                wc = (hdr >> 24) & 0xffffff;
                tableLength = (wc - 1) * 8;
                // fall through
            default:
                if (skipBytes(ds, tableLength) == -1) return -1;
                break;
            }
        }
    }

    return 1;
}

static u64 loadModules(Dataset *ds, int pass) {
    u8 buf[8];
    u64 cw;
    u64 hdr;
    int n;
    u8 *table;
    int tableLength;
    u8 tableType;
    u64 wc;

    while (TRUE) {
        n = cosDsRead(ds, buf, 8);
        if (n == -1) return -1;
        if (n == 0) {
            cw = cosDsReadCW(ds);
            if (cosDsIsEOF(cw) || cosDsIsEOD(cw)) return 0;
            continue; // EOR
        }
        hdr = getWord(buf);
        tableType = hdr >> 60;
        wc = (hdr >> 36) & 0xffffff; // word count for most table types
        tableLength = (wc - 1) * 8;
        switch (tableType) {
        case LDR_TT_XRT:
            if (pass == 2) {
                if (processXRT(ds, hdr, tableLength) == -1) return -1;
                continue;
            }
            break;
        case LDR_TT_BRT:
            if (pass == 2) {
                if (processBRT(ds, hdr, tableLength) == -1) return -1;
                continue;
            }
            break;
        case LDR_TT_TXT:
            if (pass == 2) {
                if (processTXT(ds, hdr, tableLength) == -1) return -1;
                continue;
            }
            break;
        case LDR_TT_PDT:
            if (pass == 1) {
                table = (u8 *)allocate(tableLength);
                n = cosDsRead(ds, table, tableLength);
                if (n != tableLength) {
                    free(table);
                    return -1;
                }
                if (processPDT(ds, hdr, table, tableLength) == -1) {
                    free(table);
                    return -1;
                }
                free(table);
                continue;
            }
            else if (currentModule == NULL) {
                currentModule = firstModule;
            }
            else {
                currentModule = currentModule->next;
            }
            break;
        case LDR_TT_DFT:
            wc = (hdr >> 24) & 0xffffff;
            tableLength = (wc - 1) * 8;
            break;
        default:
            // issue warning and ignore unrecognized table types
            fprintf(stderr, "Warning: unrecognized table type: %02o\n", tableType);
            break;
        }
        if (skipBytes(ds, tableLength) == -1) return -1;
    }
}

static int locateDirectory(Dataset *ds, u64 *hdr) {
    u8 buf[8];
    u64 cw;
    int n;
    int tableLength;
    u8 tableType;
    u64 word;
    u64 wc;

    while (TRUE) {
        n = cosDsRead(ds, buf, 8);
        if (n == -1) return -1;
        if (n == 0) {
            cw = cosDsReadCW(ds);
            if (cosDsIsEOF(cw) || cosDsIsEOD(cw)) break;
            continue;
        }
        word = getWord(buf);
        tableType = word >> 60;
        if (tableType == LDR_TT_DFT) {
            *hdr = word;
            return 1;
        }
        wc = (word >> 36) & 0xffffff;
        tableLength = (wc - 1) * 8;
        if (skipBytes(ds, tableLength) == -1) return -1;
    }
    cosDsRewind(ds);
    return 0;
}

static int parseOptions(int argc, char *argv[]) {
    int i;
    int firstSrcIndex;

    firstSrcIndex = argc;
    i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-m") == 0) {
            i += 1;
            if (i >= argc) {
                usage();
            }
            mFile = argv[i];
            if (strcmp(mFile, "-") == 0) {
                loadMap = stdout;
            }
            else {
                loadMap = fopen(mFile, "w");
                if (loadMap == NULL) {
                    perror(mFile);
                    exit(1);
                }
            }
        }
        else if (strcmp(argv[i], "-o") == 0) {
            i += 1;
            if (i >= argc) {
                usage();
            }
            oFile = argv[i];
        }
        else if (*argv[i] == '-') {
            usage();
        }
        else {
            firstSrcIndex = i++;
            break;
        }
        i += 1;
    }
    while (i < argc) {
        if (*argv[i] == '-') {
            usage();
        }
        i += 1;
    }
    if (firstSrcIndex >= argc) {
        usage();
    }
    return firstSrcIndex;
}

static void printAddress(u32 address, bool isParcelAddress) {
    int parcelNumber;

    if (isParcelAddress) {
        parcelNumber = address & 0x03;
        address >>= 2;
    }
    else {
        parcelNumber = 0;
    }
    fprintf(loadMap, "%o%c", address, 'a' + parcelNumber);
}

static void printLoadMap(void) {
    Module *module;

    fprintf(loadMap,
        "1Load Map                                                         Cray X-MP %s %s            %s %s\n",
        ldrName, ldrVersion, currentDate, currentTime);
    fputs(" \n", loadMap);
    fprintf(loadMap, "       Program: %s\n", firstModule->id);
    fprintf(loadMap, "        Length: %d words\n", blockLimit - 0200);
    fprintf(loadMap, "           HLM: %o (octal)\n", blockLimit);
    fputs(           " Start address: ", loadMap);
    if (startSymbol != NULL) {
        printAddress(startSymbol->value, TRUE);
    }
    else {
        fputs("<none>", loadMap);
    }
    fputs("\n", loadMap);
    for (module = firstModule; module != NULL; module = module->next) {
        printModuleSummary(module);
    }
}

static void printModuleSummary(Module *module) {
    int i;
    u8 *id;
    Symbol *symbol;

    fprintf(loadMap, " \n Module: %s", module->id);
    if (module->hasErrorFlag)
        fputs(" (Warning: error flag set)", loadMap);
    fputs("\n   Entry     Section   Address\n", loadMap);
    fputs("   --------  --------  ---------\n", loadMap);
    printSymbols(module, symbolTable);
    fputs("\n", loadMap);
    if (module->externalRefCount > 0) {
        fputs("   External  Module    Address\n", loadMap);
        fputs("   --------  --------  ---------\n", loadMap);
        for (i = 0; i < module->externalRefCount; i++) {
            id = module->externalRefTable + (i * 8);
            symbol = findSymbol(id);
            if (symbol != NULL) {
                printSymbol(symbol, TRUE);
            }
            else {
                fprintf(loadMap, "   %-8.8s  *UNSATISFIED*\n", id);
            }
        }
    }
}

static void printSymbol(Symbol *symbol, bool doDisplayModule) {
    fprintf(loadMap, "   %-8.8s  %-8.8s  ", (char *)symbol->id, doDisplayModule ? symbol->block->module->id : symbol->block->id);
    printAddress(symbol->value, symbol->isParcelAddress);
    fputs("\n", loadMap);
}

static void printSymbols(Module *module, Symbol *symbol) {
    if (symbol == NULL) return;
    printSymbols(module, symbol->left);
    if (symbol->block->module == module) printSymbol(symbol, FALSE);
    printSymbols(module, symbol->right);
}

static int processBRT(Dataset *ds, u64 hdr, int tableLength) {
    u32 baseAddress;
    u32 bitAddress;
    u32 bitAddrOffset;
    Block *block;
    int blockIndex;
    int byteCount;
    int fieldLength;
    int i;
    u32 imageBytes;
    int imageOffset;
    bool isParcelRelocation;
    u32 parcelAddress;
    int shiftBias;
    int shiftCount;
    u64 word;

    blockIndex = (hdr >> 25) & 0x7f;
    block = findBlock(currentModule, blockIndex);
    if (block == NULL) {
        fprintf(stderr, "Failed to find block %d referenced by BRT of module %s\n", blockIndex, currentModule->id);
        errorCount += 1;
        return skipBytes(ds, tableLength);
    }
    if (isSet(hdr, 28)) {
        //
        //  Process extended format table
        //
        bitAddrOffset = block->baseAddress;
        while (tableLength > 0) {
            if (readWord(ds, &word)) return -1;
            tableLength -= 8;
            blockIndex = (word >> 38) & 0x7f;
            fieldLength = (word >> 32) & 0x3f;
            isParcelRelocation = (word >> 31) & 1;
            bitAddress = word & 0x3fffffff;
            block = findBlock(currentModule, blockIndex);
            if (block == NULL) {
                fprintf(stderr, "Failed to find block %d referenced by extended relocation entry in BRT of module %s\n",
                        blockIndex, currentModule->id);
                errorCount += 1;
                continue;
            }
            baseAddress = block->baseAddress;
            imageOffset = (bitAddrOffset * 8) + (bitAddress >> 3);
            shiftCount = 7 - (bitAddress & 7);
            byteCount = (fieldLength + 7) / 8;
            if (shiftCount != 0) byteCount += 1;
            imageBytes = 0;
            for (i = byteCount - 1; i >= 0; i--) imageBytes = (imageBytes << 8) | image[imageOffset - i];
            if (isParcelRelocation)
                imageBytes += baseAddress << 2;
            else
                imageBytes += baseAddress;
            for (i = 0; i < byteCount; i++) {
                image[imageOffset--] = imageBytes & 0xff;
                imageBytes >>= 8;
            }
        }
    }
    else {
        //
        //  Process standard format table
        //
        baseAddress = block->baseAddress;
        while (tableLength > 0) {
            if (readWord(ds, &word)) return -1;
            tableLength -= 8;
            for (shiftBias = 32; shiftBias >= 0; shiftBias -= 32) {
                blockIndex = (word >> (25 + shiftBias)) & 0x7f;
                isParcelRelocation = (word >> (24 + shiftBias)) & 1;
                parcelAddress = (word >> shiftBias) & 0xffffff;
                block = findBlock(currentModule, blockIndex);
                if (block == NULL) {
                    if (blockIndex == 0x7f && parcelAddress == 0xffffff) break;
                    fprintf(stderr, "Failed to find block %d referenced by standard relocation entry in BRT of module %s\n",
                            blockIndex, currentModule->id);
                    errorCount += 1;
                    continue;
                }
                parcelAddress += block->baseAddress << 2;
                imageOffset = parcelAddress * 2;
                imageBytes = (image[imageOffset  ] << 24)
                           | (image[imageOffset+1] << 16)
                           | (image[imageOffset+2] <<  8)
                           |  image[imageOffset+3];
                if (isParcelRelocation)
                    imageBytes += baseAddress << 2;
                else
                    imageBytes += baseAddress;
                image[imageOffset  ] =  imageBytes >> 24;
                image[imageOffset+1] = (imageBytes >> 16) & 0xff;
                image[imageOffset+2] = (imageBytes >>  8) & 0xff;
                image[imageOffset+3] =  imageBytes        & 0xff;
            }
        }
    }
    return 0;
}

static int processPDT(Dataset *ds, u64 hdr, u8 *table, int tableLength) {
    Block *block;
    int blockIndex;
    int blockWordCount;
    int entryWordCount;
    int externalWordCount;
    int hdrLen;
    int i;
    bool isPrimary;
    bool isParcelAddress;
    bool isParcelRelocation;
    Module *module;
    int n;
    u8 *name;
    int offset;
    Symbol *symbol;
    u64 word;

    blockWordCount = hdr & 0xff;
    entryWordCount = (hdr >> 8) & 0x3fff;
    externalWordCount = (hdr >> 22) & 0x3fff;
    //
    //  Append new module to module list
    //
    module = (Module *)allocate(sizeof(Module));
    hdrLen = getWord(table) & 0x3fff;
    word = getWord(table + 8);
    module->hasMachineTypeExt = isSet(word, 4);
    module->hasCallingSeq = isSet(word, 7);
    if (firstModule == NULL)
        firstModule = module;
    else
        lastModule->next = module;
    lastModule = currentModule = module;
    offset = hdrLen * 8;
    //
    //  Build chain of blocks, if the module has any.
    //
    if (blockWordCount > 0) {
        //
        //  Normally, a module has at least one block, and the first
        //  block is a program block.
        //
        module->id = (char *)allocate(9);
        memcpy(module->id, table + offset, 8);
        offset += 8;
        word = getWord(table + offset);
        offset += 8;
        module->isAbsolute = isSet(word, 0);
        module->hasErrorFlag = isSet(word, 1);
        if (module->hasErrorFlag) {
            fprintf(stderr, "Warning: Module %s has error flag set\n", module->id);
            hasErrorFlag = 1;
        }
        module->origin = (word >> 24) & 0xffffff;
        module->length = word & 0xffffff;
        block = (Block *)allocate(sizeof(Block));
        block->module = module;
        block->id = module->id;
        block->length = module->length;
        if (module->isAbsolute) {
            block->baseAddress = 0;
            if (module->origin + block->length > blockLimit)
                blockLimit = module->origin + block->length;
        }
        else {
            block->baseAddress = blockLimit;
            blockLimit = block->baseAddress + block->length;
        }
        if (blockLimit > imageSize) imageSize = blockLimit;
        module->firstBlock = module->lastBlock = block;
        //
        //  The second through last blocks are common blocks.
        //
        for (i = 2; i < blockWordCount; i += 2) {
            block = (Block *)allocate(sizeof(Block));
            block->module = module;
            block->index = module->lastBlock->index + 1;
            block->id = (char *)allocate(9);
            memcpy(block->id, table + offset, 8);
            offset += 8;
            word = getWord(table + offset);
            offset += 8;
            block->isExtMem = ((word >> 48) & 0x3f) == 2;
            block->length = word & 0xffffff;
            block->baseAddress = blockLimit;
            blockLimit = blockLimit + block->length;
            if (blockLimit > imageSize) imageSize = blockLimit;
            module->lastBlock->next = block;
            module->lastBlock = block;
        }
    }
    //
    //  Process entry point definitions, if any
    //
    for (i = 0; i < entryWordCount; i += 3) {
        name = table + offset;
        offset += 8;
        word = getWord(table + offset);
        offset += 8;
        isPrimary = isSet(word, 55);
        isParcelAddress = isSet(word, 63);
        blockIndex = (word >> 1) & 0x7f;
        block = findBlock(module, blockIndex);
        if (block == NULL) {
            fprintf(stderr, "Invalid block index %d in entry point definition %.8s of module %s\n",
                blockIndex, (char *)name, module->id);
            errorCount += 1;
            offset += 8;
            continue;
        }
        word = getWord(table + offset); // relocation value
        offset += 8;
        symbol = addSymbol(name, block, word, isParcelAddress);
        if (isPrimary) {
            if (startSymbol == NULL) {
                startSymbol = symbol;
            }
            else {
                fprintf(stderr, "Warning: previous start symbol %s of module %s overrides start symbol %s of module %s\n",
                    startSymbol->id, startSymbol->block->module->id, symbol->id, currentModule->id);
            }
        }
    }
    //
    //  Process external reference declarations, if any
    //
    if (externalWordCount > 0) {
        module->externalRefCount = externalWordCount;
        n = externalWordCount * 8;
        module->externalRefTable = (u8 *)allocate(n);
        memcpy(module->externalRefTable, table + offset, n);
        offset += n;
    }
    //
    //  Obtain comment, if any
    //
    offset += 11 * 8; // skip over fixed part of trailer to optional comment
    n = tableLength - offset;
    if (n > 0) {
        module->comment = (char *)allocate(n + 1);
        memcpy(module->comment, table + offset, n);
    }

    return 0;
}

static int processTXT(Dataset *ds, u64 hdr, int tableLength) {
    Block *block;
    int blockIndex;
    int imageOffset;
    u32 loadAddress;
    int n;

    blockIndex = (hdr >> 25) & 0x7f;
    loadAddress = hdr & 0xffffff;
    block = findBlock(currentModule, blockIndex);
    if (block != NULL) {
        loadAddress += block->baseAddress;
        imageOffset = loadAddress * 8;
        if (imageOffset + tableLength > imageSize) {
            fprintf(stderr, "TXT of module %s exceeds image size (load address %o, length %d)\n",
                currentModule->id, loadAddress, tableLength);
            errorCount += 1;
            return skipBytes(ds, tableLength);
        }
        n = cosDsRead(ds, image + imageOffset, tableLength);
        return (n == tableLength) ? 0 : -1;
    }
    else {
        fprintf(stderr, "Failed to find block %d referenced by TXT of module %s\n", blockIndex, currentModule->id);
        errorCount += 1;
        return skipBytes(ds, tableLength);
    }
}

static int processXRT(Dataset *ds, u64 hdr, int tableLength) {
    u32 baseAddress;
    u32 bitAddress;
    Block *block;
    int blockIndex;
    int byteCount;
    int extIndex;
    u8 fieldLength;
    int i;
    u8 *id;
    u32 imageBytes;
    int imageOffset;
    bool isParcelRelocation;
    u32 parcelAddress;
    int shiftCount;
    Symbol *symbol;
    u64 word;

    while (tableLength > 0) {
        if (readWord(ds, &word)) return -1;
        tableLength -= 8;
        blockIndex = (word >> 51) & 0x7f;
        isParcelRelocation = isSet(word, 13);
        extIndex = (word >> 36) & 0x3fff;
        fieldLength = (word >> 30) & 0x3f;
        if (fieldLength == 0) fieldLength = 64;
        bitAddress = word & 0x3fffffff;
        block = findBlock(currentModule, blockIndex);
        if (block == NULL) {
            fprintf(stderr, "Failed to find block %d referenced by XRT of module %s\n", blockIndex, currentModule->id);
            errorCount += 1;
            continue;
        }
        baseAddress = block->baseAddress;
        if (extIndex >= currentModule->externalRefCount) {
            fprintf(stderr, "Invalid external reference index %d in XRT of module %s\n", blockIndex, currentModule->id);
            errorCount += 1;
            continue;
        }
        id = currentModule->externalRefTable + (extIndex * 8);
        symbol = findSymbol(id);
        if (symbol == NULL) {
            fprintf(stderr, "Unsatisfied external reference %.8s\n", id);
            errorCount += 1;
            continue;
        }
        imageOffset = (baseAddress * 8) + (bitAddress >> 3);
        shiftCount = 7 - (bitAddress & 7);
        byteCount = (fieldLength + 7) / 8;
        if (shiftCount != 0) byteCount += 1;
        word = 0;
        for (i = byteCount - 1; i >= 0; i--) word = (word << 8) | image[imageOffset - i];
        if (isParcelRelocation) {
            if (symbol->isParcelAddress)
                word += symbol->value << shiftCount;
            else
                word += symbol->value << (shiftCount + 2);
        }
        else if (symbol->isParcelAddress) {
            word += (symbol->value >> 2) << shiftCount;
        }
        else {
            word += symbol->value << shiftCount;
        }
        for (i = 0; i < byteCount; i++) {
            image[imageOffset--] = word & 0xff;
            word >>= 8;
        }
    }
    return 0;
}

static int readWord(Dataset *ds, u64 *word) {
    u8 buf[8];
    int i;
    int n;

    n = cosDsRead(ds, buf, 8);
    if (n != 8) return -1;
    *word = 0;
    for (i = 0; i < 8; i++) *word = (*word << 8) | buf[i];
    return 0;
}

static int resolveExternal(u8 *id) {
    Dataset *ds;
    u64 hdr;
    int i;
    u8 module[8];
    int status;

    for (i = 0; i < libraryCount; i++) {
        ds = cosDsOpen(libraryPaths[i]);
        if (ds == NULL) {
            fprintf(stderr, "Failed to open %s\n", libraryPaths[i]);
            return -1;
        }
        if (locateDirectory(ds, &hdr) != 1) {
            fprintf(stderr, "Failed to locate directory in %s\n", libraryPaths[i]);
            cosDsClose(ds);
            return -1;
        }
        status = searchLibrary(ds, id, hdr, module);
        if (status == 1) {
            addLibraryModule(libraryPaths[i], module);
            status = loadLibraryModule(ds, module, 1);
        }
        cosDsClose(ds);
        if (status != 0) return status;
    }
    return 0;
}

static int resolveExternals(void) {
    int i;
    u8 *id;
    Module *module;
    Symbol *symbol;

    for (module = firstModule; module != NULL; module = module->next) {
        for (i = 0; i < module->externalRefCount; i++) {
            id = module->externalRefTable + (i * 8);
            symbol = findSymbol(id);
            if (symbol == NULL) {
                if (resolveExternal(id) == -1) return -1;
            }
        }
    }
    return 0;
}

static int searchLibrary(Dataset *ds, u8 *id, u64 hdr, u8 *module) {
    int blockWordCount;
    u8 buf[8];
    int byteCount;
    u8 *dirEntry;
    int dirEntryLength;
    int entryWordCount;
    int ewc;
    int i;
    int n;
    int offset;
    int tableLength;
    u64 word;
    int wc;

    wc = (hdr >> 24) & 0xffffff;
    tableLength = (wc - 1) * 8;
    dirEntryLength = 0;
    dirEntry = NULL;

    while (tableLength > 0) {
        n = cosDsRead(ds, buf, 8);
        if (n != 8) return -1;
        tableLength -= 8;
        word = getWord(buf);
        ewc = (word >> 39) & 0x1fffff;
        entryWordCount = (word >> 9) & 0x7fff;
        blockWordCount = word & 0x1ff;
        byteCount = (ewc - 1) * 8;
        if (dirEntry == NULL) {
            dirEntry = (u8 *)allocate(n);
            dirEntryLength = byteCount;
        }
        else if (byteCount > dirEntryLength) {
            dirEntry = (u8 *)reallocate(dirEntry, dirEntryLength, byteCount);
            dirEntryLength = byteCount;
        }
        n = cosDsRead(ds, dirEntry, byteCount);
        if (n != byteCount) {
            free(dirEntry);
            return -1;
        }
        tableLength -= byteCount;
        memcpy(module, dirEntry, 8);
        offset = (blockWordCount * 8) + 16;
        for (i = 0; i < entryWordCount; i++) {
            if (memcmp(id, dirEntry + offset, 8) == 0) {
                free(dirEntry);
                return 1;
            }
            offset += 8;
        }
    }
    if (dirEntry != NULL) free(dirEntry);
    return 0;
}

static int skipBytes(Dataset *ds, int count) {
    u8 buf[512*8];
    int n;

    while (count > 0) {
        n = (count > sizeof(buf)) ? sizeof(buf) : count;
        n = cosDsRead(ds, buf, n);
        if (n < 1) return -1;
        count -= n;
    }
    return 0;
}

static void usage(void) {
    fputs("Usage: ldr [-m mfile][-o ofile] sfile...\n", stdout);
    fputs("  -m mfile - load map file\n", stderr);
    fputs("  -o ofile - output object file\n", stderr);
    fputs("  sfile    - source file(s)\n", stderr);
    exit(1);
}

static int writeExecutable(Dataset *ds) {
    if (writePDT(ds) == -1 || writeTXT(ds) == -1) return -1;
    cosDsWriteEOR(ds);
    cosDsWriteEOF(ds);
    cosDsWriteEOD(ds);
    return 0;
}

static int writeName(char *name, Dataset *ds) {
    int i;
    int shiftCount;
    u64 word;

    word = 0;
    for (i = 0, shiftCount = 56; i < 8; i++, shiftCount -= 8) {
        if (*name != '\0') {
            word |= ((u64)*name++) << shiftCount;
        }
        else {
            break;
        }
    }
    if (cosDsWriteWord(ds, word) == -1) return -1;
    return 0;
}

static int writePDT(Dataset *ds) {
    int entryCount;
    int i;
    char *id;
    bool isParcelRelocation;
    static u8 machineType[] = {
        'C','R','A','Y','-','X','M','P'
    };
    u64 pdtLen;
    u32 startAddress;
    u64 word;

    entryCount = (startSymbol != NULL) ? 1 : 0;
    if (entryCount == 0) {
        fputs("No start address\n", stderr);
        errorCount += 1;
    }
    pdtLen = 1                 // header word
           + 20                // header entry
           + 2                 // block count 1
           + (entryCount * 3)
           + 11;               // fixed portion of trailer
    if (firstModule->comment != NULL) {
        pdtLen += (strlen(firstModule->comment) + 7) / 8;
    }
    //
    //  Write headser word
    //
    word = ((u64)LDR_TT_PDT << 60)
         | (pdtLen << 36)
         | ((entryCount * 3) << 8)
         | 2;
    if (cosDsWriteWord(ds, word) == -1) return -1;
    //
    //  Write header entry
    //
    if (cosDsWriteWord(ds, (u64)20) == -1) return -1;  // HL field
    word = 0x0980000000000000; // machine type extensions, calling sequence, PDT type
    if (cosDsWriteWord(ds, word) == -1) return -1;
    for (i = 0; i < 10; i++) {
        if (cosDsWriteWord(ds, 0) == -1) return -1;
    }
    if (cosDsWriteWord(ds, (u64)blockLimit) == -1) return -1; // HLM for binary
    for (i = 0; i < 4; i++) {
        if (cosDsWriteWord(ds, 0) == -1) return -1;
    }
    word = 0x0000000000000003; // machine characteristics entry length
    if (cosDsWriteWord(ds, word) == -1) return -1;
    if (cosDsWrite(ds, machineType, sizeof(machineType)) == -1) return -1;
    if (cosDsWriteWord(ds, 0) == -1) return -1; // machine characteristics flags
    //
    //  Write program entry
    //
    if (writeName(firstModule->id, ds) == -1) return -1;
    word = (u64)1 << 63; // absolute flag
    if (hasErrorFlag || errorCount > 0) word |= (u64)1 << 62;
    word |= (u64)0200 << 24;        // program origin
    word |= blockLimit - 0200; // program size
    if (cosDsWriteWord(ds, word) == -1) return -1;
    //
    //  Write starting entry point
    //
    if (startSymbol != NULL) {
        id = startSymbol->id;
        isParcelRelocation = startSymbol->isParcelAddress;
        startAddress = startSymbol->value;
    }
    else {
        id = "";
        isParcelRelocation = 0;
        startAddress = 0200;
    }
    if (writeName(id, ds) == -1) return -1;
    word = 0x100; // primary entry point
    if (isParcelRelocation) word |= 1;
    if (cosDsWriteWord(ds, word) == -1) return -1;
    if (cosDsWriteWord(ds, startAddress) == -1) return -1;
    //
    //  Write trailer
    //
    if (writeName(currentDate, ds) == -1) return -1;
    if (writeName(currentTime, ds) == -1) return -1;
    if (writeName(osName, ds) == -1) return -1;
    if (writeName(osDate, ds) == -1) return -1;
    if (cosDsWriteWord(ds, 0) == -1) return -1; // reserved
    if (writeName(ldrName, ds) == -1) return -1;
    if (writeName(ldrVersion, ds) == -1) return -1;
    for (i = 0; i < 4; i++) { // reserved
        if (cosDsWriteWord(ds, 0) == -1) return -1; // reserved
    }
    if (writeString(firstModule->comment, ds) == -1) return -1;

    return 0;
}

static int writeString(char *s, Dataset *ds) {
    int i;
    int shiftCount;
    u64 word;

    if (s == NULL) return 0;

    while (*s != '\0') {
        word = 0;
        for (i = 0, shiftCount = 56; i < 8; i++, shiftCount -= 8) {
            if (*s != '\0') {
                word |= ((u64)*s++) << shiftCount;
            }
            else {
                word |= (u64)' ' << shiftCount;
            }
        }
        if (cosDsWriteWord(ds, word) == -1) return -1;
    }
    return 0;
}

static int writeTXT(Dataset *ds) {
    int byteCount;
    u64 word;
    u64 wordCount;

    //
    //  Write headser word
    //
    wordCount = blockLimit - 0200;
    byteCount = wordCount * 8;
    word = ((u64)LDR_TT_TXT << 60) | ((wordCount + 1) << 36) | 0200;
    if (cosDsWriteWord(ds, word) == -1) return -1;
    if (cosDsWrite(ds, image + (0200 * 8), byteCount) != byteCount) return -1;

    return 0;
}
