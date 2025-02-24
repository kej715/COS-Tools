/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: symbols.c
**
**  Description:
**      This file provides functions for managing entries in symbol tables.
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "const.h"
#include "proto.h"
#include "types.h"

static Symbol *addNode(char *identifier, SymbolClass class, Symbol **tree);
static void emitCommonTree(Symbol *symbol);
static Symbol *findNode(char *label, Symbol *tree);
static void freeNode(Symbol *symbol);
static void freeTree(Symbol *symbol);
static bool insertEquivVariable(Symbol *left, Symbol *right, int offset);
static void resetCommonTree(Symbol *symbol);

static Symbol *commonBlocks = NULL;
static Symbol *intrinsicFunctions = NULL;
static Symbol *labels = NULL;
static int labelCounter = 0;
static int labelPrefixIdx = 0;
static char labelPrefixes[] = {'H','I','G','J','F','K','E','L','D','M','C','N','B','O','A','P'};
static Symbol *lastLabel = NULL;
static Symbol *lastSymbol = NULL;
static DataType undefinedType = { BaseType_Undefined };

DataType implicitTypes[26];
Symbol *progUnitSym;
Symbol *symbols = NULL;

typedef struct intrinsicFnDefn {
    char *identifier;
    char *generic;
    char *externName;
    BaseType resultType;
    int argc;
    BaseType argumentTypes[MAX_INTRINSIC_ARGS];
} IntrinsicFnDefn;

