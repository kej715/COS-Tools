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
#include "fnv.h"

static int  openNextSource(int argi, int argc, char *argv[], bool *isExtText);
static void parseOptions(int argc, char *argv[]);
static void resetDefaultModule(void);
static void resetLocalSymbols(void);
static void resetQualifierStack(void);
static int  runPass(int passNo);
static void timeInit(void);
static void usage(void);
static void writeObjectCode(void);

static u16 defaultListControl = LIST_ON|LIST_XRF|LIST_XNS|LIST_WEM|LIST_WMR;
static char *lFile = NULL;
static char *oFile = NULL;

int main(int argc, char *argv[]) {
    ErrorCode code;
    int errCount;
    bool isExtText;
    Module *module;
    FILE *savedListingFile;
    Dataset *savedObjectFile;
    bool savedSyntaxIndicator;
    int srcIndex;
    int warnCount;

    defaultModule = addModule("", 0);
    parseOptions(argc, argv);
    instInit();
    errCount = 0;
    srcIndex = 1;
    warnCount = 0;
    while (srcIndex < argc) {
        srcIndex = openNextSource(srcIndex, argc, argv, &isExtText);
        timeInit();
        listInit();
        firstModule = lastModule = NULL;
        if (isExtText) {
            savedListingFile = listingFile;
            listingFile = NULL;
            savedObjectFile = objectFile;
            objectFile = NULL;
            savedSyntaxIndicator = isFlexibleSyntax;
            isFlexibleSyntax = FALSE;
        }
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
        warnCount += getWarningCount();
        listErrorSummary();
        listSymbolTable();
        writeObjectCode();
        fclose(sourceFile);
        if (lFile == NULL && listingFile != NULL) fclose(listingFile);
        if (oFile == NULL && objectFile != NULL) {
            if (cosDsWriteEOF(objectFile) == -1
                || cosDsWriteEOD(objectFile) == -1
                || cosDsClose(objectFile) == -1) {
                fprintf(stderr, "Failed to write object file for %s\n", argv[srcIndex]);
                exit(1);
            }
        }
        if (isExtText) {
            listingFile = savedListingFile;
            objectFile = savedObjectFile;
            isFlexibleSyntax = savedSyntaxIndicator;
        }
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
    if (warnCount > 0) fprintf(stderr, "%d warning%s detected\n", warnCount, warnCount > 1 ? "s" : "");
    if (errCount > 0)  fprintf(stderr, "%d error%s detected\n", errCount, errCount > 1 ? "s" : "");
    for (code = Err_DataItem; code <= Warn_RedefinedMacro; code++) {
        if ((errorUnion & (1 << code)) != 0) {
            fprintf(stderr, "%-2s %s\n", getErrorIndicator(code), getErrorMessage(code));
        }
    }
    exit(errCount > 0 || (warnCount > 0 && isFatalWarnings));
}

static int openNextSource(int argi, int argc, char *argv[], bool *isExtText) {
    char *cp;
    char *dp;
    char filePath[MAX_FILE_PATH_LENGTH+5];
    char *fp;
    char *limit;

    *isExtText = FALSE;
    while (argi < argc) {
        if (*argv[argi] != '-') break;
        if (strcmp(argv[argi], "-f") == 0) {
            argi += 1;
        }
        else if (strcmp(argv[argi], "-t") == 0) {
            argi += 1;
            if (argi >= argc) break;
            if (*argv[argi] != '-') {
                *isExtText = TRUE;
                break;
            }
        }
        else if (strcmp(argv[argi], "-w") == 0) {
            argi += 1;
        }
        else if (strcmp(argv[argi], "-x") == 0) {
            argi += 1;
        }
        else {
            argi += 2;
        }
    }
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
    argi += 1;
    if (dp == NULL) {
        dp = fp;
        strcpy(dp, ".cal");
    }
    sourceFile = fopen(filePath, "r");
    if (sourceFile == NULL) {
        perror(filePath);
        exit(1);
    }
    if (*isExtText) return argi;
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
    return argi;
}

static void parseOptions(int argc, char *argv[]) {
    char *cp;
    Fnv32_t hash;
    int i;
    int len;
    int sourceCount;
    char *sp;

    sourceCount = 0;
    i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-f") == 0) {
            isFlexibleSyntax = TRUE;
        }
        else if (strcmp(argv[i], "-i") == 0) {
            i += 1;
            if (i >= argc) {
                usage();
            }
            cp = argv[i];
            sp = NULL;
            while (*cp != '\0') {
                if (*cp == '/' || *cp == '\\') sp = cp;
                cp += 1;
            }
            sp = sp != NULL ? sp + 1 : argv[i];
            for (cp = sp; *cp != '\0'; cp++) {
                if (*cp == '.') {
                    *cp = '\0';
                    break;
                }
            }
            len = strlen(sp);
            if (len > MAX_NAME_LENGTH) {
                hash = fnv32a(sp, len, FNV1_32A_INIT);
                sprintf(sp + 4, "%04x", hash & 0xffff);
            }
            defaultModule = addModule(sp, strlen(sp));
        }
        else if (strcmp(argv[i], "-l") == 0) {
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
        else if (strcmp(argv[i], "-t") == 0) {
            i += 1;
            if (i >= argc || *argv[i] == '-') {
                usage();
            }
        }
        else if (strcmp(argv[i], "-w") == 0) {
            isFatalWarnings = TRUE;
        }
        else if (strcmp(argv[i], "-x") == 0) {
            isImplicitExternals = TRUE;
        }
        else if (*argv[i] == '-') {
            usage();
        }
        else {
            sourceCount += 1;
        }
        i += 1;
    }
    if (sourceCount < 1) usage();
}

void resetBase(void) {
    currentBase = 10;
    baseStackPtr = 0;
}

static void resetDefaultModule(void) {
    currentModule = defaultModule;
    if (currentModule->id[0] != '\0') {
        firstModule = lastModule = currentModule;
    }
    resetModule(currentModule);
    currentQualifier = findQualifier("");
    currentSection = currentModule->firstSection;
    sectionStackPtr = 0;
    macroStackPtr = 0;
    qualifierStackPtr = 0;
}

static void resetLocalSymbols(void) {
    int i;

    for (i = 0; i < MAX_LOCAL_SYMBOLS; i++) {
        localSymbolCtrs[i] = 0;
    }
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
    resetLocalSymbols();
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
    fputs("Usage: cal [-f][-i ident][-l lfile][-o ofile][-t tfile]...[-x] sfile ...\n", stdout);
    fputs("  -f       - enable flexible syntax\n", stderr);
    fputs("  -i ident - default module identifier\n", stderr);
    fputs("  -l lfile - listing file\n", stderr);
    fputs("  -o ofile - object file\n", stderr);
    fputs("  -t tfile - external text file\n", stderr);
    fputs("  -w       - exit with error status on warning indications\n", stderr);
    fputs("  -x       - enable implicit external symbols\n", stderr);
    fputs("  sfile - source file(s)\n", stderr);
    exit(1);
}

static void writeObjectCode(void) {
    Module *module;

    if (objectFile == NULL) return;

    for (module = firstModule; module != NULL; module = module->next) {
        writeObjectRecord(module, objectFile);
    }
}
