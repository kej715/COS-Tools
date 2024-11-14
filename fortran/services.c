/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: services.c
**
**  Description:
**      This file provides host-independent system services.
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "proto.h"

#if defined(__APPLE__)
#include <execinfo.h>
#endif

void *allocate(int size) {
    void *new;

    new = malloc((size_t)size);
    if (new == NULL) {
        fprintf(stderr, "Failed to allocate %d bytes", size);
        exit(1);
    }
    memset(new, 0, (size_t)size);
    return new;
}

void err(char *format, ...) {
    va_list ap;
    char buf[80];

    va_start(ap, format);
    vsprintf(buf, format, ap);
    va_end(ap);
    list(" *ERROR*   %s", buf);
    fprintf(stderr, "ERROR line %d : %s\n", lineNo, buf);
    errorCount  += 1;
    totalErrors += 1;
}

void *reallocate(void *old, int oldSize, int newSize) {
    void *new;

    new = realloc(old, (size_t)newSize);
    if (new == NULL) {
        fprintf(stderr, "Failed to reallocate %d bytes", newSize);
        exit(1);
    }
    memset((unsigned char *)new + oldSize, 0, newSize - oldSize);
    return new;
}

void printStackTrace(FILE *fp) {
#if defined(__APPLE__)
    void *callstack[128];
    int  i;
    int  frames;
    char **strs;

    frames = backtrace(callstack, 128);
    strs   = backtrace_symbols(callstack, frames);
    for (i = 1; i < frames; ++i) {
        fprintf(fp, "%s\n", strs[i]);
    }
    free(strs);
#endif
}

void warn(char *format, ...) {
    va_list ap;
    char buf[80];

    va_start(ap, format);
    vsprintf(buf, format, ap);
    va_end(ap);
    list(" *WARNING* %s", buf);
    fprintf(stderr, "WARNING line %d : %s\n", lineNo, buf);
    warningCount += 1;
}
