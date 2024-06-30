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

#define DEBUG 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "cosdataset.h"
#include "cosldr.h"
#include "fnv.h"
#include "ldrconst.h"
#include "ldrproto.h"
#include "ldrtypes.h"
#include "services.h"

static void addBlock(Module *module, Block *block);
static bool addLibraryModule(Module *module);
static Symbol *addSymbol(u8 *id, Block *block, u64 value, bool isParcelAddress);
static void addSuffix(char *inPath, char *suffix, char *outPath);
static void adjustEntryPoints(Symbol *symbol);
static void calculateBaseAddresses(Block *block);
static void calculateModuleName(char *path, u8 *name);
static int collectLibraryModules(Dataset *ds, char *sourcePath);
static Block *findBlock(Module *module, int blockIndex);
static Module *findLibraryEntry(u8 *id);
static Module *findLibraryModule(u8 *id);
static Symbol *findSymbol(u8 *id);
static u64 formMask(int len);
static u64 getField(u8 *bytes, u32 rightmostBit, u16 fieldLength);
static char *getTableType(u8 type);
static u64 getWord(u8 *bytes);
static int idcmp(u8 *id1, u8*id2, int len);
static int isLibrary(Dataset *ds, int pass, char *sourcePath, char **cachedPath);
static int loadLibraryModule(Dataset *ds, Module *module, char *libraryPath, int pass, u64 *tableHeader);
static int loadLibraryModules(int pass);
static int loadObjectModules(Dataset *ds, u8 *moduleId, int pass);
static int locateTable(Dataset *ds, u8 tableType, u64 *hdr, int *tableLength, char *sourcePath);
static int parseOptions(int argc, char *argv[]);
static void printAddress(u32 address, bool isParcelAddress);
static void printLoadMap(void);
static void printModuleSummary(Module *module);
static void printSymbol(Symbol *symbol, bool doDisplayModule);
static void printSymbols(Module *module, Symbol *symbol);
static int processBRT(Dataset *ds, u64 hdr, int tableLength);
static int processPDT(Dataset *ds, u8 *moduleId, u64 hdr, u8 *table, int tableLength);
static int processTXT(Dataset *ds, u64 hdr, int tableLength);
static int processXRT(Dataset *ds, u64 hdr, int tableLength);
static void putField(u8 *bytes, u32 rightmostBit, u16 fieldLength, u64 field);
static void putWord(u8 *bytes, u64 word);
static int readWord(Dataset *ds, u64 *word);
static bool resolveExternal(u8 *id);
static bool resolveExternals(void);
static bool resolveModuleExternals(Module *module);
static int skipBytes(Dataset *ds, int count);
static void usage(void);
static int writeExecutable(Dataset *ds);
static int writeName(char *name, Dataset *ds);
static int writePDT(Dataset *ds);
static int writeString(char *s, Dataset *ds);
static int writeTXT(Dataset *ds);

static char   currentDate[9];
static char   currentTime[9];
static u32    blockLimit = 0200;
static Module *currentModule = NULL;
static int    errorCount = 0;
static Block  *firstBlocks[BlockTypes] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL};
static Module *firstLibraryModule = NULL;
static Module *firstObjectModule = NULL;
static bool   hasErrorFlag = FALSE;
static u8     *image = NULL;
static int    imageSize = 0;
static Module *lastLibraryModule = NULL;
static Module *lastObjectModule = NULL;
static Module *libraryModuleTree;
static char   *ldrName = "xLDR";
static char   *ldrVersion = "0.1";
static int    libraryCount = 0;
static char   *libraryPaths[MAX_LIBRARIES];
static FILE   *loadMap = NULL;
static char   *mFile = NULL;
static char   *oFile = NULL;
static char   *osDate = "02/28/89";
static char   *osName = "COS 1.17";
static Symbol *startSymbol = NULL;
static Symbol *symbolTable = NULL;

#if defined(__cos)
#define IS_KEY(s) (*((s) + strlen(s) - 1) == '=')
#define AB_KEY  "AB="
#define DN_KEY  "DN="
#define LIB_KEY "LIB="
#define M_KEY   "M="
#define STDOUT  "$OUT"
#else
#define IS_KEY(s) (*(s) == '-')
#define M_KEY  "-m"
#define O_KEY  "-o"
#define STDOUT "-"
#endif

