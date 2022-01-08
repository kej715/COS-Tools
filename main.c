/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: main.c
**
**  Description:
**      This file is the main module of the CAL assembler.
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
#include "cosdataset.h"
#include "proto.h"
#include "types.h"

static void parseArgs(int argc, char *argv[]);
static void resetDefaultModule(void);
static void resetQualifierStack(void);
static int  runPass(int passNo);
static void usage(void);
static void writeModuleImages(void);

static u16 defaultListControl = LIST_ON|LIST_XRF|LIST_XNS|LIST_WEM|LIST_WMR;

int main(int argc, char *argv[]) {
    Module *module;

    parseArgs(argc, argv);
    instInit();
    listInit();
    (void)addModule("", 0);
    firstModule = lastModule = NULL;
    runPass(1);
    for (module = firstModule; module != NULL; module = module->next) {
        module->isOriginSet = FALSE;
        currentModule = module;
        emitLiterals();
        calculateBlockOffsets(currentModule);
        adjustSymbolValues(currentModule);
    }
    runPass(2);
    for (module = firstModule; module != NULL; module = module->next) {
        currentModule = module;
        emitLiterals();
    }
    listErrorSummary();
    listSymbolTable();
    writeModuleImages();
    if (listingFile != NULL) fclose(listingFile);
}

static void parseArgs(int argc, char *argv[]) {
    char *cp;
    char filePath[MAX_FILE_PATH_LENGTH+1];
    int i;
    int len;
    char *lfile;
    char *ofile;
    char *sfile;

    if (argc < 2) {
        usage();
        exit(1);
    }
    lfile = ofile = sfile = NULL;
    i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-l") == 0) {
            i += 1;
            if (i >= argc) {
                usage();
                exit(1);
            }
            lfile = argv[i];
        }
        else if (strcmp(argv[i], "-o") == 0) {
            i += 1;
            if (i >= argc) {
                usage();
                exit(1);
            }
            lfile = argv[i];
        }
        else if (*argv[i] == '-') {
            usage();
            exit(1);
        }
        else {
            sfile = argv[i];
        }
        i += 1;
    }
    if (sfile == NULL) {
        usage();
        exit(1);
    }
    sourceFile = fopen(sfile, "r");
    if (sourceFile == NULL) {
        perror(argv[i]);
        exit(1);
    }
    if (lfile == NULL) {
        cp = rindex(sfile, '.');
        if (cp == NULL)
            cp = sfile + strlen(sfile);
        len = cp - sfile;
        if (len + 4 > MAX_FILE_PATH_LENGTH) {
            fprintf(stderr, "%s: file path too long\n", sfile);
            exit(1);
        }
        sprintf(filePath, "%.*s.lst", len, sfile);
        lfile = filePath;
    }
    if (strcmp(lfile, "-") == 0) {
        listingFile = stdout;
    }
    else {
        listingFile = fopen(lfile, "w");
        if (listingFile == NULL) {
            perror(lfile);
            exit(1);
        }
    }
    if (ofile == NULL) {
        cp = rindex(sfile, '.');
        if (cp == NULL)
            cp = sfile + strlen(sfile);
        len = cp - sfile;
        if (len + 4 > MAX_FILE_PATH_LENGTH) {
            fprintf(stderr, "%s: file path too long\n", sfile);
            exit(1);
        }
        sprintf(filePath, "%.*s.obj", len, sfile);
        ofile = filePath;
    }
    objectFile = cosDsCreate(ofile);
    if (objectFile == NULL) {
        perror(ofile);
        exit(1);
    }
}

void resetBase(void) {
    currentBase = 10;
    baseStackPtr = 0;
}

static void resetDefaultModule(void) {
    currentModule = findModule("", 0);
    resetModule(currentModule);
    currentBlock = currentModule->firstBlock;
}

static void resetQualifierStack(void) {
    qualifierStackPtr = 0;
    currentQualifier = findQualifier("");
}

static int runPass(int passNo) {
    ErrorCode err;

    pass = passNo;
    listControlStackPtr = 0;
    currentListControl = defaultListControl;
    clearErrorIndications();
    resetBase();
    resetDefaultModule();
    resetQualifierStack();
    if (fseek(sourceFile, 0L, SEEK_SET) != 0) {
        fputs("Failed to rewind source file\n", stderr);
        exit(1);
    }
    while (isEof() == FALSE) {
        listControlMask = LIST_ON;
        readNextLine();
        err = parseSourceLine();
        if (err == Info_ModuleEnd) {
            if (pass == 2) listSymbolTable();
            currentModule = findModule("", 0);
            currentQualifier = findQualifier("");
        }
    }
    return 0;
}

static void usage(void) {
    fputs("Usage: cal [-l lfile][-o ofile] sfile\n", stdout);
    fputs("  -l lfile - listing file\n", stderr);
    fputs("  -o ofile - object file\n", stderr);
    fputs("  sfile - source file\n", stderr);
}

static void writeModuleImages(void) {
    Module *module;

    if (objectFile == NULL) return;

    for (module = firstModule; module != NULL; module = module->next) {
        writeObjectFile(module, objectFile);
    }
    cosDsWriteEOD(objectFile);
    cosDsClose(objectFile);
}
