/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: parse.c
**
**  Description:
**      This file provides parsing functions.
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
#include "calconst.h"
#include "calproto.h"
#include "caltypes.h"
#include "fnv.h"
#include "services.h"

typedef struct opStackEntry {
    OperatorType type;
    u8 precedence;
} OpStackEntry;

typedef struct registerDefn {
    char *pattern;
    RegisterType type;
} RegisterDefn;

static ErrorCode evaluateExprHelper(Token *expression);
static char *evaluateMicro(char *s, int len);
static ErrorCode evaluateString(Token *token);
static ErrorCode evaluateSymbol(Token *token);
static ErrorCode executeOperator(OperatorType opType);
static MacroDefn *findMacroDefn(char *id, int len);
static int findStringEnd(int cursor);
static void getFields(void);
static int getNextField(int cursor, int *start);
static char *interpolateMicros(char *dst, int dstLen, char *src, int srcLen);
static bool isRegisterDesignator(char *s, int len, Token *token);
static bool isLocCtrDelimiter(char c);
static bool isQualDelimiter(char c);
static char *parseFloat(char *s, int base, Token *token);
static char *parseInteger(char *s, int base, i64 *value);
static char *parseNumber(char *s, int base, Token *token);
static char *parseString(char *s, Token *token);
static void popArg(Value *arg);
static void popOp(OpStackEntry *op);
static void pushArg(Value *arg);
static void pushOp(Token *token);
static void resetLocationField(void);
static void squishString(char *s, int len);

static char fields[(COLUMN_LIMIT+2)*3];

static char *operatorSymbols[] = { //  indexed by OperatorType
    "",
    "-",
    "+",
    "#",
    "/",
    ">",
    "#>",
    "<",
    "#<",
    "P.",
    "W.",
    "=",
    "+",
    "-",
    "*",
    "/",
    "+F",
    "-F",
    "*F",
    "*H",
    "*R",
    "*I",
    "/H",
    ">",
    "<",
    "&",
    "!",
    "\\"
};

static RegisterDefn registerDefns[] = {
    {"A#",   RegisterType_A},
    {"A.",   RegisterType_A},
    {"S#",   RegisterType_S},
    {"S.",   RegisterType_S},
    {"PS#",  RegisterType_PS},
    {"PS.",  RegisterType_PS},
    {"QS#",  RegisterType_QS},
    {"QS.",  RegisterType_QS},
    {"ZS#",  RegisterType_ZS},
    {"ZS.",  RegisterType_ZS},
    {"V#",   RegisterType_V},
    {"V.",   RegisterType_V},
    {"PV#",  RegisterType_PV},
    {"PV.",  RegisterType_PV},
    {"QV#",  RegisterType_QV},
    {"QV.",  RegisterType_QV},
    {"B##",  RegisterType_B},
    {"B#",   RegisterType_B},
    {"B.",   RegisterType_B},
    {"SB#",  RegisterType_SB},
    {"SB.",  RegisterType_SB},
    {"T##",  RegisterType_T},
    {"T#",   RegisterType_T},
    {"T.",   RegisterType_T},
    {"SR#",  RegisterType_SR},
    {"SR.",  RegisterType_SR},
    {"ST#",  RegisterType_ST},
    {"ST.",  RegisterType_ST},
    {"SM##", RegisterType_SM},
    {"SM#",  RegisterType_SM},
    {"SM.",  RegisterType_SM},
    {"SB",   RegisterType_Sign},
    {"SM",   RegisterType_Sem},
    {"CA",   RegisterType_CA},
    {"CL",   RegisterType_CL},
    {"CE",   RegisterType_CE},
    {"CI",   RegisterType_CI},
    {"MC",   RegisterType_MC},
    {"RT",   RegisterType_RT},
    {"VL",   RegisterType_VL},
    {"VM",   RegisterType_VM},
    {"XA",   RegisterType_XA},
    {NULL,   0}
};

static char *registerNames[] = { // indexed by RegisterType
    "A",
    "B",
    "S",
    "PS",
    "QS",
    "ZS",
    "SB",
    "SM",
    "SR",
    "ST",
    "T",
    "V",
    "PV",
    "QV",
    "SM",
    "SB",
    "CA",
    "CE",
    "CI",
    "CL",
    "MC",
    "RT",
    "VL",
    "VM",
    "XA"
};

static char locCtrDelimiters[] = {
  '\0', ',', ')', '+', '-', '*', '/', '&', '!', '\\', '<', '>'
};

static char qualDelimiters[] = {
  ' ', ',', '(', '+', '-', '*', '/', '&', '!', '\\', '<', '>'
};

static Value argStack[ARG_STACK_SIZE];
static int argStackPtr = 0;
static OpStackEntry opStack[OP_STACK_SIZE];
static int opStackPtr = 0;

Token *copyToken(Token *token) {
    Token *new;

    if (token == NULL) return NULL;

    new = (Token *)allocate(sizeof(Token));
    memcpy(new, token, sizeof(Token));
    switch (token->type) {
    case TokenType_Register:
        if (token->details.regster.ptr != NULL) {
            new->details.regster.ptr = (char *)allocate(token->details.regster.len);
            memcpy(new->details.regster.ptr, token->details.regster.ptr, token->details.regster.len);
        }
        break;
    case TokenType_Name:
        if (token->details.name.ptr != NULL) {
            new->details.name.ptr = (char *)allocate(token->details.name.len);
            memcpy(new->details.name.ptr, token->details.name.ptr, token->details.name.len);
        }
        if (token->details.name.qualPtr != NULL) {
            new->details.name.qualPtr = (char *)allocate(token->details.name.qualLen);
            memcpy(new->details.name.qualPtr, token->details.name.qualPtr, token->details.name.qualLen);
        }
        break;
    case TokenType_String:
        if (token->details.string.ptr != NULL) {
            new->details.string.ptr = (char *)allocate(token->details.string.len);
            memcpy(new->details.string.ptr, token->details.string.ptr, token->details.string.len);
        }
        break;
    case TokenType_Operator:
        new->details.operator.leftArg = copyToken(token->details.operator.leftArg);
        new->details.operator.rightArg = copyToken(token->details.operator.rightArg);
        break;
    case TokenType_Number:
    case TokenType_None:
    case TokenType_Error:
        // nothing more to copy
        break;
    default:
        fprintf(stderr, "Invalid token type: %d\n", token->type);
        break;
    }
    return new;
}

bool equalTokens(Token *t1, Token *t2) {
    if (t1 == NULL)
        return t2 == NULL;
    else if (t2 == NULL)
        return FALSE;
    if (t1->type != t2->type) return FALSE;
    switch (t1->type) {
    case TokenType_Register:
        if (t1->details.operator.type == t2->details.operator.type) {
            if (t1->details.regster.ptr != NULL && t2->details.regster.ptr != NULL) {
                if (t1->details.regster.len == t2->details.regster.len)
                    return strncmp(t1->details.regster.ptr, t2->details.regster.ptr, t1->details.regster.len) == 0;
            }
            else if (t1->details.regster.ptr == NULL && t2->details.regster.ptr == NULL) {
                return t1->details.regster.ordinal == t2->details.regster.ordinal;
            }
        }
        break;
    case TokenType_Name:
        if (t1->details.name.ptr != NULL && t2->details.name.ptr != NULL
            && t1->details.name.len == t2->details.name.len)
            return strncasecmp(t1->details.name.ptr, t2->details.name.ptr, t1->details.name.len) == 0;
        break;
    case TokenType_String:
        if (t1->details.string.ptr != NULL && t2->details.string.ptr != NULL
            && t1->details.string.len == t2->details.string.len
            && t1->details.string.count == t2->details.string.count
            && t1->details.string.justification == t2->details.string.justification)
            return strncasecmp(t1->details.name.ptr, t2->details.name.ptr, t1->details.name.len) == 0;
        break;
    case TokenType_Operator:
        return t1->details.operator.type == t2->details.operator.type
            && equalTokens(t1->details.operator.leftArg, t2->details.operator.leftArg)
            && equalTokens(t1->details.operator.rightArg, t2->details.operator.rightArg);
    case TokenType_Number:
        if (t1->details.number.type == t2->details.number.type) {
            if (t1->details.number.type == NumberType_Integer)
                return t1->details.number.intValue == t2->details.number.intValue;
            else
                return t1->details.number.floatValue == t2->details.number.floatValue;
        }
        break;
    case TokenType_None:
        return TRUE;
    case TokenType_Error:
        return t1->details.error.code == t2->details.error.code;
    default:
        fprintf(stderr, "Invalid token type: %d\n", t1->type);
        break;
    }
    return FALSE;
}