int main(int argc, char *argv[]) {
    char *cachedPath;
    time_t clock;
    char *cp;
    Dataset *ds;
    int firstFileIndex;
    int fileIndex;
    u8 moduleId[9];
    char objectPath[MAX_FILE_PATH_LENGTH+1];
    int pass;
    char sourcePath[MAX_FILE_PATH_LENGTH+1];
    char *sp;
    int status;
    struct tm *tmp;
    int year;

    clock = time(NULL);
    tmp = localtime(&clock);
    year = tmp->tm_year >= 100 ? tmp->tm_year - 100 : tmp->tm_year;
    sprintf(currentDate, "%02d/%02d/%02d", tmp->tm_mon + 1, tmp->tm_mday, year);
    sprintf(currentTime, "%02d:%02d:%02d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

    firstFileIndex = parseOptions(argc, argv);

    //
    //  Execute the load in two passes. In pass one, process PDT's
    //  to build a module chain, create a symbol table of entry points,
    //  and calculate total image size. In pass two, process TXT's
    //  to load code and data into the image, process BRT's to perform
    //  relocation, and process XRT's to resolve external references.
    //  
    for (pass = 1; pass <= 2; pass++) {
#if DEBUG
        eprintf("Start pass %d", pass);
#endif
        currentModule = NULL;
        fileIndex = firstFileIndex;

        while (fileIndex < argc) {
            if (IS_KEY(argv[fileIndex])) {
#if defined(__cos)
                if (strcmp(argv[fileIndex], AB_KEY) == 0
                    || strcmp(argv[fileIndex], M_KEY) == 0) {
                    fileIndex += 2;
                    continue;
                }
#else
                if (strcmp(argv[fileIndex], O_KEY) == 0
                    || strcmp(argv[fileIndex], M_KEY) == 0) {
                    fileIndex += 2;
                    continue;
                }
#endif
                fileIndex += 1;
            }
#if defined(__cos)
            else if (strcmp(argv[fileIndex], "AB") == 0) {
                fileIndex += 1;
                continue;
            }
#endif
            addSuffix(argv[fileIndex], ".obj", sourcePath);
            ds = cosDsOpen(sourcePath);
            if (ds == NULL) {
                eprintf("Failed to open %s", sourcePath);
                exit(1);
            }
            status = isLibrary(ds, pass, sourcePath, &cachedPath);
#if DEBUG
            if (status != -1) eprintf("%s is %s", sourcePath, status == 0 ? "an object file" : "a library");
#endif
            if (status == -1) {
                eprintf("Failed to read %s", sourcePath);
                exit(1);
            }
            else if (status == 0) {
                calculateModuleName(sourcePath, moduleId);
                if (loadObjectModules(ds, moduleId, pass) == -1) {
                    eprintf("Failed to load object modules from %s", sourcePath);
                    exit(1);
                }
            }
            else if (pass == 1) {
                if (collectLibraryModules(ds, cachedPath) == -1) {
                    eprintf("Failed to read entry names from %s", cachedPath);
                    exit(1);
                }
            }
            cosDsClose(ds);
            fileIndex += 1;
        }
        if (pass == 1) {
#if DEBUG
            eputs("Resolve externals");
#endif
            if (resolveExternals() == FALSE) {
                eputs("Failed to resolve external references");
                exit(1);
            }
            if (loadLibraryModules(pass) == -1) exit(1);
            //
            //  Traverse the block lists and calculate the base address of
            //  each block based upon the load order
            //
#if DEBUG
            eputs("Calculate base addresses");
#endif
            calculateBaseAddresses(firstBlocks[BlockType_Code]);
            calculateBaseAddresses(firstBlocks[BlockType_Mixed]);
            calculateBaseAddresses(firstBlocks[BlockType_Const]);
            calculateBaseAddresses(firstBlocks[BlockType_Common]);
            calculateBaseAddresses(firstBlocks[BlockType_TaskCom]);
            calculateBaseAddresses(firstBlocks[BlockType_Data]);
            calculateBaseAddresses(firstBlocks[BlockType_Dynamic]);
            imageSize *= 8;
            image = (u8 *)allocate(imageSize);
#if DEBUG
            eputs("Adjust entry points");
#endif
            adjustEntryPoints(symbolTable);
        }
        else {
            if (loadLibraryModules(pass) == -1) exit(1);
        }
#if DEBUG
        eprintf("End pass   %d", pass);
#endif
    }
#if defined(__cos)
    if (oFile != NULL) {
#if DEBUG
        eprintf("Create %s", oFile);
#endif
        ds = cosDsCreate(oFile);
        if (ds == NULL) {
            eprintf("Failed to create %s", oFile);
            exit(1);
        }
        status = writeExecutable(ds);
        cosDsClose(ds);
        if (status == -1) unlink(oFile);
    }
#else
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
#if DEBUG
    eprintf("Create %s", objectPath);
#endif
    ds = cosDsCreate(objectPath);
    if (ds == NULL) {
        eprintf("Failed to create %s", objectPath);
        exit(1);
    }
    status = writeExecutable(ds);
    cosDsClose(ds);
    if (status == -1) unlink(objectPath);
#endif
    if (loadMap != NULL) {
#if DEBUG
        eputs("Print load map");
#endif
        printLoadMap();
        fclose(loadMap);
    }
    if (hasErrorFlag || errorCount > 0) {
        if (hasErrorFlag)
            eputs("One or more source modules have error flags set");
        if (errorCount > 0)
            eprintf("%d linkage errors detected", errorCount);
        exit(1);
    }
    exit(0);
}

static void addBlock(Module *module, Block *block) {
    BlockType blockType;
    Block *bp;
    Block *nbp;

    if (module->firstBlock == NULL) {
        module->firstBlock = block;
    }
    else {
        module->lastBlock->nextInModule = block;
    }
    module->lastBlock = block;

    blockType = block->type;
    bp = firstBlocks[blockType];
    if (bp == NULL) {
        firstBlocks[blockType] = block;
        return;
    }
    //
    //  Find first block with the same id
    //
    while (idcmp(bp->id, block->id, 8) != 0) {
        if (bp->nextInImage == NULL) {
            //
            //  The chain doesn't have any blocks with this id yet,
            //  so add it to the end
            //
            bp->nextInImage = block;
            return;
        }
        bp = bp->nextInImage;
    }
    //
    //  Find the last block in the chain with the same id,
    //  and add this one after that one
    //
    for (;;) {
        nbp = bp->nextInImage;
        if (nbp == NULL || idcmp(bp->id, nbp->id, 8) != 0) {
            bp->nextInImage = block;
            block->nextInImage = nbp;
            return;
        }
        bp = nbp;
    }
}

static bool addLibraryModule(Module *module) {
    Module *current;
    int valence;

    if (libraryModuleTree == NULL) {
        libraryModuleTree = module;
    }
    else {
        current = libraryModuleTree;
        while (current != NULL) {
            valence = idcmp(current->id, module->id, 8);
            if (valence > 0) {
                if (current->left != NULL) {
                    current = current->left;
                }
                else {
                    current->left = module;
                    break;
                }
            }
            else if (valence < 0) {
                if (current->right != NULL) {
                    current = current->right;
                }
                else {
                    current->right = module;
                    break;
                }
            }
            else {
                return FALSE;
            }
        }
    }
    if (firstLibraryModule == NULL) {
        firstLibraryModule = module;
    }
    else {
        lastLibraryModule->next = module;
    }
    lastLibraryModule = module;

    return TRUE;
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
            eprintf("Path too long: %s", inPath);
            exit(1);
        }
        *op++ = *ip;
    }
