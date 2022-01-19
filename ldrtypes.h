#ifndef LDRTYPES_H
#define LDRTYPES_H
/*--------------------------------------------------------------------------
**
**  Copyright 2022 Kevin E. Jordan
**
**  Name: ldrtypes.h
**
**  Description:
**      This file defines constants, types, and macros used by the loader.
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
#include "basetypes.h"

typedef struct block {
    struct block *next;
    struct module *module;
    char *id;
    int index;
    u32 baseAddress;
    u32 length;
    bool isExtMem;
} Block;

typedef struct module {
    struct module *next;
    char *id;
    bool hasMachineTypeExt;
    bool hasCallingSeq;
    bool isAbsolute;
    u32 origin;
    u32 length;
    Block *firstBlock;
    Block *lastBlock;
    int externalRefCount;
    char *externalRefTable;
    char *comment;
} Module;

typedef struct symbol {
    struct symbol *left;
    struct symbol *right;
    char *id;
    Block *block;
    bool isParcelRelocation;
    u64 value;
} Symbol;

#endif
