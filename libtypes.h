#ifndef LIBTYPES_H
#define LIBTYPES_H
/*--------------------------------------------------------------------------
**
**  Copyright 2023 Kevin E. Jordan
**
**  Name: libtypes.h
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

typedef struct symbol {
    struct symbol *left;
    struct symbol *right;
    char *id;
} Symbol;

typedef struct module {
    struct module *left;
    struct module *right;
    struct module *next;
    char *id;
    int blockCount;
    Symbol *blocks;
    int entryCount;
    Symbol *entries;
    int externalCount;
    Symbol *externals;
} Module;

#endif