static IntrinsicFnDefn intrinsicFnDefns[] = {
    {"INT",    NULL,    "_iint",   BaseType_Integer,  1, {BaseType_Integer}},
    {"INT",    "INT",   "_rint",   BaseType_Integer,  1, {BaseType_Real}},
    {"INT",    "INT",   "_rint",   BaseType_Integer,  1, {BaseType_Double}},
    {"IFIX",   "INT",   "_rint",   BaseType_Integer,  1, {BaseType_Real}},
    {"IDINT",  "INT",   "_rint",   BaseType_Integer,  1, {BaseType_Double}},

    {"REAL",   NULL,    "_ireal",  BaseType_Real,     1, {BaseType_Integer}},
    {"REAL",   "REAL",  "_rreal",  BaseType_Real,     1, {BaseType_Real}},
    {"REAL",   "REAL",  "_rreal",  BaseType_Real,     1, {BaseType_Double}},
    {"FLOAT",  "REAL",  "_ireal",  BaseType_Real,     1, {BaseType_Integer}},
    {"SNGL",   "REAL",  "_rreal",  BaseType_Real,     1, {BaseType_Double}},

    {"DBLE",   NULL,    "_ireal",  BaseType_Double,   1, {BaseType_Integer}},
    {"DBLE",   "DBLE",  "_rreal",  BaseType_Double,   1, {BaseType_Real}},
    {"DBLE",   "DBLE",  "_rreal",  BaseType_Double,   1, {BaseType_Double}},

    {"ICHAR",  NULL,    "_ichar",  BaseType_Integer,  1, {BaseType_Character}},

    {"CHAR",   NULL,    "_char",   BaseType_Character,1, {BaseType_Integer}},

    {"AINT",   NULL,    "_aint",   BaseType_Real,     1, {BaseType_Real}},
    {"AINT",   "AINT",  "_aint",   BaseType_Double,   1, {BaseType_Double}},
    {"DINT",   "AINT",  "_aint",   BaseType_Double,   1, {BaseType_Double}},

    {"ANINT",  NULL,    "_anint",  BaseType_Real,     1, {BaseType_Real}},
    {"ANINT",  "ANINT", "_anint",  BaseType_Double,   1, {BaseType_Double}},
    {"DNINT",  "ANINT", "_anint",  BaseType_Double,   1, {BaseType_Double}},

    {"NINT",   NULL,    "_nint",   BaseType_Integer,  1, {BaseType_Real}},
    {"NINT",   "NINT",  "_nint",   BaseType_Integer,  1, {BaseType_Double}},
    {"IDNINT", "NINT",  "_nint",   BaseType_Integer,  1, {BaseType_Double}},

    {"ABS",    NULL,    "_iabs",   BaseType_Integer,  1, {BaseType_Integer}},
    {"ABS",    "ABS",   "_rabs",   BaseType_Real,     1, {BaseType_Real}},
    {"ABS",    "ABS",   "_rabs",   BaseType_Double,   1, {BaseType_Double}},
    {"IABS",   "ABS",   "_iabs",   BaseType_Integer,  1, {BaseType_Integer}},
    {"DABS",   "ABS",   "_rabs",   BaseType_Double,   1, {BaseType_Double}},

    {"MOD",    NULL,    "_imod",   BaseType_Integer,  2, {BaseType_Integer, BaseType_Integer}},
    {"MOD",    "MOD",   "_rmod",   BaseType_Real,     2, {BaseType_Real,    BaseType_Real}},
    {"MOD",    "MOD",   "_rmod",   BaseType_Double,   2, {BaseType_Double,  BaseType_Double}},
    {"AMOD",   "MOD",   "_rmod",   BaseType_Real,     2, {BaseType_Real,    BaseType_Real}},
    {"DMOD",   "MOD",   "_rmod",   BaseType_Double,   2, {BaseType_Double,  BaseType_Double}},

    {"SIGN",   NULL,    "_isign",  BaseType_Integer,  2, {BaseType_Integer, BaseType_Integer}},
    {"SIGN",   "SIGN",  "_rsign",  BaseType_Real,     2, {BaseType_Real,    BaseType_Real}},
    {"SIGN",   "SIGN",  "_rsign",  BaseType_Double,   2, {BaseType_Double,  BaseType_Double}},
    {"ISIGN",  "SIGN",  "_isign",  BaseType_Integer,  2, {BaseType_Integer, BaseType_Integer}},
    {"DSIGN",  "SIGN",  "_rsign",  BaseType_Double,   2, {BaseType_Double,  BaseType_Double}},

    {"DIM",    NULL,    "_idim",   BaseType_Integer,  2, {BaseType_Integer, BaseType_Integer}},
    {"DIM",    "DIM",   "_rdim",   BaseType_Real,     2, {BaseType_Real,    BaseType_Real}},
    {"DIM",    "DIM",   "_rdim",   BaseType_Double,   2, {BaseType_Double,  BaseType_Double}},
    {"IDIM",   "DIM",   "_idim",   BaseType_Integer,  2, {BaseType_Integer, BaseType_Integer}},
    {"DDIM",   "DIM",   "_rdim",   BaseType_Double,   2, {BaseType_Double,  BaseType_Double}},

    {"LEN",    NULL,    "_len",    BaseType_Integer,  1, {BaseType_Character}},

    {"INDEX",  NULL,    "_index",  BaseType_Integer,  2, {BaseType_Character, BaseType_Character}},

    {"SQRT",   NULL,    "_isqrt",  BaseType_Real,     1, {BaseType_Integer}},
    {"SQRT",   "SQRT",  "_rsqrt",  BaseType_Real,     1, {BaseType_Real}},
    {"SQRT",   "SQRT",  "_rsqrt",  BaseType_Double,   1, {BaseType_Double}},
    {"DSQRT",  "SQRT",  "_rsqrt",  BaseType_Double,   1, {BaseType_Double}},

    {"EXP",    NULL,    "_iexp",   BaseType_Real,     1, {BaseType_Integer}},
    {"EXP",    "EXP",   "_rexp",   BaseType_Real,     1, {BaseType_Real}},
    {"EXP",    "EXP",   "_rexp",   BaseType_Double,   1, {BaseType_Double}},
    {"DEXP",   "EXP",   "_rexp",   BaseType_Double,   1, {BaseType_Double}},

    {"LOG",    NULL,    "_ilog",   BaseType_Real,     1, {BaseType_Integer}},
    {"LOG",    "LOG",   "_rlog",   BaseType_Real,     1, {BaseType_Real}},
    {"LOG",    "LOG",   "_rlog",   BaseType_Double,   1, {BaseType_Double}},
    {"ALOG",   "LOG",   "_rlog",   BaseType_Real,     1, {BaseType_Real}},
    {"DLOG",   "LOG",   "_rlog",   BaseType_Double,   1, {BaseType_Double}},

    {"LOG10",  NULL,    "_ilog10", BaseType_Real,     1, {BaseType_Integer}},
    {"LOG10",  "LOG10", "_rlog10", BaseType_Real,     1, {BaseType_Real}},
    {"LOG10",  "LOG10", "_rlog10", BaseType_Double,   1, {BaseType_Double}},
    {"ALOG10", "LOG10", "_rlog10", BaseType_Real,     1, {BaseType_Real}},
    {"DLOG10", "LOG10", "_rlog10", BaseType_Double,   1, {BaseType_Double}},

    {"SIN",    NULL,    "_isin",   BaseType_Real,     1, {BaseType_Integer}},
    {"SIN",    "SIN",   "_rsin",   BaseType_Real,     1, {BaseType_Real}},
    {"SIN",    "SIN",   "_rsin",   BaseType_Double,   1, {BaseType_Double}},
    {"DSIN",   "SIN",   "_rsin",   BaseType_Double,   1, {BaseType_Double}},

    {"COS",    NULL,    "_icos",   BaseType_Real,     1, {BaseType_Integer}},
    {"COS",    "COS",   "_rcos",   BaseType_Real,     1, {BaseType_Real}},
    {"COS",    "COS",   "_rcos",   BaseType_Double,   1, {BaseType_Double}},
    {"DCOS",   "COS",   "_rcos",   BaseType_Double,   1, {BaseType_Double}},

    {"TAN",    NULL,    "_itan",   BaseType_Real,     1, {BaseType_Integer}},
    {"TAN",    "TAN",   "_rtan",   BaseType_Real,     1, {BaseType_Real}},
    {"TAN",    "TAN",   "_rtan",   BaseType_Double,   1, {BaseType_Double}},
    {"DTAN",   "TAN",   "_rtan",   BaseType_Double,   1, {BaseType_Double}},

    {"ASIN",   NULL,    "_iasin",  BaseType_Real,     1, {BaseType_Integer}},
    {"ASIN",   "ASIN",  "_rasin",  BaseType_Real,     1, {BaseType_Real}},
    {"ASIN",   "ASIN",  "_rasin",  BaseType_Double,   1, {BaseType_Double}},
    {"DASIN",  "ASIN",  "_rasin",  BaseType_Double,   1, {BaseType_Double}},

    {"ACOS",   NULL,    "_iacos",  BaseType_Real,     1, {BaseType_Integer}},
    {"ACOS",   "ACOS",  "_racos",  BaseType_Real,     1, {BaseType_Real}},
    {"ACOS",   "ACOS",  "_racos",  BaseType_Double,   1, {BaseType_Double}},
    {"DACOS",  "ACOS",  "_racos",  BaseType_Double,   1, {BaseType_Double}},

    {"ATAN",   NULL,    "_iatan",  BaseType_Real,     1, {BaseType_Integer}},
    {"ATAN",   "ATAN",  "_ratan",  BaseType_Real,     1, {BaseType_Real}},
    {"ATAN",   "ATAN",  "_ratan",  BaseType_Double,   1, {BaseType_Double}},
    {"DATAN",  "ATAN",  "_ratan",  BaseType_Double,   1, {BaseType_Double}},

    {"ATAN2",  NULL,    "_iatan2", BaseType_Real,     2, {BaseType_Integer, BaseType_Integer}},
    {"ATAN2",  "ATAN2", "_ratan2", BaseType_Real,     2, {BaseType_Real,    BaseType_Real}},
    {"ATAN2",  "ATAN2", "_ratan2", BaseType_Double,   2, {BaseType_Double,  BaseType_Double}},
    {"DATAN2", "ATAN2", "_ratan2", BaseType_Double,   2, {BaseType_Double,  BaseType_Double}},

    {"SINH",   NULL,    "_isinh",  BaseType_Real,     1, {BaseType_Integer}},
    {"SINH",   "SINH",  "_rsinh",  BaseType_Real,     1, {BaseType_Real}},
    {"SINH",   "SINH",  "_rsinh",  BaseType_Double,   1, {BaseType_Double}},
    {"DSINH",  "SINH",  "_rsinh",  BaseType_Double,   1, {BaseType_Double}},

    {"COSH",   NULL,    "_icosh",  BaseType_Real,     1, {BaseType_Integer}},
    {"COSH",   "COSH",  "_rcosh",  BaseType_Real,     1, {BaseType_Real}},
    {"COSH",   "COSH",  "_rcosh",  BaseType_Double,   1, {BaseType_Double}},
    {"DCOSH",  "COSH",  "_rcosh",  BaseType_Double,   1, {BaseType_Double}},

    {"TANH",   NULL,    "_itanh",  BaseType_Real,     1, {BaseType_Integer}},
    {"TANH",   "TANH",  "_rtanh",  BaseType_Real,     1, {BaseType_Real}},
    {"TANH",   "TANH",  "_rtanh",  BaseType_Double,   1, {BaseType_Double}},
    {"DTANH",  "TANH",  "_rtanh",  BaseType_Double,   1, {BaseType_Double}},

    {"LGE",    NULL,    "_lge",    BaseType_Logical,  2, {BaseType_Character, BaseType_Character}},
    {"LGT",    NULL,    "_lgt",    BaseType_Logical,  2, {BaseType_Character, BaseType_Character}},
    {"LLE",    NULL,    "_lle",    BaseType_Logical,  2, {BaseType_Character, BaseType_Character}},
    {"LLT",    NULL,    "_llt",    BaseType_Logical,  2, {BaseType_Character, BaseType_Character}},

    {"MAX",    NULL,    "_imax",   BaseType_Integer, -1, {BaseType_Integer}},
    {"MAX",    "MAX",   "_rmax",   BaseType_Real,    -1, {BaseType_Real}},
    {"MAX",    "MAX",   "_rmax",   BaseType_Double,  -1, {BaseType_Double}},
    {"MAX0",   "MAX",   "_imax",   BaseType_Integer, -1, {BaseType_Integer}},
    {"MAX1",   "MAX",   "_imax1",  BaseType_Integer, -1, {BaseType_Real}},
    {"AMAX1",  "MAX",   "_rmax",   BaseType_Real,    -1, {BaseType_Real}},
    {"AMAX0",  NULL,    "_amax0",  BaseType_Real,    -1, {BaseType_Integer}},

    {"MIN",    NULL,    "_imin",   BaseType_Integer, -1, {BaseType_Integer}},
    {"MIN",    "MIN",   "_rmin",   BaseType_Real,    -1, {BaseType_Real}},
    {"MIN",    "MIN",   "_rmin",   BaseType_Double,  -1, {BaseType_Double}},
    {"MIN0",   "MIN",   "_imin",   BaseType_Integer, -1, {BaseType_Integer}},
    {"MIN1",   "MIN",   "_imin1",  BaseType_Integer, -1, {BaseType_Real}},
    {"AMIN1",  "MIN",   "_rmin",   BaseType_Real,    -1, {BaseType_Real}},
    {"AMIN0",  NULL,    "_amin0",  BaseType_Real,    -1, {BaseType_Integer}},

    {"LOC",    NULL,    "_loc",    BaseType_Pointer,  1, {BaseType_Integer}},
    {"LOC",   "LOC",    "_loc",    BaseType_Pointer,  1, {BaseType_Real}},
    {"LOC",   "LOC",    "_loc",    BaseType_Pointer,  1, {BaseType_Double}},
    {"LOC",   "LOC",    "_loc",    BaseType_Pointer,  1, {BaseType_Character}},

    {"VLOAD",  NULL,    "_vload",  BaseType_Integer,  3, {BaseType_Integer, BaseType_Integer, BaseType_Integer}},
    {"VLOAD", "VLOAD",  "_vload",  BaseType_Integer,  3, {BaseType_Integer, BaseType_Integer, BaseType_Real}},

    {"VSTORE", NULL,    "_vstore", BaseType_Integer,  3, {BaseType_Integer, BaseType_Integer, BaseType_Integer}},
    {"VSTORE","VSTORE", "_vstore", BaseType_Integer,  3, {BaseType_Integer, BaseType_Integer, BaseType_Real}},

    {"VVADDI", NULL,    "_vvaddi", BaseType_Integer,  3, {BaseType_Integer, BaseType_Integer, BaseType_Integer}},
    {"VVADDR", NULL,    "_vvaddr", BaseType_Integer,  3, {BaseType_Integer, BaseType_Integer, BaseType_Integer}},
    {"VVSUBI", NULL,    "_vvsubi", BaseType_Integer,  3, {BaseType_Integer, BaseType_Integer, BaseType_Integer}},
    {"VVSUBR", NULL,    "_vvsubr", BaseType_Integer,  3, {BaseType_Integer, BaseType_Integer, BaseType_Integer}},
    {"VVMULR", NULL,    "_vvmulr", BaseType_Integer,  3, {BaseType_Integer, BaseType_Integer, BaseType_Integer}},
    {"VVDIVR", NULL,    "_vvdivr", BaseType_Integer,  3, {BaseType_Integer, BaseType_Integer, BaseType_Integer}},

    {"VSADD",  NULL,    "_vsaddi", BaseType_Integer,  3, {BaseType_Integer, BaseType_Integer, BaseType_Integer}},
    {"VSADD", "VSADD",  "_vsaddr", BaseType_Integer,  3, {BaseType_Integer, BaseType_Real,    BaseType_Integer}},
    {"VSSUB",  NULL,    "_vssubi", BaseType_Integer,  3, {BaseType_Integer, BaseType_Integer, BaseType_Integer}},
    {"VSSUB", "VSSUB",  "_vssubr", BaseType_Integer,  3, {BaseType_Integer, BaseType_Real,    BaseType_Integer}},
    {"VSMUL",  NULL,    "_vsmulr", BaseType_Integer,  3, {BaseType_Integer, BaseType_Real,    BaseType_Integer}},
    {"VSDIV",  NULL,    "_vsdivr", BaseType_Integer,  3, {BaseType_Integer, BaseType_Real,    BaseType_Integer}},

    {"CLOCK",  NULL,    "_cosclk", BaseType_Integer,  0},
    {"DATE",   NULL,    "_date",   BaseType_Integer,  0},
    {"JDATE",  NULL,    "_jdate",  BaseType_Integer,  0},

    {"RTC",    NULL,    "_rtc",    BaseType_Real,     0},
    {"IRTC",   NULL,    "_irtc",   BaseType_Integer,  0},
    {"CPUTIME",NULL,    "_cputim", BaseType_Real,     0},

    {"ARGC",   NULL,    "_argc",   BaseType_Integer,  0},
    {"ARGV",   NULL,    "_argv",   BaseType_Character,1, {BaseType_Integer}},

    {"SHIFT",  NULL,    "_shift",  BaseType_Integer,  2, {BaseType_Integer, BaseType_Integer}},
    {"SHIFT", "SHIFT",  "_shift",  BaseType_Integer,  2, {BaseType_Real,    BaseType_Integer}},
    {"SHIFT", "SHIFT",  "_shift",  BaseType_Integer,  2, {BaseType_Pointer, BaseType_Integer}},

    {"SHIFTL", NULL,    "_shiftl", BaseType_Integer,  2, {BaseType_Integer, BaseType_Integer}},
    {"SHIFTL","SHIFTL", "_shiftl", BaseType_Integer,  2, {BaseType_Real,    BaseType_Integer}},
    {"SHIFTL","SHIFTL", "_shiftl", BaseType_Integer,  2, {BaseType_Pointer, BaseType_Integer}},

    {"SHIFTR", NULL,    "_shiftr", BaseType_Integer,  2, {BaseType_Integer, BaseType_Integer}},
    {"SHIFTR","SHIFTR", "_shiftr", BaseType_Integer,  2, {BaseType_Real,    BaseType_Integer}},
    {"SHIFTR","SHIFTR", "_shiftr", BaseType_Integer,  2, {BaseType_Pointer, BaseType_Integer}},

    {"MASK",   NULL,    "_mask",   BaseType_Integer,  1, {BaseType_Integer}},

    {NULL}
};

