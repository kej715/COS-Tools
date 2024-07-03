/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: fmt.h
**
**  Description:
**      This file provides type definitions used by the formatted I/O library.
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

#ifndef FMT_H
#define FMT_H

#include "../types.h"

typedef enum formatClass {
    Fmt_A = 0,
    Fmt_B,
    Fmt_BN,
    Fmt_BZ,
    Fmt_D,
    Fmt_E,
    Fmt_F,
    Fmt_G,
    Fmt_I,
    Fmt_L,
    Fmt_O,
    Fmt_P,
    Fmt_R,
    Fmt_S,
    Fmt_SP,
    Fmt_SS,
    Fmt_T,
    Fmt_TL,
    Fmt_TR,
    Fmt_X,
    Fmt_Z,
    Fmt_EOR,
    Fmt_Term,
    Fmt_Nospace,
    Fmt_String,
    Fmt_Embedded
} FormatClass;

typedef struct formatDesc {
    struct formatDesc *parent;
    struct formatDesc *sibling;
    struct formatDesc *child;
    FormatClass class;
    int repeatCount;
    int currentIteration;
    int width;
    int minDigits;
    int expLength;
    char *string;
} FormatDesc;

void _endfmt(void);
void _infmt(DataValue *value);
FormatDesc *_getfdl(void);
char *_getrcd(void);
void _inircd(void);
void _lstchr(int unitNum, DataValue *value);
void _lstdbl(int unitNum, DataValue *value);
void _lstint(int unitNum, DataValue *value);
void _lstlog(int unitNum, DataValue *value);
void _outfin(int *eor);
void _outfmt(DataValue *value, int *eor);
void _prsfmt(char *s);
void _prslst(void);
void _setfdl(FormatDesc *fdp);
void _setrcd(char *record);
void _setrfd(FormatDesc *fdp);

#endif