#if !defined(__cos)
    if (dp == NULL) {
        ip = suffix;
        while (*ip != '\0' && op < limit) {
            *op++ = *ip++;
        }
        if (op >= limit) {
            eprintf("Path too long: %s", inPath);
            exit(1);
        }
    }
#endif
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
        valence = idcmp(current->id, new->id, 8);
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
            eprintf("Duplicate entry point %s defined in module %s, previously defined in module %s",
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

static void calculateBaseAddresses(Block *block) {
    u32 limit;

    while (block != NULL) {
        if (block->isAbsolute) {
            block->baseAddress = 0;
            limit = block->origin + block->length;
            if (limit > blockLimit) blockLimit = limit;
        }
        else {
            block->baseAddress = blockLimit;
            blockLimit = blockLimit + block->length;
        }
        if (blockLimit > imageSize) imageSize = blockLimit;
        block = block->nextInImage;
    }
}

static void calculateModuleName(char *path, u8 *name) {
    char *cp;
    Fnv32_t hash;
    char *lastPeriod;
    char *lastSlash;
    int len;
    char *limit;
    char *start;

    lastPeriod = NULL;
    lastSlash = NULL;
    for (cp = path; *cp != '\0'; cp++) {
        if (*cp == '/' || *cp == '\\')
            lastSlash = cp;
        else if (*cp == '.')
            lastPeriod = cp;
    }
    start = (lastSlash != NULL) ? lastSlash + 1 : path;
    limit = (lastPeriod > lastSlash) ? lastPeriod : path + strlen(path);
    memset(name, 0, 9);
    len = limit - start;
    if (len < 9) {
        memcpy(name, start, len);
    }
    else {
        hash = fnv32a((char *)name, len, FNV1_32A_INIT);
        memcpy(name, start, 4);
        sprintf((char *)(name + 4), "%04x", hash & 0xffff);
    }
}

static int collectLibraryModules(Dataset *ds, char *sourcePath) {
    int blockWordCount;
    u8 *entries;
    int entryWordCount;
    int externWordCount;
    u64 hdr;
    Module *module;
    int n;
    int offset;
    int status;
    u8 *table;
    int tableLength;
    u64 word;

    //
    //  Locate and process all DFT's
    //
    for (;;) {
        status = locateTable(ds, LDR_TT_DFT, &hdr, &tableLength, sourcePath);
        if (status == -1 || status == 0) return status;
        //
        //  Process a DFT
        //
        table = (u8 *)allocate(tableLength);
        n = cosDsRead(ds, table, tableLength);
        if (n != tableLength) {
            eprintf("Failed to read DFT in %s", sourcePath);
            free(table);
            return -1;
        }
        word = getWord(table);
        externWordCount = (word >> 24) & 0x7fff;
        entryWordCount  = (word >> 9) & 0x7fff;
        blockWordCount  = word & 0x1ff;
        if (entryWordCount > 0 || externWordCount > 0) {
            module = (Module *)allocate(sizeof(Module));
            module->libraryPath = sourcePath;
            offset = 8;
            memcpy(module->id, table + offset, 8);
            offset += (blockWordCount * 8) + 16;
            if (entryWordCount > 0) {
                module->entryCount = entryWordCount;
                n = entryWordCount * 8;
                entries = (u8 *)allocate(n);
                module->entryTable = entries;
                memcpy(entries, table + offset, n);
                offset += n;
            }
            if (externWordCount > 0) {
                module->externalRefCount = externWordCount;
                n = externWordCount * 8;
                entries = (u8 *)allocate(n);
                module->externalRefTable = entries;
                memcpy(entries, table + offset, n);
                offset += n;
            }
#if DEBUG
            eprintf("Collect %d entry names and %d external reference names from module %.8s of library %s", entryWordCount, externWordCount, module->id, sourcePath);
#endif
            if (addLibraryModule(module) == FALSE) {
                eprintf("WARNING: Duplicate module name %.8s in library %s", module->id, sourcePath);
            }
        }
        free(table);
    }
}

static Block *findBlock(Module *module, int blockIndex) {
    Block *block;

    block = module->firstBlock;
    while (blockIndex-- > 0 && block != NULL) block = block->nextInModule;
    return block;
}

static Module *findLibraryEntry(u8 *id) {
    int i;
    Module *module;
    int offset;

    for (module = firstLibraryModule; module != NULL; module = module->next) {
        for (i = 0, offset = 0; i < module->entryCount; i++, offset += 8) {
            if (idcmp(id, module->entryTable + offset, 8) == 0) return module;
        }
    }

    return NULL;
}

static Module *findLibraryModule(u8 *id) {
    Module *current;
    int valence;

    current = libraryModuleTree;
    while (current != NULL) {
        valence = idcmp(current->id, id, 8);
        if (valence > 0)
            current = current->left;
        else if (valence < 0)
            current = current->right;
        else
            break;
    }
    return current;
}

static Symbol *findSymbol(u8 *id) {
    Symbol *current;
    int valence;

    current = symbolTable;
    while (current != NULL) {
        valence = idcmp(current->id, id, 8);
        if (valence > 0)
            current = current->left;
        else if (valence < 0)
            current = current->right;
        else
            break;
    }
    return current;
}

static u64 formMask(int len) {
    u64 mask;

    switch (len) {
    case 8:
        mask = 0xff;
        break;
    case 16:
        mask = 0xffff;
        break;
    case 22:
        mask = 0x3fffff;
        break;
    case 24:
        mask = 0xffffff;
        break;
    case 32:
        mask = 0xffffffff;
        break;
    case 64:
        mask = 0xffffffffffffffff;
        break;
    default:
        mask = 0;
        while (len-- > 0) {
            mask = (mask << 1) | 1;
        }
        break;
    }

    return mask;
}

static char *getBlockType(BlockType type) {
    switch (type) {
    case BlockType_Common : return "Common";
    case BlockType_Mixed  : return "Mixed";
    case BlockType_Code   : return "Code";
    case BlockType_Data   : return "Data";
    case BlockType_Const  : return "Const";
    case BlockType_Dynamic: return "Dynamic";
    case BlockType_TaskCom: return "TaskCom";
    default: break;
    }
    return "Unknown";
}

static u64 getField(u8 *bytes, u32 rightmostBit, u16 fieldLength) {
    u32 byteOffset;
    u64 field;
    u64 mask;
    int shiftCount;

    byteOffset = (rightmostBit >> 3) - 7;
    mask = formMask(fieldLength);
    if ((rightmostBit & 7) == 7) { /* byte-aligned */
        field = getWord(bytes + byteOffset);
    }
    else {
        field = getWord(bytes + byteOffset);
        shiftCount = 7 - (rightmostBit & 7);
        field >>= shiftCount;
        if (fieldLength >= 56) {
            field = field | ((u64)bytes[byteOffset - 1] << (64 - shiftCount));
        }
    }
    return field & mask;
}

static char *getTableType(u8 type) {
    switch (type) {
    case LDR_TT_PWT: return "PWT";
    case LDR_TT_DMT: return "DMT";
    case LDR_TT_DFT: return "DFT";
    case LDR_TT_SMT: return "SMT";
    case LDR_TT_DPT: return "DPT";
    case LDR_TT_XRT: return "XRT";
    case LDR_TT_BRT: return "BRT";
    case LDR_TT_TXT: return "TXT";
    case LDR_TT_PDT: return "PDT";
    default: break;
    }
    return "unknown table";
}

static u64 getWord(u8 *bytes) {
    int i;
    u64 word;

    word = 0;
    for (i = 0; i < 8; i++)
        word = (word << 8) | *bytes++;
    return word;
}

static int idcmp(u8 *id1, u8 *id2, int len) {
    return strncasecmp((char *)id1, (char *)id2, len);
}

static int isLibrary(Dataset *ds, int pass, char *sourcePath, char **cachedPath) {
    u8 buf[8];
    char *cp;
    u64 hdr;
    int i;
    int n;
    int status;
    u8 tableType;

    status = 0;
    *cachedPath = NULL;
    if (pass == 1) {
        n = cosDsRead(ds, buf, 8);
        cosDsRewind(ds);
        if (n != 8) return -1;
        hdr = getWord(buf);
        tableType = hdr >> 60;
        if (tableType == LDR_TT_DFT) {
            if (libraryCount >= MAX_LIBRARIES) {
                eprintf("Too many libraries specified, max is %d", MAX_LIBRARIES);
                exit(1);
            }
            cp = (char *)allocate(strlen(sourcePath) + 1);
            strcpy(cp, sourcePath);
            libraryPaths[libraryCount++] = cp;
            *cachedPath = cp;
            status = 1;
        }
    }
    else {
        for (i = 0; i < libraryCount; i++) {
            if (strcmp(libraryPaths[i], sourcePath) == 0) {
                *cachedPath = libraryPaths[i];
                status = 1;
                break;
            }
        }
    }

    return status;
}

static int loadLibraryModule(Dataset *ds, Module *module, char *libraryPath, int pass, u64 *tableHeader) {
    u8 buf[8];
    u64 hdr;
    bool isFound;
    int n;
    int offset;
    int status;
    u8 *table;
    int tableLength;
    u8 tableType;
    u64 wc;
    u64 word;

#if DEBUG
    eprintf("Load module %.8s from library %s", module->id, libraryPath);
#endif
    //
    //  Position to PDT
    //
    status = locateTable(ds, LDR_TT_PDT, &hdr, &tableLength, libraryPath);
    if (status != 1) {
        eprintf("Module %.8s in library %s has no PDT", module->id, libraryPath);
        return -1;
    }

    currentModule = module;

    if (pass == 1) {
        table = (u8 *)allocate(tableLength);
        n = cosDsRead(ds, table, tableLength);
        if (n != tableLength) {
            eprintf("Failed to read PDT in %s", libraryPath);
            free(table);
            return -1;
        }
        if (processPDT(ds, module->id, hdr, table, tableLength) == -1) {
            free(table);
            return -1;
        }
        free(table);

        return 0; /* locate next DFT */
    }
    else { // pass 2
        if (skipBytes(ds, tableLength) == -1) {
            eprintf("Failed to skip PDT in %s", libraryPath);
            return -1;
        }
        for (;;) {
            n = cosDsRead(ds, buf, 8);
            if (n == -1) {
                eprintf("Failed to read library %s", libraryPath);
                return -1;
            }
            if (n == 0) return 2; /* end of file */
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
                *tableHeader = hdr;
                return 1; /* positioned at DFT header */
            default:
                if (skipBytes(ds, tableLength) == -1) {
                    eprintf("Failed to skip %s in %s", getTableType(tableType), libraryPath);
                    return -1;
                }
                break;
            }
        }
    }
}

