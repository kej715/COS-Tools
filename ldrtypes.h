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

typedef enum blockType {
    BlockType_Common = 0,
    BlockType_Mixed,
    BlockType_Code,
    BlockType_Data,
    BlockType_Const,
    BlockType_Dynamic,
    BlockType_TaskCom
} BlockType;

// Number of distinct block types - must match number of enum values, above
#define BlockTypes 7

typedef struct block {
    struct block *nextInModule;
    struct block *nextInImage;
    struct module *module;
    u8 id[8];
    BlockType type;
    int index;
    bool hasErrorFlag;
    bool isAbsolute;
    u32 origin;
    u32 baseAddress;
    u32 length;
    bool isExtMem;
} Block;

typedef struct module {
    struct module *next;
    struct module *left;
    struct module *right;
    u8 id[8];
    char *libraryPath;
    Block *firstBlock;
    Block *lastBlock;
    int entryCount;
    u8 *entryTable;
    int externalRefCount;
    u8 *externalRefTable;
    char *comment;
    bool doLoad;
    bool isLoaded;
} Module;

typedef struct symbol {
    struct symbol *left;
    struct symbol *right;
    u8 id[8];
    Block *block;
    bool isParcelAddress;
    u64 value;
} Symbol;

typedef struct unsatisfiedExternal {
    struct unsatisfiedExternal *next;
    u8 id[8];
} UnsatisfiedExternal;

#endif