Symbol *addCommonBlock(char *name) {
    Symbol *symbol;

    return addNode(name, SymClass_NamedCommon, &commonBlocks);
}

Symbol *addLabel(char *label) {
    Symbol *new;

    new = addNode(label, SymClass_Label, &labels);
    if (new != NULL) {
        generateLabel(new->details.label.label);
        if (new->next == NULL) {
            if (lastLabel != NULL) {
                lastLabel->next = new;
            }
            lastLabel = new;
        }
    }

    return new;
}

static Symbol *addNode(char *identifier, SymbolClass class, Symbol **tree) {
    Symbol *current;
    Symbol *new;
    int valence;

    new = allocSymbol(identifier, class);
    current = *tree;
    if (current == NULL) {
        *tree = new;
        return new;
    }
    while (current != NULL) {
        valence = strcmp(current->identifier, identifier);
        if (valence > 0) {
            if (current->left != NULL) {
                current = current->left;
            }
            else {
                current->left = new;
                break;
            }
        }
        else if (valence < 0) {
            if (current->right != NULL) {
                current = current->right;
            }
            else {
                current->right = new;
                break;
            }
        }
        else if (current->isDeleted) {
            freeNode(new);
            current->class = class;
            current->isDeleted = FALSE;
            current->isShadow = FALSE;
            current->size = 0;
            memset(&current->details, 0, sizeof(SymbolDetails));
            return current;
        }
        else {
            freeNode(new);
            new = NULL;
            break;
        }
    }

    return new;
}