ErrorCode evaluateExpression(Token *expression, Value *value) {
    ErrorCode err;
    Section *relocationSection;
    Section *section;

    argStackPtr = 0;
    opStackPtr = 0;
    for (section = currentModule->firstSection; section != NULL; section = section->next) {
         section->relocationCoefficient = section->immobileCoefficient = 0;
    }
    err = evaluateExprHelper(expression);
    if (err == Err_None && argStackPtr == 1 && opStackPtr == 0) {
        popArg(value);
        if (isRelative(value)) {
            if (isImmobile(value))
                value->section->immobileCoefficient += value->coefficient;
            else
                value->section->relocationCoefficient += value->coefficient;
        }
        relocationSection = NULL;
        for (section = currentModule->firstSection; section != NULL; section = section->next) {
             if (section->relocationCoefficient == 1 && relocationSection == NULL) {
                 relocationSection = section;
             }
             else if (section->relocationCoefficient != 0) {
                 err = Err_RelocatableField;
             }
             if (section->immobileCoefficient == 1 && relocationSection == NULL) {
                 relocationSection = section;
             }
             else if (section->immobileCoefficient != 0) {
                 err = Err_RelocatableField;
             }
        }
        if (relocationSection != NULL) {
            if (isExternal(value))
                err = Err_RelocatableField;
            else
                value->section = relocationSection;
        }
    }
    else if (err == Err_Undefined) {
        value->attributes = SYM_UNDEFINED;
        value->section = NULL;
        value->intValue = 0;
    }
    else {
        value->attributes = 0;
        value->section = NULL;
        value->intValue = 0;
        if (err == Err_None) err = Err_Expression;
    }
    return err;
}

static ErrorCode evaluateExprHelper(Token *expression) {
    ErrorCode err;
    Literal *literal;
    Section *literalsSection;
    OpStackEntry op;
    Token *rightArg;
    Value val;

    err = Err_None;
    switch (expression->type) {
    case TokenType_Name:
        err = evaluateSymbol(expression);
        break;
    case TokenType_Number:
        val.type = expression->details.number.type;
        val.attributes = 0;
        val.section = NULL;
        val.coefficient = 0;
        val.intValue = expression->details.number.intValue;
        pushArg(&val);
        break;
    case TokenType_String:
        err = evaluateString(expression);
        break;
    case TokenType_Operator:
        if (expression->details.operator.type == Op_SubExpr) {
            pushOp(expression);
            err = evaluateExprHelper(expression->details.operator.rightArg);
            opStackPtr -= 1;
            return err;
        }
        if (expression->details.operator.leftArg != NULL) {
            err = evaluateExprHelper(expression->details.operator.leftArg);
            if (err > Err_None && err < Warn_Programmer) return err;
            while (opStackPtr > 0
                   && opStack[opStackPtr - 1].type != Op_SubExpr
                   && expression->details.operator.precedence >= opStack[opStackPtr - 1].precedence) {
                popOp(&op);
                err = executeOperator(op.type);
                if (err != Err_None && err < Warn_Programmer) return err;
            }
        }
        pushOp(expression);
        if (expression->details.operator.type == Op_Literal) {
            if (expression->details.operator.rightArg->type != TokenType_String) {
                err = evaluateExprHelper(expression->details.operator.rightArg);
                while (opStackPtr > 0
                       && opStack[opStackPtr - 1].type != Op_Literal
                       && expression->details.operator.precedence >= opStack[opStackPtr - 1].precedence) {
                    popOp(&op);
                    err = registerError(executeOperator(op.type));
                }
                popArg(&val);
            }
            literal = addLiteral(expression->details.operator.rightArg);
            val.type = NumberType_Integer;
            val.attributes = SYM_WORD_ADDRESS|SYM_LITERAL;
            val.section = literalsSection = currentModule->firstSection->next; // 2nd section is always literals section
            if (pass == 1 || currentModule->isAbsolute == FALSE) {
                val.attributes |= SYM_RELOCATABLE;
                val.coefficient = 1;
            }
            else {
                val.coefficient = 0;
            }
            val.intValue = (literalsSection->originOffset + literal->offset) >> 2;
            pushArg(&val);
            opStackPtr -= 1;
            return err;
        }
        else {
            err = evaluateExprHelper(expression->details.operator.rightArg);
            if (err != Err_None && err < Warn_Programmer) return err;
        }
        while (opStackPtr > 0
               && opStack[opStackPtr - 1].type != Op_SubExpr
               && opStack[opStackPtr - 1].type != Op_Literal) {
            popOp(&op);
            err = executeOperator(op.type);
            if (err != Err_None && err < Warn_Programmer) break;
        }
        break;
    default:
        err = Err_Expression;
        break;
    }
    return err;
}

static char *evaluateMicro(char *s, int len) {
    Name *name;

    name = findName(currentModule->micros, s, len);
    if (name == NULL) name = findName(defaultModule->micros, s, len);
    if (name != NULL) return name->value;
    if (len == 4) {
        if (strncasecmp("$APP", s, len) == 0) {
            return "^";
        }
        else if (strncasecmp("$CNC", s, len) == 0) {
            return "_";
        }
        else if (strncasecmp("$CPU", s, len) == 0) {
            return "CRAY XMP";
        }
        else if (strncasecmp("$MIC", s, len) == 0) {
            return "\"";
        }
    }
    else if (len == 5) {
        if (strncasecmp("$CMNT", s, len) == 0) {
            return ";";
        }
        else if (strncasecmp("$DATE", s, len) == 0) {
            return currentDate;
        }
        else if (strncasecmp("$TIME", s, len) == 0) {
            return currentTime;
        }
        else if (strncasecmp("$QUAL", s, len) == 0) {
            return currentQualifier->id;
        }
    }
    else if (len == 6 && strncasecmp("$JDATE", s, len) == 0) {
        return currentJDate;
    }
    return "";
}

static ErrorCode evaluateString(Token *token) {
    ErrorCode err;
    u8 fill;
    int n;
    char *limit;
    char *s;
    Value val;

    val.type = NumberType_Integer;
    val.attributes = 0;
    val.section = NULL;
    val.coefficient = 0;
    val.intValue = 0;
    s = token->details.string.ptr;
    limit = s + token->details.string.len;
    n = 0;
    if (token->details.string.justification == Justify_RightZeroFill) {
        while (s < limit && n++ < 8) {
            if (*s == '\'') s += 1;
            val.intValue <<= 8;
            val.intValue |= (u8)*s++;
        }
    }
    else {
        fill = (token->details.string.justification == Justify_LeftBlankFill) ? 0x20 : 0;
        while (n++ < 8) {
            val.intValue <<= 8;
            if (s < limit && *s == '\'') s += 1;
            val.intValue |= (s < limit) ? (u8)*s++ : fill;
        }
    }
    pushArg(&val);
    return Err_None;
}

static ErrorCode evaluateSymbol(Token *token) {
    ErrorCode err;
    bool isLocCtr;
    Qualifier *qualifier;
    Symbol *symbol;
    Value val;

    err = Err_None;
    symbol = findQualifiedSymbol(token);
    if (symbol != NULL) {
        val.type = symbol->value.type;
        if ((symbol->value.attributes & SYM_COUNTER) != 0) {
            val.attributes = SYM_PARCEL_ADDRESS;
            val.section = currentSection;
            val.externalSymbol = NULL;
            isLocCtr = strcmp(symbol->id, "*") == 0;
            if (isLocCtr || strcasecmp(symbol->id, "*O") == 0) {
                val.intValue = isLocCtr ? currentSection->locationCounter : currentSection->originCounter;
                switch (currentSection->type) {
                case SectionType_Mixed:
                case SectionType_Code:
                case SectionType_Data:
                    if (currentSection->module->isAbsolute) break;
                    // fall through
                case SectionType_Common:
                case SectionType_Dynamic:
                    val.attributes |= SYM_RELOCATABLE;
                    break;
                case SectionType_Stack:
                case SectionType_TaskCom:
                    val.attributes |= SYM_IMMOBILE;
                default:
                    fprintf(stderr, "Unknown section type: %d\n", currentSection->type);
                    exit(1);
                }
            }
            else if (strcasecmp(symbol->id, "*A") == 0) {
                val.intValue = currentSection->locationCounter;
            }
            else if (strcasecmp(symbol->id, "*B") == 0) {
                val.intValue = currentSection->originCounter;
            }
            else if (strcasecmp(symbol->id, "*P") == 0) {
                val.attributes = 0;
                val.intValue = currentSection->parcelBitPosCounter;
            }
            else if (strcasecmp(symbol->id, "*W") == 0) {
                val.attributes = 0;
                val.intValue = currentSection->wordBitPosCounter;
            }
            else {
                val.attributes |= SYM_UNDEFINED;
                val.intValue = 0;
                err = Err_Expression;
            }
        }
        else {
            val.attributes = symbol->value.attributes;
            val.section = symbol->value.section;
            val.intValue = symbol->value.intValue;
            if ((val.attributes & SYM_EXTERNAL)  != 0) val.externalSymbol = symbol;
            if ((val.attributes & SYM_UNDEFINED) != 0) err = Err_Undefined;
        }
    }
    else if (pass == 2 && isImplicitExternals && isUnqualifiedName(token)) {
        qualifier = findQualifier("");
        val.type = NumberType_Integer;
        val.attributes = SYM_EXTERNAL|SYM_DEFINED_P2;
        val.section = NULL;
        val.intValue = 0;
        symbol = addSymbol(token->details.name.ptr, token->details.name.len, qualifier, &val);
        val.externalSymbol = symbol;
        addExternal(currentModule, symbol);
    }
    else {
        val.type = NumberType_Integer;
        val.attributes = SYM_UNDEFINED;
        val.section = NULL;
        val.intValue = 0;
        err = Err_Undefined;
    }
    val.coefficient = (val.attributes & (SYM_RELOCATABLE|SYM_IMMOBILE)) == 0 ? 0 : 1;
    pushArg(&val);
    return err;
}

