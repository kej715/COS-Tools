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
#include "cosdataset.h"
#include "ldrconst.h"
#include "ldrproto.h"
#include "ldrtypes.h"

static void openNextSource(int argi, char *argv[]);
static int  parseOptions(int argc, char *argv[]);
static void usage(void);

static Dataset *objectFile;
static Dataset *sourceFile;

int main(int argc, char *argv[]) {
    u8 buf[512*8];
    u64 cw;
    int fileIndex;
    int n;

    fileIndex = parseOptions(argc, argv);
    objectFile = cosDsCreate(argv[fileIndex]);
    if (objectFile == NULL) {
        fprintf(stderr, "Failed to create %s\n", argv[fileIndex]);
        exit(1);
    }
    if (fileIndex + 1 >= argc) {
        usage();
        exit(1);
    }
    while (++fileIndex < argc) {
        openNextSource(fileIndex, argv);
        while (TRUE) {
            n = cosDsRead(sourceFile, buf, sizeof(buf));
            if (n == -1) {
               fprintf(stderr, "Failed to read %s\n", argv[fileIndex]);
               exit(1);
            }
            else if (n == 0) {
                cw = cosDsReadCW(sourceFile);
                printf("Control word: %016lx\n", cw);
                if (cosDsIsEOD(cw)) break;
            }
            else {
                printf("Read %d bytes\n", n);
            }
        }
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
             dp = cp;
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
    printf("Opened %s\n", filePath);
}

static int parseOptions(int argc, char *argv[]) {
    int i;
    int firstSrcIndex;

    firstSrcIndex = argc;
    i = 1;
    while (i < argc) {
        if (*argv[i] == '-') {
            usage();
            exit(1);
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
            exit(1);
        }
        i += 1;
    }
    if (firstSrcIndex >= argc) {
        usage();
        exit(1);
    }
    return firstSrcIndex;
}

static void usage(void) {
    fputs("Usage: ldr ofile sfile...\n", stdout);
    fputs("  ofile - object file\n", stderr);
    fputs("  sfile - source file(s)\n", stderr);
}