Symbol *addSymbol(char *identifier, SymbolClass class) {
    Symbol *new;

    new = addNode(identifier, class, &symbols);
    if (new != NULL && new->next == NULL && new != lastSymbol) {
        if (lastSymbol != NULL) {
            lastSymbol->next = new;
        }
        lastSymbol = new;
    }

    return new;
}

Symbol *allocSymbol(char *identifier, SymbolClass class) {
    int len;
    Symbol *symbol;
    char *s;

    symbol = (Symbol *)allocate(sizeof(Symbol));
    len = strlen(identifier);
    s = (char *)allocate(len + 1);
    memcpy(s, identifier, len);
    symbol->identifier = s;
    symbol->class = class;

    return symbol;
}

int calculateAutoOffsets(void) {
    int baseOffset;
    DataType *dt;
    Symbol *equiv;
    int equivOffset;
    int highestOffset;
    int offset;
    int size;
    Symbol *symbol;

    /*
     *  Pass 1. Calculate the offset of each symbol relative to top of stack.
     */
    offset = 0;
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        if (symbol->class == SymClass_Adjustable && symbol->details.adjustable.isStorageAssigned == FALSE) {
            symbol->details.adjustable.isStorageAssigned = TRUE;
            offset += (symbol->details.adjustable.dt.rank * 2) + 1;
        }
    }
    offset = offset << 3;
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        if (symbol->class == SymClass_Auto
            && symbol->details.variable.isStorageAssigned == FALSE
            && symbol->details.variable.isSubordinate == FALSE) {
            symbol->details.variable.isStorageAssigned = TRUE;
            symbol->details.variable.offset = offset >> 3;
            if (symbol->details.variable.nextInStorage != NULL) {
                baseOffset = offset;
                dt = getSymbolType(symbol);
                size = (dt->type == BaseType_Character) ? countArrayElements(symbol) * dt->constraint : symbol->size << 3;
                highestOffset = baseOffset + size;
                equiv = symbol->details.variable.nextInStorage;
                equivOffset = symbol->details.variable.nextOffset;
                while (equiv != NULL) {
                    dt = getSymbolType(equiv);
                    equiv->details.variable.isStorageAssigned = TRUE;
                    baseOffset += equivOffset;
                    if (dt->type == BaseType_Character) {
                        dt->firstChrOffset = baseOffset & 7;
                        size = countArrayElements(equiv) * dt->constraint;
                    }
                    else if ((baseOffset & 7) == 0) {
                        size = equiv->size << 3;
                    }
                    else {
                        err("Invalid equivalence: %s, %s\n", symbol->identifier, equiv->identifier);
                    }
                    equiv->details.variable.offset = baseOffset >> 3;
                    if (highestOffset < baseOffset + size) highestOffset = baseOffset + size;
                    equivOffset = equiv->details.variable.nextOffset;
                    equiv = equiv->details.variable.nextInStorage;
                }
                offset = (highestOffset + 7) & ~7L;
            }
            else {
                offset += symbol->size << 3;
            }
        }
        else if (symbol->class == SymClass_Function) {
            symbol->details.progUnit.offset = offset >> 3;
            offset += symbol->size << 3;
        }
    }

    offset = offset >> 3;

    /*
     *  Pass 2. Adjust offsets to be relative to frame pointer.
     */
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        if (symbol->class == SymClass_Auto) {
            symbol->details.variable.offset -= offset;
        }
        else if (symbol->class == SymClass_Function) {
            symbol->details.progUnit.offset -= offset;
        }
    }

    return offset;
}

