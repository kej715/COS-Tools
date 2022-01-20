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

static Symbol *addSymbol(char *id, Block *block, u64 value, bool isParcelRelocation);
static void adjustEntryPoints(Symbol *symbol);
static char currentDate[9];
static char currentTime[9];
static Block *findBlock(Module *module, int blockIndex);
static Symbol *findSymbol(char *id);
static u64 getWord(u8 *bytes);
static u64 loadModules(Dataset *ds, int pass);
static void openNextSource(int argi, char *argv[]);
static int parseOptions(int argc, char *argv[]);
static int processBRT(Dataset *ds, u64 hdr, int tableLength);
static int processPDT(Dataset *ds, u64 hdr, int tableLength);
static int processTXT(Dataset *ds, u64 hdr, int tableLength);
static int processXRT(Dataset *ds, u64 hdr, int tableLength);
static int readWord(Dataset *ds, u64 *word);
static int skipBytes(Dataset *ds, int count);
static void usage(void);
static void writeAddress(u32 address, bool isParcelAddress);
static int writeExecutable(void);
static void writeLoadMap(void);
static void writeModuleSummary(Module *module);
static int writeName(char *name, Dataset *ds);
static int writePDT(Dataset *ds);
static int writeString(char *s, Dataset *ds);
static void writeSymbol(Symbol *symbol, bool doDisplayModule);
static void writeSymbols(Module *module, Symbol *symbol);
static int writeTXT(Dataset *ds);

static u32     blockLimit = 0200;
static Module  *currentModule = NULL;
static int     errorCount = 0;
static Module  *firstModule = NULL;
static u8      *image = NULL;
static int     imageSize = 0;
static Module  *lastModule = NULL;
static char    *ldrName = "xLDR";
static char    *ldrVersion = "0.1";
static FILE    *loadMap = NULL;
static char    *mFile = NULL;
static Dataset *objectFile = NULL;
static char    *oFile = NULL;
static char    *osDate = "02/28/89";
static char    *osName = "COS 1.17";
static Dataset *sourceFile = NULL;
static Symbol  *startSymbol = NULL;
static Symbol  *symbolTable = NULL;

int main(int argc, char *argv[]) {
    time_t clock;
    u64 cw;
    int firstFileIndex;
    int fileIndex;
    int pass;
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
            openNextSource(fileIndex, argv);
            while (TRUE) {
                cw = loadModules(sourceFile, pass);
                if (cw == 0) {
                    fprintf(stderr, "Failed to load modules from %s\n", argv[fileIndex]);
                    errorCount += 1;
                    break;
                }
                else if (cosDsIsEOF(cw) || cosDsIsEOD(cw)) {
                    break;
                }
            }
            cosDsClose(sourceFile);
            fileIndex += 1;
        }
        if (pass == 1) {
            imageSize *= 8;
            image = (u8 *)allocate(imageSize);
            adjustEntryPoints(symbolTable);
        }
    }
    status = writeExecutable();
    cosDsClose(objectFile);
    if (status == -1) unlink(oFile);
    if (loadMap != NULL) {
        writeLoadMap();
        fclose(loadMap);
    }
}

