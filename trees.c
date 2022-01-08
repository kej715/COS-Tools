/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: trees.c
**
**  Description:
**      This file provides functions for managing trees.
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

static void adjustSymValsForQuals(Qualifier *qualifier);
static void adjustSymValsForSyms(Symbol *symbol);
static Name *allocName(char *id, int len);
static Qualifier *allocQualifier(char *id, int len);
static Symbol *allocSymbol(char *id, int len, Value *value);
static void freeName(Name *name);
static void freeQualifier(Qualifier *qualifier);
static void freeSymbol(Symbol *symbol);
static void resetBlock(Block *block);

Literal *addLiteral(Token *expression) {
    Literal *lp;
    Literal *newLiteral;

    if (currentModule->literals == NULL) {
        newLiteral = (Literal *)allocate(sizeof(Literal));
        newLiteral->expression = copyToken(expression);
        currentModule->literals = newLiteral;
        return newLiteral;
    }
    else {
        lp = currentModule->literals;
        while (TRUE) {
            if (equalTokens(expression, lp->expression)) return lp;
            if (lp->next == NULL) {
                newLiteral = (Literal *)allocate(sizeof(Literal));
                newLiteral->expression = copyToken(expression);
                lp->next = newLiteral;
                return newLiteral;
            }
            lp = lp->next;
        }
    }
}

ErrorCode addLocationSymbol(char *id, int len, u16 attributes) {
    ErrorCode err;
    Symbol *symbol;
    Value val;
    i64 value;

    if (isNameChar1(*id) == FALSE) return Err_LocationField;

    err = Err_None;
    val.type = NumberType_Integer;
    val.attributes = attributes;
    val.block = currentBlock;
    if (currentModule->isAbsolute == FALSE) val.attributes |= SYM_RELOCATABLE;
    val.intValue = currentBlock->locationCounter;
    if ((attributes & SYM_WORD_ADDRESS) != 0) val.intValue >>= 2;
    symbol = findSymbol(id, len, currentQualifier);
    if (symbol != NULL) {
        if (pass == 1) {
            if ((symbol->value.attributes & SYM_UNDEFINED) != 0) {
                symbol->value.attributes = val.attributes;
                symbol->value.block = val.block;
                symbol->value.intValue = val.intValue;
            }
            else {
                err = Err_DoubleDefinition;
            }
        }
        else if (symbol->value.intValue != val.intValue
                 || symbol->value.block != val.block
                 || ((symbol->value.attributes ^ val.attributes) & ~(SYM_UNDEFINED|SYM_ENTRY)) != 0) {
            err = Err_DoubleDefinition;
        }
    }
    else {
        symbol = addSymbol(id, len, currentQualifier, &val);
    }
    return err;
}

Module *addModule(char *id, int len) {
    Block *block;
    Module *module;
    Name *name;
    Qualifier *qualifier;
    Module *savedModule;
    Value val;

    name = addName(&moduleNames, id, len);
    module = (Module *)allocate(sizeof(Module));
    if (firstModule == NULL) {
        firstModule = module;
    }
    else {
        lastModule->next = module;
    }
    lastModule = module;
    name->value = module;
    module->id = name->id;
    module->imageSize = IMAGE_INCREMENT;
    module->image = (u8 *)allocate(module->imageSize);
    savedModule = currentModule;
    currentModule = module;
    block = (Block *)allocate(sizeof(Block));       // add nominal block
    block->id = "";
    module->firstBlock = block;
    block->next = (Block *)allocate(sizeof(Block)); // add liternals block
    block = block->next;
    block->id = "=";
    module->lastBlock = block;
    module->qualifiers = qualifier = addQualifier("", 0);
    val.type = NumberType_Integer;
    val.attributes = SYM_PARCEL_ADDRESS|SYM_COUNTER;
    val.block = NULL;
    val.intValue = 0;
    addSymbol("*",  1, qualifier, &val);
    addSymbol("*O", 2, qualifier, &val);
    val.attributes = SYM_COUNTER;
    addSymbol("*P", 2, qualifier, &val);
    addSymbol("*W", 2, qualifier, &val);
    currentModule = savedModule;
    return module;
}

Name *addName(Name **root, char *id, int len) {
    Name *current;
    Name *new;
    int valence;

    new = allocName(id, len);
    current = *root;
    if (current == NULL) {
        *root = new;
        return new;
    }
    while (current != NULL) {
        valence = strncasecmp(current->id, id, len);
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
        else if (current->id[len] == '\0') {
            freeName(new);
            new = NULL;
            break;
        }
        else {
            current = current->left;
        }
    }
    return new;
}

Qualifier *addQualifier(char *id, int len) {
    Qualifier *current;
    Qualifier *new;
    int valence;

    new = allocQualifier(id, len);
    current = currentModule->qualifiers;
    if (current == NULL) {
        currentModule->qualifiers = new;
        return new;
    }
    while (current != NULL) {
        valence = strncasecmp(current->id, id, len);
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
        else if (current->id[len] == '\0') {
            freeQualifier(new);
            new = NULL;
            break;
        }
        else {
            current = current->left;
        }
    }
    return new;
}

Symbol *addSymbol(char *id, int len, Qualifier *qualifier, Value *value) {
    Symbol *current;
    Symbol *new;
    int valence;

    new = allocSymbol(id, len, value);
    current = qualifier->symbols;
    if (current == NULL) {
        qualifier->symbols = new;
        return new;
    }

    while (current != NULL) {
        valence = strncasecmp(current->id, id, len);
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
        else if (current->id[len] == '\0') {
            freeSymbol(new);
            new = NULL;
            break;
        }
        else {
            current = current->left;
        }
    }
    return new;
}