void calculateCommonOffsets(void) {
    int baseOffset;
    Symbol *commonBlock;
    DataType *dt;
    Symbol *equiv;
    int equivOffset;
    int highestOffset;
    int offset;
    int size;
    Symbol *symbol;

    /*
     *  Pass 1. Calculate sizes of all common blocks and the offsets of variables
     *          defined within them.
     */
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        if (symbol->class == SymClass_Global
            && symbol->details.variable.isStorageAssigned == FALSE
            && symbol->details.variable.isSubordinate == FALSE) {
            commonBlock = symbol->details.variable.staticBlock;
            symbol->details.variable.offset = commonBlock->details.common.offset;
            commonBlock->details.common.offset += symbol->size;
            if (commonBlock->details.common.offset > commonBlock->details.common.limit) {
                commonBlock->details.common.limit = commonBlock->details.common.offset;
            }
        }
    }
    /*
     *  Pass 2. Assign storage.
     */
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        if (symbol->class == SymClass_Global
            && symbol->details.variable.isStorageAssigned == FALSE
            && symbol->details.variable.isSubordinate == FALSE) {
            symbol->details.variable.isStorageAssigned = TRUE;
            offset = symbol->details.variable.offset << 3;
            if (symbol->details.variable.nextInStorage != NULL) {
                baseOffset = offset;
                dt = getSymbolType(symbol);
                size = (dt->type == BaseType_Character) ? countArrayElements(symbol) * dt->constraint : symbol->size << 3;
                highestOffset = baseOffset + size;
                equiv = symbol->details.variable.nextInStorage;
                equivOffset = symbol->details.variable.nextOffset;
                while (equiv != NULL) {
                    dt = getSymbolType(equiv);
                    equiv->details.variable.isStorageAssigned = TRUE;
                    baseOffset += equivOffset;
                    if (dt->type == BaseType_Character) {
                        dt->firstChrOffset = baseOffset & 7;
                        size = countArrayElements(equiv) * dt->constraint;
                    }
                    else if ((baseOffset & 7) == 0) {
                        size = equiv->size << 3;
                    }
                    else {
                        err("Invalid equivalence: %s, %s\n", symbol->identifier, equiv->identifier);
                    }
                    equiv->details.variable.offset = baseOffset >> 3;
                    if (highestOffset < baseOffset + size) highestOffset = baseOffset + size;
                    equivOffset = equiv->details.variable.nextOffset;
                    equiv = equiv->details.variable.nextInStorage;
                }
                highestOffset = ((highestOffset + 7) & ~7L) >> 3;
                if (highestOffset > symbol->details.variable.staticBlock->details.common.limit) {
                    symbol->details.variable.staticBlock->details.common.limit = highestOffset;
                }
            }
        }
    }
}

