/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: lex.c
**
**  Description:
**      This file implements lexical analysis and tokenization.
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
#include "const.h"
#include "proto.h"
#include "types.h"

static char *getFloat(char *s, Token *token);
static char *getInteger(char *s, i64 *value);
static char *getLogicalOp(char *s, Token *token);
static char *getNumber(char *s, Token *token);
static char *getString(char *s, Token *token);
static char *matchKeyword(char *s, Token *token);
static char *setInvalidToken(char *s, Token *token);

static Keyword keywordTable[] = {
    {"ASSIGN", ASSIGN, StmtClass_Executable},
    {"BACKSPACE", BACKSPACE, StmtClass_Executable},
    {"BLOCKDATA", BLOCKDATA, StmtClass_Nonexecutable},
    {"CALL", CALL, StmtClass_Executable},
    {"CHARACTER", CHARACTER, StmtClass_Nonexecutable},
    {"CLOSE", CLOSE, StmtClass_Executable},
    {"COMMON", COMMON, StmtClass_Nonexecutable},
    {"COMPLEX", COMPLEX, StmtClass_Nonexecutable},
    {"CONTINUE", CONTINUE, StmtClass_Executable},
    {"DATA", DATA, StmtClass_Nonexecutable},
    {"DIMENSION", DIMENSION, StmtClass_Nonexecutable},
    {"DO", DO, StmtClass_Executable},
    {"DOUBLEPRECISION", DOUBLEPRECISION, StmtClass_Nonexecutable},
    {"ELSE", ELSE, StmtClass_Executable},
    {"ELSEIF", ELSEIF, StmtClass_Executable},
    {"END", END, StmtClass_Nonexecutable},
    {"ENDFILE", ENDFILE, StmtClass_Executable},
    {"ENDIF", ENDIF, StmtClass_Executable},
    {"ENTRY", ENTRY, StmtClass_Nonexecutable},
    {"EQUIVALENCE", EQUIVALENCE, StmtClass_Nonexecutable},
    {"EXTERNAL", EXTERNAL, StmtClass_Nonexecutable},
    {"FORMAT", FORMAT, StmtClass_Format},
    {"FUNCTION", FUNCTION, StmtClass_Nonexecutable},
    {"GOTO", GOTO, StmtClass_Executable},
    {"IF", IF, StmtClass_Executable},
    {"IMPLICIT", IMPLICIT, StmtClass_Nonexecutable},
    {"IMPLICITNONE", IMPLICITNONE, StmtClass_Nonexecutable},
    {"INCLUDE", INCLUDE, StmtClass_Nonexecutable},
    {"INQUIRE", INQUIRE, StmtClass_Executable},
    {"INTEGER", INTEGER, StmtClass_Nonexecutable},
    {"INTRINSIC", INTRINSIC, StmtClass_Nonexecutable},
    {"LOGICAL", LOGICAL, StmtClass_Nonexecutable},
    {"OPEN", OPEN, StmtClass_Executable},
    {"PARAMETER", PARAMETER, StmtClass_Nonexecutable},
    {"PAUSE", PAUSE, StmtClass_Executable},
    {"POINTER", POINTER, StmtClass_Nonexecutable},
    {"PRINT", PRINT, StmtClass_Executable},
    {"PROGRAM", PROGRAM, StmtClass_Nonexecutable},
    {"PUNCH", PUNCH, StmtClass_Executable},
    {"READ", READ, StmtClass_Executable},
    {"REAL", REAL, StmtClass_Nonexecutable},
    {"RETURN", RETURN, StmtClass_Executable},
    {"REWIND", REWIND, StmtClass_Executable},
    {"SAVE", SAVE, StmtClass_Nonexecutable},
    {"STOP", STOP, StmtClass_Executable},
    {"SUBROUTINE", SUBROUTINE, StmtClass_Nonexecutable},
    {"WRITE", WRITE, StmtClass_Executable}
};

#define KEYWORD_TBL_LEN 47