void adjustSymbolValues(Module *module) {
    adjustSymValsForQuals(module->qualifiers);
}

static void adjustSymValsForQuals(Qualifier *qualifier) {
    if (qualifier == NULL) return;
    adjustSymValsForSyms(qualifier->symbols);
    adjustSymValsForQuals(qualifier->left);
    adjustSymValsForQuals(qualifier->right);
}

static void adjustSymValsForSyms(Symbol *symbol) {
    if (symbol == NULL) return;
    if (symbol->value.block != NULL) {
        if ((symbol->value.attributes & SYM_WORD_ADDRESS) != 0)
            symbol->value.intValue += symbol->value.block->originOffset >> 2;
        else if ((symbol->value.attributes & SYM_PARCEL_ADDRESS) != 0)
            symbol->value.intValue += symbol->value.block->originOffset;
    }
    adjustSymValsForSyms(symbol->left);
    adjustSymValsForSyms(symbol->right);
}

static Name *allocName(char *id, int len) {
    Name *name;
    char *s;

    s = (char *)allocate(len + 1);
    name = (Name *)allocate(sizeof(Name));
    memcpy(s, id, len);
    name->id = s;
    return name;
}

static Qualifier *allocQualifier(char *id, int len) {
    Qualifier *qualifier;
    char *s;

    s = (char *)allocate(len + 1);
    qualifier = (Qualifier *)allocate(sizeof(Qualifier));
    memcpy(s, id, len);
    qualifier->id = s;
    return qualifier;
}

static Symbol *allocSymbol(char *id, int len, Value *value) {
    Symbol *symbol;
    char *s;

    s = (char *)allocate(len + 1);
    symbol = (Symbol *)allocate(sizeof(Symbol));
    memcpy(s, id, len);
    symbol->id = s;
    symbol->value.type = value->type;
    symbol->value.attributes = value->attributes;
    symbol->value.block = value->block;
    memcpy(&symbol->value, value, sizeof(Value));
    return symbol;
}

void calculateBlockOffsets(Module *module) {
    Block *block;
    u32 offset;

    offset = 0;
    block = module->firstBlock;
    while (block != NULL) {
        block->originOffset = block->originCounter = block->locationCounter = offset;
        offset = (offset + block->size + 3) & 0xfffffc;
        block = block->next;
    }
    module->size = (offset + 3) >> 2; // size in words
}

Module *findModule(char *id, int len) {
    Module *module;
    Name *name;

    name = findName(moduleNames, id, len);
    if (name != NULL) return (Module *)name->value;
    return NULL;
}

Name *findName(Name *root, char *id, int len) {
    Name *current;
    int valence;

    current = root;
    while (current != NULL) {
        valence = strncasecmp(current->id, id, (size_t)len);
        if (valence > 0)
            current = current->left;
        else if (valence < 0)
            current = current->right;
        else if (current->id[len] == '\0')
            break;
        else
            current = current->left;
    }
    return current;
}

Symbol *findQualifiedSymbol(Token *token) {
    Qualifier *qualifier;
    Symbol *symbol;

    symbol = NULL;
    if (token->type == TokenType_Name) {
        if (token->details.name.qualPtr != NULL) {
            qualifier = findQualifierWithLen(token->details.name.qualPtr, token->details.name.qualLen);
            if (qualifier != NULL) symbol = findSymbol(token->details.name.ptr, token->details.name.len, qualifier);
        }
        else {
            symbol = findSymbol(token->details.name.ptr, token->details.name.len, currentQualifier);
            if (symbol == NULL) {
                qualifier = findQualifier("");
                if (qualifier != NULL) symbol = findSymbol(token->details.name.ptr, token->details.name.len, qualifier);
            }
        }
    }
    return symbol;
}

Qualifier *findQualifier(char *id) {
    Qualifier *current;
    int valence;

    current = currentModule->qualifiers;
    while (current != NULL) {
        valence = strcasecmp(current->id, id);
        if (valence > 0)
            current = current->left;
        else if (valence < 0)
            current = current->right;
        else
            break;
    }
    return current;
}

Qualifier *findQualifierWithLen(char *id, int len) {
    Qualifier *current;
    int valence;

    current = currentModule->qualifiers;
    while (current != NULL) {
        valence = strncasecmp(current->id, id, (size_t)len);
        if (valence > 0)
            current = current->left;
        else if (valence < 0)
            current = current->right;
        else if (current->id[len] == '\0')
            break;
        else
            current = current->left;
    }
    return current;
}

Symbol *findSymbol(char *id, int len, Qualifier *qualifier) {
    Symbol *current;
    int valence;

    current = qualifier->symbols;
    while (current != NULL) {
        valence = strncasecmp(current->id, id, (size_t)len);
        if (valence > 0)
            current = current->left;
        else if (valence < 0)
            current = current->right;
        else if (current->id[len] == '\0')
            break;
        else
            current = current->left;
    }
    return current;
}

static void freeName(Name *name) {
    free(name->id);
    free(name);
}

static void freeQualifier(Qualifier *qualifier) {
    free(qualifier->id);
    free(qualifier);
}

static void freeSymbol(Symbol *symbol) {
    free(symbol->id);
    free(symbol);
}

void resetModule(Module *module) {
    Block *block;

    block = module->firstBlock;
    while (block != NULL) {
        resetBlock(block);
        block = block->next;
    }
}

static void resetBlock(Block *block) {
    block->originCounter = block->originOffset;
    block->locationCounter = block->originOffset;
    block->wordBitPosCounter = 0;
    block->parcelBitPosCounter = 0;
}