static int loadLibraryModules(int pass) {
    Dataset *ds;
    u64 hdr;
    int i;
    Module *module;
    u8 *moduleId;
    int n;
    char *path;
    int state;
    int status;
    u8 *table;
    int tableLength;

    for (i = 0; i < libraryCount; i++) {
        path = libraryPaths[i];
        ds = cosDsOpen(path);
        if (ds == NULL) {
            eprintf("Failed to open %s", path);
            return -1;
        }
        state = 0;
        for (;;) {
            /*
             *  State 0: Locate next DFT
             */
            if (state == 0) {
                status = locateTable(ds, LDR_TT_DFT, &hdr, &tableLength, path);
                if (status != 1) {
                    cosDsClose(ds);
                    if (status == 0) break;
                    return status;
                }
            }
            /*
             *  State 1: DFT header read
             */
            else if (state == 1) {
                tableLength = (((hdr >> 24) & 0xffffff) - 1) * 8;
                state = 0;
            }
            /*
             *  State 2: End of file
             */
            else /* state == 2 */ {
                cosDsClose(ds);
                break;
            }
            table = (u8 *)allocate(tableLength);
            n = cosDsRead(ds, table, tableLength);
            if (n != tableLength) {
                eprintf("Failed to read DFT in %s", path);
                free(table);
                cosDsClose(ds);
                return -1;
            }
            moduleId = table + 8;
            module = findLibraryModule(moduleId);
            if (module != NULL && module->doLoad) {
                state = loadLibraryModule(ds, module, path, pass, &hdr);
                if (state == -1) {
                    eprintf("Failed to load module %.8s from %s", moduleId, path);
                    free(table);
                    cosDsClose(ds);
                    return -1;
                }
            }
            free(table);
        }
    }

    return 0;
}

