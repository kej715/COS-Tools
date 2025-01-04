/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: units.h
**
**  Description:
**      This file provides type definitions describing FORTRAN I/O units.
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

#ifndef UNITS_H
#define UNITS_H

#include "const.h"
#include "fmt.h"

/*
 *  Unit flag bit definitions
 *
 *   Bit  Meaning
 *    0   OLD         : 1 if OLD
 *    1   NEW         : 1 if NEW
 *    2   DIRECT      : 0 if SEQUENTIAL, 1 if DIRECT
 *    3   UNFORMATTED : 0 if FORMATTED, 1 if UNFORMATTED
 *    4   BLANK       : 0 if NULL, 1 if ZERO
 *    5   SCRATCH     : 0 if ordinary file, 1 if scratch file
 *    6   IMMUTABLE   : 0 if mutable, 1 if immutable
 *    7   OPEN        : 0 if unit closed, 1 if open
 */
#define FLAG_OLD         0
#define FLAG_NEW         1
#define FLAG_DIRECT      2
#define FLAG_UNFORMATTED 3
#define FLAG_ZERO        4
#define FLAG_SCRATCH     5
#define FLAG_IMMUTABLE   6
#define FLAG_OPEN        7

#define MASK_OLD         (1 << FLAG_OLD)
#define MASK_NEW         (1 << FLAG_NEW)
#define MASK_DIRECT      (1 << FLAG_DIRECT)
#define MASK_UNFORMATTED (1 << FLAG_UNFORMATTED)
#define MASK_ZERO        (1 << FLAG_ZERO)
#define MASK_SCRATCH     (1 << FLAG_SCRATCH)
#define MASK_IMMUTABLE   (1 << FLAG_IMMUTABLE)
#define MASK_OPEN        (1 << FLAG_OPEN)

typedef unsigned long ulong;

typedef struct unit {
    char fileName[MAX_FILE_NAME_LEN+1];
    int  number;  /* unit number                        */
    int  fd;      /* OS file descriptor                 */
    int  ioStat;  /* current I/O status                 */
                  /*   =0 Transfer complete, no error   */
                  /*      not end of file               */
                  /*   >0 Error code                    */
                  /*   <0 End of file                   */
    int  recLen;  /* unformatted record length          */
    int  nextRec; /* next direct access record number   */
    int  flags;   /* flag bits                          */
    char *buf;    /* dynamically allocated input buffer */
    char *out;    /* next available char in buffer      */
    char *limit;  /* last char in buffer + 1            */
} Unit;

Unit *_allocu(int unitNum);
void _closeu(int unitNum, ulong statusRef);
void _clrios(int unitNum);
void _endfio(void);
Unit *_findu(int unitNum);
void _flufmt(int unitNum);
void _flulst(int unitNum);
void _inifio(void);
int  _iostat(int unitNum);
void _openu(int unitNum, ulong fileNameRef, ulong statusRef, ulong accessRef, ulong formattingRef, ulong blankRef, int recLen);
int  _queryu(int unitNum, ulong fileNameRef, long *existRef, long *openedRef, int *numberRef, long *namedRef,
             ulong nameRef, ulong accessRef, ulong sequentialRef, ulong directRef, ulong formattedRef, ulong unformattedRef, ulong formRef, ulong blankRef,
             int *reclRef, int *nextRecRef);
void _rdufmt(int unitNum, void *value);
void _rdurec(int unitNum);
void _wrufmt(int unitNum, DataValue *value);

#endif
