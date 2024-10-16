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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "const.h"
#include "proto.h"
#include "types.h"

static Symbol *addNode(char *identifier, SymbolClass class, Symbol **tree);
static Symbol *allocNode(char *identifier, SymbolClass class);
static void emitCommonTree(Symbol *symbol);
static Symbol *findNode(char *label, Symbol *tree);
static void freeNode(Symbol *symbol);
static void freeTree(Symbol *symbol);
static void resetCommonTree(Symbol *symbol);

static Symbol *commonBlocks = NULL;
static Symbol *intrinsicFunctions = NULL;
static Symbol *labels = NULL;
static int labelCounter = 0;
static int labelPrefixIdx = 0;
static char labelPrefixes[] = {'H','I','G','J','F','K','E','L','D','M','C','N','B','O','A','P'};
static Symbol *lastSymbol = NULL;
static DataType undefinedType = { BaseType_Undefined };

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
    {"INT",    NULL,    "_iint",   BaseType_Integer,   1, {BaseType_Integer}},
    {"INT",    "INT",   "_rint",   BaseType_Integer,   1, {BaseType_Real}},
    {"INT",    "INT",   "_rint",   BaseType_Integer,   1, {BaseType_Double}},
    {"IFIX",   "INT",   "_rint",   BaseType_Integer,   1, {BaseType_Real}},
    {"IDINT",  "INT",   "_rint",   BaseType_Integer,   1, {BaseType_Double}},

    {"REAL",   NULL,    "_ireal",  BaseType_Real,      1, {BaseType_Integer}},
    {"REAL",   "REAL",  "_rreal",  BaseType_Real,      1, {BaseType_Real}},
    {"REAL",   "REAL",  "_rreal",  BaseType_Real,      1, {BaseType_Double}},
    {"FLOAT",  "REAL",  "_ireal",  BaseType_Real,      1, {BaseType_Integer}},
    {"SNGL",   "REAL",  "_rreal",  BaseType_Real,      1, {BaseType_Double}},

    {"DBLE",   NULL,    "_ireal",  BaseType_Double,    1, {BaseType_Integer}},
    {"DBLE",   "DBLE",  "_rreal",  BaseType_Double,    1, {BaseType_Real}},
    {"DBLE",   "DBLE",  "_rreal",  BaseType_Double,    1, {BaseType_Double}},

    {"ICHAR",  NULL,    "_ichar",  BaseType_Integer,   1, {BaseType_Character}},

    {"CHAR",   NULL,    "_char",   BaseType_Character, 1, {BaseType_Integer}},

    {"AINT",   NULL,    "_aint",   BaseType_Real,      1, {BaseType_Real}},
    {"AINT",   "AINT",  "_aint",   BaseType_Double,    1, {BaseType_Double}},
    {"DINT",   "AINT",  "_aint",   BaseType_Double,    1, {BaseType_Double}},

    {"ANINT",  NULL,    "_anint",  BaseType_Real,      1, {BaseType_Real}},
    {"ANINT",  "ANINT", "_anint",  BaseType_Double,    1, {BaseType_Double}},
    {"DNINT",  "ANINT", "_anint",  BaseType_Double,    1, {BaseType_Double}},

    {"NINT",   NULL,    "_nint",   BaseType_Integer,   1, {BaseType_Real}},
    {"NINT",   "NINT",  "_nint",   BaseType_Integer,   1, {BaseType_Double}},
    {"IDNINT", "NINT",  "_nint",   BaseType_Integer,   1, {BaseType_Double}},

    {"ABS",    NULL,    "_iabs",   BaseType_Integer,   1, {BaseType_Integer}},
    {"ABS",    "ABS",   "_rabs",   BaseType_Real,      1, {BaseType_Real}},
    {"ABS",    "ABS",   "_rabs",   BaseType_Double,    1, {BaseType_Double}},
    {"IABS",   "ABS",   "_iabs",   BaseType_Integer,   1, {BaseType_Integer}},
    {"DABS",   "ABS",   "_rabs",   BaseType_Double,    1, {BaseType_Double}},

    {"MOD",    NULL,    "_imod",   BaseType_Integer,   2, {BaseType_Integer, BaseType_Integer}},
    {"MOD",    "MOD",   "_rmod",   BaseType_Real,      2, {BaseType_Real,    BaseType_Real}},
    {"MOD",    "MOD",   "_rmod",   BaseType_Double,    2, {BaseType_Double,  BaseType_Double}},
    {"AMOD",   "MOD",   "_rmod",   BaseType_Real,      2, {BaseType_Real,    BaseType_Real}},
    {"DMOD",   "MOD",   "_rmod",   BaseType_Double,    2, {BaseType_Double,  BaseType_Double}},

    {"SIGN",   NULL,    "_isign",  BaseType_Integer,   2, {BaseType_Integer, BaseType_Integer}},
    {"SIGN",   "SIGN",  "_rsign",  BaseType_Real,      2, {BaseType_Real,    BaseType_Real}},
    {"SIGN",   "SIGN",  "_rsign",  BaseType_Double,    2, {BaseType_Double,  BaseType_Double}},
    {"ISIGN",  "SIGN",  "_isign",  BaseType_Integer,   2, {BaseType_Integer, BaseType_Integer}},
    {"DSIGN",  "SIGN",  "_rsign",  BaseType_Double,    2, {BaseType_Double,  BaseType_Double}},

    {"DIM",    NULL,    "_idim",   BaseType_Integer,   2, {BaseType_Integer, BaseType_Integer}},
    {"DIM",    "DIM",   "_rdim",   BaseType_Real,      2, {BaseType_Real,    BaseType_Real}},
    {"DIM",    "DIM",   "_rdim",   BaseType_Double,    2, {BaseType_Double,  BaseType_Double}},
    {"IDIM",   "DIM",   "_idim",   BaseType_Integer,   2, {BaseType_Integer, BaseType_Integer}},
    {"DDIM",   "DIM",   "_rdim",   BaseType_Double,    2, {BaseType_Double,  BaseType_Double}},

    {"LEN",    NULL,    "_len",    BaseType_Integer,   1, {BaseType_Character}},

    {"INDEX",  NULL,    "_index",  BaseType_Integer,   2, {BaseType_Character, BaseType_Character}},

    {"SQRT",   NULL,    "_isqrt",  BaseType_Real,      1, {BaseType_Integer}},
    {"SQRT",   "SQRT",  "_rsqrt",  BaseType_Real,      1, {BaseType_Real}},
    {"SQRT",   "SQRT",  "_rsqrt",  BaseType_Double,    1, {BaseType_Double}},
    {"DSQRT",  "SQRT",  "_rsqrt",  BaseType_Double,    1, {BaseType_Double}},

    {"EXP",    NULL,    "_iexp",   BaseType_Real,      1, {BaseType_Integer}},
    {"EXP",    "EXP",   "_rexp",   BaseType_Real,      1, {BaseType_Real}},
    {"EXP",    "EXP",   "_rexp",   BaseType_Double,    1, {BaseType_Double}},
    {"DEXP",   "EXP",   "_rexp",   BaseType_Double,    1, {BaseType_Double}},

    {"LOG",    NULL,    "_ilog",   BaseType_Real,      1, {BaseType_Integer}},
    {"LOG",    "LOG",   "_rlog",   BaseType_Real,      1, {BaseType_Real}},
    {"LOG",    "LOG",   "_rlog",   BaseType_Double,    1, {BaseType_Double}},
    {"ALOG",   "LOG",   "_rlog",   BaseType_Real,      1, {BaseType_Real}},
    {"DLOG",   "LOG",   "_rlog",   BaseType_Double,    1, {BaseType_Double}},

    {"LOG10",  NULL,    "_ilog10", BaseType_Real,      1, {BaseType_Integer}},
    {"LOG10",  "LOG10", "_rlog10", BaseType_Real,      1, {BaseType_Real}},
    {"LOG10",  "LOG10", "_rlog10", BaseType_Double,    1, {BaseType_Double}},
    {"ALOG10", "LOG10", "_rlog10", BaseType_Real,      1, {BaseType_Real}},
    {"DLOG10", "LOG10", "_rlog10", BaseType_Double,    1, {BaseType_Double}},

    {"SIN",    NULL,    "_isin",   BaseType_Real,      1, {BaseType_Integer}},
    {"SIN",    "SIN",   "_rsin",   BaseType_Real,      1, {BaseType_Real}},
    {"SIN",    "SIN",   "_rsin",   BaseType_Double,    1, {BaseType_Double}},
    {"DSIN",   "SIN",   "_rsin",   BaseType_Double,    1, {BaseType_Double}},

    {"COS",    NULL,    "_icos",   BaseType_Real,      1, {BaseType_Integer}},
    {"COS",    "COS",   "_rcos",   BaseType_Real,      1, {BaseType_Real}},
    {"COS",    "COS",   "_rcos",   BaseType_Double,    1, {BaseType_Double}},
    {"DCOS",   "COS",   "_rcos",   BaseType_Double,    1, {BaseType_Double}},

    {"TAN",    NULL,    "_itan",   BaseType_Real,      1, {BaseType_Integer}},
    {"TAN",    "TAN",   "_rtan",   BaseType_Real,      1, {BaseType_Real}},
    {"TAN",    "TAN",   "_rtan",   BaseType_Double,    1, {BaseType_Double}},
    {"DTAN",   "TAN",   "_rtan",   BaseType_Double,    1, {BaseType_Double}},

    {"ASIN",   NULL,    "_iasin",  BaseType_Real,      1, {BaseType_Integer}},
    {"ASIN",   "ASIN",  "_rasin",  BaseType_Real,      1, {BaseType_Real}},
    {"ASIN",   "ASIN",  "_rasin",  BaseType_Double,    1, {BaseType_Double}},
    {"DASIN",  "ASIN",  "_rasin",  BaseType_Double,    1, {BaseType_Double}},

    {"ACOS",   NULL,    "_iacos",  BaseType_Real,      1, {BaseType_Integer}},
    {"ACOS",   "ACOS",  "_racos",  BaseType_Real,      1, {BaseType_Real}},
    {"ACOS",   "ACOS",  "_racos",  BaseType_Double,    1, {BaseType_Double}},
    {"DACOS",  "ACOS",  "_racos",  BaseType_Double,    1, {BaseType_Double}},

    {"ATAN",   NULL,    "_iatan",  BaseType_Real,      1, {BaseType_Integer}},
    {"ATAN",   "ATAN",  "_ratan",  BaseType_Real,      1, {BaseType_Real}},
    {"ATAN",   "ATAN",  "_ratan",  BaseType_Double,    1, {BaseType_Double}},
    {"DATAN",  "ATAN",  "_ratan",  BaseType_Double,    1, {BaseType_Double}},

    {"ATAN2",  NULL,    "_iatan2", BaseType_Real,      2, {BaseType_Integer, BaseType_Integer}},
    {"ATAN2",  "ATAN2", "_ratan2", BaseType_Real,      2, {BaseType_Real,    BaseType_Real}},
    {"ATAN2",  "ATAN2", "_ratan2", BaseType_Double,    2, {BaseType_Double,  BaseType_Double}},
    {"DATAN2", "ATAN2", "_ratan2", BaseType_Double,    2, {BaseType_Double,  BaseType_Double}},

    {"SINH",   NULL,    "_isinh",   BaseType_Real,     1, {BaseType_Integer}},
    {"SINH",   "SINH",  "_rsinh",   BaseType_Real,     1, {BaseType_Real}},
    {"SINH",   "SINH",  "_rsinh",   BaseType_Double,   1, {BaseType_Double}},
    {"DSINH",  "SINH",  "_rsinh",   BaseType_Double,   1, {BaseType_Double}},

    {"COSH",   NULL,    "_icosh",   BaseType_Real,     1, {BaseType_Integer}},
    {"COSH",   "COSH",  "_rcosh",   BaseType_Real,     1, {BaseType_Real}},
    {"COSH",   "COSH",  "_rcosh",   BaseType_Double,   1, {BaseType_Double}},
    {"DCOSH",  "COSH",  "_rcosh",   BaseType_Double,   1, {BaseType_Double}},

    {"TANH",   NULL,    "_itanh",   BaseType_Real,     1, {BaseType_Integer}},
    {"TANH",   "TANH",  "_rtanh",   BaseType_Real,     1, {BaseType_Real}},
    {"TANH",   "TANH",  "_rtanh",   BaseType_Double,   1, {BaseType_Double}},
    {"DTANH",  "TANH",  "_rtanh",   BaseType_Double,   1, {BaseType_Double}},

    {"LGE",    NULL,    "_lge",     BaseType_Logical,  2, {BaseType_Character, BaseType_Character}},
    {"LGT",    NULL,    "_lgt",     BaseType_Logical,  2, {BaseType_Character, BaseType_Character}},
    {"LLE",    NULL,    "_lle",     BaseType_Logical,  2, {BaseType_Character, BaseType_Character}},
    {"LLT",    NULL,    "_llt",     BaseType_Logical,  2, {BaseType_Character, BaseType_Character}},

    {"MAX",    NULL,    "_imax",    BaseType_Integer, -1, {BaseType_Integer}},
    {"MAX",    "MAX",   "_rmax",    BaseType_Real,    -1, {BaseType_Real}},
    {"MAX",    "MAX",   "_rmax",    BaseType_Double,  -1, {BaseType_Double}},
    {"MAX0",   "MAX",   "_imax",    BaseType_Integer, -1, {BaseType_Integer}},
    {"MAX1",   "MAX",   "_imax1",   BaseType_Integer, -1, {BaseType_Real}},
    {"AMAX1",  "MAX",   "_rmax",    BaseType_Real,    -1, {BaseType_Real}},
    {"AMAX0",  NULL,    "_amax0",   BaseType_Real,    -1, {BaseType_Integer}},

    {"MIN",    NULL,    "_imin",    BaseType_Integer, -1, {BaseType_Integer}},
    {"MIN",    "MIN",   "_rmin",    BaseType_Real,    -1, {BaseType_Real}},
    {"MIN",    "MIN",   "_rmin",    BaseType_Double,  -1, {BaseType_Double}},
    {"MIN0",   "MIN",   "_imin",    BaseType_Integer, -1, {BaseType_Integer}},
    {"MIN1",   "MIN",   "_imin1",   BaseType_Integer, -1, {BaseType_Real}},
    {"AMIN1",  "MIN",   "_rmin",    BaseType_Real,    -1, {BaseType_Real}},
    {"AMIN0",  NULL,    "_amin0",   BaseType_Real,    -1, {BaseType_Integer}},

    {NULL}
};