static int loadObjectModules(Dataset *ds, u8 *moduleId, int pass) {
    u8 buf[8];
    u64 cw;
    u64 hdr;
    Module *module;
    int n;
    u8 *table;
    int tableLength;
    u8 tableType;
    u64 wc;

#if DEBUG
    eprintf("Load object module %.8s", moduleId);
#endif
    for (;;) {
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
                //
                //  Append new module to module list
                //
                module = (Module *)allocate(sizeof(Module));
                memcpy(module->id, moduleId, 8);
                if (firstObjectModule == NULL)
                    firstObjectModule = module;
                else
                    lastObjectModule->next = module;
                lastObjectModule = currentModule = module;

                if (processPDT(ds, moduleId, hdr, table, tableLength) == -1) {
                    free(table);
                    return -1;
                }
                free(table);
                continue;
            }
            else if (currentModule == NULL) {
                currentModule = firstObjectModule;
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
            eprintf("Warning: unrecognized table type: %02o", tableType);
            break;
        }
        if (skipBytes(ds, tableLength) == -1) return -1;
    }
}

static int locateTable(Dataset *ds, u8 tableType, u64 *hdr, int *tableLength, char *sourcePath) {
    u8 buf[8];
    u64 cw;
    int n;
    int tl;
    u8 tt;
    u64 word;
    u64 wc;

    for (;;) {
        n = cosDsRead(ds, buf, 8);
        if (n == -1) {
            eprintf("Failed to read table header from %s", sourcePath);
            return -1;
        }
        if (n == 0) {
            cw = cosDsReadCW(ds);
            if (cosDsIsEOF(cw) || cosDsIsEOD(cw)) return 0;
            continue;
        }
        word = getWord(buf);
        tt = word >> 60;
        if (tt == LDR_TT_DFT) {
            if (tableType != LDR_TT_DFT) return 0;
            wc = (word >> 24) & 0xffffff;
        }
        else {
            wc = (word >> 36) & 0xffffff;
        }
        tl = (wc - 1) * 8;
        if (tt == tableType) {
            *hdr = word;
            *tableLength = tl;
            return 1;
        }
        if (skipBytes(ds, tl) == -1) {
            eprintf("Failed to skip %s in %s", getTableType(tableType), sourcePath);
            return -1;
        }
    }
}