int calculateSize(Symbol *symbol) {
    DataType *dt;
    int n;

    dt = getSymbolType(symbol);
    switch (dt->type) {
    case BaseType_Character:
        n = (dt->constraint > 0) ? dt->constraint : 1;
        symbol->size = (n * countArrayElements(symbol) + 7) >> 3;
        return symbol->size;
    case BaseType_Logical:
    case BaseType_Integer:
    case BaseType_Real:
    case BaseType_Label:
    case BaseType_Pointer:
    case BaseType_Double: /* TODO: change this in the future */
        symbol->size = 1;
        break;
    case BaseType_Complex:
        symbol->size = 2;
        break;
    default:
        symbol->size = 0;
        break;
    }
    symbol->size *= countArrayElements(symbol);
    return symbol->size;
}

int calculateStaticOffsets(void) {
    int baseOffset;
    DataType *dt;
    Symbol *equiv;
    int equivOffset;
    int highestOffset;
    int offset;
    int size;
    Symbol *symbol;

    offset = 0;
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        if (symbol->class == SymClass_Static
            && symbol->details.variable.isStorageAssigned == FALSE
            && symbol->details.variable.isSubordinate == FALSE) {
            symbol->details.variable.isStorageAssigned = TRUE;
            symbol->details.variable.offset = offset >> 3;
            if (symbol->details.variable.nextInStorage != NULL) {
                baseOffset = offset;
                dt = getSymbolType(symbol);
                size = (dt->type == BaseType_Character) ? countArrayElements(symbol) * dt->constraint : symbol->size << 3;
                highestOffset = baseOffset + size;
                equiv = symbol->details.variable.nextInStorage;
                equivOffset = symbol->details.variable.nextOffset;
                while (equiv != NULL) {
                    dt = getSymbolType(equiv);
                    equiv->details.variable.isStorageAssigned = TRUE;
                    baseOffset += equivOffset;
                    if (dt->type == BaseType_Character) {
                        dt->firstChrOffset = baseOffset & 7;
                        size = countArrayElements(equiv) * dt->constraint;
                    }
                    else if ((baseOffset & 7) == 0) {
                        size = equiv->size << 3;
                    }
                    else {
                        err("Invalid equivalence: %s, %s\n", symbol->identifier, equiv->identifier);
                    }
                    equiv->details.variable.offset = baseOffset >> 3;
                    if (highestOffset < baseOffset + size) highestOffset = baseOffset + size;
                    equivOffset = equiv->details.variable.nextOffset;
                    equiv = equiv->details.variable.nextInStorage;
                }
                offset = (highestOffset + 7) & ~7L;
            }
            else {
                offset += symbol->size << 3;
            }
        }
    }

    return offset >> 3;
}

Symbol *createShadow(Symbol *symbol, SymbolClass class) {
    Symbol *shadow;

    if (symbol->shadow != NULL) return NULL;
    shadow = allocSymbol(symbol->identifier, class);
    shadow->isShadow = TRUE;
    symbol->shadow = shadow;

    return shadow;
}

int countArrayElements(Symbol *symbol) {
    Bounds *bounds;
    int count;
    DataType *dt;
    int i;

    dt = getSymbolType(symbol);
    count = 1;
    for (i = 0; i < dt->rank; i++) {
        bounds = &dt->bounds[i];
        count *= (bounds->upper - bounds->lower) + 1;
    }
    return count;
}

void defineLocalVariable(Symbol *symbol) {
    if (doStaticLocals) {
        symbol->class = SymClass_Static;
        defineType(symbol);
        symbol->details.variable.offset = staticOffset;
        symbol->details.variable.staticBlock = (progUnitSym->class != SymClass_StmtFunction) ? progUnitSym : progUnitSym->details.progUnit.parentUnit;
        staticOffset += calculateSize(symbol);
    }
    else {
        symbol->class = SymClass_Auto;
        defineType(symbol);
        autoOffset -= calculateSize(symbol);
        symbol->details.variable.offset = autoOffset;
    }
}