static Symbol *addSymbol(char *id, Block *block, u64 value, bool isParcelRelocation) {
    Symbol *current;
    Symbol *new;
    int valence;

    new = (Symbol *)allocate(sizeof(Symbol));
    new->id = (char *)allocate(9);
    memcpy(new->id, id, 8);
    new->block = block;
    new->value = value;
    new->isParcelRelocation = isParcelRelocation;
    if (symbolTable == NULL) {
        symbolTable = new;
        return new;
    }
    current = symbolTable;
    while (current != NULL) {
        valence = strcmp(current->id, new->id);
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
            free(new->id);
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
    if (symbol->isParcelRelocation)
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

static Symbol *findSymbol(char *id) {
    Symbol *current;
    int valence;

    current = symbolTable;
    while (current != NULL) {
        valence = strncmp(current->id, id, 8);
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

static u64 loadModules(Dataset *ds, int pass) {
    u8 buf[512*8];
    u64 cw;
    u64 hdr;
    int n;
    int tableLength;
    u8 tableType;
    u64 wc;

    while (TRUE) {
        n = cosDsRead(ds, buf, 8);
        if (n == -1) {
            return 0;
        }
        else if (n == 0) {
            cw = cosDsReadCW(sourceFile);
            return cw;
        }
        hdr = getWord(buf);
        tableType = hdr >> 60;
        wc = (hdr >> 36) & 0xffffff;
        tableLength = (wc - 1) * 8;
        switch (tableType) {
        case LDR_TT_XRT:
            if (pass == 2) {
                if (processXRT(ds, hdr, tableLength) == -1) return 0;
                continue;
            }
            break;
        case LDR_TT_BRT:
            if (pass == 2) {
                if (processBRT(ds, hdr, tableLength) == -1) return 0;
                continue;
            }
            break;
        case LDR_TT_TXT:
            if (pass == 2) {
                if (processTXT(ds, hdr, tableLength) == -1) return 0;
                continue;
            }
            break;
        case LDR_TT_PDT:
            if (pass == 1) {
                if (processPDT(ds, hdr, tableLength) == -1) return 0;
                continue;
            }
            else if (currentModule == NULL) {
                currentModule = firstModule;
            }
            else {
                currentModule = currentModule->next;
            }
            break;
        default:
            // ignore unrecognized table types
            break;
        }
        if (skipBytes(ds, tableLength) == -1) return 0;
    }
    return 0;
}

static void openNextSource(int argi, char *argv[]) {
    char *cp;
    char *dp;
    char filePath[MAX_FILE_PATH_LENGTH+5];
    char *fp;
    char *limit;
    static char objFilePath[MAX_FILE_PATH_LENGTH+5];

    fp = filePath;
    limit = fp + MAX_FILE_PATH_LENGTH;
    dp = NULL;
    for (cp = argv[argi]; *cp != '\0'; cp++) {
         if (*cp == '/' || *cp == '\\')
             dp = NULL;
         else if (*cp == '.')
             dp = fp;
         if (fp >= limit) {
             fprintf(stderr, "Path too long: %s\n", argv[argi]);
             exit(1);
         }
         *fp++ = *cp;
    }
    *fp = '\0';
    if (dp == NULL) {
        dp = fp;
        strcpy(dp, ".obj");
    }
    sourceFile = cosDsOpen(filePath);
    if (sourceFile == NULL) {
        fprintf(stderr, "Failed to open %s\n", filePath);
        exit(1);
    }
    if (objectFile == NULL) {
        strcpy(dp, ".abs");
        strcpy(objFilePath, filePath);
        oFile = objFilePath;
        objectFile = cosDsCreate(filePath);
        if (objectFile == NULL) {
            perror(filePath);
            exit(1);
        }
    }
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
            objectFile = cosDsCreate(oFile);
            if (objectFile == NULL) {
                perror(oFile);
                exit(1);
            }
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

static int processBRT(Dataset *ds, u64 hdr, int tableLength) {
    u32 baseAddress;
    Block *block;
    int blockIndex;
    u32 imageBytes;
    int imageOffset;
    bool isExtFmt;
    bool isParcelRelocation;
    u32 parcelAddress;
    int shiftBias;
    u64 word;

    isExtFmt = isSet(hdr, 28);
    blockIndex = (hdr >> 25) & 0x7f;
    block = findBlock(currentModule, blockIndex);
    if (block == NULL) {
        fprintf(stderr, "Failed to find block %d referenced by BRT of module %s\n", blockIndex, currentModule->id);
        errorCount += 1;
        return skipBytes(ds, tableLength);
    }
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
                fprintf(stderr, "Failed to find block %d referenced by BRT of module %s\n", blockIndex, currentModule->id);
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
    return 0;
}

static int processPDT(Dataset *ds, u64 hdr, int tableLength) {
    Block *block;
    int blockIndex;
    int blockWordCount;
    int entryWordCount;
    int externalWordCount;
    int hdrLen;
    int i;
    bool isPrimary;
    bool isParcelRelocation;
    Module *module;
    int n;
    char *name;
    int offset;
    Symbol *symbol;
    u8 *table;
    u64 word;

    blockWordCount = hdr & 0xff;
    entryWordCount = (hdr >> 8) & 0x3fff;
    externalWordCount = (hdr >> 22) & 0x3fff;
    table = (u8 *)allocate(tableLength);
    n = cosDsRead(ds, table, tableLength);
    if (n != tableLength) {
        free(table);
        return -1;
    }
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
        name = (char *)(table + offset);
        offset += 8;
        word = getWord(table + offset);
        offset += 8;
        isPrimary = isSet(word, 55);
        isParcelRelocation = isSet(word, 63);
        blockIndex = (word >> 1) & 0x7f;
        block = findBlock(module, blockIndex);
        if (block == NULL) {
            fprintf(stderr, "Invalid block index %d in entry point definition %.8s of module %s\n",
                blockIndex, name, module->id);
            errorCount += 1;
            offset += 8;
            continue;
        }
        word = getWord(table + offset); // relocation value
        offset += 8;
        symbol = addSymbol(name, block, word, isParcelRelocation);
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
        module->externalRefTable = (char *)allocate(n);
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

    free(table);
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
    char *id;
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
            if (symbol->isParcelRelocation)
                word += symbol->value << shiftCount;
            else
                word += symbol->value << (shiftCount + 2);
        }
        else if (symbol->isParcelRelocation) {
            word += (symbol->value >> 2) << shiftCount;
        }
        else {
            word += symbol->value << shiftCount;
        }
        for (i = 0; i < byteCount; i++) {
            image[imageOffset--] = word & 0xff;
            word >>= 8;
        }
        word = getWord(image+imageOffset);
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
    fputs("  mfile - load map file\n", stderr);
    fputs("  ofile - output object file\n", stderr);
    fputs("  sfile - source file(s)\n", stderr);
    exit(1);
}

static void writeAddress(u32 address, bool isParcelAddress) {
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

static int writeExecutable(void) {
    if (writePDT(objectFile) == -1 || writeTXT(objectFile) == -1) return -1;
    cosDsWriteEOR(objectFile);
    cosDsWriteEOF(objectFile);
    cosDsWriteEOD(objectFile);
    return 0;
}

static void writeLoadMap(void) {
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
        writeAddress(startSymbol->value, startSymbol->isParcelRelocation);
    }
    else {
        fputs("<none>", loadMap);
    }
    fputs("\n", loadMap);
    for (module = firstModule; module != NULL; module = module->next) {
        writeModuleSummary(module);
    }
}

static void writeModuleSummary(Module *module) {
    int i;
    char *id;
    Symbol *symbol;

    fprintf(loadMap, " \n Module: %s\n", module->id);
    fputs("   Entry     Section   Address\n", loadMap);
    fputs("   --------  --------  ---------\n", loadMap);
    writeSymbols(module, symbolTable);
    fputs("\n", loadMap);
    if (module->externalRefCount > 0) {
        fputs("   External  Module    Address\n", loadMap);
        fputs("   --------  --------  ---------\n", loadMap);
        for (i = 0; i < module->externalRefCount; i++) {
            id = module->externalRefTable + (i * 8);
            symbol = findSymbol(id);
            if (symbol != NULL) {
                writeSymbol(symbol, 1);
            }
            else {
                fprintf(loadMap, "   %.8s  *UNSATISFIED*\n", id);
            }
        }
    }
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
            word |= (u64)' ' << shiftCount;
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
    u32 transferAddress;
    u64 word;

    entryCount = (startSymbol != NULL) ? 1 : 0;
    if (entryCount == 0) {
        fputs("No transfer address\n", stderr);
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
    if (errorCount > 0) word |= (u64)1 << 62;
    word |= (u64)0200 << 24;        // program origin
    word |= blockLimit - 0200; // program size
    if (cosDsWriteWord(ds, word) == -1) return -1;
    //
    //  Write starting entry point
    //
    if (startSymbol != NULL) {
        id = startSymbol->id;
        isParcelRelocation = startSymbol->isParcelRelocation;
        transferAddress = startSymbol->value;
    }
    else {
        id = "";
        isParcelRelocation = 0;
        transferAddress = 0200;
    }
    if (writeName(id, ds) == -1) return -1;
    word = 0x100; // primary entry point
    if (isParcelRelocation) word |= 1;
    if (cosDsWriteWord(ds, word) == -1) return -1;
    if (cosDsWriteWord(ds, transferAddress) == -1) return -1;
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

static void writeSymbol(Symbol *symbol, bool doDisplayModule) {
    fprintf(loadMap, "   %s  %s  ", symbol->id, doDisplayModule ? symbol->block->module->id : symbol->block->id);
    writeAddress(symbol->value, symbol->isParcelRelocation);
    fputs("\n", loadMap);
}

static void writeSymbols(Module *module, Symbol *symbol) {
    if (symbol == NULL) return;
    writeSymbols(module, symbol->left);
    if (symbol->block->module == module) writeSymbol(symbol, 0);
    writeSymbols(module, symbol->right);
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