Symbol *addCommonBlock(char *name) {
    Symbol *symbol;

    return addNode(name, SymClass_NamedCommon, &commonBlocks);
}

Symbol *addLabel(char *label) {
    Symbol *symbol;

    symbol = addNode(label, SymClass_Label, &labels);
    generateLabel(symbol->details.label.label);

    return symbol;
}

static Symbol *addNode(char *identifier, SymbolClass class, Symbol **tree) {
    Symbol *current;
    Symbol *new;
    int valence;

    new = allocNode(identifier, class);
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
    if (new != NULL) {
        if (lastSymbol != NULL) {
            lastSymbol->next = new;
        }
        lastSymbol = new;
    }

    return new;
}

static Symbol *allocNode(char *identifier, SymbolClass class) {
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
    int offset;
    Symbol *symbol;
    int totalSize;

    /*
     *  Pass 1. Calculate the size of each synbol and the total amount of storage needed.
     */
    totalSize = 0;
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        if (symbol->class == SymClass_Auto || symbol->class == SymClass_Function) {
            totalSize += calculateSize(symbol);;
        }
    }
    /*
     *  Pass 2. Calculate the offset of each symbol.
     */
    offset = -totalSize;
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        if (symbol->class == SymClass_Auto) {
            symbol->details.variable.offset = offset;
            offset += symbol->size;
        }
        else if (symbol->class == SymClass_Function) {
            symbol->details.progUnit.offset = offset;
            offset += symbol->size;
        }
    }

    return totalSize;
}

