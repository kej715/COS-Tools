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
#include "const.h"
#include "proto.h"
#include "types.h"

static Symbol *addNode(char *identifier, SymbolClass class, Symbol **tree);
static Symbol *allocNode(char *identifier, SymbolClass class);
static char *dataTypeToStr(DataType *dt);
static void freeNode(Symbol *symbol);
static void freeTree(Symbol *symbol);
static void printTree(FILE *f, Symbol *symbol);
static char *symClassToStr(SymbolClass class);

static Symbol *labels = NULL;
static int labelCounter = 0;
static Symbol *lastSymbol = NULL;
static Symbol *symbols = NULL;

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

    len = strlen(identifier);
    s = (char *)allocate(len + 1);
    symbol = (Symbol *)allocate(sizeof(Symbol));
    memcpy(s, identifier, len);
    symbol->identifier = s;
    symbol->class = class;
    memset(&symbol->details, 0, sizeof(SymbolDetails));

    return symbol;
}

int calculateLocalOffsets(void) {
    int offset;
    Symbol *symbol;
    int totalSize;

    /*
     *  Pass 1. Calculate the size of each synbol and the total amount of storage needed.
     */
    totalSize = 0;
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        totalSize += calculateSize(symbol);;
    }
    /*
     *  Pass 2. Calculate the offset of each symbol.
     */
    offset = -totalSize;
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        if (symbol->class == SymClass_Local) {
            symbol->details.variable.offset = offset;
        }
        else if (symbol->class == SymClass_Function) {
            symbol->details.progUnit.offset = offset;
        }
        offset += symbol->size;
    }

    return totalSize;
}

int calculateSize(Symbol *symbol) {
    Bounds *bounds;
    DataType *dt;
    int i;

    if (symbol->class == SymClass_Local || symbol->class == SymClass_Function) {
        dt = (symbol->class == SymClass_Local)
           ? &symbol->details.variable.dt
           : &symbol->details.progUnit.dt;
        switch (dt->type) {
        case BaseType_Character:
            symbol->size = (dt->constraint + 7) / 8;
            break;
        case BaseType_Logical:
        case BaseType_Integer:
        case BaseType_Real:
        case BaseType_Label:
        case BaseType_Pointer:
            symbol->size = 1;
            break;
        case BaseType_Double:
        case BaseType_Complex:
            symbol->size = 2;
            break;
        default:
            symbol->size = 0;
            break;
        }
        for (i = 0; i < dt->rank; i++) {
            bounds = &dt->bounds[i];
            symbol->size *= (bounds->upper - bounds->lower) + 1;
        }
    }
    else {
        symbol->size = 0;
    }

    return symbol->size;
}

static char *dataTypeToStr(DataType *dt) {
    static char buf[32];

    switch (dt->type) {
    case BaseType_Undefined: return "Undefined";
    case BaseType_Logical:   return "Logical";
    case BaseType_Integer:   return "Integer";
    case BaseType_Real:      return "Real";
    case BaseType_Double:    return "Double";
    case BaseType_Complex:   return "Complex";
    case BaseType_Label:     return "Label";
    case BaseType_Pointer:   return "Pointer";
    default:                 return "Unknown";
    case BaseType_Character:
        if (dt->constraint > 0) {
            sprintf(buf, "Character*%d", dt->constraint);
            return buf;
        }
        else if (dt->constraint == 0) {
            return "Character";
        }
        else {
            return "Character*(*)";
        }
    }
}

Symbol *findLabel(char *label) {
    Symbol *current;
    int valence;

    current = labels;
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
    Symbol *current;
    int valence;

    current = symbols;
    while (current != NULL) {
        valence = strcmp(current->identifier, identifier);
        if (valence > 0)
            current = current->left;
        else if (valence < 0)
            current = current->right;
        else
            break;
    }

    return current;
}

void freeAllSymbols(void) {
    freeTree(symbols);
    freeTree(labels);
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
    sprintf(label, "L%d", ++labelCounter);
}

void printSymbols(FILE *f) {

    if (f == NULL) return;

    fputs("1 Symbols\n", f);
    fputs("  Name                            Class      Type           Size    Location\n", f);
    fputs("  ------------------------------- ---------- -------------- ------- --------\n", f);
    printTree(f, symbols);
}

static void printTree(FILE *f, Symbol *symbol) {
    int size;

    if (symbol != NULL) {
        printTree(f, symbol->left);
        fprintf(f, "  %-31s", symbol->identifier);
        fprintf(f, " %-10s", symClassToStr(symbol->class));
        switch (symbol->class) {
        case SymClass_Function:
        case SymClass_Local:
        case SymClass_Global:
        case SymClass_Argument:
        SymClass_Parameter:
            fprintf(f, " %-14s", dataTypeToStr(&symbol->details.variable.dt));
            size = calculateSize(symbol);
            if (size > 0)
                fprintf(f, " %-7d", size);
            else
                fputs("        ", f);
            switch (symbol->class) {
            case SymClass_Local:
            case SymClass_Argument:
                fprintf(f, " %d", symbol->details.variable.offset);
                break;
            case SymClass_Function:
                fprintf(f, " %d", symbol->details.progUnit.offset);
                break;
            case SymClass_Global:
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
        fputs("\n", f);
        printTree(f, symbol->right);
    }
}

static char *symClassToStr(SymbolClass class) {
    switch (class) {
    case SymClass_Undefined:   return "Undefined";
    case SymClass_Program:     return "Program";
    case SymClass_BlockData:   return "Block Data";
    case SymClass_Subroutine:  return "Subroutine";
    case SymClass_Function:    return "Function";
    case SymClass_NamedCommon: return "Common";
    case SymClass_Local:       return "Local";
    case SymClass_Global:      return "Global";
    case SymClass_Argument:    return "Argument";
    case SymClass_Parameter:   return "Parameter";
    default:                   return "Unknown";
    }
}