/*
 *  DOUBLEPRECISION is the longest keyword, and
 *  DO and IF are the shortest.
 */
#define MIN_KW_LEN  2
#define MAX_KW_LEN 15

typedef struct operator {
    char *name;
    OperatorId id;
    int precedence;
} Operator;

static Operator logicalOpTable[] = {
    {"A", OP_AND, PREC_AND},
    {"AND", OP_AND, PREC_AND},
    {"EQ", OP_EQ, PREC_EQ},
    {"EQV", OP_EQV, PREC_EQV},
    {"GE", OP_GE, PREC_GE},
    {"GT", OP_GT, PREC_GT},
    {"LE", OP_LE, PREC_LE},
    {"LT", OP_LT, PREC_LT},
    {"NE", OP_NE, PREC_NE},
    {"NEQV", OP_NEQV, PREC_NEQV},
    {"NOT", OP_NOT, PREC_NOT},
    {"O", OP_OR, PREC_OR},
    {"OR", OP_OR, PREC_OR},
    {"X", OP_NEQV, PREC_NEQV},
    {"XOR", OP_NEQV, PREC_NEQV}
};

#define LOGICAL_OP_TBL_LEN 15

/*
 *  .FALSE. is the longest name (less the '.' delimiters)
 */
#define MAX_LOGICAL_OP_LEN 5

char *getIdentifier(char *s, Token *token) {
    static char id[MAX_ID_LENGTH+1];
    int len;
    char *start;

    len = 0;
    start = s;
    for (;;) {
        if (isalpha(*s) || isdigit(*s) || *s == '_') {
            if (len < MAX_ID_LENGTH) {
                id[len++] = toupper(*s++);
            }
            else {
                return setInvalidToken(start, token);
            }
        }
        else if (isspace(*s)) {
            s += 1;
        }
        else {
            id[len] = '\0';
            token->type = TokenType_Identifier;
            token->details.identifier.name = id;
            token->details.identifier.qualifiers = NULL;
            return s;
        }
    }
}

static char *getInteger(char *s, i64 *value) {
    bool isNegative;
    i64 val;

    val = 0;
    isNegative = FALSE;
    s = getNextChar(s);
    if (*s == '-') {
        isNegative = TRUE;
        s += 1;
    }
    else if (*s == '+') {
        s += 1;
    }
    for (;;) {
        if (isdigit(*s)) {
            val = (val * 10) + (*s - '0');
            s += 1;
        }
        else if (isspace(*s)) {
            s += 1;
        }
        else {
            *value = isNegative ? -val : val;
            return s;
        }
    }
}

static char *getFloat(char *s, Token *token) {
    f64 divisor;
    char *ep;
    f64 frac;
    bool isNegative;
    f64 val;
    i64 valE;

    val = 0.0;
    isNegative = FALSE;
    s = getNextChar(s);
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
    for (;;) {
        if (isdigit(*s)) {
            val = (val * 10.0) + (f64)(*s - '0');
            s += 1;
        }
        else if (isspace(*s)) {
            s += 1;
        }
        else {
            break;
        }
    }
    /*
     *  Process fraction part
     */
    if (*s == '.') {
        frac = 0.0;
        s += 1;
        divisor = 10.0;
        for (;;) {
            if (isdigit(*s)) {
                frac += (f64)(*s - '0') / divisor;
                divisor *= 10.0;
                s += 1;
            }
            else if (isspace(*s)) {
                s += 1;
            }
            else {
                break;
            }
        }
        val += frac;
    }
    /*
     *  Process power of 10 indication
     */
    if (*s == 'E' || *s == 'e' || *s == 'D' || *s == 'd') {
        ep = s;
        s = getNextChar(s + 1);
        if (isdigit(*s)) {
            s = ep + 1;
        }
        else if (*s == '+' || *s == '-') {
            s = getNextChar(s + 1);
            if (isdigit(*s)) {
                s = ep + 1;
            }
            else {
                s = NULL;
            }
        }
        if (s != NULL) {
            s = getInteger(s, &valE);
            while (valE > 0) {
                val *= 10.0;
                valE -= 1;
            }
            while (valE < 0) {
                val /= 10.0;
                valE += 1;
            }
        }
        else {
            s = ep;
        }
    }
    token->type = TokenType_Constant;
    token->details.constant.dt.type = BaseType_Real;
    token->details.constant.value.real = isNegative ? -val : val;

    return s;
}

