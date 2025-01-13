/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: catstr.c
**
**  Description:
**      This file contains a function that returns a FORTRAN character pointer
**      to a sequence of bytes that will accomodate a string of a specified
**      size.
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

/*
 * This module provides a pool of pointers to re-usable string storage.
 * A basic premise is that strings allocated by this module have very short
 * lifetimes. They are usually used as temporary storage of function results
 * or the string concatentation function within a statement. They are usually
 * copied almost immediately to variables that have explicitly allocated storage.
 * Thus, this module implements a simple round-robin algorithm for allocating
 * and reusing character strings.
 */

#define MAX_STRINGS 24

static char *strings[MAX_STRINGS] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
static int stringSizes[MAX_STRINGS] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};
static int nextStringIndex = 0;

unsigned long _getStr(int size) {
    unsigned long ptr;
    char *s;

    s = strings[nextStringIndex];
    if (s == NULL) {
        s = (char *)malloc(size);
        strings[nextStringIndex] = s;
        stringSizes[nextStringIndex] = size;
    }
    else if (stringSizes[nextStringIndex] < size) {
        s = (char *)realloc(strings[nextStringIndex], size);
        strings[nextStringIndex] = s;
        stringSizes[nextStringIndex] = size;
    }
    if (s == NULL) {
        fputs("Out of memory\n", stderr);
        exit(1);
    }
    ptr = ((long)size << 32) | (unsigned long)s;
    nextStringIndex += 1;
    if (nextStringIndex >= MAX_STRINGS) nextStringIndex = 0;

    return ptr;
}
