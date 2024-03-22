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
#include "services.h"
#if defined(__cos)
#include <sys/syslog.h>
#endif

static FILE *openExtText(char *fileName);
static int  openNextSource(int argi, int argc, char *argv[], bool *isExtText);
static void parseOptions(int argc, char *argv[]);
static void readEnvars(char *envp[]);
static void resetDefaultModule(void);
static void resetLocalSymbols(void);
static void resetQualifierStack(void);
static int  runPass(int passNo, bool isExtText);
static void timeInit(void);
static void usage(void);
static void writeObjectCode(void);

static u16 defaultListControl = LIST_ON|LIST_XRF|LIST_XNS|LIST_WEM|LIST_WMR;
static char *explicitIdent = NULL;
static char *lFile = NULL;
static char *oFile = NULL;
static char *textPath = NULL;

#if defined(__cos)
#define IS_KEY(s) (*((s) + strlen(s) - 1) == '=')
#define B_KEY "B="
#define F_KEY "F"
#define I_KEY "I="
#define L_KEY "L="
#define N_KEY "N="
#define R_KEY "R="
#define S_KEY "S"
#define T_KEY "T="
#define W_KEY "W"
#define X_KEY "X"
#define STDOUT "$OUT"
#else
#define IS_KEY(s) (*(s) == '-')
#define F_KEY "-f"
#define I_KEY "-i"
#define L_KEY "-l"
#define N_KEY "-n"
#define O_KEY "-o"
#define R_KEY "-r"
#define S_KEY "-s"
#define T_KEY "-t"
#define W_KEY "-w"
#define X_KEY "-x"
#define STDOUT "-"
#endif

int main(int argc, char *argv[], char *envp[]) {
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
    readEnvars(envp);
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
        runPass(1, isExtText);
        for (module = firstModule; module != NULL; module = module->next) {
            emitLiterals(module);
            createObjectBlocks(module);
            adjustSymbolValues(module);
        }
        runPass(2, isExtText);
        for (module = firstModule; module != NULL; module = module->next) {
            emitLiterals(module);
        }
        errCount += getErrorCount();
        warnCount += getWarningCount();
        listErrorSummary();
        listSymbolTable();
        writeObjectCode();
        fclose(sourceFile);
        if (lFile == NULL && listingFile != NULL) {
            fclose(listingFile);
            listingFile = NULL;
        }
        if (oFile == NULL && objectFile != NULL) {
#if defined(__cos)
            if (cosDsClose(objectFile) == -1) {
                eprintf("Failed to close object file for %s", argv[srcIndex - 1]);
                exit(1);
            }
#else
            if (cosDsWriteEOF(objectFile) == -1
                || cosDsWriteEOD(objectFile) == -1
                || cosDsClose(objectFile) == -1) {
                eprintf("Failed to write object file for %s", argv[srcIndex - 1]);
                exit(1);
            }
#endif
            objectFile = NULL;
        }
        if (isExtText) {
            listingFile = savedListingFile;
            objectFile = savedObjectFile;
            isFlexibleSyntax = savedSyntaxIndicator;
        }
    }
    if (lFile != NULL && listingFile != NULL) fclose(listingFile);
    if (oFile != NULL && objectFile != NULL) {
#if defined(__cos)
        if (cosDsClose(objectFile) == -1) {
            eputs("Failed to close object file");
            exit(1);
        }
#else
        if (cosDsWriteEOF(objectFile) == -1
            || cosDsWriteEOD(objectFile) == -1
            || cosDsClose(objectFile) == -1) {
            eputs("Failed to write object file");
            exit(1);
        }
#endif
    }
    if (warnCount > 0) eprintf("%d warning%s detected", warnCount, warnCount > 1 ? "s" : "");
    if (errCount > 0)  eprintf("%d error%s detected", errCount, errCount > 1 ? "s" : "");
    for (code = Err_DataItem; code <= Warn_RedefinedMacro; code++) {
        if ((errorUnion & (1 << code)) != 0) {
            eprintf("%-2s %s", getErrorIndicator(code), getErrorMessage(code));
        }
    }
    exit(errCount > 0 || (warnCount > 0 && isFatalWarnings));
}