static int parseOptions(int argc, char *argv[]) {
    int i;
    int firstSrcIndex;

    firstSrcIndex = -1;
    i = 1;
    while (i < argc) {
        if (strcmp(argv[i], M_KEY) == 0) {
            i += 1;
            if (i >= argc) {
                usage();
            }
            mFile = argv[i];
            if (strcmp(mFile, STDOUT) == 0) {
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
#if defined(__cos)
        else if (strcmp(argv[i], DN_KEY) == 0
                 || strcmp(argv[i], LIB_KEY) == 0) {
            i += 1;
            if (i >= argc) {
                usage();
            }
            if (firstSrcIndex < 0) firstSrcIndex = i;
        }
        else if (strcmp(argv[i], AB_KEY) == 0) {
#else
        else if (strcmp(argv[i], O_KEY) == 0) {
#endif
            i += 1;
            if (i >= argc) {
                usage();
            }
            oFile = argv[i];
        }
#if defined(__cos)
        else if (strcmp(argv[i], "AB") == 0) {
            oFile = "$ABD";
        }
#endif
        else if (IS_KEY(argv[i])) {
            usage();
        }
        else {
            if (firstSrcIndex < 0) firstSrcIndex = i;
        }
        i += 1;
    }
    if (firstSrcIndex < 0) {
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
    fprintf(loadMap, "%8o%c", address, 'a' + parcelNumber);
}

static void printLoadMap(void) {
    Module *module;

    fprintf(loadMap,
        "1Load Map                                                         Cray X-MP %s %s            %s %s\n",
        ldrName, ldrVersion, currentDate, currentTime);
    fputs(" \n", loadMap);
    fprintf(loadMap, "       Program: %s\n", firstObjectModule->id);
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
    for (module = firstObjectModule; module != NULL; module = module->next) {
        printModuleSummary(module);
    }
    for (module = firstLibraryModule; module != NULL; module = module->next) {
        if (module->doLoad) printModuleSummary(module);
    }
}

static void printModuleSummary(Module *module) {
    Block *block;
    int i;
    u8 *id;
    Symbol *symbol;

    fprintf(loadMap, " \n Module: %.8s", module->id);
    fputs("\n   Section   Type     Idx  Address    Length\n", loadMap);
      fputs("   --------  -------  ---  ---------  ------\n", loadMap);
    for (block = module->firstBlock; block != NULL; block = block->nextInModule) {
        fprintf(loadMap, "   %-8.8s  %-7.7s  %3d  ", block->id, getBlockType(block->type), block->index);
        printAddress(block->baseAddress, FALSE);
        fprintf(loadMap, "  %6d\n", block->length);
    }
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
    Block *block;
    int blockIndex;
    u64 field;
    int fieldLength;
    u32 imageBytes;
    int imageOffset;
    bool isParcelRelocation;
    u32 parcelAddress;
    int shiftBias;
    Block *targetBlock;
    u64 word;

    blockIndex = (hdr >> 25) & 0x7f;
    targetBlock = findBlock(currentModule, blockIndex);
    if (targetBlock == NULL) {
        eprintf("Failed to find block %d referenced by BRT of module %s", blockIndex, currentModule->id);
        errorCount += 1;
        return skipBytes(ds, tableLength);
    }
    if (isSet(hdr, 28)) {
        //
        //  Process extended format table
        //
        while (tableLength > 0) {
            if (readWord(ds, &word)) return -1;
            tableLength -= 8;
            blockIndex = (word >> 38) & 0x7f;
            fieldLength = (word >> 32) & 0x3f;
            if (fieldLength == 0) fieldLength = 64;
            isParcelRelocation = (word >> 31) & 1;
            bitAddress = word & 0x3fffffff;
            block = findBlock(currentModule, blockIndex);
            if (block == NULL) {
                eprintf("Failed to find block %d referenced by extended relocation entry in BRT of module %s",
                        blockIndex, currentModule->id);
                errorCount += 1;
                continue;
            }
            bitAddress += targetBlock->baseAddress << 6;
            field = getField(image, bitAddress, fieldLength);
            field += isParcelRelocation ? block->baseAddress << 2 : block->baseAddress;
            putField(image, bitAddress, fieldLength, field);
        }
    }
    else {
        //
        //  Process standard format table
        //
        baseAddress = targetBlock->baseAddress;
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
                    eprintf("Failed to find block %d referenced by standard relocation entry in BRT of module %s",
                            blockIndex, currentModule->id);
                    errorCount += 1;
                    continue;
                }
                parcelAddress += baseAddress << 2;
                imageOffset = parcelAddress * 2;
                imageBytes = (image[imageOffset  ] << 24)
                           | (image[imageOffset+1] << 16)
                           | (image[imageOffset+2] <<  8)
                           |  image[imageOffset+3];
                if (isParcelRelocation) {
                    imageBytes += block->baseAddress << 2;
                }
                else {
                    imageBytes += block->baseAddress;
                }
                image[imageOffset  ] =  imageBytes >> 24;
                image[imageOffset+1] = (imageBytes >> 16) & 0xff;
                image[imageOffset+2] = (imageBytes >>  8) & 0xff;
                image[imageOffset+3] =  imageBytes        & 0xff;
            }
        }
    }
    return 0;
}

static int processPDT(Dataset *ds, u8 *moduleId, u64 hdr, u8 *table, int tableLength) {
    Block *block;
    int blockIndex;
    int blockType;
    int blockWordCount;
    int entryWordCount;
    int externalWordCount;
    int hdrLen;
    int i;
    int idx;
    bool isPrimary;
    bool isParcelAddress;
    bool isParcelRelocation;
    int n;
    u8 *name;
    int offset;
    Symbol *symbol;
    u64 word;

    blockWordCount = hdr & 0xff;
    entryWordCount = (hdr >> 8) & 0x3fff;
    externalWordCount = (hdr >> 22) & 0x3fff;
    hdrLen = getWord(table) & 0x3fff;
    offset = hdrLen * 8;
    //
    //  Build chain of blocks, if the module has any.
    //
    idx = 0;
    for (i = 0; i < blockWordCount; i += 2) {
        block = (Block *)allocate(sizeof(Block));
        block->module = currentModule;
        block->index = idx++;
        memcpy(block->id, table + offset, 8);
        offset += 8;
        word = getWord(table + offset);
        offset += 8;
        if (isSet(word, 1)) {
            eprintf("Warning: Section %s in module %s has error flag set", block->id, currentModule->id);
            block->hasErrorFlag = hasErrorFlag = TRUE;
        }
        if (isSet(word, 0)) {
            block->isAbsolute = 1;
            block->origin = (word >> 24) & 0xffffff;
            if (isSet(word, 4))
                block->type = BlockType_Code;
            else if (isSet(word, 2))
                block->type = BlockType_Common;
            else
                block->type = BlockType_Mixed;
            block->length = word & 0xffffff;
        }
        else {
            blockType = (word >> 54) & 0x3ff;
            if (blockType < BlockTypes) {
                block->type = (BlockType)blockType;
            }
            else {
                eprintf("Warning: Section %s in module %s has unknown block type %d", block->id, currentModule->id, blockType);
                block->type = BlockType_Mixed;
            }
            block->isExtMem = ((word >> 48) & 0x3f) == 2;
            block->length = word & 0xffffff;
        }
        addBlock(currentModule, block);
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
        block = findBlock(currentModule, blockIndex);
        if (block == NULL) {
            eprintf("Invalid block index %d in entry point definition %.8s of module %s",
                blockIndex, (char *)name, currentModule->id);
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
                eprintf("Warning: previous start symbol %s of module %s overrides start symbol %s of module %s",
                    startSymbol->id, startSymbol->block->module->id, symbol->id, currentModule->id);
            }
        }
    }
    //
    //  Process external reference declarations, if any
    //
    if (externalWordCount > 0) {
        n = externalWordCount * 8;
        if (currentModule->externalRefTable != NULL) {
            if (externalWordCount > currentModule->externalRefCount) {
                free(currentModule->externalRefTable);
                currentModule->externalRefTable = (u8 *)allocate(n);
            }
        }
        else {
            currentModule->externalRefTable = (u8 *)allocate(n);
        }
        currentModule->externalRefCount = externalWordCount;
        memcpy(currentModule->externalRefTable, table + offset, n);
        offset += n;
    }
    //
    //  Obtain comment, if any
    //
    offset += 11 * 8; // skip over fixed part of trailer to optional comment
    n = tableLength - offset;
    if (n > 0 && currentModule->comment == NULL) {
        currentModule->comment = (char *)allocate(n + 1);
        memcpy(currentModule->comment, table + offset, n);
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
            eprintf("TXT of module %s exceeds image size (load address %o, length %d)",
                currentModule->id, loadAddress, tableLength);
            errorCount += 1;
            return skipBytes(ds, tableLength);
        }
        n = cosDsRead(ds, image + imageOffset, tableLength);
        return (n == tableLength) ? 0 : -1;
    }
    else {
        eprintf("Failed to find block %d referenced by TXT of module %s", blockIndex, currentModule->id);
        errorCount += 1;
        return skipBytes(ds, tableLength);
    }
}

