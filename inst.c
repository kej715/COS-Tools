/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: instructions.c
**
**  Description:
**      This file provides functions for handling all machine instructions
**      and pseudo-instructions.
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

#define INT_22_LOWER (-010000000)
#define INT_22_UPPER   007777777
#define MAX_INST_ARGS  4

typedef ErrorCode (*InstructionHandler)(void);

typedef struct InstPatternDefn {
    char *pattern;
    InstructionHandler handler;
} InstPatternDefn;

typedef enum patternNodeType {
    NodeType_FieldDelimiter = 0,
    NodeType_SubfieldDelimiter,
    NodeType_PatternEnd,
    NodeType_Register,
    NodeType_Operator,
    NodeType_Expression
} PatternNodeType;

typedef struct patternNode {
    PatternNodeType type;
    struct patternNode *next;    // link to next node in this sequence
    struct patternNode *sibling; // link to next adjacent sequence
    union {
        RegisterType regster;
        OperatorType operator;
        InstructionHandler handler;
    };
} PatternNode;

static void addEntryPoint(Module *module, Symbol *symbol);
static void addExternal(Module *module, Symbol *symbol);
static void addInstruction(char *id, u8 attributes, InstructionHandler handler);
static void addMacroCallParam(MacroCall *call, MacroParam *param, char *value, int valueLen);
static MacroLine *addMacroLine(MacroDefn *defn);
static void addMacroLineFragment(MacroLine *line, MacroFragType type, char *text, int len);
static void addMacroParam(MacroDefn *defn, MacroParamType type, char *name, int nameLen, char *value, int valueLen);
static void addPattern(char *s, InstructionHandler handler);
static NamedInstruction *allocInstruction(char *id, u8 attributes, InstructionHandler handler);
static int compareStrings(char *s1, int s1Len, char *s2, int s2Len);
static ErrorCode defineSymbol(u16 attributes);
static MacroParam *findMacroParam(MacroDefn *defn, char *name, int len);
static SectionLocation findSectionLocation(char *name, int len);
static SectionType findSectionType(char *name, int len);
static void forceInstWordBoundary(void);
static void freeInstruction(NamedInstruction *instruction);
static void freeMacroDefn(MacroDefn *defn);
static Token *generateZero(void);
static char *getDelimitedString(char *s, char **start, int *len);
static char *getNextName(char *s, char **name, int *len);
static char *getParamValue(char *s, char **value, int *valueLen);
static ErrorCode handleBranch(u16 opCode);
static ErrorCode handleOp_i_j_k(u16 opCode);
static ErrorCode handleOp_i_j_n(u16 opCode, u8 n);
static ErrorCode handleOp_i_jk(u16 opCode);
static ErrorCode handleOp_i_n(u16 opCode, u16 n);
static ErrorCode handleOp_i_n_k(u16 opCode, u8 n);
static bool isEquivNode(PatternNode *node1, PatternNode *node2);
static bool isFloatFour(Value *val);
static bool isFloatFourEighths(Value *val);
static bool isFloatOne(Value *val);
static bool isFloatSixEighths(Value *val);
static bool isFloatTwo(Value *val);
static bool isInteger(Value *val);
static bool isIntegerRange(Value *val, int lowerBound, int upperBound);
static bool isNegOne(Value *val);
static bool isOne(Value *val);
static bool isSimpleInteger(Value *val);
static bool isZero(Value *val);
static InstructionHandler matchInstruction(bool *didMatchResultField);
static ErrorCode numericMicro(int base);
static void parseError(char *s);
static char *parseNextNode(char *s, PatternNode *node);
static int popBase(void);
static ErrorCode pushBase(int base);
static void restoreBase(void);
static void setBase(void);
static void skipLines(Token *locationFieldToken, int count);

static int instArgc;
static Token *instArgv[MAX_INST_ARGS];
static PatternNode *instructionPatterns = NULL;
static NamedInstruction *namedInstructions = NULL;

/*
**--------------------------------------------------------------------------
**
**  Functions for handling pseudo-instructions
**
**--------------------------------------------------------------------------
*/

/*
 *  ABS
 */