int calculateSize(Symbol *symbol) {
    DataType *dt;

    dt = getSymbolType(symbol);
    switch (dt->type) {
    case BaseType_Character:
        symbol->size = (dt->constraint > 0) ? (dt->constraint + 7) / 8 : 1;
        break;
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
    return symbol->size * countArrayElements(symbol);
}

int calculateStaticOffsets(void) {
    int offset;
    Symbol *symbol;
    int totalSize;

    /*
     *  Pass 1. Calculate the size of each synbol and the total amount of storage needed.
     */
    totalSize = 0;
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        if (symbol->class == SymClass_Static) {
            totalSize += calculateSize(symbol);;
        }
    }
    /*
     *  Pass 2. Calculate the offset of each symbol.
     */
    offset = 0;
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        if (symbol->class == SymClass_Static) {
            symbol->details.variable.offset = offset;
            offset += symbol->size;
        }
    }

    return totalSize;
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

void emitCommonBlocks(void) {
    emitCommonTree(commonBlocks);
}

static void emitCommonTree(Symbol *symbol) {
    if (symbol != NULL) {
        emitCommonTree(symbol->left);
        emitCommonTree(symbol->right);
        emitActivateSection(symbol->identifier, "COMMON");
        emitWordBlock(symbol->details.common.label, symbol->details.common.limit);
        emitDeactivateSection(symbol->identifier);
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
        else
            break;
    }

    return current;
}

Symbol *findSymbol(char *identifier) {
    return findNode(identifier, symbols);
}

void freeAllSymbols(void) {
    freeTree(symbols);
    symbols = NULL;
    freeTree(labels);
    labels = NULL;
    lastSymbol = NULL;
}

static void freeNode(Symbol *symbol) {
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
    switch (symbol->class) {
    case SymClass_Undefined:
    case SymClass_Auto:
    case SymClass_Static:
    case SymClass_Global:
    case SymClass_Argument:
        return &symbol->details.variable.dt;
    case SymClass_Function:
        return &symbol->details.progUnit.dt;
    case SymClass_Parameter:
        return &symbol->details.param.dt;
    case SymClass_Pointee:
        return &symbol->details.pointee.dt;
    default:
        return &undefinedType;
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
            generic = findNode(defn->generic, intrinsicFunctions);
            new = addNode(defn->identifier, SymClass_Function, &intrinsicFunctions);
            if (new == NULL) {
                new = allocNode(defn->identifier, SymClass_Function);
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
        new->details.intrinsic.argc = defn->argc;
        n = defn->argc;
        if (n < 0) n = -n;
        for (i = 0; i < n; i++) {
            new->details.intrinsic.argumentTypes[i] = defn->argumentTypes[i];
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