static ErrorCode executeOperator(OperatorType opType) {
    ErrorCode err;
    Token expression;
    ErrorCode ignore;
    Value leftArg;
    Literal *literal;
    Value rightArg;

    if (argStackPtr < 1) return Err_Expression;
    err = Err_None;
    popArg(&rightArg);
    if (rightArg.type != NumberType_Integer) {
        if (opType == Op_Negate && rightArg.type == NumberType_Float) {
            rightArg.floatValue = -rightArg.floatValue;
            rightArg.coefficient = -rightArg.coefficient;
            pushArg(&rightArg);
            return err;
        }
        else {
            ignore = registerError(Warn_ExpressionElement);
        }
    }
    switch (opType) {
    case Op_Negate:
        rightArg.intValue = -rightArg.intValue;
        rightArg.coefficient = -rightArg.coefficient;
        pushArg(&rightArg);
        break;
    case Op_Plus:
        pushArg(&rightArg);
        break;
    case Op_Complement:
        if (isAbsolute(&rightArg) == FALSE) ignore = registerError(Warn_ExpressionElement);
        rightArg.intValue = ~rightArg.intValue;
        pushArg(&rightArg);
        break;
    case Op_Byte:
        if (isParcelAddress(&rightArg)) {
            rightArg.intValue *= 2;
            rightArg.attributes &= ~SYM_PARCEL_ADDRESS;
        }
        else if (isWordAddress(&rightArg)) {
            rightArg.intValue *= 8;
            rightArg.attributes &= ~SYM_WORD_ADDRESS;
        }
        rightArg.attributes |= SYM_BYTE_ADDRESS;
        if (rightArg.section == NULL) rightArg.section = currentSection;
        pushArg(&rightArg);
        break;
    case Op_Parcel:
        if (isWordAddress(&rightArg)) {
            rightArg.intValue *= 4;
            rightArg.attributes &= ~SYM_WORD_ADDRESS;
        }
        else if (isByteAddress(&rightArg)) {
            if ((rightArg.intValue & 0x01) != 0) ignore = registerError(Warn_ExpressionElement);
            rightArg.intValue /= 2;
            rightArg.attributes &= ~SYM_BYTE_ADDRESS;
        }
        rightArg.attributes |= SYM_PARCEL_ADDRESS;
        if (rightArg.section == NULL) rightArg.section = currentSection;
        pushArg(&rightArg);
        break;
    case Op_Word:
        if (isParcelAddress(&rightArg)) {
            if ((rightArg.intValue & 0x03) != 0) ignore = registerError(Warn_ExpressionElement);
            rightArg.intValue /= 4;
            rightArg.attributes &= ~SYM_PARCEL_ADDRESS;
        }
        else if (isByteAddress(&rightArg)) {
            if ((rightArg.intValue & 0x07) != 0) ignore = registerError(Warn_ExpressionElement);
            rightArg.intValue /= 8;
            rightArg.attributes &= ~SYM_BYTE_ADDRESS;
        }
        rightArg.attributes |= SYM_WORD_ADDRESS;
        if (rightArg.section == NULL) rightArg.section = currentSection;
        pushArg(&rightArg);
        break;
    case Op_Add:
        if (argStackPtr < 1) return Err_Expression;
        popArg(&leftArg);
        if (leftArg.type != NumberType_Integer)
            ignore = registerError(Warn_ExpressionElement);
        if (isExternal(&leftArg) && isExternal(&rightArg))
            err = registerError(Err_RelocatableField);
        if (isRelative(&rightArg)) {
            if (isImmobile(&rightArg))
                rightArg.section->immobileCoefficient += rightArg.coefficient;
            else
                rightArg.section->relocationCoefficient += rightArg.coefficient;
        }
        else if (isExternal(&rightArg)) {
            leftArg.attributes |= SYM_EXTERNAL;
            leftArg.externalSymbol = rightArg.externalSymbol;
        }
        if (getValueType(&leftArg) == getValueType(&rightArg)) {
            leftArg.intValue = leftArg.intValue + rightArg.intValue;
        }
        else if (isPlainValue(&leftArg)) {
            leftArg.intValue = leftArg.intValue + rightArg.intValue;
            leftArg.attributes = (leftArg.attributes & ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS))
                               | (rightArg.attributes & (SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS));
        }
        else if (isWordAddress(&leftArg)) {
            if (isPlainValue(&rightArg)) {
                leftArg.intValue = leftArg.intValue + rightArg.intValue;
            }
            else if (isParcelAddress(&rightArg)) {
                leftArg.intValue = (leftArg.intValue * 4) + rightArg.intValue;
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
            else {
                leftArg.intValue = (leftArg.intValue * 8) + rightArg.intValue;
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        else if (isParcelAddress(&leftArg)) { // left is parcel type
            if (isPlainValue(&rightArg)) {
                leftArg.intValue = leftArg.intValue + rightArg.intValue;
            }
            else if (isWordAddress(&rightArg)) {
                leftArg.intValue = leftArg.intValue + (rightArg.intValue * 4);
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
            else {
                leftArg.intValue = (leftArg.intValue * 2) + rightArg.intValue;
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        else { // left is byte type
            if (isPlainValue(&rightArg)) {
                leftArg.intValue = leftArg.intValue + rightArg.intValue;
            }
            else if (isWordAddress(&rightArg)) {
                leftArg.intValue = leftArg.intValue + (rightArg.intValue * 8);
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
            else {
                leftArg.intValue = leftArg.intValue + (rightArg.intValue * 2);
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        if (leftArg.section == NULL && isExternal(&leftArg) == FALSE)
            leftArg.section = rightArg.section;
        pushArg(&leftArg);
        break;
    case Op_Subtract:
        if (argStackPtr < 1) return Err_Expression;
        popArg(&leftArg);
        if (leftArg.type != NumberType_Integer) ignore = registerError(Warn_ExpressionElement);
        if (isExternal(&leftArg) && isExternal(&rightArg))
            err = registerError(Err_RelocatableField);
        if (isRelative(&rightArg)) {
            if (isImmobile(&rightArg))
                rightArg.section->immobileCoefficient -= rightArg.coefficient;
            else
                rightArg.section->relocationCoefficient -= rightArg.coefficient;
        }
        else if (isExternal(&rightArg)) {
            leftArg.attributes |= SYM_EXTERNAL;
            leftArg.externalSymbol = rightArg.externalSymbol;
        }
        if (getValueType(&leftArg) == getValueType(&rightArg)) {
            leftArg.intValue = leftArg.intValue - rightArg.intValue;
        }
        else if (isPlainValue(&leftArg)) {
            leftArg.intValue = leftArg.intValue - rightArg.intValue;
            leftArg.attributes = (leftArg.attributes & ~(SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS))
                               | (rightArg.attributes & (SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS));
        }
        else if (isWordAddress(&leftArg)) {
            if (isPlainValue(&rightArg)) {
                leftArg.intValue = leftArg.intValue - rightArg.intValue;
            }
            else if (isParcelAddress(&rightArg)) {
                leftArg.intValue = (leftArg.intValue * 4) - rightArg.intValue;
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
            else {
                leftArg.intValue = (leftArg.intValue * 8) - rightArg.intValue;
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        else if (isParcelAddress(&leftArg)) { // left is parcel type
            if (isPlainValue(&rightArg)) {
                leftArg.intValue = leftArg.intValue - rightArg.intValue;
            }
            else if (isWordAddress(&rightArg)) {
                leftArg.intValue = leftArg.intValue - (rightArg.intValue * 4);
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
            else {
                leftArg.intValue = (leftArg.intValue * 2) - rightArg.intValue;
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        else { // left is byte type
            if (isPlainValue(&rightArg)) {
                leftArg.intValue = leftArg.intValue - rightArg.intValue;
            }
            else if (isWordAddress(&rightArg)) {
                leftArg.intValue = leftArg.intValue - (rightArg.intValue * 8);
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
            else {
                leftArg.intValue = leftArg.intValue - (rightArg.intValue * 2);
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        if (leftArg.section == NULL && isExternal(&leftArg) == FALSE)
            leftArg.section = rightArg.section;
        pushArg(&leftArg);
        break;
    case Op_Multiply:
        if (argStackPtr < 1) return Err_Expression;
        popArg(&leftArg);
        if (leftArg.type != NumberType_Integer) ignore = registerError(Warn_ExpressionElement);
        if (isExternal(&leftArg) == FALSE || isExternal(&rightArg) == FALSE) {
            if (isAbsolute(&leftArg)) {
                if (isAbsolute(&rightArg) == FALSE) {
                    leftArg.coefficient = leftArg.intValue;
                    leftArg.attributes |= rightArg.attributes & (SYM_RELOCATABLE|SYM_IMMOBILE|SYM_EXTERNAL);
                    leftArg.section = rightArg.section;
                }
            }
            else if (isAbsolute(&rightArg)) {
                if (isAbsolute(&leftArg) == FALSE) leftArg.coefficient = rightArg.intValue;
            }
            else { // both arguments are relocatable
                ignore = registerError(Warn_ExpressionElement);
            }
            if (isExternal(&rightArg)) {
                leftArg.attributes |= SYM_EXTERNAL;
                leftArg.externalSymbol = rightArg.externalSymbol;
            }
        }
        else { // both args are external
            err = registerError(Err_RelocatableField);
        }
        if (getValueType(&leftArg) == getValueType(&rightArg)) {
            leftArg.intValue = leftArg.intValue * rightArg.intValue;
            if (isPlainValue(&leftArg) == FALSE) {
                leftArg.attributes &= ~(SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        else if (isPlainValue(&leftArg)) {
            leftArg.intValue = leftArg.intValue * rightArg.intValue;
            leftArg.attributes = (leftArg.attributes & ~(SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS))
                               | (rightArg.attributes & (SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS));
        }
        else if (isWordAddress(&leftArg)) {
            if (isPlainValue(&rightArg)) {
                leftArg.intValue = leftArg.intValue * rightArg.intValue;
            }
            else if (isParcelAddress(&rightArg)) {
                leftArg.intValue = (leftArg.intValue * 4) * rightArg.intValue;
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
            else {
                leftArg.intValue = (leftArg.intValue * 8) * rightArg.intValue;
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        else if (isParcelAddress(&leftArg)) { // left is parcel type
            if (isPlainValue(&rightArg)) {
                leftArg.intValue = leftArg.intValue * rightArg.intValue;
            }
            else if (isWordAddress(&rightArg)) {
                leftArg.intValue = leftArg.intValue * (rightArg.intValue * 4);
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
            else {
                leftArg.intValue = (leftArg.intValue * 2) * rightArg.intValue;
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        else { // left is byte type
            if (isPlainValue(&rightArg)) {
                leftArg.intValue = leftArg.intValue * rightArg.intValue;
            }
            else if (isWordAddress(&rightArg)) {
                leftArg.intValue = leftArg.intValue * (rightArg.intValue * 8);
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
            else {
                leftArg.intValue = leftArg.intValue * (rightArg.intValue * 2);
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        pushArg(&leftArg);
        break;
    case Op_Divide:
        if (argStackPtr < 1) return Err_Expression;
        popArg(&leftArg);
        if (leftArg.type != NumberType_Integer) ignore = registerError(Warn_ExpressionElement);
        if ((leftArg.attributes & (SYM_RELOCATABLE|SYM_IMMOBILE|SYM_EXTERNAL)) != 0
            || (rightArg.attributes & (SYM_RELOCATABLE|SYM_IMMOBILE|SYM_EXTERNAL)) != 0)
            err = registerError(Err_RelocatableField);
        if (rightArg.intValue == 0) {
            pushArg(&rightArg);
            return Err_Expression;
        }
        if (getValueType(&leftArg) == getValueType(&rightArg)) {
            leftArg.intValue = leftArg.intValue / rightArg.intValue;
            leftArg.attributes &= ~(SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
        }
        else if (isPlainValue(&leftArg)) {
            leftArg.intValue = leftArg.intValue / rightArg.intValue;
            leftArg.attributes &= ~(SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
            ignore = registerError(Warn_ExpressionElement);
        }
        else if (isWordAddress(&leftArg)) {
            if (isPlainValue(&rightArg)) {
                leftArg.intValue = leftArg.intValue / rightArg.intValue;
            }
            else if (isParcelAddress(&rightArg)) {
                leftArg.intValue = (leftArg.intValue * 4) / rightArg.intValue;
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
            else {
                leftArg.intValue = (leftArg.intValue * 8) / rightArg.intValue;
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        else if (isParcelAddress(&leftArg)) { // left is parcel type
            if (isPlainValue(&rightArg)) {
                leftArg.intValue = leftArg.intValue / rightArg.intValue;
            }
            else if (isWordAddress(&rightArg)) {
                leftArg.intValue = leftArg.intValue / (rightArg.intValue * 4);
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
            else {
                leftArg.intValue = (leftArg.intValue * 2) / rightArg.intValue;
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        else { // left is byte type
            if (isPlainValue(&rightArg)) {
                leftArg.intValue = leftArg.intValue / rightArg.intValue;
            }
            else if (isWordAddress(&rightArg)) {
                leftArg.intValue = leftArg.intValue / (rightArg.intValue * 8);
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
            else {
                leftArg.intValue = leftArg.intValue / (rightArg.intValue * 2);
                leftArg.attributes &= ~(SYM_BYTE_ADDRESS|SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
                ignore = registerError(Warn_ExpressionElement);
            }
        }
        pushArg(&leftArg);
        break;
    default:
        err = Err_Expression;
    }
    return err;
}

static MacroDefn *findMacroDefn(char *id, int len) {
    Name *name;

    name = findName(currentModule->macros, id, len);
    if (name == NULL) name = findName(defaultModule->macros, id, len);
    return (name != NULL) ? (MacroDefn *)name->value : NULL;
}

static int findStringEnd(int cursor) {
    char c;

    while (cursor < COLUMN_LIMIT) {
        c = sourceLine[cursor];
        if (c == '\0') break;
        if (c == '\'') {
            cursor += 1;
            if (cursor >= COLUMN_LIMIT || sourceLine[cursor] != '\'') break;
            cursor += 1;
        }
        else {
            cursor += 1;
        }
    }
    return cursor;
}

void freeToken(Token *token) {
    if (token != NULL) {
        switch (token->type) {
        case TokenType_Register:
            if (token->details.regster.ptr != NULL) free(token->details.regster.ptr);
            break;
        case TokenType_Name:
            if (token->details.name.ptr != NULL) free(token->details.name.ptr);
            if (token->details.name.qualPtr != NULL) free(token->details.name.qualPtr);
            break;
        case TokenType_String:
            if (token->details.string.ptr != NULL) free(token->details.string.ptr);
            break;
        case TokenType_Operator:
            freeToken(token->details.operator.leftArg);
            freeToken(token->details.operator.rightArg);
            break;
        case TokenType_Number:
        case TokenType_None:
        case TokenType_Error:
            // nothing more to free
            break;
        default:
            fprintf(stderr, "Invalid token type: %d\n", token->type);
            break;
        }
        free(token);
    }
}

static void getFields(void) {
    char c;
    int cursor;
    int i;
    int len;
    int resultFieldEnd;
    char *s;
    int start;

    locationField = fields;
    *locationField++ = ' ';
    *locationField = '\0';
    resultField = locationField + COLUMN_LIMIT + 2;
    *resultField++ = ' ';
    *resultField = '\0';
    operandField = resultField + COLUMN_LIMIT + 2;
    *operandField++ = ' ';
    *operandField = '\0';
    if (sourceLine[0] == '*') return;
    cursor = 0;
    if (currentSourceFormat == SourceFormat_New) {
        while (cursor < COLUMN_LIMIT) {
            cursor = getNextField(cursor, &start);
            len = cursor - start;
            if (len <= 0) break;
            if (start < 1) {
                s = interpolateMicros(locationField, COLUMN_LIMIT, &sourceLine[start], len);
                *s = '\0';
            }
            else if (*resultField == '\0' && (*locationField != '\0' || start > 0)) {
                s = interpolateMicros(resultField, COLUMN_LIMIT, &sourceLine[start], len);
                *s = '\0';
                resultFieldEnd = (s - resultField) - 1;
            }
            else if (*operandField == '\0' && *resultField != '\0') {
                s = interpolateMicros(operandField, COLUMN_LIMIT, &sourceLine[start], len);
                *s = '\0';
                break;
            }
        }
    }
    else { // SourceFormat_Old
        while (cursor < COLUMN_LIMIT) {
            cursor = getNextField(cursor, &start);
            len = cursor - start;
            if (len <= 0) break;
            if (start <= 1) {
                s = interpolateMicros(locationField, COLUMN_LIMIT, &sourceLine[start], len);
                *s = '\0';
            }
            else if (resultField[0] == '\0' && (start < 34 || locationField[0] != '\0')) {
                s = interpolateMicros(resultField, COLUMN_LIMIT, &sourceLine[start], len);
                *s = '\0';
                resultFieldEnd = (s - resultField) - 1;
            }
            else if (operandField[0] == '\0' && resultField[0] != '\0' && (start < 34 || resultFieldEnd >= 34)) {
                s = interpolateMicros(operandField, COLUMN_LIMIT, &sourceLine[start], len);
                *s = '\0';
                break;
            }
        }
    }
}

static int getNextField(int cursor, int *start) {
    char c;

    while (cursor < COLUMN_LIMIT && sourceLine[cursor] == ' ') cursor += 1;
    *start = cursor;
    while (cursor < COLUMN_LIMIT) {
        c = sourceLine[cursor];
        if (c == '\0' || c == ' ' || (c == ';' && currentSourceFormat == SourceFormat_New)) break;
        if (c == '\'') {
            if (cursor > *start) {
                switch (sourceLine[cursor - 1]) {
                case 'D': // number prefixes
                case 'd':
                case 'O':
                case 'o':
                case 'X':
                case 'x':
                    cursor += 1;
                    break;
                default:
                    cursor = findStringEnd(cursor + 1);
                    break;
                }
            }
            else {
                cursor = findStringEnd(cursor + 1);
            }
        }
        else {
            cursor += 1;
        }
    }
    return cursor;
}

char *getNextToken(char *s, Token *token) {
    char c;
    int len;
    char *start;
    Token t2;

    memset(token, 0, sizeof(Token));
    if (*s == '\0' || *s == ',') return s;
    start = s;
    //
    //  Recognize names, register designators, and special counters W.* and W.*O
    //
    if (isNameChar1(*s)) {
        s += 1;
        while (isNameChar(*s)) s += 1;
        len = s - start;
        if (*s == '.') len += 1; // possible register designator with expression
        if (isRegisterDesignator(start, len, token)) {
            if (*s == '.') {
                s += 1;
                token->details.regster.ptr = s;
                while (isNameChar(*s)) s += 1;
                token->details.regster.len = s - token->details.regster.ptr;
            }
            else {
                token->details.regster.ptr = NULL;
            }
        }
        else {
            len = s - start;
            if (len == 1) {
                switch (*start) {
                case 'A':
                case 'a':
                    if (*s == '\'') {
                        return parseString(start + 1, token);
                    }
                    break;
                case 'D':
                case 'd':
                    if (*s == '\'') {
                        return parseNumber(start + 2, 10, token);
                    }
                    break;
                case 'X':
                case 'x':
                    if (*s == '\'') {
                        return parseNumber(start + 2, 16, token);
                    }
                    break;
                case 'O':
                case 'o':
                    if (*s == '\'') {
                        return parseNumber(start + 2, 8, token);
                    }
                    // fall through for possible "O."
                case 'P':
                case 'p':
                case 'W':
                case 'w':
                    if (*s == '.') {
                        token->type = TokenType_Operator;
                        if (*start == 'P' || *start == 'p') {
                            token->details.operator.type = Op_Parcel;
                            token->details.operator.precedence = PRECEDENCE_PARCEL;
                        }
                        else if (*start == 'W' || *start == 'w') {
                            token->details.operator.type = Op_Word;
                            token->details.operator.precedence = PRECEDENCE_WORD;
                        }
                        else {
                            token->details.operator.type = Op_Byte;
                            token->details.operator.precedence = PRECEDENCE_BYTE;
                        }
                        return s + 1;
                    }
                    break;
                default:
                    // do nothing and fall through
                    break;
                }
            }
            if (len <= MAX_NAME_LENGTH) {
                token->type = TokenType_Name;
                token->details.name.ptr = start;
                token->details.name.len = len;
                token->details.name.qualPtr = NULL;
                token->details.name.qualLen = 0;
            }
            else if (isFlexibleSyntax) {
                squishString(start, len);
                token->type = TokenType_Name;
                token->details.name.ptr = start;
                token->details.name.len = MAX_NAME_LENGTH;
                token->details.name.qualPtr = NULL;
                token->details.name.qualLen = 0;
            }
            else {
                token->type = TokenType_Error;
                token->details.error.code = Err_Syntax;
            }
        }
    }
    //
    //  Recognize numbers
    //
    else if ((*s >= '0' && *s <= '9')
             || (*s == '.' && *(s + 1) >= '0' && *(s + 1) <= '9')) {
        s = parseNumber(s, (currentBase == 0) ? 8 : currentBase, token);
        if (isFlexibleSyntax
            && token->type == TokenType_Number
            && token->details.number.type == NumberType_Integer
            && (*s == 'f' || *s == 'b')) {
            static char name[MAX_NAME_LENGTH+1];
            if (*s == 'b') {
                sprintf(name, "@%ld$%d", token->details.number.intValue, localSymbolCtrs[token->details.number.intValue]);
            }
            else {
                sprintf(name, "@%ld$%d", token->details.number.intValue, localSymbolCtrs[token->details.number.intValue] + 1);
            }
            token->type = TokenType_Name;
            token->details.name.ptr = name;
            token->details.name.len = strlen(name);
            token->details.name.qualPtr = NULL;
            token->details.name.qualLen = 0;
            s += 1;
        }
    }
    //
    //  Recognize strings
    //
    else if (*s == '\'') {
        s = parseString(s, token);
    }
    //
    //  Recognize syntax for special counters *, *A, *B, *O, *P, and *W,
    //  and also recognize syntax for floating point multiplication
    //  machine instruction register references.
    //
    else if (*s == '*') {
        // preset to the most likely token type
        token->type = TokenType_Operator;
        token->details.operator.type = Op_Multiply;
        token->details.operator.precedence = PRECEDENCE_MULTIPLY;
        s += 1;
        c = *s;
        if (isLocCtrDelimiter(c)) {
            token->type = TokenType_Name;
            token->details.name.ptr = s - 1;
            token->details.name.len = 1;
        }
        else {
            switch (c) {
            case 'A':
            case 'a':
            case 'B':
            case 'b':
            case 'O':
            case 'o':
            case 'P':
            case 'p':
            case 'W':
            case 'w':
                if (isNameChar(*(s + 1)) == FALSE) {
                    token->type = TokenType_Name;
                    token->details.name.ptr = s - 1;
                    token->details.name.len = 2;
                    s += 1;
                }
                break;
            case 'F':
                (void)getNextToken(s + 1, &t2);
                if (t2.type == TokenType_Register) {
                    token->details.operator.type = Op_FloatMultiply;
                    s += 1;
                }
                break;
            case 'H':
                (void)getNextToken(s + 1, &t2);
                if (t2.type == TokenType_Register) {
                    token->details.operator.type = Op_HalfMultiply;
                    s += 1;
                }
                break;
            case 'I':
                (void)getNextToken(s + 1, &t2);
                if (t2.type == TokenType_Register) {
                    token->details.operator.type = Op_2_FloatMultiply;
                    s += 1;
                }
                break;
            case 'R':
                (void)getNextToken(s + 1, &t2);
                if (t2.type == TokenType_Register) {
                    token->details.operator.type = Op_RoundedMultiply;
                    s += 1;
                }
                break;
            default:
                // do nothing
                break;
            }
        }
    }
    //
    //  Recognize operators
    //
    else {
        token->type = TokenType_Operator;
        switch (*s) {
        case '!':
            token->details.operator.type = Op_Or;
            token->details.operator.precedence = PRECEDENCE_OR;
            break;
        case '#':
            if (*(s + 1) == '<') {
                token->details.operator.type = Op_CmplMaskLeft;
                token->details.operator.precedence = PRECEDENCE_CMPL_MASK_LEFT;
                s += 1;
            }
            else if (*(s + 1) == '>') {
                token->details.operator.type = Op_CmplMaskRight;
                token->details.operator.precedence = PRECEDENCE_CMPL_MASK_RIGHT;
                s += 1;
            }
            else {
                token->details.operator.type = Op_Complement;
                token->details.operator.precedence = PRECEDENCE_COMPLEMENT;
            }
            break;
        case '&':
            token->details.operator.type = Op_And;
            token->details.operator.precedence = PRECEDENCE_AND;
            break;
        case '-':
            token->details.operator.type = Op_Subtract;
            token->details.operator.precedence = PRECEDENCE_SUBTRACT;
            if (*(s + 1) == 'F') {
                (void)getNextToken(s + 2, &t2);
                if (t2.type == TokenType_Register) {
                    token->details.operator.type = Op_FloatSubtract;
                    s += 1;
                }
            }
            break;
        case '+':
            token->details.operator.type = Op_Add;
            token->details.operator.precedence = PRECEDENCE_ADD;
            if (*(s + 1) == 'F') {
                (void)getNextToken(s + 2, &t2);
                if (t2.type == TokenType_Register) {
                    token->details.operator.type = Op_FloatAdd;
                    s += 1;
                }
            }
            break;
        case '/':
            //
            //  Possible qualified name
            //
            if (isQualDelimiter(*(s - 1)) && (isNameChar1(*(s + 1)) || *(s + 1) == '/')) {
                start = s;
                s += 1;
                token->details.name.qualPtr = s;
                while (isNameChar(*s)) s += 1;
                if (*s == '/' && isNameChar1(*(s + 1))) {
                    token->type = TokenType_Name;
                    token->details.name.qualLen = s - token->details.name.qualPtr;
                    s += 1;
                    token->details.name.ptr = s++;
                    while (isNameChar(*s)) s += 1;
                    token->details.name.len = s - token->details.name.ptr;
                    if (token->details.name.qualLen > MAX_NAME_LENGTH
                        || token->details.name.len  > MAX_NAME_LENGTH) {
                        token->type = TokenType_Error;
                        token->details.error.code = Err_Syntax;
                    }
                    return s;
                }
                //
                //  Not a qualified name, fall through and handle as divide operator
                //
                s = start;
            }
            token->details.operator.type = Op_Divide;
            token->details.operator.precedence = PRECEDENCE_DIVIDE;
            if (*(s + 1) == 'H') {
                (void)getNextToken(s + 2, &t2);
                if (t2.type == TokenType_Register) {
                    token->details.operator.type = Op_HalfDivide;
                    s += 1;
                }
            }
            break;
        case '\\':
            token->details.operator.type = Op_Xor;
            token->details.operator.precedence = PRECEDENCE_XOR;
            break;
        case '<':
            token->details.operator.type = Op_ShiftLeft;
            token->details.operator.precedence = PRECEDENCE_SHIFT_LEFT;
            break;
        case '>':
            token->details.operator.type = Op_ShiftRight;
            token->details.operator.precedence = PRECEDENCE_SHIFT_RIGHT;
            break;
        case '=':
            token->details.operator.type = Op_Literal;
            token->details.operator.precedence = PRECEDENCE_LITERAL;
            break;
        case '(':
            token->details.operator.type = Op_SubExpr;
            token->details.operator.precedence = PRECEDENCE_SUB_EXPR;
            break;
        default:
            token->type = TokenType_Error;
            token->details.error.code = registerError(Err_Syntax);
            break;
        }
        s += 1;
    }
    return s;
}

char *getNextValue(char *s, Value *value, ErrorCode *err) {
    Token *expression;

    s = parseExpression(s, &expression);
    switch (expression->type) {
    case TokenType_Name:
    case TokenType_Number:
    case TokenType_String:
    case TokenType_Operator:
        *err = evaluateExpression(expression, value);
        break;
    default:
        *err = Err_Expression;
        break;
    }
    freeToken(expression);
    return s;
}

ErrorCode getRegisterNumber(Token *regster, int *number) {
    ErrorCode err;
    char regExpr[MAX_SOURCE_LINE_LENGTH+1];
    int limit;
    char *s;
    Value val;

    *number = 0;
    switch (regster->details.regster.type) {
    case RegisterType_A:
    case RegisterType_S:
    case RegisterType_PS:
    case RegisterType_QS:
    case RegisterType_ZS:
    case RegisterType_SB:
    case RegisterType_SR:
    case RegisterType_ST:
    case RegisterType_V:
    case RegisterType_PV:
    case RegisterType_QV:
        limit = 8;
        break;
    case RegisterType_SM:
        limit = 32;
        break;
    case RegisterType_B:
    case RegisterType_T:
        limit = 64;
        break;
    default:
        return Err_None;
    }
    if (regster->details.regster.ptr != NULL) {
        memcpy(regExpr, regster->details.regster.ptr, regster->details.regster.len);
        regExpr[regster->details.regster.len] = '\0';
        s = getNextValue(regExpr, &val, &err);
        if (err != Err_None) return err;
        if (isParcelAddress(&val)
            || isWordAddress(&val)
            || isByteAddress(&val)
            || val.type != NumberType_Integer
            || val.intValue < 0
            || val.intValue >= limit) return Err_FieldWidth;
        *number = val.intValue;
    }
    else {
        *number = regster->details.regster.ordinal;
    }
    return Err_None;
}

u16 getValueType(Value *value) {
    return value->attributes & (SYM_PARCEL_ADDRESS|SYM_WORD_ADDRESS);
}

static char *interpolateMicros(char *dst, int dstLen, char *src, int srcLen) {
    char *dstLimit;
    char *micro;
    char *srcLimit;
    char *start;

    srcLimit = src + srcLen;
    dstLimit = dst + dstLen;
    while (src < srcLimit) {
        if (*src == '"') {
            src += 1;
            start = src;
            while (src < srcLimit && *src != '"') src += 1;
            if (src < srcLimit && *src == '"') {
                micro = evaluateMicro(start, src - start);
                while (*micro != '\0' && dst < dstLimit) *dst++ = *micro++;
            }
            else {
                if (dst < dstLimit) *dst++ = '"';
                src = start - 1;
            }
        }
        else if (*src != '_' && dst < dstLimit) {
            *dst++ = *src;
        }
        src += 1;
    }
    return dst;
}

static bool isLocCtrDelimiter(char c) {
    int i;

    for (i = 0; i < sizeof(locCtrDelimiters); i++) {
        if (locCtrDelimiters[i] == c) return TRUE;
    }
    return FALSE;
}

bool isNameChar(char c) {
    return isNameChar1(c) || (c >= '0' && c <= '9');
}

bool isNameChar1(char c) {
    return (c >= 'A' && c <= 'Z')
        || (c >= 'a' && c <= 'z')
        ||  c == '$'
        ||  c == '@'
        ||  c == '%';
}

static bool isQualDelimiter(char c) {
    int i;

    for (i = 0; i < sizeof(qualDelimiters); i++) {
        if (qualDelimiters[i] == c) return TRUE;
    }
    return FALSE;
}

static bool isRegisterDesignator(char *s, int len, Token *token) {
    char c;
    int i;
    char *limit;
    int ordinal;
    char *pp;
    RegisterDefn *rdp;
    char *sp;

    i = 0;
    limit = s + len;
    while (TRUE) {
        rdp = &registerDefns[i++];
        pp = rdp->pattern;
        if (pp == NULL) break;
        sp = s;
        ordinal = 0;
        while (sp < limit && *pp != '\0') {
            if (*pp == '#') {
                if (*sp < '0' || *sp > '7') break;
                ordinal = (ordinal * 8) + (*sp - '0');
            }
            else if (*pp != *sp) {
                if (*sp < 'a' || *sp > 'z') break;
                c = toupper(*sp);
                if (*pp != c) break;
            }
            pp += 1;
            sp += 1;
        }
        if (sp >= limit && *pp == '\0') { // match found
            token->type = TokenType_Register;
            token->details.regster.type = rdp->type;
            token->details.regster.ordinal = ordinal;
            return TRUE;
        }
    }
    return FALSE;
}

bool isUnqualifiedName(Token *token) {
    return token->type == TokenType_Name && token->details.name.qualPtr == NULL;
}

char *parseExpression(char *s, Token **expression) {
    Token *leftArg;
    Token *op;
    Token *rightArg;
    Token token;

    if (*s == '(') {
        s = parseExpression(s + 1, &rightArg);
        if (*s != ')') {
            freeToken(rightArg);
            token.type = TokenType_Error;
            token.details.error.code = Err_Expression;
            *expression = copyToken(&token);
            return s;
        }
        memset(&token, 0, sizeof(Token));
        token.type = TokenType_Operator;
        token.details.operator.type = Op_SubExpr;
        token.details.operator.rightArg = rightArg;
        leftArg = copyToken(&token);
        s += 1;
        if (*s == '\0' || *s == ',' || *s == ')') {
            *expression = leftArg;
            return s;
        }
    }
    else {
        leftArg = NULL;
    }
    s = getNextToken(s, &token);
    switch (token.type) {
    case TokenType_None:
        if (leftArg != NULL) {
            freeToken(leftArg);
            token.type = TokenType_Error;
            token.details.error.code = Err_Expression;
            *expression = copyToken(&token);
        }
        else {
            *expression = copyToken(&token);
        }
        break;
    case TokenType_Register:
    case TokenType_Name:
    case TokenType_Number:
    case TokenType_String:
        if (leftArg != NULL) {
            freeToken(leftArg);
            token.type = TokenType_Error;
            token.details.error.code = Err_Expression;
            *expression = copyToken(&token);
        }
        else if (*s == '\0' || *s == ',' || *s == ')') {
            *expression = copyToken(&token);
        }
        else {
            leftArg = copyToken(&token);
            s = getNextToken(s, &token);
            if (token.type == TokenType_Operator) {
                switch (token.details.operator.type) {
                case Op_Add:
                case Op_Subtract:
                case Op_Multiply:
                case Op_Divide:
                case Op_ShiftRight:
                case Op_ShiftLeft:
                case Op_And:
                case Op_Or:
                case Op_Xor:
                    op = copyToken(&token);
                    op->details.operator.leftArg = leftArg;
                    s = parseExpression(s, &rightArg);
                    switch (rightArg->type) {
                    case TokenType_Register:
                    case TokenType_Name:
                    case TokenType_Number:
                    case TokenType_String:
                    case TokenType_Operator:
                        op->details.operator.rightArg = rightArg;
                        *expression = op;
                        break;
                    default:
                        freeToken(rightArg);
                        freeToken(op);
                        token.type = TokenType_Error;
                        token.details.error.code = Err_Expression;
                        *expression = copyToken(&token);
                        break;
                    }
                    break;
                default:
                    freeToken(leftArg);
                    token.type = TokenType_Error;
                    token.details.error.code = Err_Expression;
                    *expression = copyToken(&token);
                    break;
                }
            }
            else {
                freeToken(leftArg);
                token.type = TokenType_Error;
                token.details.error.code = Err_Expression;
                *expression = copyToken(&token);
            }
        }
        break;
    case TokenType_Operator:
        if (leftArg == NULL) { // unary operator
            switch (token.details.operator.type) {
            case Op_Subtract:
                token.details.operator.type = Op_Negate;
                token.details.operator.precedence = PRECEDENCE_NEGATE;
                break;
            case Op_Add:
                token.details.operator.type = Op_Plus;
                token.details.operator.precedence = PRECEDENCE_PLUS;
                break;
            case Op_ShiftRight:
                token.details.operator.type = Op_MaskRight;
                token.details.operator.precedence = PRECEDENCE_MASK_RIGHT;
                break;
            case Op_ShiftLeft:
                token.details.operator.type = Op_MaskLeft;
                token.details.operator.precedence = PRECEDENCE_MASK_LEFT;
                break;
            default:
                // do nothing
                break;
            }
        }
        op = copyToken(&token);
        s = parseExpression(s, &rightArg);
        switch (rightArg->type) {
        case TokenType_Register:
        case TokenType_Name:
        case TokenType_Number:
        case TokenType_String:
        case TokenType_Operator:
            op->details.operator.leftArg  = leftArg;
            op->details.operator.rightArg = rightArg;
            *expression = op;
            break;
        default:
            if (leftArg != NULL) freeToken(leftArg);
            freeToken(rightArg);
            freeToken(op);
            token.type = TokenType_Error;
            token.details.error.code = Err_Expression;
            *expression = copyToken(&token);
            break;
        }
        break;
    case TokenType_Error:
        *expression = copyToken(&token);
        break;
    default:
        token.type = TokenType_Error;
        token.details.error.code = Err_Expression;
        *expression = copyToken(&token);
        break;
    }
    return s;
}

static char *parseFloat(char *s, int base, Token *token) {
    f64 divisor;
    f64 frac;
    bool isNegative;
    f64 val;
    i64 valE;
    i64 valS;

    val = 0.0;
    isNegative = FALSE;
    if (*s == '-') {
        isNegative = TRUE;
        s += 1;
    }
    else if (*s == '+') {
        s += 1;
    }
    /*
     *  Process whole number part
     */
    if (base == 10) {
        while (*s >= '0' && *s <= '9') {
            val = (val * 10.0) + (f64)(*s - '0');
            s += 1;
        }
    }
    else if (base == 8) {
        while (*s >= '0' && *s <= '7') {
            val = (val * 8.0) + (f64)(*s - '0');
            s += 1;
        }
    }
    else { // base == 16
        while (TRUE) {
            if (*s >= '0' && *s <= '9') {
                val = (val * 16.0) + (f64)(*s - '0');
            }
            else if ((*s == 'E' || *s == 'e') && (*(s + 1) == '+' || *(s + 1) == '-')) {
                break;
            }
            else if (*s >= 'A' && *s <= 'F') {
                val = (val * 16.0) + (f64)((*s - 'A') + 10);
            }
            else if (*s >= 'a' && *s <= 'f') {
                val = (val * 16.0) + (f64)((*s - 'a') + 10);
            }
            else {
                break;
            }
            s += 1;
        }
    }
    /*
     *  Process fraction part
     */
    if (*s == '.') {
        frac = 0.0;
        s += 1;
        if (base == 10) {
            divisor = 10.0;
            while (*s >= '0' && *s <= '9') {
                frac += (f64)(*s - '0') / divisor;
                divisor *= 10.0;
                s += 1;
            }
        }
        else if (base == 8) {
            divisor = 8.0;
            while (*s >= '0' && *s <= '7') {
                frac += (f64)(*s - '0') / divisor;
                divisor *= 8.0;
                s += 1;
            }
        }
        else { // base == 16
            divisor = 16.0;
            while (TRUE) {
                if (*s >= '0' && *s <= '9') {
                    frac += (f64)(*s - '0') / divisor;
                }
                else if ((*s == 'E' || *s == 'e') && (*(s + 1) == '+' || *(s + 1) == '-')) {
                    break;
                }
                else if (*s >= 'A' && *s <= 'F') {
                    frac += (f64)((*s - 'A') + 10) / divisor;
                }
                else if (*s >= 'a' && *s <= 'f') {
                    frac += (f64)((*s - 'a') + 10) / divisor;
                }
                else {
                    break;
                }
                divisor *= 16.0;
                s += 1;
            }
        }
        val += frac;
    }
    /*
     *  Process power of 10 indication
     */
    if ((*s == 'E' || *s == 'e')
        && (((*s + 1) >= '0' && (*s + 1) <= '9')
            || ((*(s + 1) == '+' || *(s + 1) == '-') && *(s + 2) >= '0' && *(s + 2) <= '9'))) {
        s = parseInteger(s + 1, base, &valE);
        while (valE > 0) {
            val *= 10.0;
            valE -= 1;
        }
        while (valE < 0) {
            val /= 10.0;
            valE += 1;
        }
    }
    /*
     *  Process power of 2 indication
     */
    if (*s == 'S'
        && (((*s + 1) >= '0' && (*s + 1) <= '9')
            || ((*(s + 1) == '+' || *(s + 1) == '-') && (*s + 2) >= '0' && (*s + 2) <= '9'))) {
        s = parseInteger(s + 1, base, &valS);
        while (valS > 0) {
            val *= 2.0;
            valE -= 1;
        }
        while (valS < 0) {
            val /= 2.0;
            valE += 1;
        }
    }
    token->type = TokenType_Number;
    token->details.number.type = NumberType_Float;
    token->details.number.floatValue = isNegative ? -val : val;
    return s;
}

static char *parseInteger(char *s, int base, i64 *value) {
    bool isNegative;
    i64 val;

    val = 0;
    isNegative = FALSE;
    if (*s == '-') {
        isNegative = TRUE;
        s += 1;
    }
    else if (*s == '+') {
        s += 1;
    }
    if (base == 10) {
        while (*s >= '0' && *s <= '9') {
            val = (val * 10) + (*s - '0');
            s += 1;
        }
    }
    else if (base == 8) {
        while (*s >= '0' && *s <= '7') {
            val = (val * 8) + (*s - '0');
            s += 1;
        }
    }
    else { // base == 16
        while (TRUE) {
            if (*s >= '0' && *s <= '9') {
                val = (val * 16) + (*s - '0');
            }
            else if ((*s == 'E' || *s == 'e') && (*(s + 1) == '+' || *(s + 1) == '-')) {
                break;
            }
            else if (*s >= 'A' && *s <= 'F') {
                val = (val * 16) + (*s - 'A') + 10;
            }
            else if (*s >= 'a' && *s <= 'f') {
                val = (val * 16) + (*s - 'a') + 10;
            }
            else {
                break;
            }
            s += 1;
        }
    }
    *value = isNegative ? -val : val;
    return s;
}

static char *parseNumber(char *s, int base, Token *token) {
    ErrorCode err;
    char *start;
    i64 shiftCount;
    i64 value;

    start = s;
    s = parseInteger(start, base, &value);
    if (*s == '.' || *s == 'E' || *s == 'e') return parseFloat(start, base, token);
    if (*s == 'S') {
        if ((*(s + 1) >= '0' && *(s + 1) <= '9')
            || ((*(s + 1) == '+' || *(s + 1) == '-') && *(s + 2) >= '0' && *(s + 2) <= '9')) {
            s = parseInteger(s + 1, base, &shiftCount);
            if (shiftCount >= 0)
                value <<= shiftCount;
            else
                value >>= -shiftCount;
        }
    }
    token->type = TokenType_Number;
    token->details.number.type = NumberType_Integer;
    token->details.number.intValue = value;
    return s;
}

/*
 *  parseSourceLine - parse a line of source text
 *
 *  This is the main processing function of the assembler. The line of source code
 *  to be processed is expected to be found in the global sourceLine array.
 */
ErrorCode parseSourceLine(void) {
    ErrorCode err;
    NamedInstruction *inst;
    int len;
    MacroDefn *macroDefn;
    char *s;
    Symbol *symbol;
    Token token;

    err = Err_None;
    resetLocationField();
    resetErrorRegistrations();
    listSource();
    if (sourceLine[0] == '*' || sourceLine[0] == '\0') {
        listFlush(currentSection);
        return err;
    }
    getFields();
    if (isFlexibleSyntax
        && (locationField[0] == '#'
            || (locationField[0] == '\0' && resultField[0] == '#'))) {
        listFlush(currentSection);
        return err;
    }
    //
    //  Process the location field. It may have at most one token,
    //  and the one token must be an unqualified name.
    //
    s = getNextToken(locationField, &token);
    switch (token.type) {
    case TokenType_Name:
        if (*s == '\0' && isUnqualifiedName(&token)) {
            locationFieldToken = copyToken(&token);
        }
        else {
            err = registerError(Err_LocationField);
        }
        break;
    case TokenType_None:
        // do nothing
        break;
    case TokenType_Error:
        err = registerError(token.details.error.code);
        break;
    default:
        err = registerError(Err_LocationField);
        break;
    }
    //
    //  Process the result field. First, check for a macro call, then
    //  check for a pseudo-instruction or a named machine instruction.
    //  if no matches are found for those, attempt to match other
    //  machine instruction patterns.
    //
    len = strlen(resultField);
    if (len > 0) {
        //
        //  Process macro calls
        //
        macroDefn = findMacroDefn(resultField, len);
        if (macroDefn != NULL) {
            listCodeLocation(currentSection);
            err = registerError(callMacro(macroDefn, locationFieldToken));
        }
        //
        //  Process pseudo-instructions and named machine instructions
        //
        else {
            inst = findInstruction(resultField, len);
            if (inst != NULL) {
                if ((inst->attributes & INST_MACHINE) != 0) {
                    if (currentModule->id[0] != '\0' && isCodeSection(currentSection)) {
                        if (locationFieldToken != NULL) {
                            err = registerError(addLocationSymbol(currentSection,
                                                                  locationFieldToken->details.name.ptr,
                                                                  locationFieldToken->details.name.len,
                                                                  SYM_PARCEL_ADDRESS));
                        }
                        err = registerError((*inst->handler)());
                    }
                    else {
                        err = registerError(Err_InstructionPlacement);
                    }
                }
                else { // pseudo-instruction
                    err = registerError((*inst->handler)());
                }
            }
            //
            //  Process other machine instruction forms
            //
            else if (currentModule->id[0] != '\0' && isCodeSection(currentSection)) {
                err = registerError(processMachineInstruction());
            }
            else {
                err = registerError(Err_InstructionPlacement);
            }
        }
    }
    listErrorIndications();
    listFlush(currentSection);
    return err;
}

static char *parseString(char *s, Token *token) {
    char *start;
    i64 count;
    int n;

    s += 1;
    start = s;
    n = 0;
    while (*s != '\0') {
        if (*s == '\'') {
            if (*(s + 1) != '\'') break;
            s += 1;
        }
        n += 1;
        s += 1;
    }
    if (*s == '\'') {
        token->type = TokenType_String;
        token->details.string.ptr = start;
        token->details.string.len = s - start;
        token->details.string.count = 0;
        s += 1;
        if (*s >= '0' && *s <= '9') {
            s = parseInteger(s, currentBase == 0 ? 10 : currentBase, &count);
            token->details.string.count = count;
        }
        else if (*s == '*') {
            token->details.string.count = n;
            s += 1;
            if (*s == 'Z' || *s == 'z') token->details.string.count += 1;
        }
        else {
            if (*s == 'Z' || *s == 'z') n += 1;
            token->details.string.count = (n + 7) & ~7;
        }
        switch (*s) {
        case 'H':
        case 'h':
            s += 1;
        default:
            token->details.string.justification = Justify_LeftBlankFill;
            break;
        case 'L':
        case 'l':
            token->details.string.justification = Justify_LeftZeroFill;
            s += 1;
            break;
        case 'R':
        case 'r':
            token->details.string.justification = Justify_RightZeroFill;
            s += 1;
            break;
        case 'Z':
        case 'z':
            token->details.string.justification = Justify_LeftZeroEnd;
            s += 1;
            break;
        }
    }
    else {
        token->type = TokenType_Error;
        token->details.error.code = registerError(Err_Syntax);
    }
    return s;
}

void printToken(FILE *file, Token *token) {
    if (file == NULL) return;
    switch (token->type) {
    case TokenType_Register:
        fputs(registerNames[token->details.regster.type], file);
        if (token->details.regster.type < RegisterType_Sem) {
            if (token->details.regster.ptr != NULL) {
                fprintf(file, "%.*s", token->details.regster.len, token->details.regster.ptr);
            }
            else {
                fprintf(file, "%d", token->details.regster.ordinal);
            }
        }
        break;
    case TokenType_Name:
        if (token->details.name.ptr != NULL) {
            fprintf(file, "%.*s", token->details.name.len, token->details.name.ptr);
        }
        else {
            fputs("[[null name]]", file);
        }
        break;
    case TokenType_Number:
        fprintf(file, "%lo", token->details.number.intValue);
        break;
    case TokenType_String:
        if (token->details.string.ptr != NULL) {
            fprintf(file, "'%.*s'", token->details.string.len, token->details.string.ptr);
            if (token->details.string.count != 0) fprintf(file, "%d", token->details.string.count);
            switch (token->details.string.justification) {
            case Justify_LeftBlankFill:
            default:
                break;
            case Justify_LeftZeroFill:
                fputs("L", file);
                break;
            case Justify_RightZeroFill:
                fputs("R", file);
                break;
            case Justify_LeftZeroEnd:
                fputs("Z", file);
                break;
            }
        }
        else {
            fputs("[[null string]]", file);
        }
        break;
    case TokenType_Operator:
        if (token->details.operator.type == Op_SubExpr) {
            fputc('(', file);
            printToken(file, token->details.operator.rightArg);
            fputc(')', file);
        }
        else {
            if (token->details.operator.leftArg != NULL)
                printToken(file, token->details.operator.leftArg);
            fprintf(file, "%s", operatorSymbols[token->details.operator.type]);
            if (token->details.operator.rightArg != NULL)
                printToken(file, token->details.operator.rightArg);
        }
        break;
    case TokenType_None:
        break;
    case TokenType_Error:
        fprintf(file, "{{ %s }}", getErrorMessage(token->details.error.code));
        break;
    default:
        fprintf(file, "??%d??", token->type);
        break;
    }
}

static void popArg(Value *arg) {
    Value *stackEntry;

    stackEntry = &argStack[--argStackPtr];
    memcpy(arg, stackEntry, sizeof(Value));
}

static void popOp(OpStackEntry *op) {
    OpStackEntry *stackEntry;

    stackEntry = &opStack[--opStackPtr];
    memcpy(op, stackEntry, sizeof(OpStackEntry));
}

static void pushArg(Value *arg) {
    Value *stackEntry;

    stackEntry = &argStack[argStackPtr++];
    memcpy(stackEntry, arg, sizeof(Value));
}

static void pushOp(Token *token) {
    OpStackEntry *stackEntry;

    stackEntry = &opStack[opStackPtr++];
    stackEntry->type = token->details.operator.type;
    stackEntry->precedence = token->details.operator.precedence;
}

static void resetLocationField(void) {
    if (locationFieldToken != NULL) {
        freeToken(locationFieldToken);
        locationFieldToken = NULL;
    }
}

static void squishString(char *s, int len) {
    Fnv32_t hash;

    hash = fnv32a(s, len, FNV1_32A_INIT);
    sprintf(s + 4, "%04x", hash & 0xffff);
}
