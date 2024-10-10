/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: fmt.c
**
**  Description:
**      This file contains functions supporting formatted I/O.
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

#include "const.h"
#include "fmt.h"

static char fmtBuf[MAX_FMT_LEN+1];

void _prsfmt(unsigned long strDesc) {
    char *bp;
    char *limit;
    char *s;

    s = (char *)(strDesc & 0xffffffff);
    limit = s + (strDesc >> 32);
    bp = fmtBuf;
    while (s < limit) *bp++ = *s++;
    *bp = '\0';

    _przfmt(fmtBuf);
}
