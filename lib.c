/*--------------------------------------------------------------------------
**
**  Copyright 2023 Kevin E. Jordan
**
**  Name: lib.c
**
**  Description:
**      This file is the main module of the COS library utility.
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
#include "libconst.h"
#include "libproto.h"
#include "libtypes.h"
#include "services.h"

static void addBlock(Module *module, char *id);
static void addEntry(Module *module, char *id);
static void addExternal(Module *module, char *id);
static Module *addModule(char *id);
static void addSuffix(char *inPath, char *suffix, char *outPath);
static Module *findModule(char *id);
static u64 getWord(u8 *bytes);
static bool isOmittedName(char *id, char *argv[]);
static u64 copyModules(Dataset *ds, char *argv[]);
static int parseOptions(int argc, char *argv[]);
static void printListing(FILE *listingFile);
static void printModules(Module *module, FILE *listingFile);
static int printSymbols(Symbol *symbol, int ordinal, FILE *listingFile);
static int processPDT(Dataset *ds, u64 hdr, int tableLength, char *argv[]);
static int skipBytes(Dataset *ds, int count);
static void usage(void);
static int writeBytes(Dataset *ds, u8 *buf, int len);
static int writeDirectory(Dataset *ds);
static int writeEOR(Dataset *ds);
static int writeName(char *name, Dataset *ds);
static int writeNames(Symbol *symbol, Dataset *ds);
static int writeWord(Dataset *ds, u64 word);

static char    currentDate[9];
static char    currentTime[9];
static int     firstOmittedNameIdx = -1;
static int     firstSourceFileIdx = -1;
static Module  *lastModule = NULL;
static int     lastOmittedNameIdx = -1;
static char    *libName = "xLIB";
static char    *libVersion = "0.1";
static FILE    *listingFile = NULL;
static char    *lFile = NULL;
static Module  *modules = NULL;
static char    *oFile = NULL;
static Dataset *outputFile = NULL;

int main(int argc, char *argv[]) {
    time_t clock;
    u64 cw;
    char *dp;
    Dataset *ds;
    int fileIndex;
    char outputPath[MAX_FILE_PATH_LENGTH+1];
    char *op;
    char sourcePath[MAX_FILE_PATH_LENGTH+1];
    char *sp;
    char tempPath[MAX_FILE_PATH_LENGTH+1];
    struct tm *tmp;

    clock = time(NULL);
    tmp = localtime(&clock);
    sprintf(currentDate, "%02d/%02d/%02d", tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_year - 100);
    sprintf(currentTime, "%02d:%02d:%02d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

    firstSourceFileIdx = parseOptions(argc, argv);

    if (oFile != NULL) {
        dp = NULL;
        op = tempPath;
        sp = oFile;
        while (*sp != '\0') {
            if (*sp == '/' || *sp == '\\')
                dp = NULL;
            else if (*sp == '.')
                dp = op;
            *op++ = *sp++;
        }
        strcpy(dp != NULL ? dp : op, ".tmp");
        outputFile = cosDsCreate(tempPath);
        if (outputFile == NULL) {
            perror(tempPath);
            exit(1);
        }
    }

    //
    //  Traverse source files, distinguishing libraries from plain object files, and build
    //  a tree of modules with their entrypoint and external reference symbols. Copy each unique
    //  module to the output file and append a directory record at the end.
    //  
    fileIndex = firstSourceFileIdx;
    while (fileIndex < argc) {
        addSuffix(argv[fileIndex], ".obj", sourcePath);
        ds = cosDsOpen(sourcePath);
        if (ds == NULL) {
            fprintf(stderr, "Failed to open %s\n", sourcePath);
            exit(1);
        }
        while (TRUE) {
            cw = copyModules(ds, argv);
            if (cw == -1) {
                fprintf(stderr, "Failed to copy modules from %s\n", sourcePath);
                exit(1);
            }
            else if (cosDsIsEOF(cw) || cosDsIsEOD(cw)) {
                break;
            }
        }
        cosDsClose(ds);
        fileIndex += 1;
    }
    if (outputFile != NULL) {
        if (writeDirectory(outputFile) == -1
            || cosDsWriteEOR(outputFile) == -1
            || cosDsWriteEOF(outputFile) == -1
            || cosDsWriteEOD(outputFile) == -1
            || cosDsClose(outputFile) == -1) {
            fprintf(stderr, "Failed to write output file %s\n", tempPath);
            unlink(tempPath);
            exit(1);
        }
        addSuffix(oFile, ".lib", outputPath);
        unlink(outputPath);
        if (rename(tempPath, outputPath) == -1) {
            perror(outputPath);
            fprintf(stderr, "Failed to rename %s to %s\n", tempPath, outputPath);
            exit(1);
        }
    }
    if (listingFile != NULL) {
        printListing(listingFile);
        fclose(listingFile);
    }
}

static void addBlock(Module *module, char *id) {
    Symbol *current;
    Symbol *new;
    int valence;

    new = (Symbol *)allocate(sizeof(Symbol));
    new->id = (char *)allocate(9);
    memcpy(new->id, id, 8);
    current = module->blocks;
    if (current == NULL) {
        module->blocks = new;
        module->blockCount = 1;
        return;
    }
    while (current != NULL) {
        valence = strcmp(current->id, new->id);
        if (valence > 0) {
            if (current->left != NULL) {
                current = current->left;
            }
            else {
                current->left = new;
                module->blockCount += 1;
                break;
            }
        }
        else if (valence < 0) {
            if (current->right != NULL) {
                current = current->right;
            }
            else {
                current->right = new;
                module->blockCount += 1;
                break;
            }
        }
        else {
            free(new->id);
            free(new);
            break;
        }
    }
}

static void addEntry(Module *module, char *id) {
    Symbol *current;
    Symbol *new;
    int valence;

    new = (Symbol *)allocate(sizeof(Symbol));
    new->id = (char *)allocate(9);
    memcpy(new->id, id, 8);
    current = module->entries;
    if (current == NULL) {
        module->entries = new;
        module->entryCount = 1;
        return;
    }
    while (current != NULL) {
        valence = strcmp(current->id, new->id);
        if (valence > 0) {
            if (current->left != NULL) {
                current = current->left;
            }
            else {
                current->left = new;
                module->entryCount += 1;
                break;
            }
        }
        else if (valence < 0) {
            if (current->right != NULL) {
                current = current->right;
            }
            else {
                current->right = new;
                module->entryCount += 1;
                break;
            }
        }
        else {
            free(new->id);
            free(new);
            break;
        }
    }
}

static void addExternal(Module *module, char *id) {
    Symbol *current;
    Symbol *new;
    int valence;

    new = (Symbol *)allocate(sizeof(Symbol));
    new->id = (char *)allocate(9);
    memcpy(new->id, id, 8);
    current = module->externals;
    if (current == NULL) {
        module->externals = new;
        module->externalCount = 1;
        return;
    }
    while (current != NULL) {
        valence = strcmp(current->id, new->id);
        if (valence > 0) {
            if (current->left != NULL) {
                current = current->left;
            }
            else {
                current->left = new;
                module->externalCount += 1;
                break;
            }
        }
        else if (valence < 0) {
            if (current->right != NULL) {
                current = current->right;
            }
            else {
                current->right = new;
                module->externalCount += 1;
                break;
            }
        }
        else {
            free(new->id);
            free(new);
            break;
        }
    }
}

static Module *addModule(char *id) {
    Module *current;
    Module *new;
    int valence;

    new = (Module *)allocate(sizeof(Module));
    new->id = (char *)allocate(9);
    memcpy(new->id, id, 8);
    current = modules;
    if (current == NULL) {
        modules = lastModule = new;
        return new;
    }
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
            fprintf(stderr, "Logic error - duplicate module detected: %s\n", new->id);
            exit(1);
        }
    }
    lastModule->next = new;
    lastModule = new;
    return new;
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

static u64 copyModules(Dataset *ds, char *argv[]) {
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
            fputs("Failed to read source file\n", stderr);
            return -1;
        }
        else if (n == 0) {
            cw = cosDsReadCW(ds);
            if (cosDsIsEOR(cw) && writeEOR(outputFile) == -1) return -1;
            return cw;
        }
        hdr = getWord(buf);
        tableType = hdr >> 60;
        if (tableType == LDR_TT_DFT) {
            wc = (hdr >> 24) & 0xffffff;
        }
        else {
            wc = (hdr >> 36) & 0xffffff;
        }
        tableLength = (wc - 1) * 8;
        switch (tableType) {
        case LDR_TT_PDT:
            if (processPDT(ds, hdr, tableLength, argv) == -1) {
                fputs("Failed to process PDT\n", stderr);
                return -1;
            }
            break;
        case LDR_TT_DFT:
            if (skipBytes(ds, tableLength) == -1) {
                fputs("Failed to skip DFT\n", stderr);
                return -1;
            }
            break;
        default:
            if (writeWord(outputFile, hdr) == -1) return -1;
            while (tableLength > 0) {
                n = (tableLength > sizeof(buf)) ? sizeof(buf) : tableLength;
                if (cosDsRead(ds, buf, n) != n) {
                    fprintf(stderr, "Failed to read table type %02o from source file\n", tableType);
                    return -1;
                }
                if (writeBytes(outputFile, buf, n) != n) return -1;
                tableLength -= n;
            }
            break;
        }
    }
    return 0;
}

static Module *findModule(char *id) {
    Module *current;
    int valence;

    current = modules;
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

static bool isOmittedName(char *id, char *argv[]) {
    char *cp1;
    char *cp2;
    int i;

    if (firstOmittedNameIdx >= 0) {
        for (i = firstOmittedNameIdx; i <= lastOmittedNameIdx; i++) {
            cp1 = id;
            cp2 = argv[i];
            while (*cp1 == *cp2) {
                if (*cp1 == '\0') return TRUE;
                cp1 += 1;
                cp2 += 1;
            }
        }
    }
    return FALSE;
}

static int parseOptions(int argc, char *argv[]) {
    int i;
    int firstSrcIndex;

    firstSrcIndex = argc;
    i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-l") == 0) {
            if (firstOmittedNameIdx >= 0 && lastOmittedNameIdx < 0) lastOmittedNameIdx = i - 1;
            i += 1;
            if (i >= argc) {
                usage();
            }
            lFile = argv[i];
            if (strcmp(lFile, "-") == 0) {
                listingFile = stdout;
            }
            else {
                listingFile = fopen(lFile, "w");
                if (listingFile == NULL) {
                    perror(lFile);
                    exit(1);
                }
            }
        }
        else if (strcmp(argv[i], "-o") == 0) {
            if (firstOmittedNameIdx >= 0 && lastOmittedNameIdx < 0) lastOmittedNameIdx = i - 1;
            i += 1;
            if (i >= argc) {
                usage();
            }
            oFile = argv[i];
        }
        else if (strcmp(argv[i], "-r") == 0) {
            i += 1;
            if (i >= argc || *argv[i] == '-' || firstOmittedNameIdx >= 0) {
                usage();
            }
            firstOmittedNameIdx = i;
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

static void printListing(FILE *listingFile) {
    Module *module;

    fprintf(listingFile,
        "1Library Content                                                  Cray X-MP %s %s            %s %s\n ",
        libName, libVersion, currentDate, currentTime);
    printModules(modules, listingFile);
    fputs("\n", listingFile);
}

static void printModules(Module *module, FILE *listingFile) {
    int ordinal;

    if (module == NULL) return;

    printModules(module->left, listingFile);
    fprintf(listingFile, "\n Module: %s\n ", module->id);
    if (module->blocks != NULL) {
        fputs("  Blocks:\n   ", listingFile);
        ordinal = printSymbols(module->blocks, 0, listingFile);
        if (ordinal != 7) fputs("\n ", listingFile);
    }
    if (module->entries != NULL) {
        fputs("  Entry points:\n   ", listingFile);
        ordinal = printSymbols(module->entries, 0, listingFile);
        if (ordinal != 7) fputs("\n  ", listingFile);
    }
    if (module->externals != NULL) {
        fputs("  External references:\n   ", listingFile);
        ordinal = printSymbols(module->externals, 0, listingFile);
        if (ordinal != 7) fputs("\n ", listingFile);
    }
    printModules(module->right, listingFile);
}

static int printSymbols(Symbol *symbol, int ordinal, FILE *listingFile) {
    if (symbol == NULL) return ordinal;
    ordinal = printSymbols(symbol->left, ordinal, listingFile);
    fprintf(listingFile, "   %-8.8s", symbol->id);
    ordinal += 1;
    if ((ordinal % 8) == 0) fputs("\n   ", listingFile);
    return printSymbols(symbol->right, ordinal, listingFile);
}

static int processPDT(Dataset *ds, u64 hdr, int tableLength, char *argv[]) {
    int blockWordCount;
    int entryWordCount;
    int externalWordCount;
    int hdrLen;
    int i;
    Module *module;
    char moduleId[9];
    int n;
    char *name;
    int offset;
    Symbol *symbol;
    u8 *table;

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
    hdrLen = getWord(table) & 0x3fff;
    offset = hdrLen * 8;
    //
    //  Traverse chain of blocks, if the module has any.
    //
    if (blockWordCount > 0) {
        //
        //  Normally, a module has at least one block, and the first
        //  block is a program block.
        //
        memcpy(moduleId, table + offset, 8);
        moduleId[8] = '\0';
        module = findModule(moduleId);
        if (module == NULL && isOmittedName(moduleId, argv) == FALSE) {
            module = addModule(moduleId);
            //
            //  Process block names
            //
            for (i = 0; i < blockWordCount; i += 2) {
                name = (char *)(table + offset);
                offset += 16;
                addBlock(module, name);
            }
            //
            //  Process entry point definitions, if any
            //
            for (i = 0; i < entryWordCount; i += 3) {
                name = (char *)(table + offset);
                offset += 24;
                addEntry(module, name);
            }
            //
            //  Process external reference declarations, if any
            //
            for (i = 0; i < externalWordCount; i++) {
                name = (char *)(table + offset);
                offset += 8;
                addExternal(module, name);
            }
            //
            //  Write PDT of new module to output file
            //
            if (writeWord(outputFile, hdr) == -1
                || writeBytes(outputFile, table, tableLength) != tableLength) {
                free(table);
                return -1;
            }
        }
    }

    free(table);
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
    fputs("Usage: lib [-l lfile][-r name...][-o ofile] sfile...\n", stdout);
    fputs("  -l lfile - listing file\n", stderr);
    fputs("  -o ofile - output library file\n", stderr);
    fputs("  -r name  - name(s) of modules to omit from output library file\n", stderr);
    fputs("  sfile    - source object and library file(s)\n", stderr);
    exit(1);
}

static int writeDirectory(Dataset *ds) {
    int i;
    char *id;
    u64 dirLen;
    Module *module;
    int size;
    u32 startAddress;
    u64 word;

    if (ds == NULL) return 0;

    //
    //  Calculate directory size
    //
    dirLen = 1; // header word
    for (module = modules; module != NULL; module = module->next) {
         dirLen += module->blockCount + module->entryCount + module->externalCount + 3;
    }
    //
    //  Write directory header word
    //
    word = ((u64)LDR_TT_DFT << 60)
         | (dirLen << 24)
         | ('D' << 16)
         | ('0' << 8)
         | '1';
    if (cosDsWriteWord(ds, word) == -1) return -1;
    //
    //  Write module entries
    //
    for (module = modules; module != NULL; module = module->next) {
        word = ((u64)1 << 60)
             | ((u64)(module->blockCount + module->entryCount + module->externalCount + 3) << 39)
             | (module->externalCount << 24)
             | (module->entryCount    <<  9)
             | module->blockCount;
        if (cosDsWriteWord(ds, word) == -1) return -1;
        if (writeName(module->id, ds) == -1) return -1;
        word = 0; // TODO: set FWA of module
        if (cosDsWriteWord(ds, word) == -1) return -1;
        if (writeNames(module->blocks, ds) == -1
            || writeNames(module->entries, ds) == -1
            || writeNames(module->externals, ds) == -1) return -1;
    }

    return 0;
}

static int writeEOR(Dataset *ds) {
    if (ds != NULL) {
        if (cosDsWriteEOR(ds) == -1) {
            fputs("Failed to write EOR to output file\n", stderr);
            return -1;
        }
    }
    return 0;
}

static int writeName(char *name, Dataset *ds) {
    int i;
    int shiftCount;
    u64 word;

    if (ds != NULL) {
        word = 0;
        for (i = 0, shiftCount = 56; i < 8; i++, shiftCount -= 8) {
            if (*name != '\0') {
                word |= ((u64)*name++) << shiftCount;
            }
            else {
                break;
            }
        }
        if (cosDsWriteWord(ds, word) == -1) {
            fprintf(stderr, "Failed to write name '%s' to output file\n", name);
            return -1;
        }
    }
    return 0;
}

static int writeNames(Symbol *symbol, Dataset *ds) {
    if (symbol != NULL) {
        if (writeName(symbol->id, ds) == -1
            || writeNames(symbol->left, ds) == -1
            || writeNames(symbol->right, ds) == -1) return -1;
    }
    return 0;
}

static int writeWord(Dataset *ds, u64 word) {
    if (ds != NULL) {
        if (cosDsWriteWord(ds, word) == -1) {
            fputs("Failed to write word output file\n", stderr);
            return -1;
        }
    }
    return 0;
}

static int writeBytes(Dataset *ds, u8 *buf, int len) {
    int n;

    if (ds == NULL) return len;

    n = cosDsWrite(ds, buf, len);
    if (n != len) {
        if (n == -1) {
            fputs("Failed to write output file\n", stderr);
        }
        else {
            fprintf(stderr, "Truncated write to output file, %d != %d\n", len, n);
        }
    }
    return n;
}