void defineType(Symbol *symbol) {
    DataType *dt;

    switch (symbol->class) {
    case SymClass_Auto:
    case SymClass_Static:
    case SymClass_Adjustable:
    case SymClass_Global:
    case SymClass_Argument:
        dt = &symbol->details.variable.dt;
        break;
    case SymClass_Undefined:
        if (symbol->isFnRef == FALSE) return;
    case SymClass_Function:
        dt = &symbol->details.progUnit.dt;
        break;
    case SymClass_Pointee:
        dt = &symbol->details.pointee.dt;
        break;
    case SymClass_Parameter:
        dt = &symbol->details.param.dt;
        break;
    default:
        return;
    }
    if (dt->type == BaseType_Undefined) {
        dt->type = implicitTypes[toupper(symbol->identifier[0]) - 'A'].type;
    }
}

void emitCommonBlocks(void) {
    emitCommonTree(commonBlocks);
}

static void emitCommonTree(Symbol *symbol) {
    if (symbol != NULL) {
        emitCommonTree(symbol->left);
        emitActivateSection(symbol->identifier, "COMMON");
        emitWordBlockZ(symbol->details.common.label, symbol->details.common.limit);
        emitDeactivateSection(symbol->identifier);
        emitCommonTree(symbol->right);
    }
}

Symbol *findCommonBlock(char *name) {
    return findNode(name, commonBlocks);
}

Symbol *findIntrinsicFunction(char *name) {
    return findNode(name, intrinsicFunctions);
}

Symbol *findLabel(char *label) {
    return findNode(label, labels);
}

static Symbol *findNode(char *label, Symbol *tree) {
    Symbol *current;
    int valence;

    current = tree;
    while (current != NULL) {
        valence = strcmp(current->identifier, label);
        if (valence > 0)
            current = current->left;
        else if (valence < 0)
            current = current->right;
        else if (current->isDeleted)
            return NULL;
        else
            break;
    }

    return current;
}

Symbol *findSymbol(char *identifier) {
    Symbol *symbol;

    symbol = findNode(identifier, symbols);
    return (symbol != NULL && symbol->shadow != NULL) ? symbol->shadow : symbol;
}

void freeAllSymbols(void) {
    freeTree(symbols);
    symbols = NULL;
    lastSymbol = NULL;
    freeTree(labels);
    labels = NULL;
    lastLabel = NULL;
}

static void freeNode(Symbol *symbol) {
    if (symbol->shadow != NULL) freeNode(symbol->shadow);
    if (symbol->class == SymClass_Parameter
        && symbol->details.param.dt.type == BaseType_Character
        && symbol->details.param.value.character.string != NULL) {
        free(symbol->details.param.value.character.string);
    }
    free(symbol->identifier);
    free(symbol);
}

static void freeTree(Symbol *symbol) {
    if (symbol != NULL) {
        freeTree(symbol->left);
        freeTree(symbol->right);
        freeNode(symbol);
    }
}

void generateLabel(char *label) {
    sprintf(label, "L%c%d", labelPrefixes[labelPrefixIdx], ++labelCounter);
    labelPrefixIdx = (labelPrefixIdx + 1) & 0x0f;
}

Symbol *getSymbolRoot(void) {
    return symbols;
}

DataType *getSymbolType(Symbol *symbol) {
    static DataType intrinsicType;

    switch (symbol->class) {
    case SymClass_Undefined:
    case SymClass_Auto:
    case SymClass_Static:
    case SymClass_Adjustable:
    case SymClass_Global:
    case SymClass_Argument:
        return &symbol->details.variable.dt;
    case SymClass_Function:
        return &symbol->details.progUnit.dt;
    case SymClass_Intrinsic:
        memset(&intrinsicType, 0, sizeof(DataType));
        intrinsicType.type = symbol->details.intrinsic.resultType;
        return &intrinsicType;
    case SymClass_Parameter:
        return &symbol->details.param.dt;
    case SymClass_Pointee:
        return &symbol->details.pointee.dt;
    default:
        return &undefinedType;
    }
}

static bool insertEquivVariable(Symbol *left, Symbol *right, int offset) {
    int leftOffset;
    Symbol *next;

    switch (left->class) {
    case SymClass_Auto:
        switch (right->class) {
        case SymClass_Auto:
            break;
        case SymClass_Global:
            if (offset != 0) return FALSE;
        case SymClass_Static:
            left->class = right->class;
            left->details.variable.staticBlock = right->details.variable.staticBlock;
            break;
        default:
            return FALSE;
        }
        break;
    case SymClass_Static:
        switch (right->class) {
        case SymClass_Static:
            break;
        case SymClass_Auto:
            right->class = left->class;
            right->details.variable.staticBlock = left->details.variable.staticBlock;
            break;
        case SymClass_Global:
            if (offset != 0) return FALSE;
            left->class = right->class;
            left->details.variable.staticBlock = right->details.variable.staticBlock;
            break;
        default:
            return FALSE;
        }
        break;
    case SymClass_Global:
        switch (right->class) {
        case SymClass_Static:
        case SymClass_Auto:
            right->class = left->class;
            right->details.variable.staticBlock = left->details.variable.staticBlock;
            break;
        case SymClass_Global:
            if (offset != 0 || left->details.variable.staticBlock != right->details.variable.staticBlock) return FALSE;
            break;
        default:
            return FALSE;
        }
        break;
    default:
        return FALSE;
    }

    right->details.variable.nextInStorage = NULL;
    right->details.variable.isSubordinate = TRUE;
    next = left->details.variable.nextInStorage;
    while (next != NULL && left->details.variable.nextOffset < offset) {
        offset -= left->details.variable.nextOffset;
        left = next;
        next = next->details.variable.nextInStorage;
    }
    left->details.variable.nextInStorage = right;
    leftOffset = left->details.variable.nextOffset;
    left->details.variable.nextOffset = offset;
    if (next != NULL) {
        right->details.variable.nextInStorage = next;
        right->details.variable.nextOffset = leftOffset - offset;
    }

    return TRUE;
}

