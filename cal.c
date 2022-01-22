/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: cal.c
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
#include <time.h>
#include "calproto.h"
#include "caltypes.h"
#include "cosdataset.h"

static void openNextSource(int argi, char *argv[]);
static int  parseOptions(int argc, char *argv[]);
static void resetDefaultModule(void);
static void resetQualifierStack(void);
static int  runPass(int passNo);
static void timeInit(void);
static void usage(void);
static void writeObjectCode(void);

static u16 defaultListControl = LIST_ON|LIST_XRF|LIST_XNS|LIST_WEM|LIST_WMR;
static char *lFile = NULL;
static char *oFile = NULL;

int main(int argc, char *argv[]) {
    int errCount;
    Module *module;
    int srcIndex;

    srcIndex = parseOptions(argc, argv);
    instInit();
    defaultModule = addModule("", 0);
    errCount = 0;
    while (srcIndex < argc) {
        openNextSource(srcIndex, argv);
        timeInit();
        listInit();
        firstModule = lastModule = NULL;
        runPass(1);
        for (module = firstModule; module != NULL; module = module->next) {
            emitLiterals(module);
            createObjectBlocks(module);
            adjustSymbolValues(module);
        }
        runPass(2);
        for (module = firstModule; module != NULL; module = module->next) {
            emitLiterals(module);
        }
        errCount += getErrorCount();
        listErrorSummary();
        listSymbolTable();
        writeObjectCode();
        fclose(sourceFile);
        if (lFile == NULL) fclose(listingFile);
        if (oFile == NULL) {
            if (cosDsWriteEOF(objectFile) == -1
                || cosDsWriteEOD(objectFile) == -1
                || cosDsClose(objectFile) == -1) {
                fprintf(stderr, "Failed to write object file for %s\n", argv[srcIndex]);
                exit(1);
            }
        }
        srcIndex += 1;
    }
    if (lFile != NULL) fclose(listingFile);
    if (oFile != NULL) {
        if (cosDsWriteEOF(objectFile) == -1
            || cosDsWriteEOD(objectFile) == -1
            || cosDsClose(objectFile) == -1) {
            fputs("Failed to write object file\n", stdout);
            exit(1);
        }
    }
    if (errCount > 0) {
        fprintf(stderr, "%d errors detected\n", errCount);
        exit(1);
    }
}

static void openNextSource(int argi, char *argv[]) {
    char *cp;
    char *dp;
    char filePath[MAX_FILE_PATH_LENGTH+5];
    char *fp;
    char *limit;

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
        strcpy(dp, ".cal");
    }
    sourceFile = fopen(filePath, "r");
    if (sourceFile == NULL) {
        perror(filePath);
        exit(1);
    }
    if (lFile == NULL) {
        strcpy(dp, ".lst");
        listingFile = fopen(filePath, "w");
        if (listingFile == NULL) {
            perror(filePath);
            exit(1);
        }
    }
    if (oFile == NULL) {
        strcpy(dp, ".obj");
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
        if (strcmp(argv[i], "-l") == 0) {
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

void resetBase(void) {
    currentBase = 10;
    baseStackPtr = 0;
}

static void resetDefaultModule(void) {
    currentModule = defaultModule;
    resetModule(currentModule);
    currentSection = currentModule->firstSection;
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

static void timeInit(void) {
    time_t clock;
    struct tm *tmp;

    clock = time(NULL);
    tmp = localtime(&clock);
    sprintf(currentDate, "%02d/%02d/%02d", tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_year - 100);
    sprintf(currentTime, "%02d:%02d:%02d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
    sprintf(currentJDate, "%02d/%03d", tmp->tm_year - 100, tmp->tm_yday + 1);
}

static void usage(void) {
    fputs("Usage: cal [-l lfile][-o ofile] sfile\n", stdout);
    fputs("  -l lfile - listing file\n", stderr);
    fputs("  -o ofile - object file\n", stderr);
    fputs("  sfile - source file\n", stderr);
    exit(1);
}

static void writeObjectCode(void) {
    Module *module;

    if (objectFile == NULL) return;

    for (module = firstModule; module != NULL; module = module->next) {
        writeObjectRecord(module, objectFile);
    }
}
