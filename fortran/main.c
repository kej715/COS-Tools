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
    while (sp < dp && len < 8) {
        if (isalnum(*sp)) {
            *cp++ = toupper(*sp);
            len += 1;
        }
        sp += 1;
    }
    *cp = '\0';
    
    compile(name);
}

#if defined(__cos)
#define IS_KEY(s) (*((s) + strlen(s) - 1) == '=')
#define I_KEY "I="
#define L_KEY "L="
#define O_KEY "O="
#define S_KEY "S"
#define STDOUT "$OUT"
#else
#define IS_KEY(s) (*(s) == '-')
#define I_KEY "-i"
#define L_KEY "-l"
#define O_KEY "-o"
#define S_KEY "-s"
#define STDOUT "-"
#endif

static char *parseOptions(int argc, char *argv[]) {
    int i;
    char *sourcePath;

    i = 1;
    sourcePath = NULL;
    while (i < argc) {
        if (strcmp(argv[i], I_KEY) == 0) {
            i += 1;
            if (i >= argc) {
                usage();
            }
            sourceFile = fopen(argv[i], "r");
            if (sourceFile == NULL) {
                perror(argv[i]);
                exit(1);
            }
            if (sourcePath == NULL) sourcePath = argv[i];
        }
        else if (strcmp(argv[i], L_KEY) == 0) {
            i += 1;
            if (i >= argc) {
                usage();
            }
            if (strcmp(argv[i], STDOUT) == 0) {
                listingFile = stdout;
            }
            else if (strcmp(argv[i], "0") != 0) {
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
            objectFile = fopen(argv[i], "w");
            if (objectFile == NULL) {
                perror(argv[i]);
                exit(1);
            }
        }
        else if (strcmp(argv[i], S_KEY) == 0) {
            doEchoSource = TRUE;
        }
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
        i += 1;
    }

    return sourcePath;
}

static void usage(void) {
    fputs("usage: cft77 [-l lpath][-o opath][-s] spath\n", stderr);
    fputs("  lpath - pathname of output listing file\n", stderr);
    fputs("  opath - pathname of output object file\n", stderr);
    fputs("  spath - pathname of input source file\n", stderr);
}
