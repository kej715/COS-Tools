/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
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
#include "services.h"
#if defined(__cos)
#include <sys/syslog.h>
#endif

void *allocate(int size) {
    void *new;

    new = malloc((size_t)size);
    if (new == NULL) {
        eprintf("Failed to allocate %d bytes", size);
        exit(1);
    }
    memset(new, 0, (size_t)size);
    return new;
}

void eprintf(char *format, ...) {
    va_list ap;
#if defined(__cos)
    char buf[120];
#endif

    va_start(ap, format);
#if defined(__cos)
    vsprintf(buf, format, ap);
    syslog(buf, SYSLOG_USER, 1, 1);
#else
    vfprintf(stderr, format, ap);
    fputs("\n", stderr);
#endif
    va_end(ap);
}

void eputs(char *s) {
#if defined(__cos)
    syslog(s, SYSLOG_USER, 1, 1);
#else
    fputs(s, stderr);
    fputs("\n", stderr);
#endif
}

void *reallocate(void *old, int oldSize, int newSize) {
    void *new;

    new = realloc(old, (size_t)newSize);
    if (new == NULL) {
        eprintf("Failed to reallocate %d bytes", newSize);
        exit(1);
    }
    memset((unsigned char *)new + oldSize, 0, newSize - oldSize);
    return new;
}
