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
#include "calconst.h"
#include "calproto.h"
#include "caltypes.h"
#include "services.h"

static void adjustSymValsForQuals(Qualifier *qualifier);
static void adjustSymValsForSyms(Symbol *symbol);
static Name *allocName(char *id, int len);
static Qualifier *allocQualifier(char *id, int len);
static Symbol *allocSymbol(char *id, int len, Value *value);
static void freeName(Name *name);
static void freeQualifier(Qualifier *qualifier);
static void freeSymbol(Symbol *symbol);
static void resetSection(Section *section);

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

ErrorCode addLocationSymbol(Section *section, char *id, int len, u16 attributes) {
    ErrorCode err;
    Symbol *symbol;
    Value val;
    i64 value;

    if (isNameChar1(*id) == FALSE) return Err_LocationField;

    err = Err_None;
    val.type = NumberType_Integer;
    val.attributes = attributes | getRelativeAttribute(section);
    val.section = section;
    val.intValue = section->locationCounter;
    if ((attributes & SYM_WORD_ADDRESS) != 0) val.intValue >>= 2;
    symbol = findSymbol(id, len, currentQualifier);
    if (symbol != NULL) {
        if (pass == 1) {
            if ((symbol->value.attributes & SYM_UNDEFINED) != 0) {
                symbol->value.attributes = val.attributes;
                symbol->value.section = val.section;
                symbol->value.intValue = val.intValue;
            }
            else {
                err = Err_DoubleDefinition;
            }
        }
        else if (symbol->value.intValue != val.intValue
                 || symbol->value.section != val.section
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
    Section *section;
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
    savedModule = currentModule;
    currentModule = module;
    section = addSection(module, "",  0, SectionType_Mixed, SectionLocation_CM);
    section = addSection(module, "=", 1, SectionType_Mixed, SectionLocation_CM);
    module->qualifiers = qualifier = addQualifier("", 0);
    val.type = NumberType_Integer;
    val.attributes = SYM_PARCEL_ADDRESS|SYM_COUNTER;
    val.section = NULL;
    val.intValue = 0;
    addSymbol("*",  1, qualifier, &val);
    addSymbol("*A", 2, qualifier, &val);
    addSymbol("*a", 2, qualifier, &val);
    addSymbol("*B", 2, qualifier, &val);
    addSymbol("*b", 2, qualifier, &val);
    addSymbol("*O", 2, qualifier, &val);
    addSymbol("*o", 2, qualifier, &val);
    val.attributes = SYM_COUNTER;
    addSymbol("*P", 2, qualifier, &val);
    addSymbol("*p", 2, qualifier, &val);
    addSymbol("*W", 2, qualifier, &val);
    addSymbol("*w", 2, qualifier, &val);
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
        valence = strncmp(current->id, id, len);
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
        valence = strncmp(current->id, id, len);
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

Section *addSection(Module *module, char *id, int len, SectionType type, SectionLocation location) {
    Section *section;

    section = (Section *)allocate(sizeof(Section));
    section->id = (char *)allocate(len + 1);
    memcpy(section->id, id, len);
    section->module = module;
    section->type = type;
    section->location = location;
    if (module->lastSection != NULL) {
        module->lastSection->next = section;
    }
    else {
        module->firstSection = section;
    }
    module->lastSection = section;
    return section;
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
        valence = strncmp(current->id, id, len);
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
    if (symbol->value.section != NULL) {
        if ((symbol->value.attributes & SYM_WORD_ADDRESS) != 0)
            symbol->value.intValue += symbol->value.section->originOffset >> 2;
        else if ((symbol->value.attributes & SYM_PARCEL_ADDRESS) != 0)
            symbol->value.intValue += symbol->value.section->originOffset;
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
    symbol->value.section = value->section;
    memcpy(&symbol->value, value, sizeof(Value));
    return symbol;
}

void createObjectBlocks(Module *module) {
    u16 index;
    ObjectBlock *localObjectBlock;
    ObjectBlock *objectBlock;
    u32 offset;
    Section *section;

    localObjectBlock = NULL;
    index = 0;
    offset = 0;
    for (section = module->firstSection; section != NULL; section = section->next) {
        switch (section->type) {
        case SectionType_Mixed:
        case SectionType_Code:
        case SectionType_Data:
            if (localObjectBlock == NULL) {
                localObjectBlock = (ObjectBlock *)allocate(sizeof(ObjectBlock));
                localObjectBlock->id = module->id;
                localObjectBlock->index = index++;
                localObjectBlock->type = SectionType_Mixed;
                localObjectBlock->location = section->location;
                if (module->lastObjectBlock != NULL) {
                    module->lastObjectBlock->next = localObjectBlock;
                }
                else {
                    module->firstObjectBlock = localObjectBlock;
                }
                module->lastObjectBlock = localObjectBlock;
            }
            section->originOffset = section->originCounter = section->locationCounter = offset;
            section->objectBlock = localObjectBlock;
            offset = (offset + section->size + 3) & 0xfffffc;
            break;
        case SectionType_Common:
            objectBlock = (ObjectBlock *)allocate(sizeof(ObjectBlock));
            objectBlock->id = section->id;
            objectBlock->index = index++;
            objectBlock->type = section->type;
            objectBlock->location = section->location;
            if (module->lastObjectBlock != NULL) {
                module->lastObjectBlock->next = objectBlock;
            }
            else {
                module->firstObjectBlock = objectBlock;
            }
            module->lastObjectBlock = section->objectBlock = objectBlock;
            // fall through
        case SectionType_Stack:
        case SectionType_Dynamic:
        case SectionType_TaskCom:
            section->originOffset = section->originCounter = section->locationCounter = 0;
            break;
        default:
            fprintf(stderr, "Unknown section type: %d\n", section->type);
            exit(1);
        }
    }
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
        valence = strncmp(current->id, id, (size_t)len);
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
    Module *savedModule;
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
        if (symbol == NULL && currentModule != defaultModule) {
            savedModule = currentModule;
            currentModule = defaultModule;
            symbol = findQualifiedSymbol(token);
            currentModule = savedModule;
        }
    }
    return symbol;
}

Qualifier *findQualifier(char *id) {
    Qualifier *current;
    int valence;

    current = currentModule->qualifiers;
    while (current != NULL) {
        valence = strcmp(current->id, id);
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
        valence = strncmp(current->id, id, (size_t)len);
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
        valence = strncmp(current->id, id, (size_t)len);
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

u16 getRelativeAttribute(Section *section) {
    switch (section->type) {
    case SectionType_Mixed:
    case SectionType_Code:
    case SectionType_Data:
        return currentModule->isAbsolute ? 0 : SYM_RELOCATABLE;
    case SectionType_Stack:
    case SectionType_TaskCom:
        return SYM_IMMOBILE;
    case SectionType_Common:
    case SectionType_Dynamic:
        return SYM_RELOCATABLE;
    default:
        fprintf(stderr, "Unknown section type: %d\n", section->type);
        exit(1);
    }
}

bool isAbsolute(Value *val) {
    return (val->attributes & (SYM_IMMOBILE|SYM_RELOCATABLE|SYM_EXTERNAL)) == 0;
}

bool isCodeSection(Section *section) {
    return section != NULL && (section->type == SectionType_Mixed || section->type == SectionType_Code);
}

bool isCommonSection(Section *section) {
    return section != NULL
        && (section->type == SectionType_Common
            || section->type == SectionType_Dynamic
            || section->type == SectionType_TaskCom);
}

bool isDataSection(Section *section) {
    return section != NULL
        && (section->type == SectionType_Mixed
            || section->type == SectionType_Data
            || (section->type == SectionType_Common && *section->id != '\0'));
}

bool isDefined(Value *val) {
    return (val->attributes & SYM_UNDEFINED) == 0;
}

bool isExternal(Value *val) {
    return (val->attributes & SYM_EXTERNAL) != 0;
}

bool isImmobile(Value *val) {
    return (val->attributes & SYM_IMMOBILE) != 0;
}

bool isNamedCommonSection(Section *section) {
    return section != NULL && section->type == SectionType_Common && *section->id != '\0';
}

bool isParcelAddress(Value *value) {
    return (value->attributes & SYM_PARCEL_ADDRESS) != 0;
}

bool isPlainValue(Value *value) {
    return (value->attributes & (SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS)) == 0;
}

bool isRelative(Value *val) {
    return (val->attributes & (SYM_RELOCATABLE|SYM_IMMOBILE)) != 0;
}

bool isRelocatable(Value *val) {
    return (val->attributes & SYM_RELOCATABLE) != 0;
}

bool isWordAddress(Value *value) {
    return (value->attributes & SYM_WORD_ADDRESS) != 0;
}

void resetModule(Module *module) {
    Section *section;

    for (section = module->firstSection; section != NULL; section = section->next) {
        resetSection(section);
    }
}

static void resetSection(Section *section) {
    section->originCounter = section->originOffset;
    section->locationCounter = section->originOffset;
    section->wordBitPosCounter = 0;
    section->parcelBitPosCounter = 0;
}