static ErrorCode ABS(void) {
    currentModule->isAbsolute = TRUE;
    return (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
}

static ErrorCode ALIGN(void) {
    return Err_ResultField;
}

static ErrorCode BASE(void) {
    ErrorCode err;
    char *s;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    s = operandField;
    if (strlen(s) == 1) {
        switch (*s) {
        case 'D':
        case 'd':
            err = pushBase(currentBase);
            currentBase = 10;
            break;
        case 'M':
        case 'm':
            err = pushBase(currentBase);
            currentBase = 0;
            break;
        case 'O':
        case 'o':
            err = pushBase(currentBase);
            currentBase = 8;
            break;
        case '*':
            currentBase = popBase();
            break;
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

static ErrorCode BITP(void) {
    ErrorCode err;
    char *s;
    Value val;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    setBase();
    s = getNextValue(operandField, &val, &err);
    restoreBase();
    if (err != Err_None) (void)registerError(err);
    if (*s != '\0') err = Err_OperandField;
    if (val.intValue == 16) {
        if (currentSection->parcelBitPosCounter > 0) {
            currentSection->originCounter += 1;
            currentSection->locationCounter += 1;
        }
        currentSection->parcelBitPosCounter = 0;
        currentSection->wordBitPosCounter = 0;
    }
    else if (val.intValue >= 0 && val.intValue < 16) {
        currentSection->parcelBitPosCounter = val.intValue;
        currentSection->wordBitPosCounter = ((currentSection->locationCounter & 0x03) * 16) + val.intValue;
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

static ErrorCode BITW(void) {
    ErrorCode err;
    char *s;
    Value val;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    setBase();
    s = getNextValue(operandField, &val, &err);
    restoreBase();
    if (err != Err_None) (void)registerError(err);
    if (*s != '\0') err = Err_OperandField;
    if (val.intValue == 64) {
        currentSection->wordBitPosCounter = 0;
        currentSection->parcelBitPosCounter = 0;
        currentSection->originCounter = (currentSection->originCounter & 0xfffffc) + 4;
        currentSection->locationCounter = (currentSection->locationCounter & 0xfffffc) + 4;
    }
    else if (val.intValue >= 0 && val.intValue < 64) {
        currentSection->wordBitPosCounter = val.intValue;
        currentSection->parcelBitPosCounter = val.intValue % 16;
        currentSection->originCounter = (currentSection->originCounter & 0xfffffc) + (val.intValue / 16);
        currentSection->locationCounter = (currentSection->locationCounter & 0xfffffc) + (val.intValue / 16);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

static ErrorCode BLOCK(void) {
    Section *section;
    ErrorCode err;
    char *id;
    int len;
    char *s;
    Token token;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    if (currentModule->id[0] == '\0') return Err_InstructionPlacement;
    if (strcmp(operandField, "*") == 0) {
        if (sectionStackPtr > 0) {
            currentSection = sectionStack[--sectionStackPtr];
        }
        return Err_None;
    }
    s = getNextToken(operandField, &token);
    if (*s != '\0') return Err_OperandField;
    if (token.type == TokenType_None) {
        id = "";
        len = 0;
    }
    else if (isUnqualifiedName(&token)) {
        id = token.details.name.ptr;
        len = token.details.name.len;
    }
    else {
        return Err_OperandField;
    }
    for (section = currentModule->firstSection; section != NULL; section = section->next) {
        if (strncmp(section->id, id, len) == 0
            && section->id[len] == '\0'
            && section->type == SectionType_Mixed
            && section->location == SectionLocation_CM) break;
    }
    if (section == NULL) {
        if (pass == 1) {
            section = addSection(currentModule, id, len, SectionType_Mixed, SectionLocation_CM);
        }
        else {
            fprintf(stderr, "Section vanished in pass 2: %.*s\n", token.details.name.len, token.details.name.ptr);
            exit(1);
        }
    }
    if (sectionStackPtr >= BLOCK_STACK_SIZE) return Err_TooManyEntries;
    sectionStack[sectionStackPtr++] = currentSection;
    currentSection = section;
    return err;
}

static ErrorCode BSS(void) {
    ErrorCode err;
    u32 firstAddress;
    u32 limitAddress;
    char *s;
    Value val;

    err = Err_None;
    forceInstWordBoundary();
    listCodeLocation(currentSection);
    if (locationFieldToken != NULL) {
        err = registerError(addLocationSymbol(currentSection, locationFieldToken->details.name.ptr,
                                              locationFieldToken->details.name.len, SYM_WORD_ADDRESS));
    }
    s = getNextValue(operandField, &val, &err);
    if (err != Err_None) return err;
    if (*s != '\0'
        || isInteger(&val) == FALSE
        || isIntegerRange(&val, 0, 0x3fffff) == FALSE
        || isAbsolute(&val) == FALSE
        || isParcelAddress(&val))
        return Err_OperandField;
    listValue(&val);
    firstAddress = currentSection->originCounter;
    advanceBitPosition(currentSection, val.intValue * 64);
    limitAddress = currentSection->originCounter;
    if (isCodeSection(currentSection) || isDataSection(currentSection))
        reserveStorage(currentSection, firstAddress, limitAddress - firstAddress);
    return err;
}

static ErrorCode BSSZ(void) {
    ErrorCode err;
    char *s;
    u16 savedListControl;
    Value val;

    if (isDataSection(currentSection) == FALSE) return Err_InstructionPlacement;
    err = Err_None;
    forceInstWordBoundary();
    listCodeLocation(currentSection);
    if (locationFieldToken != NULL) {
        err = registerError(addLocationSymbol(currentSection, locationFieldToken->details.name.ptr,
                                              locationFieldToken->details.name.len, SYM_WORD_ADDRESS));
    }
    s = getNextValue(operandField, &val, &err);
    if (err != Err_None) return err;
    if (*s != '\0'
        || isInteger(&val) == FALSE
        || isIntegerRange(&val, 0, 0x3fffff) == FALSE
        || isAbsolute(&val) == FALSE
        || isParcelAddress(&val))
        return Err_OperandField;
    listValue(&val);
    savedListControl = currentListControl;
    currentListControl = 0;
    while (val.intValue-- > 0) {
        val.intValue = 0;
        val.attributes = 0;
        val.section = currentSection;
        emitFieldStart(currentSection);
        emitFieldBits(currentSection, &val, 64, FALSE);
        emitFieldEnd(currentSection);
    }
    currentListControl = savedListControl;
    return Err_None;
}

/*
 *  COMMENT 'character string'
 */
static ErrorCode COMMENT(void) {
    ErrorCode err;
    char *s;
    Token token;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    s = getNextToken(operandField, &token);
    if (token.type == TokenType_String) {
        if (currentModule->comment != NULL) free(currentModule->comment);
        currentModule->comment = (char *)allocate(token.details.string.len + 1);
        memcpy(currentModule->comment, token.details.string.ptr, token.details.string.len);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

static ErrorCode COMMON(void) {
    Section *section;
    ErrorCode err;
    char *id;
    int len;
    char *s;
    Token token;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    if (currentModule->id[0] == '\0') return Err_InstructionPlacement;
    if (strcmp(operandField, "*") == 0) {
        if (sectionStackPtr > 0) {
            currentSection = sectionStack[--sectionStackPtr];
        }
        return Err_None;
    }
    s = getNextToken(operandField, &token);
    if (*s != '\0') return Err_OperandField;
    if (token.type == TokenType_None) {
        id = "";
        len = 0;
    }
    else if (isUnqualifiedName(&token)) {
        id = token.details.name.ptr;
        len = token.details.name.len;
    }
    else {
        return Err_OperandField;
    }
    for (section = currentModule->firstSection; section != NULL; section = section->next) {
        if (strncmp(section->id, id, len) == 0 && section->id[len] == '\0') break;
    }
    if (section == NULL) {
        if (pass == 1) {
            section = addSection(currentModule, id, len, SectionType_Common, SectionLocation_CM);
        }
        else {
            fprintf(stderr, "Section vanished in pass 2: %.*s\n", token.details.name.len, token.details.name.ptr);
            exit(1);
        }
    }
    else if (section->type != SectionType_Common || section->location != SectionLocation_CM) {
        return Err_DoubleDefinition;
    }
    if (sectionStackPtr >= BLOCK_STACK_SIZE) return Err_TooManyEntries;
    sectionStack[sectionStackPtr++] = currentSection;
    currentSection = section;
    return err;
}


static ErrorCode CON(void) {
    ErrorCode err;
    char *s;
    Value val;

    if (*operandField == '\0') return Err_OperandField;
    if (isDataSection(currentSection) == FALSE) return Err_InstructionPlacement;
    forceWordBoundary(currentSection);
    if (locationFieldToken != NULL) {
        err = registerError(addLocationSymbol(currentSection, locationFieldToken->details.name.ptr,
                                              locationFieldToken->details.name.len, SYM_WORD_ADDRESS));
    }
    listCodeLocation(currentSection);
    s = operandField;
    while (*s != '\0') {
        if (*s == ',') {
            val.type = NumberType_Integer;
            val.attributes = 0;
            val.section = NULL;
            val.intValue = 0;
        }
        else {
            s = getNextValue(s, &val, &err);
            if (err != Err_None) (void)registerError(err);
        }
        emitFieldStart(currentSection);
        emitFieldBits(currentSection, &val, 64, FALSE);
        emitFieldEnd(currentSection);
        if (*s == ',') {
            listFlush(currentSection);
            listCodeLocation(currentSection);
            s += 1;
        }
    }
    return err;
}

static ErrorCode DATA(void) {
    ErrorCode err;
    Token *expression;
    char *s;
    Value val;

    if (*operandField == '\0') return Err_OperandField;
    if (isDataSection(currentSection) == FALSE) return Err_InstructionPlacement;
    if (locationFieldToken != NULL) {
        forceWordBoundary(currentSection);
        err = registerError(addLocationSymbol(currentSection, locationFieldToken->details.name.ptr,
                                              locationFieldToken->details.name.len, SYM_WORD_ADDRESS));
    }
    listCodeLocation(currentSection);
    s = operandField;
    while (*s != '\0') {
        err = Err_None;
        s = parseExpression(s, &expression);
        switch (expression->type) {
        case TokenType_None:
            err = Err_OperandField;
            break;
        case TokenType_Error:
            err = expression->details.error.code;
            break;
        case TokenType_String:
            emitString(currentSection, expression->details.string.ptr, expression->details.string.len,
                expression->details.string.count, expression->details.string.justification);
            break;
        default:
            err = evaluateExpression(expression, &val);
            emitFieldStart(currentSection);
            emitFieldBits(currentSection, &val, 64, FALSE);
            emitFieldEnd(currentSection);
            break;
        }
        freeToken(expression);
        if (*s == ',') {
            s += 1;
            if (currentSection->wordBitPosCounter == 0) {
                listFlush(currentSection);
                listCodeLocation(currentSection);
            }
        }
        else if (*s != '\0') {
            err = Err_OperandField;
        }
        if (err != Err_None) break;
    }
    return err;
}

static ErrorCode DECMIC(void) {
    return numericMicro(10);
}

static ErrorCode DUP(void) {
    return Err_ResultField;
}

static ErrorCode ECHO(void) {
    return Err_ResultField;
}

static ErrorCode EJECT(void) {
    ErrorCode err;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    listControlMask = LIST_LIS;
    listEject();
    return err;
}

static ErrorCode ELSE(void) {
    if (locationFieldToken == NULL) return Err_LocationField;
    if (*operandField != '\0') return Err_OperandField;
    skipLines(locationFieldToken, 0);
    return Err_None;
}

/*
 *  END
 */
static ErrorCode END(void) {
    ErrorCode err;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    if (*operandField != '\0') err = registerError(Err_OperandField);
    if (currentModule->id[0] == '\0') err = Err_InstructionPlacement;
    return err;
}

static ErrorCode ENDDUP(void) {
    return Err_ResultField;
}

static ErrorCode ENDIF(void) {
    if (locationFieldToken == NULL) return Err_LocationField;
    if (*operandField != '\0') return Err_OperandField;
    return Err_None;
}

static ErrorCode ENDM(void) {
    return Err_ResultField;
}

static ErrorCode ENDTEXT(void) {
    return Err_ResultField;
}

static ErrorCode ENTRY(void) {
    ErrorCode err;
    char *s;
    Qualifier *qualifier;
    Symbol *symbol;
    Token token;
    Value val;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    s = operandField;
    qualifier = findQualifier("");
    while (*s != '\0') {
        s = getNextToken(s, &token);
        if (isUnqualifiedName(&token)) {
            symbol = findSymbol(token.details.name.ptr, token.details.name.len, qualifier);
            if (symbol == NULL) {
                val.type = NumberType_Integer;
                val.attributes = SYM_UNDEFINED;
                val.section = NULL;
                val.intValue = 0;
                symbol = addSymbol(token.details.name.ptr, token.details.name.len, qualifier, &val);
            }
            else if ((symbol->value.attributes & (SYM_EXTERNAL|SYM_REDEFINABLE)) != 0) {
                symbol = NULL;
                err = registerError(Err_OperandField);
            }
            if (pass == 1 && symbol != NULL) {
                symbol->value.attributes |= SYM_ENTRY;
                addEntryPoint(currentModule, symbol);
            }
            else if (pass == 2 && symbol != NULL && (symbol->value.attributes & SYM_UNDEFINED) != 0) {
                err = registerError(Err_Undefined);
            }
        }
        else if (token.type != TokenType_None) {
            err = registerError(Err_OperandField);
            break;
        }
        if (*s == ',') s += 1;
    }
    return err;
}

static ErrorCode EQU(void) {
    return defineSymbol(0);
}

static ErrorCode ERRIF(void) {
    return Err_ResultField;
}

static ErrorCode ERROR(void) {
    ErrorCode code;

    if (locationFieldToken == NULL) return Err_Programmer;
    if (locationFieldToken->type != TokenType_Name) return Err_LocationField;
    code = getErrorCode(locationFieldToken->details.name.ptr, locationFieldToken->details.name.len);
    return (code != 0) ? code : Err_LocationField;
}

static ErrorCode EXT(void) {
    ErrorCode err;
    int n;
    char *s;
    Qualifier *qualifier;
    Symbol *symbol;
    Token token;
    Value val;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    if (currentModule->isAbsolute) return Warn_ExternalDeclaration;
    s = operandField;
    qualifier = findQualifier("");
    n = 0;
    while (*s != '\0') {
        s = getNextToken(s, &token);
        if (isUnqualifiedName(&token)) {
            symbol = findSymbol(token.details.name.ptr, token.details.name.len, qualifier);
            if (symbol == NULL) {
                val.type = NumberType_Integer;
                val.attributes = SYM_EXTERNAL;
                val.section = NULL;
                val.intValue = 0;
                symbol = addSymbol(token.details.name.ptr, token.details.name.len, qualifier, &val);
                n += 1;
                if (pass == 1 && symbol != NULL) {
                    addExternal(currentModule, symbol);
                }
            }
            else if ((symbol->value.attributes & SYM_EXTERNAL) == 0) {
                err = registerError(Err_DoubleDefinition);
            }
            else {
                if (pass == 2) symbol->value.attributes |= SYM_DEFINED_P2;
                n += 1;
            }
        }
        else {
            err = registerError(Err_OperandField);
            break;
        }
        if (*s == ',') s += 1;
    }
    if (n < 1) {
        err = Err_OperandField;
    }
    return err;
}

static ErrorCode FORMAT(void) {
    ErrorCode err;
    char *s;
    Token token;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    if (strcmp(operandField, "*") == 0) {
        if (sourceFormatStackPtr > 0) {
            currentSourceFormat = sourceFormatStack[--sourceFormatStackPtr];
        }
        return Err_None;
    }
    s = getNextToken(operandField, &token);
    if (*s != '\0') return Err_OperandField;
    if (sourceFormatStackPtr >= SOURCE_FORMAT_STACK_SIZE) return Err_TooManyEntries;
    sourceFormatStack[sourceFormatStackPtr++] = currentSourceFormat;
    if (token.type == TokenType_None) {
        currentSourceFormat = defaultSourceFormat;
    }
    else if (isUnqualifiedName(&token) && token.details.name.len == 3) {
        if (strncasecmp(token.details.name.ptr, "NEW", 3) == 0) {
            currentSourceFormat = SourceFormat_New;
        }
        else if (strncasecmp(token.details.name.ptr, "OLD", 3) == 0) {
            currentSourceFormat = SourceFormat_Old;
        }
        else {
            sourceFormatStackPtr -= 1;
            err = Err_OperandField;
        }
    }
    else {
        sourceFormatStackPtr -= 1;
        err = Err_OperandField;
    }
    return err;
}

static ErrorCode IDENT(void) {
    ErrorCode err;
    Module *module;
    char *s;
    Token token;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    if (currentModule->id[0] != '\0') err = registerError(Err_InstructionPlacement);
    s = getNextToken(operandField, &token);
    if (token.type != TokenType_Name || *s != '\0') return Err_OperandField;
    if (pass == 1) {
        if (findModule(token.details.name.ptr, token.details.name.len) != NULL) return Err_DoubleDefinition;
        currentModule = addModule(token.details.name.ptr, token.details.name.len);
    }
    else { // pass == 2
        module = findModule(token.details.name.ptr, token.details.name.len);
        if (module == NULL) {
            fprintf(stderr, "Module vanished in pass 2: %.*s\n", token.details.name.len, token.details.name.ptr);
            exit(1);
        }
        resetModule(module);
        currentModule = module;
    }
    currentQualifier = findQualifier("");
    currentSection = currentModule->firstSection;
    sectionStackPtr = 0;
    macroStackPtr = 0;
    qualifierStackPtr = 0;
    resetBase();
    listEject();

    return err;
}

static bool hasAttrVAL(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && isPlainValue(&val);
}
static bool hasAttrPA(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && isParcelAddress(&val);
}
static bool hasAttrWA(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && isWordAddress(&val);
}
static bool hasAttrABS(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && isAbsolute(&val);
}
static bool hasAttrIMM(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && isImmobile(&val);
}
static bool hasAttrREL(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && isRelocatable(&val);
}
static bool hasAttrEXT(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && isExternal(&val);
}
static bool hasAttrCODE(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && isCodeSection(val.section);
}
static bool hasAttrDATA(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && isDataSection(val.section);
}
static bool hasAttrMIXED(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && (isCodeSection(val.section) || isDataSection(val.section));
}
static bool hasAttrCOMMON(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && isCommonSection(val.section);
}
static bool hasAttrTASKCOM(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && val.section != NULL && val.section->type == SectionType_TaskCom;
}
static bool hasAttrDYNAMIC(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && val.section != NULL && val.section->type == SectionType_Dynamic;
}
static bool hasAttrSTACK(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && val.section != NULL && val.section->type == SectionType_Stack;
}
static bool hasAttrCM(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && val.section != NULL && val.section->location == SectionLocation_CM;
}
static bool hasAttrEM(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && val.section != NULL && val.section->location == SectionLocation_EM;
}
static bool hasAttrLM(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return isDefined(&val) && val.section != NULL && val.section->location == SectionLocation_LM;
}
static bool hasAttrDEF(Token *expression, ErrorCode *err) {
    Value val;

    *err = evaluateExpression(expression, &val);
    return (pass == 1) ? isDefined(&val) : (val.attributes & SYM_DEFINED_P2) != 0;
}
static bool hasAttrSET(Token *expression, ErrorCode *err) {
    Symbol *symbol;

    symbol = findQualifiedSymbol(expression);
    return symbol != NULL && (symbol->value.attributes & SYM_REDEFINABLE) != 0;
}
static bool hasAttrREG(Token *expression, ErrorCode *err) {
    return expression->type == TokenType_Register;
}
static bool hasAttrMIC(Token *expression, ErrorCode *err) {
    return isUnqualifiedName(expression)
        && findName(currentModule->micros, expression->details.name.ptr, expression->details.name.len) != NULL;
}

typedef struct attrEvalDefn {
    char *keyword;
    int len;
    bool (*evaluator)(Token *expression, ErrorCode *err);
} AttrEvalDefn;

static AttrEvalDefn attrEvalDefns[] = {
    {"VAL",     3, hasAttrVAL},
    {"PA",      2, hasAttrPA},
    {"WA",      2, hasAttrWA},
    {"ABS",     3, hasAttrABS},
    {"IMM",     3, hasAttrIMM},
    {"REL",     3, hasAttrREL},
    {"EXT",     3, hasAttrEXT},
    {"CODE",    4, hasAttrCODE},
    {"DATA",    4, hasAttrDATA},
    {"MIXED",   5, hasAttrMIXED},
    {"COM",     3, hasAttrCOMMON},
    {"COMMON",  6, hasAttrCOMMON},
    {"TASKCOM", 7, hasAttrTASKCOM},
    {"DYNAMIC", 7, hasAttrDYNAMIC},
    {"STACK",   5, hasAttrSTACK},
    {"CM",      2, hasAttrCM},
    {"EM",      2, hasAttrEM},
    {"LM",      2, hasAttrLM},
    {"DEF",     3, hasAttrDEF},
    {"SET",     3, hasAttrSET},
    {"REG",     3, hasAttrREG},
    {"MIC",     3, hasAttrMIC},
    {NULL,      0, NULL}
};


static ErrorCode IFA(void) {
    bool cond;
    Value count;
    AttrEvalDefn *defn;
    ErrorCode err;
    Token *exp;
    int len;
    char *op;
    Token opToken;
    char *s;
    bool targetCond;
    Token token;

    if (*operandField == '#') {
        targetCond = FALSE;
        s = operandField + 1;
    }
    else {
        targetCond = TRUE;
        s = operandField;
    }
    s = getNextToken(s, &opToken);
    if (isUnqualifiedName(&opToken) == FALSE || *s != ',') return Err_OperandField;
    op = opToken.details.name.ptr;
    len = opToken.details.name.len;
    if (len == 3 && strncasecmp(op, "REG", 3) == 0) {
        s = getNextToken(s + 1, &token);
        exp = copyToken(&token);
    }
    else {
        s = parseExpression(s + 1, &exp);
    }
    if (*s == ',') {
        setBase();
        s = getNextValue(s + 1, &count, &err);
        restoreBase();
        if (err != Err_None) return err;
        if (isSimpleInteger(&count) == FALSE || count.intValue < 0) {
            freeToken(exp);
            return Err_OperandField;
        }
    }
    else if (locationFieldToken == NULL) {
        freeToken(exp);
        return Err_OperandField;
    }
    err = Err_None;
    for (defn = attrEvalDefns; defn->keyword != NULL; defn++) {
        if (defn->len == len && strncasecmp(defn->keyword, op, len) == 0) break;
    }
    if (defn->keyword != NULL) {
        cond = (*defn->evaluator)(exp, &err);
    }
    else {
        err = Err_OperandField;
    }
    freeToken(exp);
    if (err != Err_None && err != Err_Undefined) return err;
    if (cond != targetCond) skipLines(locationFieldToken, count.intValue);

    return Err_None;
}

static ErrorCode IFC(void) {
    bool cond;
    Value count;
    ErrorCode err;
    char *op;
    Token opToken;
    char *s;
    char *s1;
    int s1Len;
    char *s2;
    int s2Len;
    Value val;
    int valence;

    s = operandField;
    if (*s == '\0') return Err_OperandField;
    if (*s == ',') {
        s1 = "";
        s1Len = 0;
    }
    else {
        s = getDelimitedString(s, &s1, &s1Len);
        if (s1 == NULL) return Err_OperandField;
    }
    if (*s == ',') s += 1;
    s = getNextToken(s, &opToken);
    if (isUnqualifiedName(&opToken) == FALSE || opToken.details.name.len != 2) return Err_OperandField;
    if (*s != ',') return Err_OperandField;
    s += 1;
    if (*s == ',' || *s == '\0') {
        s2 = "";
        s2Len = 0;
    }
    else {
        s = getDelimitedString(s, &s2, &s2Len);
        if (s2 == NULL) return Err_OperandField;
    }
    if (*s == ',') {
        setBase();
        s = getNextValue(s + 1, &count, &err);
        restoreBase();
        if (err != Err_None) return err;
        if (isSimpleInteger(&count) == FALSE || count.intValue < 0) return Err_OperandField;
    }
    else if (locationFieldToken == NULL) {
        return Err_OperandField;
    }
    valence = compareStrings(s1, s1Len, s2, s2Len);
    op = opToken.details.name.ptr;
    if (strncasecmp(op, "LT", 2) == 0)
        cond = valence < 0;
    else if (strncasecmp(op, "LE", 2) == 0)
        cond = valence <= 0;
    else if (strncasecmp(op, "GT", 2) == 0)
        cond = valence > 0;
    else if (strncasecmp(op, "GE", 2) == 0)
        cond = valence >= 0;
    else if (strncasecmp(op, "EQ", 2) == 0)
        cond = valence == 0;
    else if (strncasecmp(op, "NE", 2) == 0)
        cond = valence != 0;
    else
        return Err_OperandField;
    if (cond == FALSE) skipLines(locationFieldToken, count.intValue);

    return Err_None;
}

static ErrorCode IFE(void) {
    bool cond;
    Value count;
    ErrorCode err;
    char *op;
    Token opToken;
    char *s;
    Value val1;
    Value val2;

    s = getNextValue(operandField, &val1, &err);
    if (err != Err_None) return err;
    if (*s != ',') return Err_OperandField;
    s = getNextToken(s + 1, &opToken);
    if (isUnqualifiedName(&opToken) == FALSE || opToken.details.name.len != 2) return Err_OperandField;
    if (*s != ',') return Err_OperandField;
    s = getNextValue(s + 1, &val2, &err);
    if (err != Err_None) return err;
    if (*s == ',') {
        setBase();
        s = getNextValue(s + 1, &count, &err);
        restoreBase();
        if (err != Err_None) return err;
        if (isSimpleInteger(&count) == FALSE || count.intValue < 0) return Err_OperandField;
    }
    else if (locationFieldToken == NULL) {
        return Err_OperandField;
    }
    op = opToken.details.name.ptr;
    if (strncasecmp(op, "LT", 2) == 0)
        cond = val1.intValue < val2.intValue;
    else if (strncasecmp(op, "LE", 2) == 0)
        cond = val1.intValue <= val2.intValue;
    else if (strncasecmp(op, "GT", 2) == 0)
        cond = val1.intValue > val2.intValue;
    else if (strncasecmp(op, "GE", 2) == 0)
        cond = val1.intValue >= val2.intValue;
    else if (strncasecmp(op, "EQ", 2) == 0)
        cond = val1.intValue == val2.intValue;
    else if (strncasecmp(op, "NE", 2) == 0)
        cond = val1.intValue != val2.intValue;
    else
        return Err_OperandField;
    if (cond == FALSE) skipLines(locationFieldToken, count.intValue);

    return Err_None;
}

typedef struct listControlDefn {
    char *keyword;
    u16 flag;
} ListControlDefn;

static ListControlDefn listControlDefns[] = {
    {"ON",   LIST_ON},
    {"OFF",  0},
    {"XRF",  LIST_XRF},
    {"NXRF", 0},
    {"XNS",  LIST_XNS},
    {"NXNS", 0},
    {"DUP",  LIST_DUP},
    {"NDUP", 0},
    {"MAC",  LIST_MAC},
    {"NMAC", 0},
    {"MIF",  LIST_MIF},
    {"NMIF", 0},
    {"MIC",  LIST_MIC},
    {"NMIC", 0},
    {"LIS",  LIST_LIS},
    {"NLIS", 0},
    {"WEM",  LIST_WEM},
    {"NWEM", 0},
    {"TXT",  LIST_TXT},
    {"NTXT", 0},
    {"WRP",  LIST_WRP},
    {"NWRP", 0},
    {"WMR",  LIST_WMR},
    {"NWMR", 0},
    {NULL,   0}
};

static ErrorCode LIST(void) {
    ListControlDefn *defn;
    int len;
    u16 listControl;
    char *name;
    char *s;
    Token token;

    s = operandField;
    if (strcmp(s, "*") == 0) {
        if (listControlStackPtr > 0) currentListControl = listControlStack[--listControlStackPtr];
        return Err_None;
    }
    listControl = 0;
    while (*s != '\0') {
        s = getNextToken(s, &token);
        if (isUnqualifiedName(&token) == FALSE) return Err_OperandField;
        name = token.details.name.ptr;
        len = token.details.name.len;
        defn = &listControlDefns[0];
        while (defn->keyword != NULL) {
            if (strncasecmp(name, defn->keyword, len) == 0 && defn->keyword[len] == '\0') {
                listControl |= defn->flag;
                break;
            }
            defn += 1;
        }
        if (defn->keyword == NULL) return Err_OperandField;
        if (*s == ',') s += 1;
    }
    if (listControlStackPtr >= LIST_CONTROL_STACK_SIZE) return Err_TooManyEntries;
    listControlStack[listControlStackPtr++] = currentListControl;
    currentListControl = listControl;
    listControlMask = LIST_LIS;

    return Err_None;
}

/*
 *  LOC value
 */
static ErrorCode LOC(void) {
    ErrorCode err;
    char *s;
    Value val;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    if (isCodeSection(currentSection) == FALSE && isDataSection(currentSection) == FALSE)
        return Err_InstructionPlacement;
    forceInstWordBoundary();
    s = getNextValue(operandField, &val, &err);
    if (err != Err_None) return err;
    if (*s != '\0'
        || isParcelAddress(&val)
        || isInteger(&val) == FALSE
        || val.intValue < 0
        || (val.attributes & (SYM_EXTERNAL|SYM_UNDEFINED)) != 0
        || isAbsolute(&val) != currentModule->isAbsolute
        || (val.section != NULL && val.section != currentSection))
        return Err_OperandField;
    currentSection->locationCounter = val.intValue * 4;
    return err;
}

static ErrorCode LOCAL(void) {
    return Err_ResultField;
}

static ErrorCode MACRO(void) {
    int col;
    MacroDefn *defn;
    ErrorCode err;
    char *id;
    char *keyword;
    int keywordLen;
    int len;
    MacroLine *line;
    char *locationParam;
    int locationParamLen;
    char *macroName;
    int macroNameLen;
    Name *name;
    MacroParam *pp;
    char *s;
    char *start;
    char *value;
    int valueLen;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    if (*operandField != '\0') err = registerError(Err_OperandField);
    listErrorIndications();
    listFlush(currentSection);
    //
    //  Read and process the prototype statement. Discard comments and empty lines
    //  until the prototype statement is found, then parse the location field
    //  parameter, if one is present. Next, parse the macro name, then positional
    //  parameter names, then keyword parameters.
    //
    while (TRUE) {
        if (isEof()) return Err_InstructionPlacement;
        readNextLine();
        listSource();
        if (sourceLine[0] != '*' || sourceLine[0] != '\0') break;
        listFlush(currentSection);
    }
    s = sourceLine;
    while (*s == ' ') s += 1;
    col = s - sourceLine;
    //
    //  If the first non-blank character occurs in the first two columns, then
    //  a location field parameter is present.
    //
    if (col < 2) {
        s = getNextName(s, &locationParam, &locationParamLen);
        if (locationParamLen == 0) return Err_LocationField;
        while (*s == ' ') s += 1;
    }
    else {
        locationParam = NULL;
    }
    //
    //  Parse the macro name and register a new definition for it.
    //
    s = getNextName(s, &macroName, &macroNameLen);
    if (macroNameLen == 0) return Err_ResultField;
    name = findName(currentModule->macros, macroName, macroNameLen);
    if (name == NULL) {
        name = addName(&currentModule->macros, macroName, macroNameLen);
    }
    else {
        defn = (MacroDefn *)name->value;
        if (defn->creationPass == 2 && (currentListControl & (LIST_WEM|LIST_WMR)) == (LIST_WEM|LIST_WMR))
            err = registerError(Warn_RedefinedMacro);
        freeMacroDefn(defn);
    }
    defn = (MacroDefn *)allocate(sizeof(MacroDefn));
    defn->creationPass = pass;
    name->value = defn;
    if (locationParam != NULL) {
        pp = (MacroParam *)allocate(sizeof(MacroParam));
        pp->name = (char *)allocate(locationParamLen + 1);
        memcpy(pp->name, locationParam, locationParamLen);
        defn->locationParam = pp;
    }
    //
    //  Parse positional parameter names
    //
    while (*s == ' ') s += 1;
    while (*s != '\0') {
        s = getNextName(s, &start, &len);
        if (len == 0) return Err_OperandField;
        if (*s == '=') { // start of keyword parameters
            s = start;
            break;
        }
        else if (*s == ',' || *s == '\0') {
            addMacroParam(defn, MacroParamType_Positional, start, len, NULL, 0);
            if (*s != '\0') s += 1;
        }
        else {
            return Err_OperandField;
        }
    }
    //
    //  Parse keyword parameter names and their default values
    //
    while (*s != '\0') {
        s = getNextName(s, &keyword, &keywordLen);
        if (keywordLen == 0 || *s != '=') return Err_OperandField;
        s += 1;
        s = getParamValue(s, &value, &valueLen);
        if (value == NULL) return Err_OperandField;
        addMacroParam(defn, MacroParamType_Keyword, keyword, keywordLen, value, valueLen);
        if (*s != '\0') s += 1;
    }
    //
    //  Parse macro body
    //
    listErrorIndications();
    listFlush(currentSection);
    while (TRUE) {
        if (isEof()) return Err_InstructionPlacement;
        readNextLine();
        listSource();
        if (sourceLine[0] == '*' || sourceLine[0] == '\0') {
            listErrorIndications();
            listFlush(currentSection);
            continue;
        }
        s = sourceLine;
        while (*s == ' ') s += 1;
        //
        //  If the first non-blank character occurs in the first two columns, then
        //  a location field is present, and it might match the macro name.
        //
        locationParamLen = 0;
        if (s - sourceLine < 2) {
            s = getNextName(s, &locationParam, &locationParamLen);
            while (*s == ' ') s += 1;
        }
        s = getNextName(s, &id, &len);
        if (locationParamLen == macroNameLen
            && strncmp(locationParam, name->id, locationParamLen) == 0
            && len == 4
            && strncasecmp(id, "ENDM", 4) == 0) {
            return Err_None; // end of macro definition
        }
        //
        //  End of macro definition not detected, so parse the line to
        //  detect parameter references, and generate fragments accordingly.
        //
        line = addMacroLine(defn);
        s = start = sourceLine;
        while (*s != '\0') {
            while (isNameChar1(*s) == FALSE && *s != '\0') s += 1;
            if (isNameChar1(*s)) {
                id = s++;
                while (isNameChar(*s)) s += 1;
                pp = findMacroParam(defn, id, s - id);
                if (pp != NULL) {
                    addMacroLineFragment(line, MacroFragType_Text, start, id - start);
                    addMacroLineFragment(line, MacroFragType_ParamRef, id, s - id);
                    start = s;
                }
            }
        }
        if (s > start)
            addMacroLineFragment(line, MacroFragType_Text, start, s - start);
        listFlush(currentSection);
    }

    return Err_None;
}

static ErrorCode MICRO(void) {
    char *defn;
    char delim;
    char *dp;
    ErrorCode err;
    int exp1;
    int exp2;
    int len;
    char *limit;
    Name *name;
    char *s;
    char *start;
    Value val;

    if (locationFieldToken == NULL || locationFieldToken->type != TokenType_Name || locationField[0] == '*')
        return Err_LocationField;

    err = Err_None;
    name = findName(currentModule->micros, locationFieldToken->details.name.ptr, locationFieldToken->details.name.len);
    if (name == NULL) {
        name = addName(&currentModule->micros, locationFieldToken->details.name.ptr, locationFieldToken->details.name.len);
    }
    s = operandField;
    if (*s == '\0') {
        if (name->value != NULL) free(name->value);
        defn = (char *)allocate(1);
        *defn = '\0';
        name->value = defn;
        return err;
    }
    delim = *s;
    s = getDelimitedString(s, &start, &len);
    if (start == NULL) return Err_OperandField;
    limit = start + len;
    //
    //  Get length (exp1) and index (exp2) of first character
    //
    exp1 = 1000;
    exp2 = 0;
    if (*s == ',') {
        s = getNextValue(s + 1, &val, &err);
        if (err != Err_None) (void)registerError(err);
        if (isSimpleInteger(&val)) {
            exp1 = (val.intValue <= 0) ? 0 : val.intValue;
        }
        else {
            err = registerError(Err_OperandField);
        }
        if (*s == ',') {
            s = getNextValue(s + 1, &val, &err);
            if (err != Err_None) (void)registerError(err);
            if (isSimpleInteger(&val)) {
                exp2 = (val.intValue <= 1) ? 0 : val.intValue - 1;
            }
            else {
                err = registerError(Err_OperandField);
            }
        }
    }
    if (*s != '\0') return Err_OperandField;
    //
    //  Register definition
    //
    defn = dp = (char *)allocate(len + 1);
    s = start;
    while (s < limit && exp2-- > 0) {
        if (*s == delim) s += 1;
        s += 1;
    }
    while (s < limit && exp1-- > 0) {
        if (*s == delim) s += 1;
        *dp++ = *s++;
    }
    *dp = '\0';
    if (name->value != NULL) free(name->value);
    name->value = defn;

    return err;
}

static ErrorCode MICSIZE(void) {
    ErrorCode err;
    char *s;
    Name *name;
    Symbol *symbol;
    Token token;
    Value val;

    if (locationFieldToken == NULL) return Err_None;

    if (locationFieldToken->type != TokenType_Name || locationField[0] == '*') return Err_LocationField;

    err = Err_None;
    s = getNextToken(operandField, &token);
    if (err != Err_None) return err;
    if (isUnqualifiedName(&token) == FALSE || *s != '\0') return Err_OperandField;
    name = findName(currentModule->micros, token.details.name.ptr, token.details.name.len);
    if (name == NULL) return Err_Undefined;
    symbol = findSymbol(locationFieldToken->details.name.ptr, locationFieldToken->details.name.len, currentQualifier);
    val.type = NumberType_Integer;
    val.attributes = SYM_REDEFINABLE;
    val.section = NULL;
    val.intValue = strlen((char *)name->value);
    if (symbol == NULL) {
        symbol = addSymbol(locationFieldToken->details.name.ptr, locationFieldToken->details.name.len, currentQualifier, &val);
    }
    else if ((symbol->value.attributes & SYM_REDEFINABLE) != 0) {
        symbol->value.attributes = val.attributes;
        symbol->value.section = val.section;
        symbol->value.intValue = val.intValue;
    }
    else if (symbol->value.attributes != val.attributes || symbol->value.intValue != val.intValue) {
        err = Err_DoubleDefinition;
    }
    if (pass == 2) symbol->value.attributes |= SYM_DEFINED_P2;

    if (err == Err_None || err >= Warn_Programmer) listValue(&val);

    return err;
}

static ErrorCode MODULE(void) {
    return Err_ResultField;
}

static ErrorCode OCTMIC(void) {
    return numericMicro(8);
}

static ErrorCode OPDEF(void) {
    return Err_ResultField;
}

static ErrorCode OPSYN(void) {
    return Err_ResultField;
}

/*
 *  ORG value
 */
static ErrorCode ORG(void) {
    ErrorCode err;
    bool isNominalSection;
    i64 originValue;
    char *s;
    Value val;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    if (isCodeSection(currentSection) == FALSE && isDataSection(currentSection) == FALSE)
        return Err_InstructionPlacement;
    s = operandField;
    if (*s == '\0') {
        val.type = NumberType_Integer;
        val.attributes = getRelativeAttribute(currentSection);
        val.section = currentSection;
        val.intValue = 0;
    }
    else {
        forceInstWordBoundary();
        s = getNextValue(operandField, &val, &err);
        if (err != Err_None) return err;
    }
    if (*s != '\0'
        || isParcelAddress(&val)
        || isInteger(&val) == FALSE
        || val.intValue < 0
        || (val.attributes & (SYM_EXTERNAL|SYM_UNDEFINED)) != 0
        || isAbsolute(&val) != currentModule->isAbsolute
        || (val.section != NULL && val.section != currentSection))
        return Err_OperandField;
    originValue = val.intValue * 4;
    isNominalSection = currentSection == currentModule->firstSection;
    if (isRelocatable(&val) == FALSE && (isNominalSection == FALSE || currentModule->isAbsolute == FALSE)) {
        return Err_OperandField;
    }
    else if (isRelocatable(&val) && isNominalSection && currentModule->isAbsolute) {
        return Err_OperandField;
    }
    else if (originValue < currentSection->originCounter) {
        return Err_OperandField;
    }
    currentSection->originCounter = currentSection->locationCounter = originValue;
    return err;
}

static ErrorCode QUAL(void) {
    ErrorCode err;
    char *s;
    Token token;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    if (currentModule->id[0] == '\0') return Err_InstructionPlacement;
    if (strcmp(operandField, "*") == 0) {
        if (qualifierStackPtr > 0) {
            currentQualifier = qualifierStack[--qualifierStackPtr];
        }
        return Err_None;
    }
    s = getNextToken(operandField, &token);
    if (*s != '\0') return Err_OperandField;
    if (token.type == TokenType_None) {
        token.type = TokenType_Name;
        token.details.name.ptr = "";
        token.details.name.len = 0;
        token.details.name.qualPtr = NULL;
    }
    if (isUnqualifiedName(&token)) {
        if (qualifierStackPtr >= QUALIFIER_STACK_SIZE) return Err_TooManyEntries;
        qualifierStack[qualifierStackPtr++] = currentQualifier;
        currentQualifier = findQualifierWithLen(token.details.name.ptr, token.details.name.len);
        if (currentQualifier == NULL) currentQualifier = addQualifier(token.details.name.ptr, token.details.name.len);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

static ErrorCode REP(void) {
    return Err_ResultField;
}

static ErrorCode SECTION(void) {
    ErrorCode err;
    int i;
    char *id;
    bool isCommon;
    int len;
    SectionLocation location;
    SectionLocation locations[2];
    char *s;
    Section *section;
    Symbol *symbol;
    Token token;
    SectionType type;
    SectionType types[2];
    Value val;

    if (currentModule->id[0] == '\0') return Err_InstructionPlacement;
    if (strcmp(operandField, "*") == 0) {
        if (sectionStackPtr > 0) {
            currentSection = sectionStack[--sectionStackPtr];
        }
        return Err_None;
    }
    if (locationFieldToken != NULL) {
        id = locationFieldToken->details.name.ptr;
        len = locationFieldToken->details.name.len;
    }
    else {
        id = "";
        len = 0;
    }
    s = operandField;
    for (i = 0; i < 2; i++) {
        types[i] = SectionType_None;
        locations[i] = SectionLocation_None;
        s = getNextToken(s, &token);
        if (isUnqualifiedName(&token)) {
            types[i] = findSectionType(token.details.name.ptr, token.details.name.len);
            if (types[i] == SectionType_None) {
                locations[i] = findSectionLocation(token.details.name.ptr, token.details.name.len);
                if (locations[i] == SectionLocation_None) return Err_OperandField;
            }
        }
        else if (token.type == TokenType_None) {
            types[i] = SectionType_None;
            locations[i] = SectionLocation_None;
        }
        else {
            return Err_OperandField;
        }
        if (*s == ',' && i == 0) s += 1;
    }
    if (*s != '\0') return Err_OperandField;
    if (types[0] != SectionType_None) {
        if (types[1] == SectionType_None)
            type = types[0];
        else
            return Err_OperandField;
    }
    else if (types[1] != SectionType_None) {
        type = types[1];
    }
    else {
        type = SectionType_Mixed;
    }
    if (locations[0] != SectionLocation_None) {
        if (locations[1] == SectionLocation_None)
            location = locations[0];
        else
            return Err_OperandField;
    }
    else if (locations[1] != SectionLocation_None) {
        location = locations[1];
    }
    else {
        location = SectionLocation_CM;
    }
    isCommon = type == SectionType_Common
            || type == SectionType_Dynamic
            || type == SectionType_TaskCom;
    for (section = currentModule->firstSection; section != NULL; section = section->next) {
        if (strncmp(section->id, id, len) == 0
            && section->id[len] == '\0'
            && ((section->type == type && section->location == location)
                || isCommon || isCommonSection(section)))
            break;
    }
    if (section == NULL) {
        if (pass == 1) {
            section = addSection(currentModule, id, len, type, location);
        }
        else {
            fprintf(stderr, "Section vanished in pass 2: %.*s\n", token.details.name.len, token.details.name.ptr);
            exit(1);
        }
    }
    else if ((isCommon || isCommonSection(section)) && (type != section->type || location != section->location)) {
        return Err_DoubleDefinition;
    }
    if (type == SectionType_TaskCom) {
        if (len < 1) return Err_LocationField;
        symbol = findSymbol(id, len, currentQualifier);
        if (symbol == NULL) {
            val.type = NumberType_Integer;
            val.intValue = 0;
            val.attributes = SYM_WORD_ADDRESS|SYM_RELOCATABLE;
            val.section = section;
            symbol = addSymbol(id, len, currentQualifier, &val);
        }
        else if (pass == 1) {
            if ((symbol->value.attributes & SYM_UNDEFINED) != 0) {
                symbol->value.type = NumberType_Integer;
                symbol->value.attributes = SYM_WORD_ADDRESS|SYM_RELOCATABLE;
                symbol->value.section = section;
                symbol->value.intValue = 0;
            }
            else {
                return Err_DoubleDefinition;
            }
        }
        else {
            symbol->value.attributes |= SYM_DEFINED_P2;
            if (symbol->value.intValue != 0
                || symbol->value.section != section
                || symbol->value.attributes != (SYM_WORD_ADDRESS|SYM_RELOCATABLE)) {
                return Err_DoubleDefinition;
            }
        }
    }
    if (sectionStackPtr >= BLOCK_STACK_SIZE) return Err_TooManyEntries;
    sectionStack[sectionStackPtr++] = currentSection;
    currentSection = section;
    return Err_None;
}

static ErrorCode SET(void) {
    return defineSymbol(SYM_REDEFINABLE);
}

static ErrorCode SKIP(void) {
    Value count;
    ErrorCode err;
    char *s;

    if (*operandField != '\0') {
        setBase();
        s = getNextValue(operandField, &count, &err);
        restoreBase();
        if (err != Err_None) return err;
        if (isSimpleInteger(&count) == FALSE || count.intValue < 0) return Err_OperandField;
    }
    else if (locationFieldToken == NULL) {
        return Err_OperandField;
    }
    skipLines(locationFieldToken, count.intValue);

    return Err_None;
}

static ErrorCode SPACE(void) {
    ErrorCode err;
    char *s;
    Value val;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    setBase();
    s = getNextValue(operandField, &val, &err);
    restoreBase();
    if (err != Err_None) return err;
    if (isSimpleInteger(&val) && *s == '\0') {
        listControlMask = LIST_LIS;
        if (val.intValue > 0) {
            listFlush(currentSection);
            resetErrorRegistrations();
            listControlMask = LIST_ON;
            listClearSource();
            while (val.intValue-- > 0) listFlush(currentSection);
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

static ErrorCode STACK(void) {
    ErrorCode err;
    char *s;
    Value val;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    if (*operandField == '\0') return err;
    s = getNextValue(operandField, &val, &err);
    if (err != Err_None) return err;
    if (val.type == NumberType_Integer
        && (val.attributes & (SYM_EXTERNAL|SYM_RELOCATABLE|SYM_IMMOBILE|SYM_LITERAL|SYM_UNDEFINED|SYM_PARCEL_ADDRESS)) == 0
        && val.intValue >= 0
        && *s == '\0') {
        currentModule->stackSize += val.intValue;
        listValue(&val);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

static ErrorCode START(void) {
    ErrorCode err;
    char *s;
    Symbol *symbol;
    Token token;

    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    s = getNextToken(operandField, &token);
    if (isUnqualifiedName(&token) && *s == '\0') {
        if (pass == 2) {
            symbol = findSymbol(token.details.name.ptr, token.details.name.len, findQualifier(""));
            if (symbol == NULL) {
                err = Err_Undefined;
            }
            else if (symbol->value.type == NumberType_Integer
                     && (symbol->value.attributes & (SYM_WORD_ADDRESS|SYM_PARCEL_ADDRESS)) != 0
                     && (symbol->value.attributes & (SYM_EXTERNAL|SYM_UNDEFINED)) == 0) {
                if (currentModule->start == NULL) {
                    currentModule->start = symbol;
                    addEntryPoint(currentModule, symbol);
                 }
                 else {
                     err = Err_OperandField;
                 }
            }
            else {
                err = Err_OperandField;
            }
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

static ErrorCode STOPDUP(void) {
    return Err_ResultField;
}

static ErrorCode SUBTITLE(void) {
    ErrorCode err;
    char *s;
    Token token;

    listControlMask = LIST_LIS;
    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    s = getNextToken(operandField, &token);
    if (token.type == TokenType_String && token.details.string.len <= MAX_TITLE_LENGTH) {
        memcpy(subtitle, token.details.string.ptr, token.details.string.len);
        subtitle[token.details.string.len] = '\0';
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

static ErrorCode TEXT(void) {
    return Err_ResultField;
}

static ErrorCode TITLE(void) {
    ErrorCode err;
    char *s;
    Token token;

    listControlMask = LIST_LIS;
    err = (locationFieldToken == NULL) ? Err_None : registerError(Warn_IgnoredLocationSymbol);
    s = getNextToken(operandField, &token);
    if (token.type == TokenType_String && token.details.string.len <= MAX_TITLE_LENGTH) {
        memcpy(title, token.details.string.ptr, token.details.string.len);
        title[token.details.string.len] = '\0';
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

static ErrorCode VWD(void) {
    ErrorCode err;
    int fieldWidth;
    char *s;
    Token token;
    Value val;

    if (*operandField == '\0') return Err_OperandField;
    if (isDataSection(currentSection) == FALSE) return Err_InstructionPlacement;
    if (locationFieldToken != NULL) {
        forceWordBoundary(currentSection);
        err = registerError(addLocationSymbol(currentSection, locationFieldToken->details.name.ptr,
                                              locationFieldToken->details.name.len, SYM_WORD_ADDRESS));
    }
    emitFieldStart(currentSection);
    listCodeLocation(currentSection);
    s = operandField;
    while (*s != '\0') {
        err = Err_None;
        setBase();
        s = getNextToken(s, &token);
        switch (token.type) {
        case TokenType_Name:
        case TokenType_Number:
            err = evaluateExpression(&token, &val);
            if (err == Err_None) {
                if (isSimpleInteger(&val) && isIntegerRange(&val, 0, 64)) {
                    fieldWidth = val.intValue;
                }
                else {
                    err = Err_OperandField;
                }
            }
            break;
        case TokenType_Error:
            err = token.details.error.code;
            break;
        default:
            err = Err_OperandField;
            break;
        } 
        restoreBase();
        if (*s == '/') {
            s += 1;
        }
        else {
            err = Err_OperandField;
        }
        if (err != Err_None) break;
        s = getNextValue(s, &val, &err);
        if (err != Err_None) (void)registerError(err);
        emitFieldBits(currentSection, &val, fieldWidth, FALSE);
        if (*s == ',') {
            s += 1;
            if (currentSection->wordBitPosCounter == 0) listFlush(currentSection);
        }
        else if (*s != '\0') {
            err = Err_OperandField;
        }
        if (err != Err_None) break;
    }
    emitFieldEnd(currentSection);
    return err;
}

/*
**--------------------------------------------------------------------------
**
**  Functions for handling machine instructions
**
**--------------------------------------------------------------------------
*/

/*
**  Ai Ak
*/
static ErrorCode Ai__Ak(void) {
    return handleOp_i_n_k(030, 0);
}

/*
**  Ai -Ak
*/
static ErrorCode Ai__negAk(void) {
    return handleOp_i_n_k(031, 0);
}

/*
**  Ai Aj+Ak
*/
static ErrorCode Ai__Aj_add_Ak(void) {
    return handleOp_i_j_k(030);
}

/*
**  Ai Aj+1
*/
static ErrorCode Ai__Aj_add_1(void) {
    ErrorCode err;
    int i;
    int j;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j));
    err = registerError(evaluateExpression(instArgv[2], &val));
    if (isOne(&val)) {
        emit_gh_i_j_k(currentSection, 030, i, j, 0);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Ai Aj*Ak
*/
static ErrorCode Ai__Aj_mul_Ak(void) {
    return handleOp_i_j_k(032);
}

/*
**  Ai Aj-Ak
*/
static ErrorCode Ai__Aj_sub_Ak(void) {
    return handleOp_i_j_k(031);
}

/*
**  Ai Aj-1
*/
static ErrorCode Ai__Aj_sub_1(void) {
    ErrorCode err;
    int i;
    int j;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j));
    err = registerError(evaluateExpression(instArgv[2], &val));
    if (isOne(&val)) {
        emit_gh_i_j_k(currentSection, 031, i, j, 0);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Ai exp
**  Ai -1
*/
static ErrorCode Ai__X(void) {
    ErrorCode err;
    int i;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val));
    if (isIntegerRange(&val, INT_22_LOWER, INT_22_UPPER)) {
        if (isNegOne(&val)) { //  Ai -1
            emit_gh_i_jk(currentSection, 031, i, 0);
        }
        else if (isSimpleInteger(&val) == FALSE) {
            emit_gh_i_jkm(currentSection, 020, i, &val);
        }
        else if (val.intValue >= 0 && val.intValue < 64) {
            emit_gh_i_jk(currentSection, 022, i, val.intValue);
        }
        else if (val.intValue >= 0) {
            emit_gh_i_jkm(currentSection, 020, i, &val);
        }
        else {
            val.intValue ^= MASK22;
            emit_gh_i_jkm(currentSection, 021, i, &val);
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Ai exp,Ah
*/
static ErrorCode Ai__X_Ah(void) {
    ErrorCode err;
    int h;
    int i;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val));
    err = registerError(getRegisterNumber(instArgv[2], &h));
    if (isIntegerRange(&val, INT_22_LOWER, INT_22_UPPER) && isParcelAddress(&val) == FALSE) {
        emit_g_h_i_jkm(currentSection, 010, h, i, &val);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Ai exp,0
**  Ai exp,
*/
static ErrorCode Ai__X_X(void) {
    ErrorCode err;
    int i;
    Value val1;
    Value val2;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val1));
    err = registerError(evaluateExpression(instArgv[2], &val2));
    if (isZero(&val2) == FALSE) err = registerError(Err_OperandField);
    if (isIntegerRange(&val1, INT_22_LOWER, INT_22_UPPER) && isParcelAddress(&val1) == FALSE) {
        emit_gh_i_jkm(currentSection, 0100, i, &val1);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Ai Bjk
*/
static ErrorCode Ai__Bjk(void) {
    return handleOp_i_jk(024);
}

/*
**  Ai CA,Aj
*/
static ErrorCode Ai__CA_Aj(void) {
    ErrorCode err;
    int i;
    int j;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[2], &j));
    emit_gh_i_j_k(currentSection, 033, i, j, 0);
    return err;
}

/*
**  Ai CE,Aj
*/
static ErrorCode Ai__CE_Aj(void) {
    ErrorCode err;
    int i;
    int j;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[2], &j));
    emit_gh_i_j_k(currentSection, 033, i, j, 1);
    return err;
}

/*
**  Ai CI
*/
static ErrorCode Ai__CI(void) {
    return handleOp_i_n(033, 0);
}

/*
**  Ai PSj
*/
static ErrorCode Ai__PSj(void) {
    return handleOp_i_j_n(026, 0);
}

/*
**  Ai QSj
*/
static ErrorCode Ai__QSj(void) {
    return handleOp_i_j_n(026, 1);
}

/*
**  Ai Sj
*/
static ErrorCode Ai__Sj(void) {
    return handleOp_i_j_n(023, 0);
}

/*
**  Ai SBj
*/
static ErrorCode Ai__SBj(void) {
    return handleOp_i_j_n(026, 7);
}

/*
**  Ai VL
*/
static ErrorCode Ai__VL(void) {
    return handleOp_i_n(023, 1);
}

/*
**  Ai ZSj
*/
static ErrorCode Ai__ZSj(void) {
    return handleOp_i_j_n(027, 0);
}

/*
**  Bjk Ai
*/
static ErrorCode Bjk__Ai(void) {
    ErrorCode err;
    int i;
    int jk;

    err = registerError(getRegisterNumber(instArgv[0], &jk));
    err = registerError(getRegisterNumber(instArgv[1], &i));
    emit_gh_i_jk(currentSection, 025, i, jk);
    return err;
}

/*
**  Bjk,Ai exp,A0
*/
static ErrorCode Bjk_Ai__X_A0(void) {
    ErrorCode err;
    int i;
    int jk;
    int z;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &jk));
    err = registerError(getRegisterNumber(instArgv[1], &i));
    err = registerError(evaluateExpression(instArgv[2], &val));
    err = registerError(getRegisterNumber(instArgv[3], &z));
    if (z == 0 && isZero(&val)) {
        emit_gh_i_jk(currentSection, 034, i, jk);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  CA,Aj Ak
*/
static ErrorCode CA_Aj__Ak(void) {
    ErrorCode err;
    int j;
    int k;

    err = registerError(getRegisterNumber(instArgv[1], &j));
    err = getRegisterNumber(instArgv[2], &k);
    emit_gh_i_j_k(currentSection, 001, 0, j, k);
    return err;
}

/*
**  CI,Aj
*/
static ErrorCode CI_Aj(void) {
    ErrorCode err;
    int j;

    err = registerError(getRegisterNumber(instArgv[1], &j));
    emit_gh_i_j_k(currentSection, 001, 2, j, 0);
    return err;
}

/*
**  CL,Aj Ak
*/
static ErrorCode CL_Aj__Ak(void) {
    ErrorCode err;
    int j;
    int k;

    err = registerError(getRegisterNumber(instArgv[1], &j));
    err = getRegisterNumber(instArgv[2], &k);
    emit_gh_i_j_k(currentSection, 001, 1, j, k);
    return err;
}

/*
**  CCI
*/
static ErrorCode CCI(void) {
    emit_gh_i_j_k(currentSection, 001, 4, 0, 5);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  CIPI
*/
static ErrorCode CIPI(void) {
    emit_gh_ijk(currentSection, 001, 0402);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  CLN exp
*/
static ErrorCode CLN(void) {
    ErrorCode err;
    char *s;
    Value val;

    s = getNextValue(operandField, &val, &err);
    if (err != Err_None) (void)registerError(err);
    if (*s != '\0') err = Err_OperandField;
    if (isSimpleInteger(&val) && isIntegerRange(&val, 0, 5)) {
        emit_gh_i_j_k(currentSection, 001, 4, val.intValue, 3);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  CMR
*/
static ErrorCode CMR(void) {
    emit_gh_i_jk(currentSection, 002, 7, 0);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  DBM
*/
static ErrorCode DBM(void) {
    emit_gh_i_jk(currentSection, 002, 5, 0);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  DCI
*/
static ErrorCode DCI(void) {
    emit_gh_i_j_k(currentSection, 001, 4, 0, 7);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  DFI
*/
static ErrorCode DFI(void) {
    emit_gh_i_jk(currentSection, 002, 2, 0);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  DRI
*/
static ErrorCode DRI(void) {
    emit_gh_i_jk(currentSection, 002, 4, 0);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  EBM
*/
static ErrorCode EBM(void) {
    emit_gh_i_jk(currentSection, 002, 6, 0);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  ECI
*/
static ErrorCode ECI(void) {
    emit_gh_i_j_k(currentSection, 001, 4, 0, 6);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  EFI
*/
static ErrorCode EFI(void) {
    emit_gh_i_jk(currentSection, 002, 1, 0);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  ERI
*/
static ErrorCode ERI(void) {
    emit_gh_i_jk(currentSection, 002, 3, 0);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  ERR
*/
static ErrorCode ERR(void) {
    emit_gh_ijk(currentSection, 000, 0);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  EX
*/
static ErrorCode EX(void) {
    emit_gh_ijk(currentSection, 004, 0);
    return (*operandField == '\0') ? Err_None : Err_OperandField;
}

/*
**  IP 0
**  IP 1
*/
static ErrorCode IP(void) {
    ErrorCode err;
    char *s;
    Value val;

    s = getNextValue(operandField, &val, &err);
    if (*s != '\0') err = Err_OperandField;
    if (err != Err_None) (void)registerError(err);
    if (isSimpleInteger(&val) && isIntegerRange(&val, 0, 1)) {
        emit_gh_ijk(currentSection, 001, val.intValue == 0 ? 0402 : 0401);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  J Bjk
**  J exp
*/
static ErrorCode J(void) {
    ErrorCode err;
    Token *expr;
    int regNum;
    char *s;
    Token token;
    Value val;

    err = Err_None;
    s = parseExpression(operandField, &expr);
    if (expr->type == TokenType_Register && expr->details.regster.type == RegisterType_B) {
        err = (*s == '\0') ? getRegisterNumber(expr, &regNum) : Err_OperandField;
        emit_gh_i_jk(currentSection, 005, 0, (err == Err_None) ? regNum : 0);
    }
    else {
        err = handleBranch(006);
    }
    freeToken(expr);
    return err;
}

/*
**  JAM exp
*/
static ErrorCode JAM(void) {
    return handleBranch(013);
}

/*
**  JAN exp
*/
static ErrorCode JAN(void) {
    return handleBranch(011);
}

/*
**  JAP exp
*/
static ErrorCode JAP(void) {
    return handleBranch(012);
}

/*
**  JAZ exp
*/
static ErrorCode JAZ(void) {
    return handleBranch(010);
}

/*
**  JSM exp
*/
static ErrorCode JSM(void) {
    return handleBranch(017);
}

/*
**  JSN exp
*/
static ErrorCode JSN(void) {
    return handleBranch(015);
}

/*
**  JSP exp
*/
static ErrorCode JSP(void) {
    return handleBranch(016);
}

/*
**  JSZ exp
*/
static ErrorCode JSZ(void) {
    return handleBranch(014);
}

/*
**  MC,Aj
*/
static ErrorCode MC_Aj(void) {
    ErrorCode err;
    int j;

    err = registerError(getRegisterNumber(instArgv[1], &j));
    emit_gh_i_j_k(currentSection, 001, 2, j, 1);
    return err;
}

/*
**  PASS
*/
static ErrorCode PASS(void) {
    ErrorCode err;

    err = (*operandField == '\0') ? Err_None : Err_OperandField;
    emit_gh_ijk(currentSection, 001, 0);
    return err;
}

/*
**  PCI Sj
*/
static ErrorCode PCI(void) {
    ErrorCode err;
    int j;
    char *s;
    Token token;

    s = getNextToken(operandField, &token);
    if (token.type != TokenType_Register || token.details.regster.type != RegisterType_S || *s != '\0') return Err_OperandField;
    err = registerError(getRegisterNumber(&token, &j));
    emit_gh_i_j_k(currentSection, 001, 4, j, 4);
    return err;
}

/*
**  R exp
*/
static ErrorCode R(void) {
    return handleBranch(007);
}

/*
**  RT Sj
*/
static ErrorCode RT__Sj(void) {
    ErrorCode err;
    int j;

    err = registerError(getRegisterNumber(instArgv[1], &j));
    emit_gh_i_j_k(currentSection, 001, 4, j, 0);
    return err;
}

/*
**  Si Ak
*/
static ErrorCode Si__Ak(void) {
    return handleOp_i_n_k(071, 0);
}

/*
**  Si +Ak
*/
static ErrorCode Si__ExtendAk(void) {
    return handleOp_i_n_k(071, 1);
}

/*
**  Si +FAk
*/
static ErrorCode Si__FAk(void) {
    return handleOp_i_n_k(071, 2);
}

/*
**  Si Sk
*/
static ErrorCode Si__Sk(void) {
    return handleOp_i_n_k(051, 0);
}

/*
**  Si #Sk
*/
static ErrorCode Si__CmplSk(void) {
    return handleOp_i_n_k(047, 0);
}

/*
**  Si -Sk
*/
static ErrorCode Si__NegSk(void) {
    return handleOp_i_n_k(061, 0);
}

/*
**  Si -FSk
*/
static ErrorCode Si__NegFSk(void) {
    return handleOp_i_n_k(063, 0);
}

/*
**  Si +FSk
*/
static ErrorCode Si__NormFSk(void) {
    return handleOp_i_n_k(062, 0);
}

/*
**  Si Sj+Sk
*/
static ErrorCode Si__Sj_add_Sk(void) {
    return handleOp_i_j_k(060);
}

/*
**  Si Sj+FSk
*/
static ErrorCode Si__Sj_add_FSk(void) {
    return handleOp_i_j_k(062);
}

/*
**  Si Sj-Sk
*/
static ErrorCode Si__Sj_sub_Sk(void) {
    return handleOp_i_j_k(061);
}

/*
**  Si Sj-FSk
*/
static ErrorCode Si__Sj_sub_FSk(void) {
    return handleOp_i_j_k(063);
}

/*
**  Si Sj*FSk
*/
static ErrorCode Si__Sj_mul_FSk(void) {
    return handleOp_i_j_k(064);
}

/*
**  Si Sj*HSk
*/
static ErrorCode Si__Sj_mul_HSk(void) {
    return handleOp_i_j_k(065);
}

/*
**  Si Sj*ISk
*/
static ErrorCode Si__Sj_mul_ISk(void) {
    return handleOp_i_j_k(067);
}

/*
**  Si Sj*RSk
*/
static ErrorCode Si__Sj_mul_RSk(void) {
    return handleOp_i_j_k(066);
}

/*
**  Si /HSj
*/
static ErrorCode Si__RecipSj(void) {
    return handleOp_i_j_n(070, 0);
}

/*
**  Si Sj&Sk
*/
static ErrorCode Si__Sj_and_Sk(void) {
    return handleOp_i_j_k(044);
}

/*
**  Si Sj&SB
*/
static ErrorCode Si__Sj_and_SB(void) {
    return handleOp_i_j_n(044, 0);
}

/*
**  Si SB&Sj
*/
static ErrorCode Si__SB_and_Sj(void) {
    ErrorCode err;
    int i;
    int j;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[2], &j));
    emit_gh_i_j_k(currentSection, 044, i, j, 0);
    return err;
}

/*
**  Si #Sk&Sj
*/
static ErrorCode Si__CmplSk_and_Sj(void) {
    ErrorCode err;
    int i;
    int j;
    int k;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &k));
    err = registerError(getRegisterNumber(instArgv[2], &j));
    emit_gh_i_j_k(currentSection, 045, i, j, k);
    return err;
}

/*
**  Si #SB&Sj
*/
static ErrorCode Si__CmplSB_and_Sj(void) {
    ErrorCode err;
    int i;
    int j;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[2], &j));
    emit_gh_i_j_k(currentSection, 045, i, j, 0);
    return err;
}

/*
**  Si Sj!Sk
*/
static ErrorCode Si__Sj_or_Sk(void) {
    return handleOp_i_j_k(051);
}

/*
**  Si Sj!SB
*/
static ErrorCode Si__Sj_or_SB(void) {
    return handleOp_i_j_n(051, 0);
}

/*
**  Si SB!Sj
*/
static ErrorCode Si__SB_or_Sj(void) {
    ErrorCode err;
    int i;
    int j;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[2], &j));
    emit_gh_i_j_k(currentSection, 051, i, j, 0);
    return err;
}

/*
**  Si Sj!Si&Sk
*/
static ErrorCode Si__Si_merge_Sj(void) {
    ErrorCode err;
    int i;
    int i2;
    int j;
    int k;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j));
    err = registerError(getRegisterNumber(instArgv[2], &i2));
    err = registerError(getRegisterNumber(instArgv[3], &k));
    if (i != i2) err = Err_OperandField;
    emit_gh_i_j_k(currentSection, 050, i, j, k);
    return err;
}

/*
**  Si Sj!Si&SB
*/
static ErrorCode Si__Si_merge_SB(void) {
    ErrorCode err;
    int i;
    int i2;
    int j;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j));
    err = registerError(getRegisterNumber(instArgv[2], &i2));
    if (i != i2) err = Err_OperandField;
    emit_gh_i_j_k(currentSection, 050, i, j, 0);
    return err;
}

/*
**  Si Sj\Sk
*/
static ErrorCode Si__Sj_xor_Sk(void) {
    return handleOp_i_j_k(046);
}

/*
**  Si Sj\SB
*/
static ErrorCode Si__Sj_xor_SB(void) {
    return handleOp_i_j_n(046, 0);
}

/*
**  Si SB\Sj
*/
static ErrorCode Si__SB_xor_Sj(void) {
    ErrorCode err;
    int i;
    int j;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[2], &j));
    emit_gh_i_j_k(currentSection, 046, i, j, 0);
    return err;
}

/*
**  Si Si<Ak
*/
static ErrorCode Si__Si_left_Ak(void) {
    ErrorCode err;
    int i;
    int i2;
    int k;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &i2));
    err = registerError(getRegisterNumber(instArgv[2], &k));
    if (i == i2) {
        emit_gh_i_j_k(currentSection, 056, i, 0, k);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Si Si,Sj<Ak
*/
static ErrorCode Si__SiSj_left_Ak(void) {
    ErrorCode err;
    int i;
    int i2;
    int j;
    int k;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &i2));
    err = registerError(getRegisterNumber(instArgv[2], &j));
    err = registerError(getRegisterNumber(instArgv[3], &k));
    if (i == i2) {
        emit_gh_i_j_k(currentSection, 056, i, j, k);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Si Si,Sj<1
*/
static ErrorCode Si__SiSj_left_X(void) {
    ErrorCode err;
    int i;
    int i2;
    int j;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &i2));
    err = registerError(getRegisterNumber(instArgv[2], &j));
    err = registerError(evaluateExpression(instArgv[3], &val));
    if (i == i2 && isOne(&val)) {
        emit_gh_i_j_k(currentSection, 056, i, j, 0);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  S0 Si<exp
**  Si Si<exp
*/
static ErrorCode Si__Si_left_X(void) {
    ErrorCode err;
    int i;
    int i2;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &i2));
    err = registerError(evaluateExpression(instArgv[2], &val));
    if (isSimpleInteger(&val) && isIntegerRange(&val, 0, 64)) {
        if (i == 0) {
            if (val.intValue == 64) {
                emit_gh_ijk(currentSection, 053, 0);
            }
            else {
                emit_gh_i_jk(currentSection, 052, i2, val.intValue);
            }
        }
        else if (i == i2) {
            if (val.intValue == 64) {
                emit_gh_i_jk(currentSection, 055, i, 0);
            }
            else {
                emit_gh_i_jk(currentSection, 054, i, val.intValue);
            }
        }
        else {
            err = Err_OperandField;
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Si Si>Ak
*/
static ErrorCode Si__Si_right_Ak(void) {
    ErrorCode err;
    int i;
    int i2;
    int k;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &i2));
    err = registerError(getRegisterNumber(instArgv[2], &k));
    if (i == i2) {
        emit_gh_i_j_k(currentSection, 057, i, 0, k);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Si Sj,Si>Ak
*/
static ErrorCode Si__SjSi_right_Ak(void) {
    ErrorCode err;
    int i;
    int i2;
    int j;
    int k;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j));
    err = registerError(getRegisterNumber(instArgv[2], &i2));
    err = registerError(getRegisterNumber(instArgv[3], &k));
    if (i == i2) {
        emit_gh_i_j_k(currentSection, 057, i, j, k);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Si Sj,Si>1
*/
static ErrorCode Si__SjSi_right_X(void) {
    ErrorCode err;
    int i;
    int i2;
    int j;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j));
    err = registerError(getRegisterNumber(instArgv[2], &i2));
    err = registerError(evaluateExpression(instArgv[3], &val));
    if (i == i2 && isOne(&val)) {
        emit_gh_i_j_k(currentSection, 057, i, j, 0);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  S0 Si>exp
**  Si Si>exp
*/
static ErrorCode Si__Si_right_X(void) {
    ErrorCode err;
    int i;
    int i2;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &i2));
    err = registerError(evaluateExpression(instArgv[2], &val));
    if (isSimpleInteger(&val) && isIntegerRange(&val, 0, 64)) {
        if (i == 0) {
            if (val.intValue == 0) {
                emit_gh_ijk(currentSection, 052, 0);
            }
            else {
                emit_gh_i_jk(currentSection, 053, i2, 64 - val.intValue);
            }
        }
        else if (i == i2) {
            if (val.intValue == 0) {
                emit_gh_i_jk(currentSection, 054, i, 0);
            }
            else {
                emit_gh_i_jk(currentSection, 055, i, 64 - val.intValue);
            }
        }
        else {
            err = Err_OperandField;
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Si #Sj\Sk
*/
static ErrorCode Si__CmplSj_xor_Sk(void) {
    return handleOp_i_j_k(047);
}

/*
**  Si #Sj\SB
*/
static ErrorCode Si__CmplSj_xor_SB(void) {
    return handleOp_i_j_n(047, 0);
}

/*
**  Si #SB\Sj
*/
static ErrorCode Si__CmplSB_xor_Sj(void) {
    ErrorCode err;
    int i;
    int j;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[2], &j));
    emit_gh_i_j_k(currentSection, 047, i, j, 0);
    return err;
}

/*
**  Si Vj,Ak
*/
static ErrorCode Si__Vj_Ak(void) {
    return handleOp_i_j_k(076);
}

/*
**  Si #<exp
*/
static ErrorCode Si__CmplMaskLeft(void) {
    ErrorCode err;
    int i;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val));
    if (isSimpleInteger(&val) && isIntegerRange(&val, 0, 64)) {
        if (val.intValue == 0) {
            emit_gh_i_jk(currentSection, 042, i, 0);
        }
        else {
            emit_gh_i_jk(currentSection, 043, i, 64 - val.intValue);
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Si #>exp
*/
static ErrorCode Si__CmplMaskRight(void) {
    ErrorCode err;
    int i;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val));
    if (isSimpleInteger(&val) && isIntegerRange(&val, 0, 64)) {
        if (val.intValue == 64) {
            emit_gh_i_jk(currentSection, 043, i, 0);
        }
        else {
            emit_gh_i_jk(currentSection, 042, i, val.intValue);
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Si #SB
*/
static ErrorCode Si__CmplSB(void) {
    return handleOp_i_n(047, 0);
}

/*
**  Si >exp
*/
static ErrorCode Si__MaskLeft(void) {
    ErrorCode err;
    int i;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val));
    if (isSimpleInteger(&val) && isIntegerRange(&val, 0, 64)) {
        if (val.intValue == 64) {
            emit_gh_i_jk(currentSection, 042, i, 0);
        }
        else {
            emit_gh_i_jk(currentSection, 043, i, val.intValue);
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Si <exp
*/
static ErrorCode Si__MaskRight(void) {
    ErrorCode err;
    int i;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val));
    if (isSimpleInteger(&val) && isIntegerRange(&val, 0, 64)) {
        if (val.intValue == 0) {
            emit_gh_i_jk(currentSection, 043, i, 0);
        }
        else {
            emit_gh_i_jk(currentSection, 042, i, 64 - val.intValue);
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Si SB
*/
static ErrorCode Si__SB(void) {
    return handleOp_i_n(051, 0);
}

/*
**  Si RT
*/
static ErrorCode Si__RT(void) {
    return handleOp_i_n(072, 0);
}

/*
**  Si SM
*/
static ErrorCode Si__SM(void) {
    return handleOp_i_n(072, 2);
}

/*
**  Si VM
*/
static ErrorCode Si__VM(void) {
    return handleOp_i_n(073, 0);
}

/*
**  Si STj
*/
static ErrorCode Si__STj(void) {
    return handleOp_i_j_n(072, 3);
}

/*
**  Si SRj
*/
static ErrorCode Si__SRj(void) {
    return handleOp_i_j_n(073, 1);
}

/*
**  Si Tjk
*/
static ErrorCode Si__Tjk(void) {
    return handleOp_i_jk(074);
}

/*
**  Si exp
*/
static ErrorCode Si__X(void) {
    ErrorCode err;
    int i;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val));
    if (isIntegerRange(&val, INT_22_LOWER, INT_22_UPPER)) {
        if (isSimpleInteger(&val) == FALSE) {
            emit_gh_i_jkm(currentSection, 040, i, &val);
        }
        else if (isZero(&val)) {
            emit_gh_i_jk(currentSection, 043, i, 0);
        }
        else if (isOne(&val)) {
            emit_gh_i_jk(currentSection, 042, i, 077);
        }
        else if (isNegOne(&val)) {
            emit_gh_i_jk(currentSection, 042, i, 0);
        }
        else if (val.intValue >= 0) {
            emit_gh_i_jkm(currentSection, 040, i, &val);
        }
        else {
            val.intValue ^= MASK22;
            emit_gh_i_jkm(currentSection, 041, i, &val);
        }
    }
    else if (isFloatOne(&val)) {
        emit_gh_i_jk(currentSection, 071, i, 050);
    }
    else if (isFloatTwo(&val)) {
        emit_gh_i_jk(currentSection, 071, i, 060);
    }
    else if (isFloatFour(&val)) {
        emit_gh_i_jk(currentSection, 071, i, 070);
    }
    else if (isFloatFourEighths(&val)) {
        emit_gh_i_jk(currentSection, 071, i, 040);
    }
    else if (isFloatSixEighths(&val)) {
        emit_gh_i_jk(currentSection, 071, i, 030);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Si exp,Ah
*/
static ErrorCode Si__X_Ah(void) {
    ErrorCode err;
    int h;
    int i;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val));
    err = registerError(getRegisterNumber(instArgv[2], &h));
    if (isIntegerRange(&val, INT_22_LOWER, INT_22_UPPER) && isParcelAddress(&val) == FALSE) {
        emit_g_h_i_jkm(currentSection, 012, h, i, &val);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Si exp,0
**  Si exp,
*/
static ErrorCode Si__X_X(void) {
    ErrorCode err;
    int i;
    Value val1;
    Value val2;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val1));
    err = registerError(evaluateExpression(instArgv[2], &val2));
    if (isZero(&val2) == FALSE) err = registerError(Err_OperandField);
    if (isIntegerRange(&val1, INT_22_LOWER, INT_22_UPPER) && isParcelAddress(&val1) == FALSE) {
        emit_gh_i_jkm(currentSection, 0120, i, &val1);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  SBj Ai
*/
static ErrorCode SBj__Ai(void) {
    ErrorCode err;
    int i;
    int j;

    err = registerError(getRegisterNumber(instArgv[0], &j));
    err = registerError(getRegisterNumber(instArgv[1], &i));
    emit_gh_i_j_k(currentSection, 027, i, j, 7);
    return err;
}

/*
**  SIPI exp
*/
static ErrorCode SIPI(void) {
    ErrorCode err;
    char *s;
    Value val;

    if (*operandField == '\0') {
        emit_gh_i_j_k(currentSection, 001, 4, 0, 1);
        return Err_None;
    }
    s = getNextValue(operandField, &val, &err);
    if (err != Err_None) (void)registerError(err);
    if (*s != '\0') err = Err_OperandField;
    if (isSimpleInteger(&val) && isIntegerRange(&val, 0, 3)) {
        emit_gh_i_j_k(currentSection, 001, 4, val.intValue, 1);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  SM Si
*/
static ErrorCode SM__Si(void) {
    ErrorCode err;
    int i;

    err = registerError(getRegisterNumber(instArgv[1], &i));
    emit_gh_i_jk(currentSection, 073, i, 2);
    return err;
}

/*
**  SMjk exp
*/
static ErrorCode SMjk__X(void) {
    ErrorCode err;
    int jk;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &jk));
    err = registerError(evaluateExpression(instArgv[1], &val));
    if (isZero(&val)) {
        emit_gh_i_jk(currentSection, 003, 6, jk);
    }
    else if (isOne(&val)) {
        emit_gh_i_jk(currentSection, 003, 7, jk);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  SMjk exp,TS
*/
static ErrorCode SMjk__X_X(void) {
    Token *arg2;
    ErrorCode err;
    int jk;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &jk));
    err = registerError(evaluateExpression(instArgv[1], &val));
    if (isOne(&val)) {
        arg2 = instArgv[2];
        if (arg2->type == TokenType_Name
            && arg2->details.name.len == 2
            && strncasecmp(arg2->details.name.ptr, "TS", 2) == 0) {
            emit_gh_i_jk(currentSection, 003, 4, jk);
        }
        else {
            err = Err_OperandField;
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  STj Si
*/
static ErrorCode STj__Si(void) {
    ErrorCode err;
    int i;
    int j;

    err = registerError(getRegisterNumber(instArgv[0], &j));
    err = registerError(getRegisterNumber(instArgv[1], &i));
    emit_gh_i_j_k(currentSection, 073, i, j, 3);
    return err;
}

/*
**  Tjk Si
*/
static ErrorCode Tjk__Si(void) {
    ErrorCode err;
    int i;
    int jk;

    err = registerError(getRegisterNumber(instArgv[0], &jk));
    err = registerError(getRegisterNumber(instArgv[1], &i));
    emit_gh_i_jk(currentSection, 075, i, jk);
    return err;
}

/*
**  Tjk,Ai exp,A0
*/
static ErrorCode Tjk_Ai__X_A0(void) {
    ErrorCode err;
    int i;
    int jk;
    int z;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &jk));
    err = registerError(getRegisterNumber(instArgv[1], &i));
    err = registerError(evaluateExpression(instArgv[2], &val));
    err = registerError(getRegisterNumber(instArgv[3], &z));
    if (z == 0 && isZero(&val)) {
        emit_gh_i_jk(currentSection, 036, i, jk);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Vi Vk
*/
static ErrorCode Vi__Vk(void) {
    return handleOp_i_n_k(0142, 0);
}

/*
**  Vi -Vk
*/
static ErrorCode Vi__NegVk(void) {
    return handleOp_i_n_k(0156, 0);
}

/*
**  Vi Sj+Vk
*/
static ErrorCode Vi__Sj_add_Vk(void) {
    return handleOp_i_j_k(0154);
}

/*
**  Vi Vj+Vk
*/
static ErrorCode Vi__Vj_add_Vk(void) {
    return handleOp_i_j_k(0155);
}

/*
**  Vi Sj-Vk
*/
static ErrorCode Vi__Sj_sub_Vk(void) {
    return handleOp_i_j_k(0156);
}

/*
**  Vi Vj-Vk
*/
static ErrorCode Vi__Vj_sub_Vk(void) {
    return handleOp_i_j_k(0157);
}

/*
**  Vi Sj&Vk
*/
static ErrorCode Vi__Sj_and_Vk(void) {
    return handleOp_i_j_k(0140);
}

/*
**  Vi Vj&Vk
*/
static ErrorCode Vi__Vj_and_Vk(void) {
    return handleOp_i_j_k(0141);
}

/*
**  Vi Sj!Vk
*/
static ErrorCode Vi__Sj_or_Vk(void) {
    return handleOp_i_j_k(0142);
}

/*
**  Vi Vj!Vk
*/
static ErrorCode Vi__Vj_or_Vk(void) {
    return handleOp_i_j_k(0143);
}

/*
**  Vi Sj\Vk
*/
static ErrorCode Vi__Sj_xor_Vk(void) {
    return handleOp_i_j_k(0144);
}

/*
**  Vi Vj\Vk
*/
static ErrorCode Vi__Vj_xor_Vk(void) {
    return handleOp_i_j_k(0145);
}

/*
**  Vi Sj!Vk&VM
*/
static ErrorCode Vi__Sj_merge_Vk(void) {
    return handleOp_i_j_k(0146);
}

/*
**  Vi Vj!Vk&VM
*/
static ErrorCode Vi__Vj_merge_Vk(void) {
    return handleOp_i_j_k(0147);
}

/*
**  Vi #VM&Vk
*/
static ErrorCode Vi__0_merge_Vk(void) {
    ErrorCode err;
    int i;
    int k;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[2], &k));
    emit_gh_i_j_k(currentSection, 0146, i, 0, k);
    return err;
}

/*
**  Vi Vj<Ak
*/
static ErrorCode Vi__Vj_left_Ak(void) {
    return handleOp_i_j_k(0150);
}

/*
**  Vi Vj<1
*/
static ErrorCode Vi__Vj_left_1(void) {
    ErrorCode err;
    int i;
    int j;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j));
    err = registerError(evaluateExpression(instArgv[2], &val));
    if (isOne(&val)) {
        emit_gh_i_j_k(currentSection, 0150, i, j, 0);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Vi Vj>Ak
*/
static ErrorCode Vi__Vj_right_Ak(void) {
    return handleOp_i_j_k(0151);
}

/*
**  Vi Vj>1
*/
static ErrorCode Vi__Vj_right_1(void) {
    ErrorCode err;
    int i;
    int j;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j));
    err = registerError(evaluateExpression(instArgv[2], &val));
    if (isOne(&val)) {
        emit_gh_i_j_k(currentSection, 0151, i, j, 0);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Vi Vj,Vj<Ak
*/
static ErrorCode Vi__VjVj_left_Ak(void) {
    ErrorCode err;
    int i;
    int j1;
    int j2;
    int k;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j1));
    err = registerError(getRegisterNumber(instArgv[2], &j2));
    err = registerError(getRegisterNumber(instArgv[3], &k));
    if (j1 == j2) {
        emit_gh_i_j_k(currentSection, 0152, i, j1, k);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Vi Vj,Vj<1
*/
static ErrorCode Vi__VjVj_left_1(void) {
    ErrorCode err;
    int i;
    int j1;
    int j2;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j1));
    err = registerError(getRegisterNumber(instArgv[2], &j2));
    err = registerError(evaluateExpression(instArgv[3], &val));
    if (j1 == j2 && isOne(&val)) {
        emit_gh_i_j_k(currentSection, 0152, i, j1, 0);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Vi Vj,Vj>Ak
*/
static ErrorCode Vi__VjVj_right_Ak(void) {
    ErrorCode err;
    int i;
    int j1;
    int j2;
    int k;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j1));
    err = registerError(getRegisterNumber(instArgv[2], &j2));
    err = registerError(getRegisterNumber(instArgv[3], &k));
    if (j1 == j2) {
        emit_gh_i_j_k(currentSection, 0153, i, j1, k);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Vi Vj,Vj>1
*/
static ErrorCode Vi__VjVj_right_1(void) {
    ErrorCode err;
    int i;
    int j1;
    int j2;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j1));
    err = registerError(getRegisterNumber(instArgv[2], &j2));
    err = registerError(evaluateExpression(instArgv[3], &val));
    if (j1 == j2 && isOne(&val)) {
        emit_gh_i_j_k(currentSection, 0153, i, j1, 0);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Vi +FVk
*/
static ErrorCode Vi__NormFVk(void) {
    return handleOp_i_n_k(0170, 0);
}

/*
**  Vi -FVk
*/
static ErrorCode Vi__NegFVk(void) {
    return handleOp_i_n_k(0172, 0);
}

/*
**  Vi Sj+FVk
*/
static ErrorCode Vi__Sj_add_FVk(void) {
    return handleOp_i_j_k(0170);
}

/*
**  Vi Vj+FVk
*/
static ErrorCode Vi__Vj_add_FVk(void) {
    return handleOp_i_j_k(0171);
}

/*
**  Vi Sj-FVk
*/
static ErrorCode Vi__Sj_sub_FVk(void) {
    return handleOp_i_j_k(0172);
}

/*
**  Vi Vj-FVk
*/
static ErrorCode Vi__Vj_sub_FVk(void) {
    return handleOp_i_j_k(0173);
}

/*
**  Vi Sj*FVk
*/
static ErrorCode Vi__Sj_mul_FVk(void) {
    return handleOp_i_j_k(0160);
}

/*
**  Vi Vj*FVk
*/
static ErrorCode Vi__Vj_mul_FVk(void) {
    return handleOp_i_j_k(0161);
}

/*
**  Vi Sj*HVk
*/
static ErrorCode Vi__Sj_mul_HVk(void) {
    return handleOp_i_j_k(0162);
}

/*
**  Vi Vj*HVk
*/
static ErrorCode Vi__Vj_mul_HVk(void) {
    return handleOp_i_j_k(0163);
}

/*
**  Vi Sj*IVk
*/
static ErrorCode Vi__Sj_mul_IVk(void) {
    return handleOp_i_j_k(0166);
}

/*
**  Vi Vj*IVk
*/
static ErrorCode Vi__Vj_mul_IVk(void) {
    return handleOp_i_j_k(0167);
}

/*
**  Vi Sj*RVk
*/
static ErrorCode Vi__Sj_mul_RVk(void) {
    return handleOp_i_j_k(0164);
}

/*
**  Vi Vj*RVk
*/
static ErrorCode Vi__Vj_mul_RVk(void) {
    return handleOp_i_j_k(0165);
}

/*
**  Vi /HVj
*/
static ErrorCode Vi__RecipHVj(void) {
    return handleOp_i_j_n(0174, 0);
}

/*
**  Vi PVj
*/
static ErrorCode Vi__PVj(void) {
    return handleOp_i_j_n(0174, 1);
}

/*
**  Vi QVj
*/
static ErrorCode Vi__QVj(void) {
    return handleOp_i_j_n(0174, 2);
}

/*
**  Vi 0
*/
static ErrorCode Vi__0(void) {
    ErrorCode err;
    int i;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val));
    if (isZero(&val)) {
        emit_gh_i_j_k(currentSection, 0145, i, i, i);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Vi ,A0,Ak
*/
static ErrorCode Vi__0_A0_Ak(void) {
    ErrorCode err;
    int i;
    int k;
    Value val;
    int z;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val));
    err = registerError(getRegisterNumber(instArgv[2], &z));
    err = registerError(getRegisterNumber(instArgv[3], &k));
    if (z == 0 && isZero(&val)) {
        emit_gh_i_j_k(currentSection, 0176, i, 0, k);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Vi ,A0,Vk
*/
static ErrorCode Vi__0_A0_Vk(void) {
    ErrorCode err;
    int i;
    int k;
    Value val;
    int z;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val));
    err = registerError(getRegisterNumber(instArgv[2], &z));
    err = registerError(getRegisterNumber(instArgv[3], &k));
    if (z == 0 && isZero(&val)) {
        emit_gh_i_j_k(currentSection, 0176, i, 1, k);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Vi ,A0,1
*/
static ErrorCode Vi__0_A0_1(void) {
    ErrorCode err;
    int i;
    Value val1;
    Value val2;
    int z;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(evaluateExpression(instArgv[1], &val1));
    err = registerError(getRegisterNumber(instArgv[2], &z));
    err = registerError(evaluateExpression(instArgv[3], &val2));
    if (z == 0 && isZero(&val1) && isOne(&val2)) {
        emit_gh_i_jk(currentSection, 0176, i, 0);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Vi,Ak Sj
*/
static ErrorCode Vi_Ak__Sj(void) {
    ErrorCode err;
    int i;
    int j;
    int k;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &k));
    err = registerError(getRegisterNumber(instArgv[2], &j));
    emit_gh_i_j_k(currentSection, 077, i, j, k);
    return err;
}

/*
**  Vi,Ak exp
*/
static ErrorCode Vi_Ak__X(void) {
    ErrorCode err;
    int i;
    int k;
    Value val;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &k));
    err = registerError(evaluateExpression(instArgv[2], &val));
    if (isZero(&val)) {
        emit_gh_i_j_k(currentSection, 077, i, 0, k);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  Vi,VM Vj,Z
**  Vi,VM Vj,N
**  Vi,VM Vj,P
**  Vi,VM Vj,M
*/
static ErrorCode Vi_VM__Vj_ID(void) {
    Token *arg3;
    ErrorCode err;
    int i;
    int j;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[2], &j));
    arg3 = instArgv[3];
    if (arg3->type == TokenType_Name && arg3->details.name.len == 1) {
        switch (*arg3->details.name.ptr) {
        case 'Z':
            emit_gh_i_j_k(currentSection, 0175, i, j, 4);
            break;
        case 'N':
            emit_gh_i_j_k(currentSection, 0175, i, j, 5);
            break;
        case 'P':
            emit_gh_i_j_k(currentSection, 0175, i, j, 6);
            break;
        case 'M':
            emit_gh_i_j_k(currentSection, 0175, i, j, 7);
            break;
        default:
            err = Err_OperandField;
            break;
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  VL Ak
*/
static ErrorCode VL__Ak(void) {
    ErrorCode err;
    int k;

    err = registerError(getRegisterNumber(instArgv[1], &k));
    emit_gh_i_j_k(currentSection, 002, 0, 0, k);
    return err;
}

/*
**  VL exp
*/
static ErrorCode VL__X(void) {
    ErrorCode err;
    Value val;

    err = registerError(evaluateExpression(instArgv[1], &val));
    if (isOne(&val)) {
        emit_gh_ijk(currentSection, 002, 0);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  VM Sj
*/
static ErrorCode VM__Sj(void) {
    ErrorCode err;
    int j;

    err = registerError(getRegisterNumber(instArgv[1], &j));
    emit_gh_i_j_k(currentSection, 003, 0, j, 0);
    return err;
}

/*
**  VM exp
*/
static ErrorCode VM__X(void) {
    ErrorCode err;
    Value val;

    err = registerError(evaluateExpression(instArgv[1], &val));
    if (isZero(&val)) {
        emit_gh_ijk(currentSection, 003, 0);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  VM Vj,Z
**  VM Vj,N
**  VM Vj,P
**  VM Vj,M
*/
static ErrorCode VM__Vj_ID(void) {
    Token *arg2;
    ErrorCode err;
    int j;

    err = registerError(getRegisterNumber(instArgv[1], &j));
    arg2 = instArgv[2];
    if (arg2->type == TokenType_Name && arg2->details.name.len == 1) {
        switch (*arg2->details.name.ptr) {
        case 'Z':
            emit_gh_i_j_k(currentSection, 0175, 0, j, 0);
            break;
        case 'N':
            emit_gh_i_j_k(currentSection, 0175, 0, j, 1);
            break;
        case 'P':
            emit_gh_i_j_k(currentSection, 0175, 0, j, 2);
            break;
        case 'M':
            emit_gh_i_j_k(currentSection, 0175, 0, j, 3);
            break;
        default:
            err = Err_OperandField;
            break;
        }
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  XA Aj
*/
static ErrorCode XA__Aj(void) {
    ErrorCode err;
    int j;

    err = registerError(getRegisterNumber(instArgv[1], &j));
    emit_gh_i_j_k(currentSection, 001, 3, j, 0);
    return err;
}

/*
**  exp,A0 Bjk,Ai
*/
static ErrorCode X_Ah__Bjk_Ai(void) {
    ErrorCode err;
    int i;
    int jk;
    int z;
    Value val;

    err = registerError(evaluateExpression(instArgv[0], &val));
    err = registerError(getRegisterNumber(instArgv[1], &z));
    err = registerError(getRegisterNumber(instArgv[2], &jk));
    err = registerError(getRegisterNumber(instArgv[3], &i));
    if (z == 0 && isZero(&val)) {
        emit_gh_i_jk(currentSection, 035, i, jk);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  exp,A0 Tjk,Ai
*/
static ErrorCode X_Ah__Tjk_Ai(void) {
    ErrorCode err;
    int i;
    int jk;
    int z;
    Value val;

    err = registerError(evaluateExpression(instArgv[0], &val));
    err = registerError(getRegisterNumber(instArgv[1], &z));
    err = registerError(getRegisterNumber(instArgv[2], &jk));
    err = registerError(getRegisterNumber(instArgv[3], &i));
    if (z == 0 && isZero(&val)) {
        emit_gh_i_jk(currentSection, 037, i, jk);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  exp,Ah Ai
**  ,Ah Ai
*/
static ErrorCode X_Ah__Ai(void) {
    ErrorCode err;
    int h;
    int i;
    Value val;

    err = registerError(evaluateExpression(instArgv[0], &val));
    err = registerError(getRegisterNumber(instArgv[1], &h));
    err = registerError(getRegisterNumber(instArgv[2], &i));
    if (isIntegerRange(&val, INT_22_LOWER, INT_22_UPPER) && isParcelAddress(&val) == FALSE) {
        emit_g_h_i_jkm(currentSection, 011, h, i, &val);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  exp,0 Ai
**  exp, Ai
*/
static ErrorCode X_X__Ai(void) {
    ErrorCode err;
    int i;
    Value val1;
    Value val2;

    err = registerError(evaluateExpression(instArgv[0], &val1));
    err = registerError(evaluateExpression(instArgv[1], &val2));
    err = registerError(getRegisterNumber(instArgv[2], &i));
    if (isZero(&val2) == FALSE) err = registerError(Err_OperandField);
    if (isIntegerRange(&val1, INT_22_LOWER, INT_22_UPPER) && isParcelAddress(&val1) == FALSE) {
        emit_gh_i_jkm(currentSection, 0110, i, &val1);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  exp,Ah Si
**  ,Ah Si
*/
static ErrorCode X_Ah__Si(void) {
    ErrorCode err;
    int h;
    int i;
    Value val;

    err = registerError(evaluateExpression(instArgv[0], &val));
    err = registerError(getRegisterNumber(instArgv[1], &h));
    err = registerError(getRegisterNumber(instArgv[2], &i));
    if (isIntegerRange(&val, INT_22_LOWER, INT_22_UPPER) && isParcelAddress(&val) == FALSE) {
        emit_g_h_i_jkm(currentSection, 013, h, i, &val);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  exp,0 Si
**  exp, Si
*/
static ErrorCode X_X__Si(void) {
    ErrorCode err;
    int i;
    Value val1;
    Value val2;

    err = registerError(evaluateExpression(instArgv[0], &val1));
    err = registerError(evaluateExpression(instArgv[1], &val2));
    err = registerError(getRegisterNumber(instArgv[2], &i));
    if (isZero(&val2) == FALSE) err = registerError(Err_OperandField);
    if (isIntegerRange(&val1, INT_22_LOWER, INT_22_UPPER) && isParcelAddress(&val1) == FALSE) {
        emit_gh_i_jkm(currentSection, 0130, i, &val1);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  ,A0,Ak Vj
*/
static ErrorCode X_A0_Ak__Vj(void) {
    ErrorCode err;
    int j;
    int k;
    Value val;
    int z;

    err = registerError(evaluateExpression(instArgv[0], &val));
    err = registerError(getRegisterNumber(instArgv[1], &z));
    err = registerError(getRegisterNumber(instArgv[2], &k));
    err = registerError(getRegisterNumber(instArgv[3], &j));
    if (z == 0 && isZero(&val)) {
        emit_gh_i_j_k(currentSection, 0177, 0, j, k);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  ,A0,Vk Vj
*/
static ErrorCode X_A0_Vk__Vj(void) {
    ErrorCode err;
    int j;
    int k;
    Value val;
    int z;

    err = registerError(evaluateExpression(instArgv[0], &val));
    err = registerError(getRegisterNumber(instArgv[1], &z));
    err = registerError(getRegisterNumber(instArgv[2], &k));
    err = registerError(getRegisterNumber(instArgv[3], &j));
    if (z == 0 && isZero(&val)) {
        emit_gh_i_j_k(currentSection, 0177, 1, j, k);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  ,A0,1 Vj
*/
static ErrorCode X_A0_1__Vj(void) {
    ErrorCode err;
    int j;
    Value val1;
    Value val2;
    int z;

    err = registerError(evaluateExpression(instArgv[0], &val1));
    err = registerError(getRegisterNumber(instArgv[1], &z));
    err = registerError(evaluateExpression(instArgv[2], &val2));
    err = registerError(getRegisterNumber(instArgv[3], &j));
    if (z == 0 && isZero(&val1) && isOne(&val2)) {
        emit_gh_i_j_k(currentSection, 0177, 0, j, 0);
    }
    else {
        err = Err_OperandField;
    }
    return err;
}

/*
**  addEntryPoint - add an entry point definition to a module's chain of them
*/
static void addEntryPoint(Module *module, Symbol *symbol) {
    Symbol *currentEntryPoint;

    if (module->entryPoints == NULL) {
        module->entryPoints = symbol;
        return;
    }
    currentEntryPoint = module->entryPoints;
    while (TRUE) {
        if (strcmp(currentEntryPoint->id, symbol->id) == 0) return;
        if (currentEntryPoint->next == NULL) {
            currentEntryPoint->next = symbol;
            return;
        }
        currentEntryPoint = currentEntryPoint->next;
    }
}

/*
**  addExternal - add an external definition to a module's chain of them
*/
static void addExternal(Module *module, Symbol *symbol) {
    Symbol *currentExternal;

    if (module->externals == NULL) {
        symbol->externalIndex = 0;
        module->externals = symbol;
        return;
    }
    currentExternal = module->externals;
    while (TRUE) {
        if (strcmp(currentExternal->id, symbol->id) == 0) return;
        if (currentExternal->next == NULL) {
            symbol->externalIndex = currentExternal->externalIndex + 1;
            currentExternal->next = symbol;
            return;
        }
        currentExternal = currentExternal->next;
    }
}

/*
**  addInstruction - add a named instruction definition to the instruction tree
*/
static void addInstruction(char *id, u8 attributes, InstructionHandler handler) {
    NamedInstruction *current;
    NamedInstruction *new;
    int valence;

    new = allocInstruction(id, attributes, handler);
    current = namedInstructions;
    if (current == NULL) {
        namedInstructions = new;
        return;
    }

    while (current != NULL) {
        valence = strcasecmp(current->id, id);
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
            freeInstruction(new);
            new = NULL;
            break;
        }
    }
}

/*
 *  addMacroCallParam - add a parameter name and value to a macro call structure
 */
static void addMacroCallParam(MacroCall *call, MacroParam *param, char *value, int valueLen) {
    int len;
    MacroParam *new;

    new = (MacroParam *)allocate(sizeof(MacroParam));
    new->type = param->type;
    len = strlen(param->name);
    new->name = (char *)allocate(len + 1);
    memcpy(new->name, param->name, len);
    new->value = (char *)allocate(valueLen + 1);
    memcpy(new->value, value, valueLen);
    new->next = call->params;
    call->params = new;
}

/*
 *  addMacroLine - add a line structure to a macro definition
 */
static MacroLine *addMacroLine(MacroDefn *defn) {
    MacroLine *lp;
    MacroLine *new;

    new = (MacroLine *)allocate(sizeof(MacroLine));
    if (defn->body == NULL) {
        defn->body = new;
    }
    else {
        lp = defn->body;
        while (lp->next != NULL) lp = lp->next;
        lp->next = new;
    }
    return new;
}

/*
 *  addMacroLineFragment - add a fragment to a macro line structure chain
 */
static void addMacroLineFragment(MacroLine *line, MacroFragType type, char *text, int len) {
    MacroFragment *fp;
    MacroFragment *new;

    if (len < 1) return;
    new = (MacroFragment *)allocate(sizeof(MacroFragment));
    new->type = type;
    new->text = (char *)allocate(len + 1);
    memcpy(new->text, text, len);
    if (line->fragments == NULL) {
        line->fragments = new;
    }
    else {
        fp = line->fragments;
        while (fp->next != NULL) fp = fp->next;
        fp->next = new;
    }
}

/*
 *  addMacroParam - add a parameter definition to a macro definition
 */
static void addMacroParam(MacroDefn *defn, MacroParamType type, char *name, int nameLen, char *value, int valueLen) {
    MacroParam *new;
    MacroParam *pp;

    new = (MacroParam *)allocate(sizeof(MacroParam));
    new->type = type;
    new->name = (char *)allocate(nameLen + 1);
    memcpy(new->name, name, nameLen);
    if (value != NULL) {
        new->value = (char *)allocate(valueLen + 1);
        memcpy(new->value, value, valueLen);
    }
    if (defn->params == NULL) {
        defn->params = new;
    }
    else {
        pp = defn->params;
        while (pp->next != NULL) pp = pp->next;
        pp->next = new;
    }
}

/*
 *  addPattern - add a machine instruction pattern to the collection of patterns
 */
static void addPattern(char *s, InstructionHandler handler) {
    PatternNode *newNode;
    PatternNode *node;
    PatternNode **nodep;
    
    nodep = &instructionPatterns;
    while (TRUE) {
        newNode = (PatternNode *)allocate(sizeof(PatternNode));
        s = parseNextNode(s, newNode);
        if (*nodep == NULL) {
            *nodep = newNode;
            if (newNode->type == NodeType_PatternEnd) {
                newNode->handler = handler;
                return;
            }
            nodep = &newNode->next;
        }
        else {
            node = *nodep;
            while (TRUE) {
                if (isEquivNode(node, newNode)) {
                    if (node->type == NodeType_PatternEnd) {
                        fputs("Duplicate instruction pattern\n", stderr);
                        exit(1);
                    }
                    nodep = &node->next;
                    free(newNode);
                    break;
                }
                else if (node->sibling != NULL) {
                    node = node->sibling;
                }
                else {
                    node->sibling = newNode;
                    if (newNode->type == NodeType_PatternEnd) {
                        newNode->handler = handler;
                        return;
                    }
                    nodep = &newNode->next;
                    break;
                }
            }
        }
    }
}

/*
**  allocInstruction - allocate an instance of a named Instruction structure
*/
static NamedInstruction *allocInstruction(char *id, u8 attributes, InstructionHandler handler) {
    int len;
    NamedInstruction *instruction;
    char *s;

    len = strlen(id) + 1;
    s = (char *)allocate(len);
    instruction = (NamedInstruction *)allocate(sizeof(NamedInstruction));
    memcpy(s, id, len);
    instruction->id = s;
    instruction->attributes = attributes;
    instruction->handler = handler;
    return instruction;
}

ErrorCode callMacro(MacroDefn *defn, Token *locationFieldToken) {
    ErrorCode err;
    MacroCall *call;
    char *keyword;
    int keywordLen;
    int len;
    MacroParam *pp;
    char *s;
    char *start;
    char *value;
    int valueLen;

    err = Err_None;
    call = (MacroCall *)allocate(sizeof(MacroCall));
    call->defn = defn;
    if (locationFieldToken != NULL) {
        if (defn->locationParam != NULL) {
            addMacroCallParam(call, defn->locationParam, locationFieldToken->details.name.ptr, locationFieldToken->details.name.len);
        }
        else {
            (void)registerError(Warn_IgnoredLocationSymbol);
        }
    }
    else if (defn->locationParam != NULL) {
        addMacroCallParam(call, defn->locationParam, "", 0);
    }
    //
    //  Parse positional parameters
    //
    s = operandField;
    pp = defn->params;
    while (*s != '\0' && pp != NULL && pp->type != MacroParamType_Keyword) {
        s = getParamValue(s, &start, &len);
        if (start == NULL) {
            freeMacroCall(call);
            return Err_OperandField;
        }
        addMacroCallParam(call, pp, start, len);
        if (*s != '\0') s += 1;
        pp = pp->next;
    }
    if (*s == '\0' && pp != NULL) {
        while (pp != NULL && pp->type == MacroParamType_Positional) {
            addMacroCallParam(call, pp, "", 0);
            pp = pp->next;
        }
    }
    //
    //  Parse keyword parameters
    //
    while (*s != '\0') {
        s = getNextName(s, &keyword, &keywordLen);
        if (keywordLen == 0 || *s != '=') {
            err = Err_OperandField;
            break;
        }
        s += 1;
        s = getParamValue(s, &value, &valueLen);
        if (value == NULL) {
            err = Err_OperandField;
            break;
        }
        pp = findMacroParam(defn, keyword, keywordLen);
        if (pp == NULL) {
            err = Err_OperandField;
            break;
        }
        addMacroCallParam(call, pp, value, valueLen);
        if (*s != '\0') s += 1;
    }
    if (err != Err_None) {
        freeMacroCall(call);
        return err;
    }
    //
    //  Push the call on the macro call stack
    //
    call->nextLine = defn->body;
    if (macroStackPtr < MACRO_STACK_SIZE) {
        macroStack[macroStackPtr++] = call;
    }
    else {
        freeMacroCall(call);
        err = Err_TooManyEntries;
    }

    return err;
}

static int compareStrings(char *s1, int s1Len, char *s2, int s2Len) {
    int n;
    int valence;

    n = s1Len <= s2Len ? s1Len : s2Len;
    while (n-- > 0) {
        valence = *s1++ - *s2++;
        if (valence != 0) return valence;
    }
    return s1Len - s2Len;
}

static ErrorCode defineSymbol(u16 attributes) {
    ErrorCode err;
    char *s;
    Symbol *symbol;
    Token token;
    Value val;

    if (locationFieldToken == NULL || locationFieldToken->type != TokenType_Name || locationField[0] == '*')
        return Err_LocationField;
    
    s = getNextValue(operandField, &val, &err);
    if (err != Err_None) (void)registerError(err);

    if (*s == ',') {
        s = getNextToken(s + 1, &token);
        if (token.type == TokenType_Name && token.details.name.len == 1) {
            switch (*token.details.name.ptr) {
            case 'P':
                if (isWordAddress(&val)) val.intValue *= 4;
                val.attributes = SYM_PARCEL_ADDRESS;
                break;
            case 'V':
                if (isRelocatable(&val)) err = registerError(Err_OperandField);
                val.attributes = 0;
                val.section = NULL;
                break;
            case 'W':
                if (isParcelAddress(&val)) val.intValue /= 4;
                val.attributes = SYM_WORD_ADDRESS;
                break;
            default:
                err = registerError(Err_OperandField);
                break;
            }
        }
        else {
            err = registerError(Err_OperandField);
        }
    }
    else if (*s != '\0') {
        err = registerError(Err_OperandField);
    }
    
    symbol = findSymbol(locationFieldToken->details.name.ptr, locationFieldToken->details.name.len, currentQualifier);
    val.attributes |= attributes;
    if (symbol == NULL) {
        symbol = addSymbol(locationFieldToken->details.name.ptr, locationFieldToken->details.name.len, currentQualifier, &val);
    }
    else if ((symbol->value.attributes & (SYM_UNDEFINED|SYM_REDEFINABLE)) != 0) {
        symbol->value.attributes = val.attributes;
        symbol->value.section = val.section;
        symbol->value.intValue = val.intValue;
    }
    else if (symbol->value.attributes != val.attributes || symbol->value.intValue != val.intValue) {
        err = Err_DoubleDefinition;
    }
    if (pass == 2) symbol->value.attributes |= SYM_DEFINED_P2;

    if (err == Err_None || err >= Warn_Programmer) listValue(&val);

    return err;
}

/*
 *  findInstruction - find a named instruction definition in the instruction tree
 */
NamedInstruction *findInstruction(char *id, int len) {
    NamedInstruction *current;
    int valence;

    current = namedInstructions;
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

/*
 *  findMacroParam - find a macro parameter definition
 */
static MacroParam *findMacroParam(MacroDefn *defn, char *name, int len) {
    MacroParam *pp;

    if (defn->locationParam != NULL
        && strncmp(defn->locationParam->name, name, len) == 0
        && defn->locationParam->name[len] == '\0')
        return defn->locationParam;
    pp = defn->params;
    while (pp != NULL) {
        if (strncmp(pp->name, name, len) == 0 && pp->name[len] == '\0')
            return pp;
        pp = pp->next;
    }
    return NULL;
}

/*
 *  findSectionLocation - fina a match for the name of a section location
 */
typedef struct sectionLocationTableEntry {
    char *name;
    int len;
    SectionLocation location;
} SectionLocationTableEntry;

static SectionLocationTableEntry sectionLocationTable[] = {
    {"CM", 2, SectionLocation_CM},
    {"EM", 2, SectionLocation_EM},
/*  {"LM", 2, SectionLocation_LM}, */
    {NULL, 0, 0}
};

static SectionLocation findSectionLocation(char *name, int len) {
    SectionLocationTableEntry *entry;

    for (entry = &sectionLocationTable[0]; entry->name != NULL; entry++) {
        if (entry->len == len && strncasecmp(entry->name, name, len) == 0)
            return entry->location;
    }
    return SectionLocation_None;
}

/*
 *  findSectionType - fina a match for the name of a section type
 */
typedef struct sectionTypeTableEntry {
    char *name;
    int len;
    SectionType type;
} SectionTypeTableEntry;

static SectionTypeTableEntry sectionTypeTable[] = {
    {"MIXED",   5, SectionType_Mixed},
    {"CODE",    4, SectionType_Code},
    {"DATA",    4, SectionType_Data},
    {"STACK",   5, SectionType_Stack},
    {"COMMON",  6, SectionType_Common},
    {"DYNAMIC", 7, SectionType_Dynamic},
    {"TASKCOM", 7, SectionType_TaskCom},
    {NULL,      0, 0}
};

static SectionType findSectionType(char *name, int len) {
    SectionTypeTableEntry *entry;

    for (entry = &sectionTypeTable[0]; entry->name != NULL; entry++) {
        if (entry->len == len && strncasecmp(entry->name, name, len) == 0)
            return entry->type;
    }
    return SectionType_None;
}

/*
 *  forceInstWordBoundary - advance location and origin counters to next instruction word boundary, if necessary
 */
static void forceInstWordBoundary(void) {
    u16 savedListControl;

    savedListControl = currentListControl;
    currentListControl = 0;
    while ((currentSection->locationCounter & 0x03) != 0) {
        emit_gh_ijk(currentSection, 001, 0);
    }
    currentListControl = savedListControl;
}

/*
 *  freeinstruction - free storage allocated to a named instruction instance
 */
static void freeInstruction(NamedInstruction *instruction) {
    free(instruction->id);
    free(instruction);
}

void freeMacroCall(MacroCall *call) {
    MacroParam *pp;
    MacroParam *ppNext;

    pp = call->params;
    while (pp != NULL) {
        ppNext = pp->next;
        if (pp->name != NULL) free(pp->name);
        if (pp->value != NULL) free(pp->value);
        free(pp);
        pp = ppNext;
    }
    free(call);
}

static void freeMacroDefn(MacroDefn *defn) {
    MacroFragment *fp;
    MacroFragment *fpNext;
    MacroParam *pp;
    MacroParam *ppNext;
    MacroLine *lp;
    MacroLine *lpNext;

    if (defn->locationParam != NULL) {
        free(defn->locationParam->name);
        free(defn->locationParam);
    }
    lp = defn->body;
    while (lp != NULL) {
        lpNext = lp->next;
        fp = lp->fragments;
        while (fp != NULL) {
           fpNext = fp->next;
           free(fp->text);
           free(fp);
           fp = fpNext;
        }
        free(lp);
        lp = lpNext;
    }
    pp = defn->params;
    while (pp != NULL) {
        ppNext = pp->next;
        free(pp->name);
        if (pp->value != NULL) free(pp->value);
        free(pp);
        pp = ppNext;
    }
    free(defn);
}

/*
 *  generateZero - generate an instance of TokenType_Number with an integer value of 0.
 */
static Token *generateZero(void) {
    Token *zero;

    zero = (Token *)allocate(sizeof(Token));
    zero->type = TokenType_Number;
    zero->details.number.type = NumberType_Integer;
    return zero;
}

/*
 *  getDelimitedString - get the next delimited substring from a string
 */
static char *getDelimitedString(char *s, char **start, int *len) {
    char *begin;
    char delim;

    *start = NULL;
    *len = 0;

    delim = *s++;
    begin = s;
    //
    //  Find end of micro definition
    //
    while (*s != '\0') {
        if (*s == delim) {
            if (*(s + 1) != delim) break;
            s += 1;
        }
        s += 1;
    }
    if (*s == delim) {
        *start = begin;
        *len = s - begin;
        s += 1;
    }
    return s;
}

/*
 *  getNextName - get next name from a string
 */
static char *getNextName(char *s, char **name, int *len) {
    char *start;

    start = s;
    if (isNameChar1(*s)) {
        s += 1;
        while (isNameChar(*s)) s += 1;
    }
    *name = start;
    *len = s - start;
    return s;
}

/*
 *  getParamValue - get next macro parameter value from a string
 */
static char *getParamValue(char *s, char **value, int *valueLen) {
    int depth;
    Token *expression;
    char *start;

    if (*s == ' ' || *s == ',') {
        *value = "";
        *valueLen = 0;
        while (*s == ' ') s += 1;
    }
    else if (*s == '(') {
        depth = 1;
        *value = ++s;
        while (*s != '\0') {
            if (*s == ')') {
                depth -= 1;
                if (depth < 1) break;
            }
            else if (*s == '(') {
                depth += 1;
            }
            s += 1;
        }
        if (*s != ')') {
            *value = NULL;
            return s;
        }
        *valueLen = s - *value;
        s += 1;
        if (*s != '\0' && *s != ',') {
            *value = NULL;
            return s;
        }
    }
    else {
        start = s;
        s = parseExpression(s, &expression);
        if (expression->type != TokenType_Error) {
            *value = start;
            *valueLen = s - start;
        }
        else {
            *value = NULL;
        }
        freeToken(expression);
    }
    return s;
}

static ErrorCode handleBranch(u16 opCode) {
    ErrorCode err;
    char *s;
    Value val;

    s = getNextValue(operandField, &val, &err);
    if (*s != '\0') err = Err_OperandField;
    if (isWordAddress(&val)) {
        val.intValue <<= 2;
        val.attributes = (val.attributes & ~SYM_WORD_ADDRESS) | SYM_PARCEL_ADDRESS;
    }
    emit_gh_ijkm(currentSection, opCode, &val);
    return err;
}

static ErrorCode handleOp_i_j_k(u16 opCode) {
    ErrorCode err;
    int i;
    int j;
    int k;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j));
    err = registerError(getRegisterNumber(instArgv[2], &k));
    emit_gh_i_j_k(currentSection, opCode, i, j, k);
    return err;
}

static ErrorCode handleOp_i_j_n(u16 opCode, u8 n) {
    ErrorCode err;
    int i;
    int j;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &j));
    emit_gh_i_j_k(currentSection, opCode, i, j, n);
    return err;
}

static ErrorCode handleOp_i_jk(u16 opCode) {
    ErrorCode err;
    int i;
    int jk;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &jk));
    emit_gh_i_jk(currentSection, opCode, i, jk);
    return err;
}

static ErrorCode handleOp_i_n(u16 opCode, u16 n) {
    ErrorCode err;
    int i;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    emit_gh_i_jk(currentSection, opCode, i, n);
    return err;
}

static ErrorCode handleOp_i_n_k(u16 opCode, u8 n) {
    ErrorCode err;
    int i;
    int k;

    err = registerError(getRegisterNumber(instArgv[0], &i));
    err = registerError(getRegisterNumber(instArgv[1], &k));
    emit_gh_i_j_k(currentSection, opCode, i, n, k);
    return err;
}

static InstPatternDefn instructionPatternDefns[] = {
    {"Ai Ak",       Ai__Ak},
    {"Ai -Ak",      Ai__negAk},
    {"Ai Aj+Ak",    Ai__Aj_add_Ak},
    {"Ai Aj+$",     Ai__Aj_add_1},
    {"Ai Aj-Ak",    Ai__Aj_sub_Ak},
    {"Ai Aj-$",     Ai__Aj_sub_1},
    {"Ai Aj*Ak",    Ai__Aj_mul_Ak},
    {"Ai Bjk",      Ai__Bjk},
    {"Ai CA,Aj",    Ai__CA_Aj},
    {"Ai CE,Aj",    Ai__CE_Aj},
    {"Ai CI",       Ai__CI},
    {"Ai PSj",      Ai__PSj},
    {"Ai QSj",      Ai__QSj},
    {"Ai Sj",       Ai__Sj},
    {"Ai SBj",      Ai__SBj},
    {"Ai VL",       Ai__VL},
    {"Ai ZSj",      Ai__ZSj},
    {"Ai $",        Ai__X},
    {"Ai $,Ah",     Ai__X_Ah},
    {"Ai $,$",      Ai__X_X},
    {"Bjk Ai",      Bjk__Ai},
    {"Bjk,Ai $,Ah", Bjk_Ai__X_A0},
    {"CA,Aj Ak",    CA_Aj__Ak},
    {"CI,Aj",       CI_Aj},
    {"CL,Aj Ak",    CL_Aj__Ak},
    {"MC,Aj",       MC_Aj},
    {"RT Sj",       RT__Sj},
    {"Si Ak",       Si__Ak},
    {"Si +Ak",      Si__ExtendAk},
    {"Si +FAk",     Si__FAk},
    {"Si Sk",       Si__Sk},
    {"Si #Sk",      Si__CmplSk},
    {"Si -Sk",      Si__NegSk},
    {"Si -FSk",     Si__NegFSk},
    {"Si +FSk",     Si__NormFSk},
    {"Si Sj+Sk",    Si__Sj_add_Sk},
    {"Si Sj+FSk",   Si__Sj_add_FSk},
    {"Si Sj-Sk",    Si__Sj_sub_Sk},
    {"Si Sj-FSk",   Si__Sj_sub_FSk},
    {"Si Sj*FSk",   Si__Sj_mul_FSk},
    {"Si Sj*HSk",   Si__Sj_mul_HSk},
    {"Si Sj*RSk",   Si__Sj_mul_RSk},
    {"Si Sj*ISk",   Si__Sj_mul_ISk},
    {"Si /HSj",     Si__RecipSj},
    {"Si Sj&Sk",    Si__Sj_and_Sk},
    {"Si Sj&SB",    Si__Sj_and_SB},
    {"Si SB&Sj",    Si__SB_and_Sj},
    {"Si Sj!Sk",    Si__Sj_or_Sk},
    {"Si Sj!SB",    Si__Sj_or_SB},
    {"Si SB!Sj",    Si__SB_or_Sj},
    {"Si Sj!Si&Sk", Si__Si_merge_Sj},
    {"Si Sj!Si&SB", Si__Si_merge_SB},
    {"Si Sj\\Sk",   Si__Sj_xor_Sk},
    {"Si Sj\\SB",   Si__Sj_xor_SB},
    {"Si SB\\Sj",   Si__SB_xor_Sj},
    {"Si Si<Ak",    Si__Si_left_Ak},
    {"Si Si<$",     Si__Si_left_X},
    {"Si Si,Sj<Ak", Si__SiSj_left_Ak},
    {"Si Si,Sj<$",  Si__SiSj_left_X},
    {"Si Si>Ak",    Si__Si_right_Ak},
    {"Si Si>$",     Si__Si_right_X},
    {"Si Sj,Si>Ak", Si__SjSi_right_Ak},
    {"Si Sj,Si>$",  Si__SjSi_right_X},
    {"Si #Sk&Sj",   Si__CmplSk_and_Sj},
    {"Si #SB&Sj",   Si__CmplSB_and_Sj},
    {"Si #Sj\\Sk",  Si__CmplSj_xor_Sk},
    {"Si #Sj\\SB",  Si__CmplSj_xor_SB},
    {"Si #SB\\Sj",  Si__CmplSB_xor_Sj},
    {"Si #<$",      Si__CmplMaskLeft},
    {"Si #>$",      Si__CmplMaskRight},
    {"Si #SB",      Si__CmplSB},
    {"Si Vj,Ak",    Si__Vj_Ak},
    {"Si >$",       Si__MaskLeft},
    {"Si <$",       Si__MaskRight},
    {"Si SB",       Si__SB},
    {"Si RT",       Si__RT},
    {"Si SM",       Si__SM},
    {"Si VM",       Si__VM},
    {"Si STj",      Si__STj},
    {"Si SRj",      Si__SRj},
    {"Si Tjk",      Si__Tjk},
    {"Si $",        Si__X},
    {"Si $,Ah",     Si__X_Ah},
    {"Si $,$",      Si__X_X},
    {"SBj Ai",      SBj__Ai},
    {"SM Si",       SM__Si},
    {"SMjk $",      SMjk__X},
    {"SMjk $,$",    SMjk__X_X},
    {"STj Si",      STj__Si},
    {"Tjk Si",      Tjk__Si},
    {"Tjk,Ai $,Ah", Tjk_Ai__X_A0},
    {"Vi Vk",       Vi__Vk},
    {"Vi -Vk",      Vi__NegVk},
    {"Vi Sj+Vk",    Vi__Sj_add_Vk},
    {"Vi Vj+Vk",    Vi__Vj_add_Vk},
    {"Vi Sj-Vk",    Vi__Sj_sub_Vk},
    {"Vi Vj-Vk",    Vi__Vj_sub_Vk},
    {"Vi Sj&Vk",    Vi__Sj_and_Vk},
    {"Vi Vj&Vk",    Vi__Vj_and_Vk},
    {"Vi Sj!Vk",    Vi__Sj_or_Vk},
    {"Vi Vj!Vk",    Vi__Vj_or_Vk},
    {"Vi Sj\\Vk",   Vi__Sj_xor_Vk},
    {"Vi Vj\\Vk",   Vi__Vj_xor_Vk},
    {"Vi Sj!Vk&VM", Vi__Sj_merge_Vk},
    {"Vi Vj!Vk&VM", Vi__Vj_merge_Vk},
    {"Vi #VM&Vk",   Vi__0_merge_Vk},
    {"Vi Vj<Ak",    Vi__Vj_left_Ak},
    {"Vi Vj<$",     Vi__Vj_left_1},
    {"Vi Vj>Ak",    Vi__Vj_right_Ak},
    {"Vi Vj>$",     Vi__Vj_right_1},
    {"Vi Vj,Vj<Ak", Vi__VjVj_left_Ak},
    {"Vi Vj,Vj<$",  Vi__VjVj_left_1},
    {"Vi Vj,Vj>Ak", Vi__VjVj_right_Ak},
    {"Vi Vj,Vj>$",  Vi__VjVj_right_1},
    {"Vi +FVk",     Vi__NormFVk},
    {"Vi -FVk",     Vi__NegFVk},
    {"Vi Sj+FVk",   Vi__Sj_add_FVk},
    {"Vi Vj+FVk",   Vi__Vj_add_FVk},
    {"Vi Sj-FVk",   Vi__Sj_sub_FVk},
    {"Vi Vj-FVk",   Vi__Vj_sub_FVk},
    {"Vi Sj*FVk",   Vi__Sj_mul_FVk},
    {"Vi Vj*FVk",   Vi__Vj_mul_FVk},
    {"Vi Sj*HVk",   Vi__Sj_mul_HVk},
    {"Vi Vj*HVk",   Vi__Vj_mul_HVk},
    {"Vi Sj*IVk",   Vi__Sj_mul_IVk},
    {"Vi Vj*IVk",   Vi__Vj_mul_IVk},
    {"Vi Sj*RVk",   Vi__Sj_mul_RVk},
    {"Vi Vj*RVk",   Vi__Vj_mul_RVk},
    {"Vi /HVj",     Vi__RecipHVj},
    {"Vi PVj",      Vi__PVj},
    {"Vi QVj",      Vi__QVj},
    {"Vi $",        Vi__0},
    {"Vi $,Ai,Ak",  Vi__0_A0_Ak},
    {"Vi $,Ai,Vk",  Vi__0_A0_Vk},
    {"Vi $,Ai,$",   Vi__0_A0_1},
    {"Vi,Ak Sj",    Vi_Ak__Sj},
    {"Vi,Ak $",     Vi_Ak__X},
    {"Vi,VM Vj,$",  Vi_VM__Vj_ID},
    {"VL Ak",       VL__Ak},
    {"VL $",        VL__X},
    {"VM Sj",       VM__Sj},
    {"VM Vj,$",     VM__Vj_ID},
    {"VM $",        VM__X},
    {"XA Aj",       XA__Aj},
    {"$,Ah Ai",     X_Ah__Ai},
    {"$,Ah Si",     X_Ah__Si},
    {"$,Ah Bjk,Ai", X_Ah__Bjk_Ai},
    {"$,Ah Tjk,Ai", X_Ah__Tjk_Ai},
    {"$,Ai,Ak Vj",  X_A0_Ak__Vj},
    {"$,Ai,Vk Vj",  X_A0_Vk__Vj},
    {"$,Ai,$ Vj",   X_A0_1__Vj},
    {"$,$ Ai",      X_X__Ai},
    {"$,$ Si",      X_X__Si},
    {NULL, NULL}
};

/*
 *  instInit - build the instruction handler tree
 */
void instInit(void) {
    InstPatternDefn *patternDefn;

    /*
     *  Machine instruction patterns
     */
    patternDefn = &instructionPatternDefns[0];
    while (patternDefn->pattern != NULL) {
        addPattern(patternDefn->pattern, patternDefn->handler);
        patternDefn += 1;
    }
    /*
     *  Pseudo-instructions
     */
    addInstruction("MACRO",   0, MACRO);
    addInstruction("QUAL",    0, QUAL);
    addInstruction("BITP",    0, BITP);
    addInstruction("IDENT",   0, IDENT);
    addInstruction("ALIGN",   0, ALIGN);
    addInstruction("SET",     0, SET);
    addInstruction("BLOCK",   0, BLOCK);
    addInstruction("LOC",     0, LOC);
    addInstruction("EXT",     0, EXT);
    addInstruction("CON",     0, CON);
    addInstruction("MODULE",  0, MODULE);
    addInstruction("DECMIC",  0, DECMIC);
    addInstruction("IFE",     0, IFE);
    addInstruction("ERROR",   0, ERROR);
    addInstruction("ABS",     0, ABS);
    addInstruction("ENDIF",   0, ENDIF);
    addInstruction("BASE",    0, BASE);
    addInstruction("EJECT",   0, EJECT);
    addInstruction("DATA",    0, DATA);
    addInstruction("OPSYN",   0, OPSYN);
    addInstruction("ENDM",    0, ENDM);
    addInstruction("FORMAT",  0, FORMAT);
    addInstruction("START",   0, START);
    addInstruction("=",       0, EQU);
    addInstruction("LIST",    0, LIST);
    addInstruction("SECTION", 0, SECTION);
    addInstruction("ERRIF",   0, ERRIF);
    addInstruction("BITW",    0, BITW);
    addInstruction("END",     0, END);
    addInstruction("ORG",     0, ORG);
    addInstruction("COMMON",  0, COMMON);
    addInstruction("LOCAL",   0, LOCAL);
    addInstruction("TITLE",   0, TITLE);
    addInstruction("REP",     0, REP);
    addInstruction("VWD",     0, VWD);
    addInstruction("IFC",     0, IFC);
    addInstruction("OCTMIC",  0, OCTMIC);
    addInstruction("ENDDUP",  0, ENDDUP);
    addInstruction("BSSZ",    0, BSSZ);
    addInstruction("ELSE",    0, ELSE);
    addInstruction("TEXT",    0, TEXT);
    addInstruction("DUP",     0, DUP);
    addInstruction("SUBTITLE",0, SUBTITLE);
    addInstruction("SKIP",    0, SKIP);
    addInstruction("STACK",   0, STACK);
    addInstruction("IFA",     0, IFA);
    addInstruction("OPDEF",   0, OPDEF);
    addInstruction("MICRO",   0, MICRO);
    addInstruction("ECHO",    0, ECHO);
    addInstruction("ENDTEXT", 0, ENDTEXT);
    addInstruction("STOPDUP", 0, STOPDUP);
    addInstruction("COMMENT", 0, COMMENT);
    addInstruction("SPACE",   0, SPACE);
    addInstruction("MICSIZE", 0, MICSIZE);
    addInstruction("ENTRY",   0, ENTRY);
    addInstruction("BSS",     0, BSS);
    //
    // Named machine instructions
    //
    addInstruction("PASS",    INST_MACHINE, PASS);
    addInstruction("DCI",     INST_MACHINE, DCI);
    addInstruction("ERR",     INST_MACHINE, ERR);
    addInstruction("EX",      INST_MACHINE, EX);
    addInstruction("DBM",     INST_MACHINE, DBM);
    addInstruction("DFI",     INST_MACHINE, DFI);
    addInstruction("J",       INST_MACHINE, J);
    addInstruction("EBM",     INST_MACHINE, EBM);
    addInstruction("CIPI",    INST_MACHINE, CIPI);
    addInstruction("CLN",     INST_MACHINE, CLN);
    addInstruction("IP",      INST_MACHINE, IP);
    addInstruction("PCI",     INST_MACHINE, PCI);
    addInstruction("JAN",     INST_MACHINE, JAN);
    addInstruction("JAZ",     INST_MACHINE, JAZ);
    addInstruction("JAM",     INST_MACHINE, JAM);
    addInstruction("JAP",     INST_MACHINE, JAP);
    addInstruction("SIPI",    INST_MACHINE, SIPI);
    addInstruction("R",       INST_MACHINE, R);
    addInstruction("CMR",     INST_MACHINE, CMR);
    addInstruction("DRI",     INST_MACHINE, DRI);
    addInstruction("JSN",     INST_MACHINE, JSN);
    addInstruction("JSZ",     INST_MACHINE, JSZ);
    addInstruction("JSM",     INST_MACHINE, JSM);
    addInstruction("JSP",     INST_MACHINE, JSP);
    addInstruction("ERI",     INST_MACHINE, ERI);
    addInstruction("EFI",     INST_MACHINE, EFI);
    addInstruction("CCI",     INST_MACHINE, CCI);
    addInstruction("ECI",     INST_MACHINE, ECI);
}

static bool isEquivNode(PatternNode *node1, PatternNode *node2) {
    if (node1->type == node2->type) {
        switch (node1->type) {
        case NodeType_Register:
            return node1->regster == node2->regster;
        case NodeType_Operator:
            return node1->operator == node2->operator;
        default:
            return TRUE;
        }
    }
    return FALSE;
}

static bool isFloatFour(Value *val) {
    return val->type == NumberType_Float && val->floatValue == 4.0;
}

static bool isFloatFourEighths(Value *val) {
    return val->type == NumberType_Float && val->floatValue == 0.5;
}

static bool isFloatOne(Value *val) {
    return val->type == NumberType_Float && val->floatValue == 1.0;
}

static bool isFloatSixEighths(Value *val) {
    return val->type == NumberType_Float && val->floatValue == 0.75;
}

static bool isFloatTwo(Value *val) {
    return val->type == NumberType_Float && val->floatValue == 2.0;
}

static bool isInteger(Value *val) {
    return val->type == NumberType_Integer;
}

static bool isIntegerRange(Value *val, int lowerBound, int upperBound) {
    return (isInteger(val) && val->intValue >= lowerBound && val->intValue <= upperBound);
}

static bool isNegOne(Value *val) {
    return isSimpleInteger(val) && val->intValue == -1;
}

static bool isOne(Value *val) {
    return isSimpleInteger(val) && val->intValue == 1;
}

static bool isSimpleInteger(Value *val) {
    return val->type == NumberType_Integer
        && (val->attributes & (SYM_EXTERNAL|SYM_RELOCATABLE|SYM_IMMOBILE|SYM_LITERAL|SYM_UNDEFINED|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS)) == 0;
}

static bool isZero(Value *val) {
    return isSimpleInteger(val) && val->intValue == 0;
}

static InstructionHandler matchInstruction(bool *didMatchResultField) {
    PatternNodeType delimiter;
    PatternNode *expNode;
    Token *expression;
    char *fields[2];
    int i;
    PatternNode *node;
    PatternNode *opNode;
    char *s;
    char *s2;
    char *start;
    Token token;
    Token token2;
    
    *didMatchResultField = FALSE;
    fields[0] = resultField;
    fields[1] = operandField;
    i = 0;
    start = fields[i++];
    node = instructionPatterns;
    instArgc = 0;
    while (TRUE) {
        s = getNextToken(start, &token);
        switch (token.type) {
        case TokenType_None:
            while (TRUE) {
                if (node->type == NodeType_Expression) break;
                node = node->sibling;
                if (node == NULL) return NULL;
            }
            instArgv[instArgc++] = generateZero();
            break;
        case TokenType_Error:
            if (*start != '(') return NULL;
            // fall through
        case TokenType_Name:
        case TokenType_Number:
        case TokenType_String:
            while (TRUE) {
                if (node->type == NodeType_Expression) break;
                node = node->sibling;
                if (node == NULL) return NULL;
            }
            s = parseExpression(start, &expression);
            instArgv[instArgc++] = expression;
            break;
        case TokenType_Register:
            while (TRUE) {
                if (node->type == NodeType_Register && node->regster == token.details.regster.type) break;
                node = node->sibling;
                if (node == NULL) return NULL;
            }
            instArgv[instArgc++] = copyToken(&token);
            break;
        case TokenType_Operator:
            if (start == fields[i - 1] || *(start - 1) == ',') {
            }
            else {
                while (node != NULL) {
                    if (node->type == NodeType_Operator && node->operator == token.details.operator.type) break;
                    node = node->sibling;
                    if (node == NULL) return NULL;
                }
            }
            opNode = expNode = NULL;
            while (node != NULL) {
                if (node->type == NodeType_Operator && node->operator == token.details.operator.type)
                    opNode = node;
                else if (node->type == NodeType_Expression)
                    expNode = node;
                node = node->sibling;
            }
            if (start == fields[i - 1] || *(start - 1) == ',') {
                if (expNode == NULL) {
                    if (opNode == NULL) return NULL;
                    node = opNode;
                }
                else if (opNode != NULL) {
                    switch (opNode->operator) {
                    case Op_CmplMaskLeft:
                    case Op_CmplMaskRight:
                    case Op_MaskLeft:
                    case Op_MaskRight:
                    case Op_ShiftLeft:
                    case Op_ShiftRight:
                        node = opNode;
                        break;
                    default:
                        s2 = getNextToken(s, &token2);
                        if (token2.type == TokenType_Register) {
                            node = opNode;
                        }
                        else {
                            s = parseExpression(start, &expression);
                            instArgv[instArgc++] = expression;
                            node = expNode;
                        }
                        break;
                    }
                }
                else {
                    s = parseExpression(start, &expression);
                    instArgv[instArgc++] = expression;
                    node = expNode;
                }
            }
            else if (opNode != NULL) {
                node = opNode;
            }
            else {
                return NULL;
            }
            break;
        default:
            return NULL;
        }
        node = node->next;
        if (*s == '\0') {
            if (i < 2 && *operandField != '\0') {
                s = fields[i++];
                delimiter = NodeType_FieldDelimiter;
            }
            else {
                delimiter = NodeType_PatternEnd;
            }
            while (TRUE) {
                if (node->type == delimiter) break;
                node = node->sibling;
                if (node == NULL) return NULL;
            }
            *didMatchResultField = TRUE;
            if (node->type == NodeType_PatternEnd) return node->handler;
            node = node->next;
        }
        else if (*s == ',') {
            while (TRUE) {
                if (node->type == NodeType_SubfieldDelimiter) break;
                node = node->sibling;
                if (node == NULL) return NULL;
            }
            node = node->next;
            s += 1;
        }
        start = s;
    }
}

static ErrorCode numericMicro(int base) {
    char buf[9];
    int count;
    ErrorCode err;
    int i;
    int len;
    i64 n;
    Name *name;
    char *s;
    Value val;

    err = Err_None;
    if (locationFieldToken->type != TokenType_Name || locationField[0] == '*') err = registerError(Err_LocationField);
    name = findName(currentModule->micros, locationFieldToken->details.name.ptr, locationFieldToken->details.name.len);
    if (name == NULL) {
        name = addName(&currentModule->micros, locationFieldToken->details.name.ptr, locationFieldToken->details.name.len);
    }
    s = getNextValue(operandField, &val, &err);
    if (err != Err_None) (void)registerError(err);
    if (isSimpleInteger(&val)) {
        n = val.intValue;
    }
    else {
        n = 0;
        err = registerError(Err_OperandField);
    }
    count = 0;
    if (*s == ',') {
        s = getNextValue(s + 1, &val, &err);
        (void)registerError(err);
        if (isSimpleInteger(&val) && isIntegerRange(&val, 0, 8)) {
            count = val.intValue;
        }
        else {
            err = Err_OperandField;
        }
    }
    if (*s != '\0') err = Err_OperandField;
    i = 7;
    buf[i--] = (n % base) + '0';
    n /= base;
    while (i >= 0 && n != 0) {
        buf[i--] = (n % base) + '0';
        n /= base;
    }
    count -= 7 - i;
    while (count > 0 && i >= 0) {
        buf[i--] = '0';
        count -= 1;
    }
    if (name->value != NULL) free(name->value);
    len = 7 - i;
    name->value = allocate(len + 1);
    memcpy(name->value, &buf[i + 1], len);

    return err;
}

static void parseError(char *s) {
    fprintf(stderr, "Unrecognized character in instruction pattern: \"%s\"\n", s);
    exit(1);
}

static char *parseNextNode(char *s, PatternNode *node) {
    memset(node, 0, sizeof(PatternNode));
    switch (*s) {
    case 'A':
        s += 1;
        if (*s >= 'h' && *s <= 'k') {
            node->type = NodeType_Register;
            node->regster = RegisterType_A;
        }
        else {
            parseError(s - 1);
        }
        break;
    case 'B':
        s += 1;
        if (*s == 'j' && *(s + 1) == 'k') {
            node->type = NodeType_Register;
            node->regster = RegisterType_B;
            s += 1;
        }
        else {
            parseError(s - 1);
        }
        break;
    case 'C':
        s += 1;
        switch (*s) {
        case 'A':
            node->type = NodeType_Register;
            node->regster = RegisterType_CA;
            break;
        case 'E':
            node->type = NodeType_Register;
            node->regster = RegisterType_CE;
            break;
        case 'I':
            node->type = NodeType_Register;
            node->regster = RegisterType_CI;
            break;
        case 'L':
            node->type = NodeType_Register;
            node->regster = RegisterType_CL;
            break;
        default:
            parseError(s - 1);
            break;
        }
        break;
    case 'M':
        s += 1;
        if (*s == 'C') {
            node->type = NodeType_Register;
            node->regster = RegisterType_MC;
        }
        else {
            parseError(s - 1);
        }
        break;
    case 'P':
        s += 1;
        if (*s == 'S' || *s == 'V') {
            if (*(s + 1) == 'j') {
                node->type = NodeType_Register;
                node->regster = (*s == 'S') ? RegisterType_PS : RegisterType_PV;
                s += 1;
            }
            else {
                parseError(s - 1);
            }
        }
        else {
            parseError(s - 1);
        }
        break;
    case 'Q':
        s += 1;
        if (*s == 'S' || *s == 'V') {
            if (*(s + 1) == 'j') {
                node->type = NodeType_Register;
                node->regster = (*s == 'S') ? RegisterType_QS : RegisterType_QV;
                s += 1;
            }
            else {
                parseError(s - 1);
            }
        }
        else {
            parseError(s - 1);
        }
        break;
    case 'R':
        s += 1;
        if (*s == 'T') {
            node->type = NodeType_Register;
            node->regster = RegisterType_RT;
        }
        else {
            parseError(s - 1);
        }
        break;
    case 'S':
        s += 1;
        switch (*s) {
        case 'i':
        case 'j':
        case 'k':
            node->type = NodeType_Register;
            node->regster = RegisterType_S;
            break;
        case 'B':
            if (*(s + 1) == 'j') {
                node->type = NodeType_Register;
                node->regster = RegisterType_SB;
                s += 1;
            }
            else {
                node->type = NodeType_Register;
                node->regster = RegisterType_Sign;
            }
            break;
        case 'M':
            s += 1;
            if (*s == 'j' && *(s + 1) == 'k') {
                node->type = NodeType_Register;
                node->regster = RegisterType_SM;
                s += 1;
            }
            else {
                node->type = NodeType_Register;
                node->regster = RegisterType_Sem;
                s -= 1;
            }
            break;
        case 'R':
            s += 1;
            if (*s == 'j') {
                node->type = NodeType_Register;
                node->regster = RegisterType_SR;
            }
            else {
                parseError(s - 1);
            }
            break;
        case 'T':
            s += 1;
            if (*s == 'j') {
                node->type = NodeType_Register;
                node->regster = RegisterType_ST;
            }
            else {
                parseError(s - 1);
            }
            break;
        default:
            parseError(s - 1);
            break;
        }
        break;
    case 'T':
        s += 1;
        if (*s == 'j' && *(s + 1) == 'k') {
            node->type = NodeType_Register;
            node->regster = RegisterType_T;
            s += 1;
        }
        else {
            parseError(s - 1);
        }
        break;
    case 'V':
        s += 1;
        switch (*s) {
        case 'i':
        case 'j':
        case 'k':
            node->type = NodeType_Register;
            node->regster = RegisterType_V;
            break;
        case 'L':
            node->type = NodeType_Register;
            node->regster = RegisterType_VL;
            break;
        case 'M':
            node->type = NodeType_Register;
            node->regster = RegisterType_VM;
            break;
        default:
            parseError(s - 1);
            break;
        }
        break;
    case 'X':
        s += 1;
        if (*s == 'A') {
            node->type = NodeType_Register;
            node->regster = RegisterType_XA;
        }
        else {
            parseError(s - 1);
        }
        break;
    case 'Z':
        s += 1;
        if (*s == 'S' && *(s + 1) == 'j') {
            node->type = NodeType_Register;
            node->regster = RegisterType_ZS;
            s += 1;
        }
        else {
            parseError(s - 1);
        }
        break;
    case '+':
        node->type = NodeType_Operator;
        if (*(s + 1) == 'F') {
            node->operator = Op_FloatAdd;
            s += 1;
        }
        else {
            node->operator = Op_Add;
        }
        break;
    case '-':
        node->type = NodeType_Operator;
        if (*(s + 1) == 'F') {
            node->operator = Op_FloatSubtract;
            s += 1;
        }
        else {
            node->operator = Op_Subtract;
        }
        break;
    case '*':
        node->type = NodeType_Operator;
        switch (*(s + 1)) {
        case 'F':
            node->operator = Op_FloatMultiply;
            s += 1;
            break;
        case 'H':
            node->operator = Op_HalfMultiply;
            s += 1;
            break;
        case 'I':
            node->operator = Op_2_FloatMultiply;
            s += 1;
            break;
        case 'R':
            node->operator = Op_RoundedMultiply;
            s += 1;
            break;
        default:
            node->operator = Op_Multiply;
            break;
        }
        break;
    case '&':
        node->type = NodeType_Operator;
        node->operator = Op_And;
        break;
    case '!':
        node->type = NodeType_Operator;
        node->operator = Op_Or;
        break;
    case '/':
        node->type = NodeType_Operator;
        if (*(s + 1) == 'H') {
            node->operator = Op_HalfDivide;
            s += 1;
        }
        else {
            node->operator = Op_Divide;
        }
        break;
    case '\\':
        node->type = NodeType_Operator;
        node->operator = Op_Xor;
        break;
    case '#':
        node->type = NodeType_Operator;
        if (*(s + 1) == '<') {
            node->operator = Op_CmplMaskLeft;
            s += 1;
        }
        else if (*(s + 1) == '>') {
            node->operator = Op_CmplMaskRight;
            s += 1;
        }
        else {
            node->operator = Op_Complement;
        }
        break;
    case '<':
        node->type = NodeType_Operator;
        node->operator = Op_ShiftLeft;
        break;
    case '>':
        node->type = NodeType_Operator;
        node->operator = Op_ShiftRight;
        break;
    case '$':
        node->type = NodeType_Expression;
        break;
    case ',':
        node->type = NodeType_SubfieldDelimiter;
        break;
    case ' ':
        node->type = NodeType_FieldDelimiter;
        break;
    case '\0':
        node->type = NodeType_PatternEnd;
        s -= 1;
        break;
    default:
        parseError(s);
        break;
    }
    return s + 1;
}

static int popBase(void) {
    return (baseStackPtr > 0) ? baseStack[--baseStackPtr] : 10;
}

ErrorCode processMachineInstruction() {
    bool didMatchResultField;
    ErrorCode err;
    InstructionHandler handler;
    int i;

    err = Err_None;
    if (locationFieldToken != NULL) {
        err = registerError(addLocationSymbol(currentSection, locationFieldToken->details.name.ptr,
                                              locationFieldToken->details.name.len, SYM_PARCEL_ADDRESS));
    }
    handler = matchInstruction(&didMatchResultField);
    if (handler != NULL) {
        err = (*handler)();
    }
    else {
        err = didMatchResultField ? Err_OperandField : Err_ResultField;
    }
    for (i = 0; i < instArgc; i++) freeToken(instArgv[i]);
    return err;
}

static ErrorCode pushBase(int base) {
    ErrorCode err;

    if (baseStackPtr < BASE_STACK_SIZE) {
        baseStack[baseStackPtr++] = base;
        err = Err_None;
    }
    else {
        err = Err_TooManyEntries;
    }
    return err;
}

static int savedBase;

static void restoreBase(void) {
    currentBase = savedBase;
}

static void setBase(void) {
    savedBase = currentBase;
    if (currentBase == 0) currentBase = 10;
}

static void skipLines(Token *locationFieldToken, int count) {
    Token *condToken;
    char *s;
    Token token;

    listErrorIndications();
    if (locationFieldToken == NULL) {
        while (count > 0 && isEof() == FALSE) {
            listFlush(currentSection);
            readNextLine();
            listSource();
            if (sourceLine[0] != '*') count -= 1;
        }
    }
    else {
        condToken = copyToken(locationFieldToken);
        while (isEof() == FALSE) {
            listFlush(currentSection);
            readNextLine();
            listSource();
            s = sourceLine;
            if (isNameChar1(*s) || isNameChar1(*(s + 1))) {
                if (*s == ' ') s += 1;
                s = getNextToken(s, &token);
                if (isUnqualifiedName(&token)
                    && *s == ' '
                    && condToken->details.name.len == token.details.name.len
                    && strncmp(condToken->details.name.ptr, token.details.name.ptr, token.details.name.len) == 0)
                    break;
            }
        }
        freeToken(condToken);
    }
}