static char *getLogicalOp(char *s, Token *token) {
    int i;
    char id[MAX_LOGICAL_OP_LEN+1];
    int lowerBound;
    char *start;
    int upperBound;
    int v;

    i = 0;
    start = s;
    s += 1;
    for (;;) {
        if (isalpha(*s) && i < MAX_LOGICAL_OP_LEN) {
            id[i++] = toupper(*s++);
        }
        else if (*s == '.') {
            id[i] = '\0';
            s += 1;
            break;
        }
        else if (isspace(*s)) {
            s += 1;
        }
        else {
            return setInvalidToken(start, token);
        }
    }
    lowerBound = 0;
    upperBound = LOGICAL_OP_TBL_LEN;
    while (lowerBound < upperBound) {
        i = lowerBound + ((upperBound - lowerBound) / 2);
        v = strcmp(id, logicalOpTable[i].name);
        if (v == 0) {
            token->type = TokenType_Operator;
            token->details.operator.id = logicalOpTable[i].id;
            token->details.operator.precedence = logicalOpTable[i].precedence;
            return s;
        }
        else if (v < 0) {
            upperBound = i;
        }
        else {
            lowerBound = i + 1;
        }
    }
    if (strcmp(id, "TRUE") == 0) {
        token->type = TokenType_Constant;
        token->details.constant.dt.type = BaseType_Logical;
        token->details.constant.value.logical = ~(u64)0;
        return s;
    }
    else if (strcmp(id, "FALSE") == 0) {
        token->type = TokenType_Constant;
        token->details.constant.dt.type = BaseType_Logical;
        token->details.constant.value.logical = 0;
        return s;
    }

    return setInvalidToken(start, token);
}

char *getNextChar(char *s) {
    while (*s != '\0' && isspace(*s)) s += 1;
    return s;
}

char *getNextToken(char *s, Token *token, bool doMatchKeywords) {
    char *start;

    memset(token, 0, sizeof(Token));
    s = getNextChar(s);
    if (*s == '\0') return s;

    switch (*s) {
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
    case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
        if (doMatchKeywords) {
            s = matchKeyword(s, token);
            if (token->type == TokenType_Invalid) {
                s = getIdentifier(s, token);
            }
        }
        else {
            s = getIdentifier(s, token);
        }
        break;
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
        return getNumber(s, token);
    case '.':
        start = s;
        s = getNextChar(s + 1);
        if (isalpha(*s)) {
            return getLogicalOp(start, token);
        }
        else if (isdigit(*s)) {
            return getNumber(start, token);
        }
        else {
            return setInvalidToken(start, token);
        }
        break;
    case '+':
        token->type = TokenType_Operator;
        token->details.operator.id = OP_ADD;
        token->details.operator.precedence = PREC_ADD;
        s += 1;
        break;
    case '-':
        token->type = TokenType_Operator;
        token->details.operator.id = OP_SUB;
        token->details.operator.precedence = PREC_SUB;
        s += 1;
        break;
    case '*':
        if (*(s + 1) == '*') {
            token->type = TokenType_Operator;
            token->details.operator.id = OP_EXP;
            token->details.operator.precedence = PREC_EXP;
            s += 1;
        }
        else {
            token->type = TokenType_Operator;
            token->details.operator.id = OP_MUL;
            token->details.operator.precedence = PREC_MUL;
        }
        s += 1;
        break;
    case '/':
        if (*(s + 1) == '/') {
            token->type = TokenType_Operator;
            token->details.operator.id = OP_CAT;
            token->details.operator.precedence = PREC_CAT;
            s += 1;
        }
        else {
            token->type = TokenType_Operator;
            token->details.operator.id = OP_DIV;
            token->details.operator.precedence = PREC_DIV;
        }
        s += 1;
        break;
    case '\'':
    case '"' :
        return getString(s, token);
    default:
        return setInvalidToken(s, token);
    }

    return s;
}