static int processXRT(Dataset *ds, u64 hdr, int tableLength) {
    u32 bitAddress;
    Block *block;
    int blockIndex;
    int extIndex;
    u64 field;
    u8 fieldLength;
    u8 *id;
    bool isParcelRelocation;
    u32 parcelAddress;
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
            eprintf("Failed to find block %d referenced by XRT of module %s", blockIndex, currentModule->id);
            errorCount += 1;
            continue;
        }
        if (extIndex >= currentModule->externalRefCount) {
            eprintf("Invalid external reference index %d in XRT of module %s", blockIndex, currentModule->id);
            errorCount += 1;
            continue;
        }
        id = currentModule->externalRefTable + (extIndex * 8);
        symbol = findSymbol(id);
        if (symbol == NULL) {
            eprintf("Unsatisfied external reference %.8s", id);
            errorCount += 1;
            continue;
        }
        bitAddress += block->baseAddress << 6;
        field = getField(image, bitAddress, fieldLength);
        if (isParcelRelocation) {
            if (symbol->isParcelAddress)
                field += symbol->value;
            else
                field += symbol->value << 2;
        }
        else if (symbol->isParcelAddress) {
            field += symbol->value >> 2;
        }
        else {
            field += symbol->value;
        }
        putField(image, bitAddress, fieldLength, field);
    }
    return 0;
}