bool linkVariables(Symbol *fromSym, int fromOffset, Symbol *toSym, int toOffset) {
    Symbol *left;
    DataType *leftDt;
    Symbol *next;
    int nextOffset;
    int offset;
    Symbol *right;
    DataType *rightDt;

    if (fromSym == toSym) return FALSE;

    leftDt         = getSymbolType(fromSym);
    rightDt        = getSymbolType(toSym);
    if (leftDt->type != BaseType_Character) {
        fromOffset = fromOffset << 3;
    }
    if (rightDt->type != BaseType_Character) {
        toOffset = toOffset << 3;
    }
    offset = fromOffset - toOffset;
    if (toSym->class == SymClass_Global && fromSym->class != SymClass_Global) {
        left   = toSym;
        right  = fromSym;
    }
    else if (offset >= 0) {
        left   = fromSym;
        right  = toSym;
    }
    else {
        left   = toSym;
        right  = fromSym;
        offset = -offset;
    }
    next = right->details.variable.nextInStorage;
    nextOffset = right->details.variable.nextOffset;
    if (insertEquivVariable(left, right, offset) == FALSE) return FALSE;
    while (next != NULL) {
        left = right;
        right = next;
        offset = nextOffset;
        next = right->details.variable.nextInStorage;
        nextOffset = right->details.variable.nextOffset;
        right->details.variable.nextInStorage = NULL;
        if (insertEquivVariable(left, right, offset) == FALSE) return FALSE;
    }

    return TRUE;
}

void presetImplicit(void) {
    char c;

    for (c = 'A'; c <  'I'; c++) implicitTypes[c - 'A'].type = BaseType_Real;
    for (c = 'I'; c <  'O'; c++) implicitTypes[c - 'A'].type = BaseType_Integer;
    for (c = 'O'; c <= 'Z'; c++) implicitTypes[c - 'A'].type = BaseType_Real;
}

void presetOffsetCalculation(void) {
    Symbol *symbol;
    int size;

    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        switch (symbol->class) {
        case SymClass_Auto:
        case SymClass_Static:
        case SymClass_Global:
            size = calculateSize(symbol);
            break;
        case SymClass_Function:
            size = calculateSize(symbol);
            break;
        default:
            break;
        }
    }
}

void registerIntrinsicFunctions(void) {
    IntrinsicFnDefn *defn;
    Symbol *generic;
    int i;
    int n;
    Symbol *new;

    for (defn = intrinsicFnDefns; defn->identifier != NULL; defn++) {
        if (defn->generic != NULL) {
            generic = findIntrinsicFunction(defn->generic);
            new = addNode(defn->identifier, SymClass_Intrinsic, &intrinsicFunctions);
            if (new == NULL) {
                new = allocSymbol(defn->identifier, SymClass_Intrinsic);
            }
            new->next = generic->next;
            generic->next = new;
        }
        else {
            new = addNode(defn->identifier, SymClass_Function, &intrinsicFunctions);
            new->details.intrinsic.isGeneric = TRUE;
        }
        new->details.intrinsic.externName = defn->externName;
        new->details.intrinsic.resultType = defn->resultType;
        new->details.intrinsic.argc       = defn->argc;
        n = defn->argc;
        if (n < 0) n = -n;
        for (i = 0; i < n; i++) {
            new->details.intrinsic.argumentTypes[i] = defn->argumentTypes[i];
        }
    }
}

void removeAllShadows(void) {
    Symbol *symbol;

    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        removeShadow(symbol);
    }
}

void removeShadow(Symbol *symbol) {
    if (symbol->shadow != NULL) {
        freeNode(symbol->shadow);
        symbol->shadow = NULL;
    }
    if (symbol->isShadow) {
        symbol->isDeleted = TRUE;
        symbol->isShadow = FALSE;
        symbol->class = SymClass_Undefined;
    }
}

void reportUnresolvedLabels(void) {
    Symbol *label;

    for (label = labels; label != NULL; label = label->next) {
        if (label->details.label.forwardRef) {
            err("Missing line label: %s\n", label->identifier);
        }
    }
}

void resetCommonBlocks(void) {
    resetCommonTree(commonBlocks);
}

static void resetCommonTree(Symbol *symbol) {
    if (symbol != NULL) {
        resetCommonTree(symbol->left);
        resetCommonTree(symbol->right);
        symbol->details.common.offset = 0;
    }
}

void resolveTypes(void) {
    Symbol *symbol;

    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        defineType(symbol);
    }
}