static FILE *openExtText(char *fileName) {
    char *cp;
    char filePath[MAX_FILE_PATH_LENGTH+1];
    FILE *fp;
    int len;
    char *sp;

    if (textPath == NULL || *fileName == '.' || *fileName == '/' || *fileName == '\\') return NULL;
    sp = cp = textPath;
    while (TRUE) {
        while (*cp != '\0' && *cp != ':' && *cp != ';') cp += 1;
        len = cp - sp;
        if (len > 0) {
            memcpy(filePath, sp, len);
            filePath[len] = '/';
            strcpy(filePath + len + 1, fileName);
            fp = fopen(filePath, "r");
            if (fp != NULL) {
                strcpy(sourceFilePath, filePath);
                return fp;
            }
        }
        if (*cp == '\0') return NULL;
        cp += 1;
        sp = cp;
    }
}

static int openNextSource(int argi, int argc, char *argv[], bool *isExtText) {
    char *cp;
    char *dp;
    char filePath[MAX_FILE_PATH_LENGTH+5];
    char *fp;
    char *limit;

    *isExtText = FALSE;
    while (argi < argc) {
        if (!IS_KEY(argv[argi])) break;
        if (strcmp(argv[argi], F_KEY) == 0) {
            argi += 1;
        }
        else if (strcmp(argv[argi], I_KEY) == 0) {
            argi += 1;
            if (argi >= argc) break;
            if (!IS_KEY(argv[argi])) {
                *isExtText = FALSE;
                break;
            }
        }
        else if (strcmp(argv[argi], S_KEY) == 0) {
            argi += 1;
        }
        else if (strcmp(argv[argi], T_KEY) == 0) {
            argi += 1;
            if (argi >= argc) break;
            if (!IS_KEY(argv[argi])) {
                *isExtText = TRUE;
                break;
            }
        }
        else if (strcmp(argv[argi], W_KEY) == 0) {
            argi += 1;
        }
        else if (strcmp(argv[argi], X_KEY) == 0) {
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
             eprintf("Path too long: %s", argv[argi]);
             exit(1);
         }
         *fp++ = *cp;
    }
    *fp = '\0';
    argi += 1;
#if !defined(__cos)
    if (dp == NULL) {
        dp = fp;
        strcpy(dp, ".cal");
    }
#endif
    sourceFile = fopen(filePath, "r");
    if (sourceFile == NULL) {
        if (*isExtText) sourceFile = openExtText(filePath);
        if (sourceFile == NULL) {
            perror(filePath);
            exit(1);
        }
    }
    if (*isExtText) return argi;
    strcpy(sourceFilePath, filePath);

    if (lFile == NULL) {
#if defined(__cos)
        listingFile = stdout;
#else
        strcpy(dp, ".lst");
        listingFile = fopen(filePath, "w");
        if (listingFile == NULL) {
            perror(filePath);
            exit(1);
        }
#endif
    }
    if (oFile == NULL) {
#if defined(__cos)
        strcpy(filePath, "$BLD");
#else
        strcpy(dp, ".obj");
#endif
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
        if (strcmp(argv[i], F_KEY) == 0) {
            isFlexibleSyntax = TRUE;
        }
#if defined(__cos)
        else if (strcmp(argv[i], I_KEY) == 0) {
            i += 1;
            if (i >= argc || IS_KEY(argv[i])) {
                usage();
            }
            sourceCount += 1;
        }
#endif
        else if (strcmp(argv[i], L_KEY) == 0) {
            i += 1;
            if (i >= argc) {
                usage();
            }
            lFile = argv[i];
            if (strcmp(lFile, STDOUT) == 0) {
                listingFile = stdout;
            }
            else if (strcmp(lFile, "0") != 0) {
                listingFile = fopen(lFile, "w");
                if (listingFile == NULL) {
                    perror(lFile);
                    exit(1);
                }
            }
        }
        else if (strcmp(argv[i], N_KEY) == 0) {
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
            explicitIdent = sp;
        }
#if defined(__cos)
        else if (strcmp(argv[i], B_KEY) == 0) {
#else
        else if (strcmp(argv[i], O_KEY) == 0) {
#endif
            i += 1;
            if (i >= argc) {
                usage();
            }
            oFile = argv[i];
            if (strcmp(oFile, "0") != 0) {
                objectFile = cosDsCreate(oFile);
                if (objectFile == NULL) {
                    perror(oFile);
                    exit(1);
                }
            }
            else {
                objectFile = NULL;
            }
        }
        else if (strcmp(argv[i], S_KEY) == 0) {
            isSectionStackingEnabled = FALSE;
        }
#if !defined(__cos)
        else if (strcmp(argv[i], "-T") == 0) {
            i += 1;
            if (i >= argc || IS_KEY(argv[i])) {
                usage();
            }
            textPath = argv[i];
        }
#endif
        else if (strcmp(argv[i], T_KEY) == 0) {
            i += 1;
            if (i >= argc || IS_KEY(argv[i])) {
                usage();
            }
        }
        else if (strcmp(argv[i], W_KEY) == 0) {
            isFatalWarnings = TRUE;
        }
        else if (strcmp(argv[i], X_KEY) == 0) {
            isImplicitExternals = TRUE;
        }
        else if (IS_KEY(argv[i])) {
            usage();
        }
        else {
            sourceCount += 1;
        }
        i += 1;
    }
    if (sourceCount < 1) usage();
}

static void readEnvars(char *envp[]) {
    char *cp;
    char *env;
    int i;

    for (i = 0; envp[i] != NULL; i++) {
        env = envp[i];
        cp  = env;
        while (*cp != '\0' && *cp != '=') cp += 1;
        if (strncmp(env, "TEXTPATH", cp - env) == 0) {
            textPath = cp + 1;
            break;
        }
    }
}

void resetBase(void) {
    currentBase = 10;
    baseStackPtr = 0;
}

static void resetDefaultModule(void) {
    currentModule = defaultModule;
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

static int runPass(int passNo, bool isExtText) {
    ErrorCode err;
    Module *module;

    pass = passNo;
    listControlStackPtr = 0;
    currentListControl = defaultListControl;
    clearErrorIndications();
    resetBase();
    resetDefaultModule();
    resetLocalSymbols();
    resetQualifierStack();
    if (fseek(sourceFile, 0L, SEEK_SET) != 0) {
        eputs("Failed to rewind source file");
        exit(1);
    }
    if (explicitIdent != NULL && isExtText == FALSE) {
        if (pass == 1) {
            currentModule = addModule(explicitIdent, strlen(explicitIdent));
        }
        else { // pass == 2
            module = findModule(explicitIdent, strlen(explicitIdent));
            if (module == NULL) {
                eprintf("Module vanished in pass 2: %s", explicitIdent);
                exit(1);
            }
            resetModule(module);
            currentModule = module;
        }
        currentQualifier = findQualifier("");
        currentSection = currentModule->firstSection;
        sectionStackPtr = 0;
        macroStackPtr = 0;
        qualifierStackPtr = 0;
        listEject();
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
    int year;

    clock = time(NULL);
    tmp = localtime(&clock);
    year = tmp->tm_year >= 100 ? tmp->tm_year - 100 : tmp->tm_year;
    sprintf(currentDate, "%02d/%02d/%02d", tmp->tm_mon + 1, tmp->tm_mday, year);
    sprintf(currentTime, "%02d:%02d:%02d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
    sprintf(currentJDate, "%02d/%03d", year, tmp->tm_yday + 1);
}

static void usage(void) {
#if defined(__cos)
    eputs("Usage: CAL[,B=ofile][,F][,I=sfile][,L=lfile][,N=ident][,T=tfile]...[,W][,X].");
    eputs("  B=ofile - object file");
    eputs("  F       - enable flexible syntax");
    eputs("  I=sfile - source file");
    eputs("  L=lfile - listing file");
    eputs("  N=ident - default module identifier");
    eputs("  S       - disable section stacking");
    eputs("  T=tfile - external text file");
    eputs("  W       - exit with error status on warning indications");
    eputs("  X       - enable implicit external symbols");
#else
    eputs("Usage: cal [-f][-l lfile][-n ident][-o ofile][-T dlist][-t tfile]...[-w][-x] sfile ...");
    eputs("  -f       - enable flexible syntax");
    eputs("  -l lfile - listing file");
    eputs("  -o ofile - object file");
    eputs("  -s       - disable section stacking");
    eputs("  -T dlist - text file directory list");
    eputs("  -t tfile - external text file");
    eputs("  -w       - exit with error status on warning indications");
    eputs("  -x       - enable implicit external symbols");
    eputs("  sfile - source file(s)");
#endif
    exit(1);
}

static void writeObjectCode(void) {
    Module *module;

    if (objectFile != NULL) {
        for (module = firstModule; module != NULL; module = module->next) {
            writeObjectRecord(module, objectFile);
        }
    }
}