static char *getNumber(char *s, Token *token) {
    char *cp;
    char *start;
    i64 value;

    start = s;
    s = getInteger(start, &value);
    if (*s == '.') {
        cp = getNextChar(s + 1);
        if (isalpha(*cp)) { // could be .AND., .OR., etc.
            token->type = TokenType_Constant;
            token->details.constant.dt.type = BaseType_Integer;
            token->details.constant.value.integer = value;
        }
        else {
            s = getFloat(start, token);
        }
    }
    else if (*s == 'E' || *s == 'e' || *s == 'D' || *s == 'd') {
        s = getFloat(start, token);
    }
    else if (*s == 'H' || *s == 'h') {
        token->type = TokenType_Constant;
        token->details.constant.dt.type = BaseType_Character;
        token->details.constant.value.character.length = value;
        token->details.constant.value.character.string = cp = (char *)allocate(value + 1);
        s += 1;
        while (*s != '\0' && value > 0) {
            *cp++ = *s++;
            value -= 1;
        }
        while (value-- > 0) *cp++ = ' ';
        *cp = '\0';
    }
    else {
        token->type = TokenType_Constant;
        token->details.constant.dt.type = BaseType_Integer;
        token->details.constant.value.integer = value;
    }

    return s;
}

static char *getString(char *s, Token *token) {
    char *cp;
    int len;
    char quote;
    char *start;

    quote = *s++;
    start = s;

    while (*s != '\0') {
        if (*s == quote) {
            if (*(s + 1) == quote) {
                s += 2;
            }
            else {
                break;
            }
        }
        else {
            s += 1;
        }
    }
    if (*s != quote) {
        return setInvalidToken(start - 1, token);
    }
    len = s - start;
    token->type = TokenType_Constant;
    token->details.constant.dt.type = BaseType_Character;
    token->details.constant.value.character.string = cp = (char *)allocate(len + 1);
    s = start;
    for (;;) {
        if (*s == quote) {
            if (*(s + 1) == quote) {
                *cp++ = quote;
                s += 2;
            }
            else {
                break;
            }
        }
        else {
            *cp++ = *s++;
        }
    }
    *cp = '\0';
    token->details.constant.value.character.length = cp - token->details.constant.value.character.string;

    return s + 1;
}

static char *matchKeyword(char *s, Token *token) {
    int i;
    char id[MAX_KW_LEN+1];
    int len;
    int lowerBound;
    char *start;
    int upperBound;
    int v;

    len = 0;
    start = s;
    for (;;) {
        if (isalpha(*s) && len < MAX_KW_LEN) {
            id[len++] = toupper(*s++);
        }
        else if (isspace(*s)) {
            s += 1;
        }
        else {
            id[len] = '\0';
            break;
        }
    }
    while (len >= MIN_KW_LEN) {
        lowerBound = 0;
        upperBound = KEYWORD_TBL_LEN;
        while (lowerBound < upperBound) {
            i = lowerBound + ((upperBound - lowerBound) / 2);
            v = strcmp(id, keywordTable[i].name);
            if (v == 0) {
                token->type = TokenType_Keyword;
                token->details.keyword.id = keywordTable[i].id;
                token->details.keyword.class = keywordTable[i].class;
                s = start;
                while (len > 0) {
                    if (isalpha(*s)) len -= 1;
                    s += 1;
                }
                return s;
            }
            else if (v < 0) {
                upperBound = i;
            }
            else {
                lowerBound = i + 1;
            }
        }
        id[--len] = '\0';
    }

    return setInvalidToken(start, token);
}

static char *setInvalidToken(char *s, Token *token) {
    token->type = TokenType_Invalid;
    token->details.invalid.lineNo = lineNo;
    token->details.invalid.column = s - stmtBuf;
    return s;
}