static void putField(u8 *bytes, u32 rightmostBit, u16 fieldLength, u64 field) {
    u32 byteOffset;
    u64 mask;
    int shiftCount;
    u64 word;

    mask = formMask(fieldLength);
    field &= mask;
    byteOffset = (rightmostBit >> 3) - 7;
    if ((rightmostBit & 7) == 7) { /* byte-aligned */
        word = (getWord(bytes + byteOffset) & ~mask) | field;
        putWord(bytes + byteOffset, word);
    }
    else {
        shiftCount = 7 - (rightmostBit & 7);
        word = (getWord(bytes + byteOffset) & ~(mask << shiftCount)) | (field << shiftCount);
        putWord(bytes + byteOffset, word);
        if (fieldLength >= 56) {
            byteOffset -= 1;
            mask = formMask(shiftCount);
            bytes[byteOffset] = (bytes[byteOffset] & ~mask) | (field >> (64 - shiftCount));
        }
    }
}

static void putWord(u8 *bytes, u64 word) {
    int i;

    for (i = 7; i >= 0; i--) {
        *(bytes + i) = word & 0xff;
        word >>= 8;
    }
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

static bool resolveExternal(u8 *id) {
    int i;
    Module *module;

    module = findLibraryEntry(id);
    if (module != NULL) {
#if DEBUG
        eprintf("%.8s found in module %.8s of library %s", (char *)id, module->id, module->libraryPath);
#endif
        if (module->doLoad == FALSE) {
            module->doLoad = TRUE;
            if (resolveModuleExternals(module) == FALSE) return FALSE;
        }

        return TRUE;
    }
#if DEBUG
    eprintf("%.8s not found in any libraries", (char *)id);
#endif
    return FALSE;
}

static bool resolveExternals(void) {
    Module *module;

    for (module = firstObjectModule; module != NULL; module = module->next) {
        if (resolveModuleExternals(module) == FALSE) return FALSE;
    }
    return TRUE;
}

static bool resolveModuleExternals(Module *module) {
    int i;
    u8 *id;
    int offset;

    for (i = 0, offset = 0; i < module->externalRefCount; i++, offset += 8) {
        id = module->externalRefTable + offset;
        if (findSymbol(id) == NULL && resolveExternal(id) == FALSE) {
            return FALSE;
        }
    }
    return TRUE;
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
#if defined(__cos)
    eputs("Usage: LDR[,AB[=ofile]][,DN=rfile[:rfile...]][,LIB=lfile[:lfile...]][,M=mfile].");
    eputs("  AB=ofile  - output object file (default is $ABD)");
    eputs("  DN=rfile  - relocatable object file");
    eputs("  LIB=lfile - library file");
    eputs("  M=mfile   - load map file");
#else
    eputs("Usage: ldr [-m mfile][-o ofile] sfile...");
    eputs("  -m mfile - load map file");
    eputs("  -o ofile - output object file");
    eputs("  sfile    - source file(s)");
#endif
    exit(1);
}

static int writeExecutable(Dataset *ds) {
    if (writePDT(ds) == -1 || writeTXT(ds) == -1) return -1;
#if !defined(__cos)
    cosDsWriteEOR(ds);
    cosDsWriteEOF(ds);
    cosDsWriteEOD(ds);
#endif
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
        eputs("No start address");
        errorCount += 1;
    }
    pdtLen = 1                 // header word
           + 20                // header entry
           + 2                 // block count 1
           + (entryCount * 3)
           + 11;               // fixed portion of trailer
    if (firstObjectModule->comment != NULL) {
        pdtLen += (strlen(firstObjectModule->comment) + 7) / 8;
    }
    //
    //  Write header word
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
    if (writeName((char *)firstObjectModule->id, ds) == -1) return -1;
    word = (u64)1 << 63; // absolute flag
    if (hasErrorFlag || errorCount > 0) word |= (u64)1 << 62;
    word |= (u64)0200 << 24;        // program origin
    word |= blockLimit - 0200; // program size
    if (cosDsWriteWord(ds, word) == -1) return -1;
    //
    //  Write starting entry point
    //
    if (startSymbol != NULL) {
        id = (char *)startSymbol->id;
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
    if (writeString(firstObjectModule->comment, ds) == -1) return -1;

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
    //  Write header word
    //
    wordCount = blockLimit - 0200;
    byteCount = wordCount * 8;
    word = ((u64)LDR_TT_TXT << 60) | ((wordCount + 1) << 36) | 0200;
    if (cosDsWriteWord(ds, word) == -1) return -1;
    if (cosDsWrite(ds, image + (0200 * 8), byteCount) != byteCount) return -1;

    return 0;
}
