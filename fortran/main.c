/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: main.c
**
**  Description:
**      This file is the main module of the compiler.
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "const.h"
#include "proto.h"
#include "types.h"

static char *parseOptions(int argc, char *argv[]);
static void usage(void);

int main(int argc, char *argv[]) {
    char *cp;
    char *dp;
    int len;
    char name[9];
    char *sp;
    char *sourcePath;

    sourcePath = parseOptions(argc, argv);
    dp = NULL;
    sp = NULL;
    for (cp = sourcePath; *cp != '\0'; cp++) {
        if (*cp == '/' || *cp == '\\') {
            sp = cp;
            dp = NULL;
        }
        else if (*cp == '.') {
            dp = cp;
        }
    }
    sp = (sp == NULL) ? sourcePath : sp + 1;
    if (dp == NULL) dp = cp;
    len = 0;
    cp = name;
    *cp++ = '%';
    while (sp < dp && len < 7) {
        if (isalnum(*sp)) {
            *cp++ = toupper(*sp);
            len += 1;
        }
        sp += 1;
    }
    *cp = '\0';

    registerIntrinsicFunctions();
    compile(name);

    if (objectFile != NULL) fclose(objectFile);

    exit(0);
}

#if defined(__cos)
#define IS_KEY(s) (*((s) + strlen(s) - 1) == '=')
#define A_KEY "ALLOC="
#define I_KEY "I="
#define L_KEY "L="
#define O_KEY "O="
#define S_KEY "S"
#define STDIN  "$IN"
#define STDOUT "$OUT"
#else
#define IS_KEY(s) (*(s) == '-')
#define A_KEY "-a"
#define L_KEY "-l"
#define O_KEY "-o"
#define S_KEY "-s"
#define STDIN  "-"
#define STDOUT "-"
#endif

static char *parseOptions(int argc, char *argv[]) {
    int i;
    char *objectPath;
    char *sourcePath;

#if defined(__cos)
    listingFile = stdout;
    objectPath  = "ZZZZCAL";
#else
    objectPath  = NULL;
#endif
    sourceFile  = stdin;
    sourcePath  = NULL;
    i = 1;
    while (i < argc) {
        if (strcmp(argv[i], A_KEY) == 0) {
            i += 1;
            if (i >= argc) {
                usage();
            }
            if (strcasecmp(argv[i], "static") == 0) {
                doStaticLocalsDefault = TRUE;
            }
            else if (strcasecmp(argv[i], "stack") == 0 || strcasecmp(argv[i], "auto") == 0) {
                doStaticLocalsDefault = FALSE;
            }
            else {
                usage();
            }
        }
#if defined(__cos)
        else if (strcmp(argv[i], I_KEY) == 0) {
            i += 1;
            if (i >= argc || sourcePath != NULL) {
                usage();
            }
            sourcePath = argv[i];
            if (strcmp(sourcePath, STDIN) != 0) {
                sourceFile = fopen(argv[i], "r");
                if (sourceFile == NULL) {
                    perror(sourcePath);
                    exit(1);
                }
            }
        }
#endif
        else if (strcmp(argv[i], L_KEY) == 0) {
            i += 1;
            if (i >= argc) {
                usage();
            }
            if (strcmp(argv[i], STDOUT) == 0) {
                listingFile = stdout;
            }
            else if (strcmp(argv[i], "0") == 0) {
                listingFile = NULL;
            }
            else {
                listingFile = fopen(argv[i], "w");
                if (listingFile == NULL) {
                    perror(argv[i]);
                    exit(1);
                }
            }
        }
        else if (strcmp(argv[i], O_KEY) == 0) {
            i += 1;
            if (i >= argc) {
                usage();
            }
            objectPath = argv[i];
            if (strcmp(objectPath, "0") == 0) {
                objectFile = NULL;
            }
            else {
                objectFile = fopen(objectPath, "w");
                if (objectFile == NULL) {
                    perror(objectPath);
                    exit(1);
                }
            }
        }
        else if (strcmp(argv[i], S_KEY) == 0) {
            doEchoSource = TRUE;
        }
#if defined(__cos)
        else {
            usage();
        }
#else
        else if (IS_KEY(argv[i])) {
            usage();
        }
        else { // TODO: change this to allow multiple source files
            sourceFile = fopen(argv[i], "r");
            if (sourceFile == NULL) {
                perror(argv[i]);
                exit(1);
            }
            if (sourcePath == NULL) sourcePath = argv[i];
        }
#endif
        i += 1;
    }
    if (sourcePath == NULL) sourcePath = STDIN;
#if defined(__cos)
    if (objectFile == NULL && strcmp(objectPath, "0") != 0) {
        objectFile = fopen(objectPath, "w");
        if (objectFile == NULL) {
            perror(objectPath);
            exit(1);
        }
    }
#endif

    return sourcePath;
}

static void usage(void) {
#if defined(__cos)
    fputs("usage: KFTC [ALLOC=STATIC|STACK|AUTO][I=sfile][L=lfile][O=ofile][S]\n", stderr);
    fputs("  ALLOC=key - variable storage allocation strategy\n", stderr);
    fputs("              STATIC : variables are allocated in static storage\n", stderr);
    fputs("              STACK or AUTO : variables are allocated on the runtime stack\n", stderr);
    fputs("  I=sfile   - FORTRAN source code file (default $IN)\n", stderr);
    fputs("  L=lfile   - listing file (default $OUT)\n", stderr);
    fputs("  O=ofile   - output file (default ZZZZCAL)\n", stderr);
    fputs("  S         - echo source code lines to output file\n", stderr);
#else
    fputs("usage: kftc [-a static|stack|auto][-l lfile][-o ofile][-s] sfile\n", stderr);
    fputs("  -a key    - variable storage allocation strategy\n", stderr);
    fputs("              static : variables are allocated in static storage\n", stderr);
    fputs("              stack or auto : variables are allocated on the runtime stack\n", stderr);
    fputs("  -l lfile  - listing file (default none)\n", stderr);
    fputs("  -o ofile  - output file (default none)\n", stderr);
    fputs("  -s        - echo source code lines to output file\n", stderr);
    fputs("  sfile     - FORTRAN source code file\n", stderr);
#endif
    exit(1);
}
