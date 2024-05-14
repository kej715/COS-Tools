/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: compile.c
**
**  Description:
**      This file implements the parser for the FORTRAN language.
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "binops.h"
#include "codegen.h"
#include "coercion.h"
#include "const.h"
#include "proto.h"
#include "types.h"

#define DEBUG 1

typedef struct doStackEntry {
    char startLabel[8];
    char endLabel[8];
    Symbol *termLabelSym;
    Symbol *loopVariable;
    BaseType loopVariableType;
} DoStackEntry;

typedef struct ifStackEntry {
    char blockEndLabel[8];
    char ifEndLabel[8];
} IfStackEntry;

typedef enum parsingState {
    STATE_PROG_UNIT = 0,
    STATE_IMPLICIT,
    STATE_SPECIFICATION,
    STATE_DEFINITION,
    STATE_EXECUTABLE
} ParsingState;

static char *baseTypeToStr(BaseType type);
static char *collectStmt(void);
static Token *copyToken(Token *token);
static Symbol *defineLocalVariable(char *id, DataType *dt);
static char *eatWsp(char *s);
static void err(char *format, ...);
static bool evaluateExpression(Token *expression, OperatorArgument *result);
static bool evaluateExprHelper(Token *expression);
static bool evaluateIdentifier(char *id);
static bool executeOperator(OperatorId op);
static void freeToken(Token *token);
static DataType *getDataType(OperatorArgument *arg);
static char *getLabel(char *s, char *label);
static void list(char *format, ...);
static char *opIdToStr(OperatorId id);
static void parseArithmeticIF(char *s, Register reg);
static void parseAssignment(char *s, char *id);
static char *parseCharConstraint(char *s, DataType *dt);
static char *parseDataType(char *s, Token *token, DataType *type);
static char *parseDimDecl(char *s, Symbol *symbol);
static char *parseExpression(char *s, Token **expression);
static char *parseFmtSpec(char *s);
static void parseInputList(char *s);
static void parseLogicalIF(char *s, Register reg, bool isFromLogIf);
static void parseOutputList(char *s);
static char *parseTypeDecl(char *s, DataType *dt);
static char *parseUnitAndFmt(char *s, int defaultUnit);
static void popArg(OperatorArgument *arg);
static void popOp(OperatorDetails *op);
static void presetImplicit(void);
static void presetProgUnit(void); 
static void pushArg(OperatorArgument *arg);
static void pushOp(OperatorDetails *op);
static char *readLine(int *lineLength);
static void verifyEOS(char *s);
static void warn(char *format, ...);

static void parseASSIGN(char *s);
static void parseBLOCKDATA(char *s);
static void parseCOMMON(char *s);
static void parseDATA(char *s);
static void parseDIMENSION(char *s);
static void parseDO(char *s);
static void parseELSE(char *s);
static void parseELSEIF(char *s);
static void parseEND(char *s);
static void parseENDIF(char *s);
static void parseENTRY(char *s);
static void parseEQUIVALENCE(char *s);
static void parseEXTERNAL(char *s);
static void parseFORMAT(char *s);
static void parseFUNCTION(char *s);
static void parseGOTO(char *s);
static void parseIF(char *s, bool isFromLogIf);
static void parseINCLUDE(char *s);
static void parseINTRINSIC(char *s);
static void parseNAMELIST(char *s);
static void parseSAVE(char *s);
static void parseSUBROUTINE(char *s);
static void parseIMPLICIT(char *s);
static void parseIMPLICITNONE(char *s);
static void parsePARAMETER(char *s);
static void parsePRINT(char *s);
static void parsePROGRAM(char *s);
static void parseREAD(char *s);
static void parseWRITE(char *s);

#if DEBUG
static char *argClassToStr(ArgumentClass class);
static void printExpression(FILE *f, Token *expression);
static void printToken(FILE *f, Token *token);
static char *tokenIdToStr(TokenId id);
static char *tokenTypeToStr(TokenType type);
#endif

static Symbol defaultProgSym = {
    NULL, NULL, NULL, "MAIN", SymClass_Program
};
static Symbol *currentLabel;
static int errorCount = 0;
static DataType implicitTypes[26];
static char lineBuf[MAX_LINE_LENGTH+1];
static int localOffset = 0;
static Symbol *progUnitSym;
static ParsingState state;
static int warningCount = 0;

static OperatorArgument argStack[MAX_ARG_STACK_SIZE];
static int argStkPtr;
static DoStackEntry doStack[MAX_DO_STACK_SIZE];
static int doStackPtr = 0;
static IfStackEntry ifStack[MAX_IF_STACK_SIZE];
static int ifStackPtr = 0;
static OperatorDetails opStack[MAX_OP_STACK_SIZE];
static int opStkPtr;

static char *collectStmt() {
    int lineLength;
    char *lp;
    char *s;
    char *stmtLimit;

    s = stmtBuf;
    *s = '\0';
    stmtLimit = stmtBuf + MAX_STMT_LENGTH;

    for (;;) {
        lp = lineBuf;
        if (*lp == '\0') {
            lp = readLine(&lineLength);
            if (lp == NULL) {
                *s = '\0';
                return (s > stmtBuf) ? stmtBuf : NULL;
            }
        }
        if (*lp == 'C' || *lp == 'c' || *lp == '*' || *lp == '!') {
            list("%6d: %s\n", ++lineNo, lp);
            lineBuf[0] = '\0';
        }
        else if (stmtBuf[0] == '\0') {
            list("%6d: %s\n", ++lineNo, lp);
            while (*lp != '\0') *s++ = *lp++;
            lineBuf[0] = '\0';
        }
        else if (lineBuf[5] != ' ' && lineBuf[5] != '0') {
            list("%6d: %s\n", ++lineNo, lp);
            lp = lineBuf + 6;
            while (*lp != '\0' && s < stmtLimit) *s++ = *lp++;
            lineBuf[0] = '\0';
        }
        else {
            *s = '\0';
            break;
        }
    }

    return stmtBuf;
}

void compile(char *name) {
    DataType dt;
    DoStackEntry *entry;
    int i;
    char lineLabel[6];
    char *lp;
    Register reg1, reg2;
    char *s;
    char *start;
    StatementClass stmtClass;
    Symbol *sym;
    Token token;

    lineBuf[0] = '\0';
    lineNo = 0;
    emitStart(name);
    presetProgUnit();

    for (;;) {
        start = s = collectStmt();
        if (s == NULL) {
            emitEnd();
            break;
        }
        if (doEchoSource && objectFile != NULL) fprintf(objectFile, "* %s\n", start);
        lp = lineLabel;
        currentLabel = NULL;
        for (i = 0; i < 5; i++) {
            if (isdigit(*s)) {
                *lp++ = *s++;
            }
            else if (isspace(*s)) {
                s += 1;
            }
            else {
                err("Invalid line label");
                lp = lineLabel;
                s = start + 5;
                break;
            }
        }
        *lp = '\0';
        s = getNextToken(s + 1, &token);
        if (token.type == TokenType_Keyword) {
            stmtClass = token.details.keyword.class;
        }
        else if (token.type == TokenType_Identifier) {
            stmtClass = StmtClass_Executable;
        }
        else {
            stmtClass = StmtClass_None;
        }
        if (lineLabel[0] != '\0') {
            sym = findLabel(lineLabel);
            if (sym == NULL) {
                currentLabel = addLabel(lineLabel);
                currentLabel->details.label.class = stmtClass;
            }
            else if (sym->details.label.forwardRef) {
                currentLabel = sym;
                if (sym->details.label.class == StmtClass_None) {
                    sym->details.label.class = stmtClass;
                }
                else if (sym->details.label.class != stmtClass
                         && (stmtClass != StmtClass_Executable || sym->details.label.class < StmtClass_Executable)) {
                    err("Invalid statement type for label");
                }
                currentLabel->details.label.forwardRef = FALSE;
            }
            else {
                err("Duplicate line label: %s", lineLabel);
            }
        }

        if (token.type == TokenType_None) continue;

        if (currentLabel != NULL) {
            if (currentLabel->details.label.class >= StmtClass_Executable) emitLabel(currentLabel->details.label.label);
        }

        switch (state) {
        case STATE_PROG_UNIT:
            presetImplicit();
            state = STATE_IMPLICIT;
            if (token.type == TokenType_Keyword) {
                switch (token.details.keyword.id) {
                case BLOCKDATA:
                    parseBLOCKDATA(s);
                    continue;
                case END:
                    parseEND(s);
                    presetProgUnit();
                    continue;
                case FUNCTION:
                    parseFUNCTION(s);
                    continue;
                case PROGRAM:
                    parsePROGRAM(s);
                    continue;
                case SUBROUTINE:
                    parseSUBROUTINE(s);
                    continue;
                default:
                    break;
                }
            }
            /*
             *  Token is not a recognized program unit declaration token,
             *  so set the current program unit type to Program, and fall
             *  through to process possible IMPLICIT and IMPLICITNONE
             *  statements.
             */
            progUnitSym = &defaultProgSym;
            emitProlog(progUnitSym);

        case STATE_IMPLICIT:
            if (token.type == TokenType_Keyword) {
                switch (token.details.keyword.id) {
                case END:
                    parseEND(s);
                    presetProgUnit();
                    continue;
                case ENTRY: /* ENTRY may occur almost anywhere */
                    parseENTRY(s);
                    continue;
                case FORMAT: /* FORMAT may occur almost anywhere */
                    parseFORMAT(s);
                    continue;
                case IMPLICIT:
                    parseIMPLICIT(s);
                    continue;
                case IMPLICITNONE:
                    parseIMPLICITNONE(s);
                    continue;
                case PARAMETER:
                    parsePARAMETER(s);
                    continue;
                default:
                    break;
                }
            }
            /*
             *  Token is not IMPLICIT or IMPLICITNONE, so fall through to process
             *  possible specification statements.
             */
            state = STATE_SPECIFICATION;

        case STATE_SPECIFICATION:
            if (token.type == TokenType_Keyword) {
                switch (token.details.keyword.id) {
                case CHARACTER:
                case COMPLEX:
                case DOUBLEPRECISION:
                case INTEGER:
                case LOGICAL:
                case POINTER:
                case REAL:
                    s = parseDataType(s, &token, &dt);
                    if (dt.type != BaseType_Undefined) {
                        s = parseTypeDecl(s, &dt);
                    }
                    continue;
                case COMMON:
                    parseCOMMON(s);
                    continue;
                case DATA:
                    parseDATA(s);
                    continue;
                case DIMENSION:
                    parseDIMENSION(s);
                    continue;
                case END:
                    parseEND(s);
                    presetProgUnit();
                    continue;
                case ENTRY: /* ENTRY may occur almost anywhere */
                    parseENTRY(s);
                    continue;
                case EQUIVALENCE:
                    parseEQUIVALENCE(s);
                    continue;
                case EXTERNAL:
                    parseEXTERNAL(s);
                    continue;
                case FORMAT: /* FORMAT may occur almost anywhere */
                    parseFORMAT(s);
                    continue;
                case INCLUDE:
                    parseINCLUDE(s);
                    continue;
                case INTRINSIC:
                    parseINTRINSIC(s);
                    continue;
                case NAMELIST:
                    parseNAMELIST(s);
                    continue;
                case PARAMETER:
                    parsePARAMETER(s);
                    continue;
                case SAVE:
                    parseSAVE(s);
                    continue;
                default:
                    break;
                }
            }
            /*
             *  Token is not a specification statement, so fall through to process
             *  possible definition statements.
             */
            state = STATE_DEFINITION;

        case STATE_DEFINITION:
            /*
             *  Token is not a specification statement, so fall through to process
             *  possible definition statements.
             */
            state = STATE_EXECUTABLE;
            localOffset = -calculateLocalOffsets();

        case STATE_EXECUTABLE:
            if (token.type == TokenType_Keyword) {
                switch (token.details.keyword.id) {
                case ASSIGN:
                    parseASSIGN(s);
                    break;
                case BACKSPACE:
                    break;
                case BUFFERIN:
                    break;
                case BUFFEROUT:
                    break;
                case CALL:
                    break;
                case CONTINUE:
                    break;
                case CLOSE:
                    break;
                case DECODE:
                    break;
                case DO:
                    parseDO(s);
                    break;
                case ELSE:
                    parseELSE(s);
                    break;
                case ELSEIF:
                    parseELSEIF(s);
                    break;
                case ENCODE:
                    break;
                case END:
                    parseEND(s);
                    presetProgUnit();
                    continue;
                case ENDFILE:
                    break;
                case ENDIF:
                    parseENDIF(s);
                    break;
                case FORMAT:
                    parseFORMAT(s);
                    break;
                case GOTO:
                    parseGOTO(s);
                    break;
                case IF:
                    parseIF(s, FALSE);
                    break;
                case OPEN:
                    break;
                case PAUSE:
                    break;
                case PRINT:
                    parsePRINT(s);
                    break;
                case PUNCH:
                    break;
                case READ:
                    parseREAD(s);
                    break;
                case RETURN:
                    break;
                case REWIND:
                    break;
                case WRITE:
                    parseWRITE(s);
                    break;
                default:
                    err("Misplaced statement");
                    break;
                }
            }
            else if (token.type == TokenType_Identifier) {
                parseAssignment(s, token.details.identifier);
            }
            else {
                err("Invalid statement");
            }
            break;
        default:
            fprintf(stderr, "Invalid compiler state: %d\n", state);
            break;
        }
        if (currentLabel != NULL && currentLabel->details.label.class == StmtClass_Do_Term) {
            if (doStackPtr < 1 || currentLabel != doStack[doStackPtr - 1].termLabelSym) {
                err("Misplaced DO termination label");
                continue;
            }
            while (doStackPtr > 0 && currentLabel == doStack[doStackPtr - 1].termLabelSym) {
                entry = &doStack[--doStackPtr];
                reg1 = emitLoadStack(0);
                reg2 = emitLoadStack(2);
                emitAddReg(reg1, reg2, entry->loopVariableType);
                emitStoreStack(reg1, 0);
                emitDecrTrip();
                freeRegister(reg1);
                freeRegister(reg2);
                emitBranch(entry->startLabel);
                emitLabel(entry->endLabel);
                emitAdjustSP(3);
            }
        }
    }
}

static Token *copyToken(Token *token) {
    int len;
    Token *new;

    if (token == NULL) return NULL;

    new = (Token *)allocate(sizeof(Token));
    memcpy(new, token, sizeof(Token));
    switch (token->type) {
    case TokenType_Identifier:
        if (token->details.identifier != NULL) {
            len = strlen(token->details.identifier);
            new->details.identifier = (char *)allocate(len + 1);
            memcpy(new->details.identifier, token->details.identifier, len);
        }
        break;
    case TokenType_Constant:
        if (token->details.constant.dt.type == BaseType_Character) {
            len = strlen(token->details.constant.value.chr.string);
            new->details.constant.value.chr.string = (char *)allocate(len + 1);
            memcpy(new->details.constant.value.chr.string, token->details.constant.value.chr.string, len);
        }
        break;
    case TokenType_Operator:
        new->details.operator.leftArg = copyToken(token->details.operator.leftArg);
        new->details.operator.rightArg = copyToken(token->details.operator.rightArg);
        break;
    default:
        break;
    }

    return new;
}

static Symbol *defineLocalVariable(char *id, DataType *dt) {
    Symbol *symbol;

    symbol = addSymbol(id, SymClass_Local);
    if (symbol != NULL) {
        symbol->details.variable.dt = *dt;
    }

    return symbol;
}

static char *eatWsp(char *s) {
    while (isspace(*s)) s += 1;
    return s;
}

static void err(char *format, ...) {
    va_list ap;
    char buf[80];

    va_start(ap, format);
    vsprintf(buf, format, ap);
    va_end(ap);
    list(" *ERROR*   %s\n", buf);
    fprintf(stderr, "ERROR line %d : %s\n", lineNo, buf);
    errorCount += 1;
}

static bool evaluateExpression(Token *expression, OperatorArgument *result) {
    bool err;
    Token token;

    argStkPtr = 0;
    opStkPtr = 0;
    err = evaluateExprHelper(expression);
    if (err == FALSE && argStkPtr == 1 && opStkPtr == 0) {
        popArg(result);
    }

    return err;
}

static bool evaluateExprHelper(Token *expression) {
    OperatorArgument arg;
    bool err;
    OperatorDetails op;
    Token *rightArg;

    err = FALSE;
    switch (expression->type) {
    case TokenType_Identifier:
        err = evaluateIdentifier(expression->details.identifier);
        break;
    case TokenType_Constant:
        arg.class = ArgClass_Constant;
        arg.details.constant = expression->details.constant;
        pushArg(&arg);
        break;
    case TokenType_Operator:
        if (expression->details.operator.id == OP_SEXPR) {
            pushOp(&expression->details.operator);
            err = evaluateExprHelper(expression->details.operator.rightArg);
            opStkPtr -= 1;
            return err;
        }
        if (expression->details.operator.leftArg != NULL) {
            err = evaluateExprHelper(expression->details.operator.leftArg);
            if (err) return err;
            while (opStkPtr > 0
                   && opStack[opStkPtr - 1].id != OP_SEXPR
                   && expression->details.operator.precedence >= opStack[opStkPtr - 1].precedence) {
                popOp(&op);
                err = executeOperator(op.id);
                if (err) return err;
            }
        }
        pushOp(&expression->details.operator);
        err = evaluateExprHelper(expression->details.operator.rightArg);
        if (err) return err;
        while (opStkPtr > 0 && opStack[opStkPtr - 1].id != OP_SEXPR) {
            popOp(&op);
            err = executeOperator(op.id);
            if (err) break;
        }
        break;
    default:
        err = TRUE;
        break;
    }

    return err;
}

static bool evaluateIdentifier(char *id) {
    OperatorArgument arg;
    Symbol *symbol;

    symbol = findSymbol(id);
    if (symbol == NULL) {
        symbol = defineLocalVariable(id, &implicitTypes[toupper(id[0]) - 'A']);
        localOffset -= calculateSize(symbol);
        symbol->details.variable.offset = localOffset;
    }

    switch (symbol->class) {
    case SymClass_Local:
        arg.class = ArgClass_Local;
        arg.details.symbol = symbol;
        break;
    case SymClass_Global:
        arg.class = ArgClass_Global;
        arg.details.symbol = symbol;
        break;
    case SymClass_Argument:
        arg.class = ArgClass_Argument;
        arg.details.symbol = symbol;
        break;
    case SymClass_Function:
        arg.class = ArgClass_Function;
        arg.details.symbol = symbol;
        // TODO: handle external/intrinsic function call
        break;
    case SymClass_Parameter:
        arg.class = ArgClass_Constant;
        arg.details.constant = symbol->details.param;
        break;
    default:
        err("Invalid symbol reference");
        return TRUE;
    }

    pushArg(&arg);
    return FALSE;
}

static bool executeOperator(OperatorId op) {
    BaseType argType;
    void (*binop)(OperatorArgument *leftArg, OperatorArgument *rightArg);
    bool isBop;
    bool isConstResult;
    OperatorArgument leftArg;
    DataType *leftType;
    OperatorArgument rightArg;
    DataType *rightType;

    popArg(&rightArg);
    rightType = getDataType(&rightArg);
    isConstResult = isConstant(rightArg);
    if (isLoadable(rightArg)) {
        emitLoadVar(&rightArg);
    }
    else if (isFunction(rightArg)) {
        emitFnCall(&rightArg);
    }
    isBop = isBinaryOp(op);
    if (isBop) {
        popArg(&leftArg);
        leftType = getDataType(&leftArg);
        isConstResult = isConstResult && isConstant(leftArg);
        argType = calculateCoercedType(op, leftType->type, rightType->type);
        if (argType == BaseType_Undefined) {
            err("Invalid argument type");
            return TRUE;
        }
        if (isLoadable(leftArg)) {
            emitLoadVar(&leftArg);
        }
        else if (isFunction(leftArg)) {
            emitFnCall(&leftArg);
        }
        if (leftType->type  != argType) leftType->type  = coerceArgument(&leftArg,  leftType->type,  argType);
        if (rightType->type != argType) rightType->type = coerceArgument(&rightArg, rightType->type, argType);
        if (!isConstResult && isConstant(rightArg)) emitLoadConst(&rightArg);
    }

    switch (op) {
    /*
     *  Unary operators
     */
    case OP_NEG:
        if (isConstResult) {
            switch (rightArg.details.constant.dt.type) {
            case BaseType_Integer:
            case BaseType_Pointer:
                rightArg.details.constant.value.integer = -rightArg.details.constant.value.integer;
                break;
            case BaseType_Real:
                rightArg.details.constant.value.real = -rightArg.details.constant.value.real;
                break;
            case BaseType_Logical:
                rightArg.details.constant.value.logical = ~rightArg.details.constant.value.logical;
                break;
            case BaseType_Double:
                rightArg.details.constant.value.dp.high = -rightArg.details.constant.value.dp.high;
                rightArg.details.constant.value.dp.low = -rightArg.details.constant.value.dp.low;
                break;
            case BaseType_Complex:
                rightArg.details.constant.value.complex.real = -rightArg.details.constant.value.complex.real;
                rightArg.details.constant.value.complex.imaginary = -rightArg.details.constant.value.complex.imaginary;
                break;
            default:
                err("Invalid argument type to '%s'", opIdToStr(op));
                return TRUE;
            }
        }
        else {
        }
        break;
    case OP_NOT:
        if (isConstResult) {
            switch (rightArg.details.constant.dt.type) {
            case BaseType_Integer:
            case BaseType_Pointer:
            case BaseType_Logical:
                rightArg.details.constant.value.logical = ~rightArg.details.constant.value.logical;
                break;
            default:
                err("Invalid argument type to '%s'", opIdToStr(op));
                return TRUE;
            }
        }
        else {
        }
        break;
    case OP_PLUS:
        if (isConstResult) {
            switch (rightArg.details.constant.dt.type) {
            case BaseType_Integer:
            case BaseType_Pointer:
            case BaseType_Real:
            case BaseType_Logical:
            case BaseType_Double:
            case BaseType_Complex:
                break;
            default:
                err("Invalid argument type to '%s'", opIdToStr(op));
                return TRUE;
            }
        }
        else {
        }
        break;
    /*
     *  Binary comparison operators
     */
    case OP_EQ:
    case OP_GE:
    case OP_GT:
    case OP_LE:
    case OP_LT:
    case OP_NE:
    /*
     *  Binary arithmetic operators
     */
    case OP_ADD:
    case OP_DIV:
    case OP_EXP:
    case OP_MUL:
    case OP_SUB:
    /*
     *  Binary logical operators
     */
    case OP_AND:
    case OP_OR:
    case OP_EQV:
    case OP_NEQV:
    /*
     *  Binary character operators
     */
    case OP_CAT:
        binop = isConstResult ? cstBinOps[op - OP_ADD][argType] : genBinOps[op - OP_ADD][argType];
        if (binop != NULL) {
            (*binop)(&leftArg, &rightArg);
        }
        else {
            err("Operator '%s' does not accept %s", opIdToStr(op), baseTypeToStr(argType));
            return TRUE;
        }
        break;
    default:
        err("Unrecognized operator");
        return TRUE;
    }

    if (isConstResult) {
        if (op >= OP_EQ && op <= OP_NE) {
            rightArg.details.constant.dt.type = BaseType_Logical;
        }
    }
    else {
        rightArg.class = ArgClass_Calculation;
        if (isBop) {
            if (op >= OP_EQ && op <= OP_NE) {
                rightArg.details.calculation.type = BaseType_Logical;
            }
            else {
                rightArg.details.calculation.type = argType;
            }
            freeRegister(leftArg.reg);
        }
    }
    pushArg(&rightArg);

    return FALSE;
}

static void freeToken(Token *token) {
    if (token != NULL) {
        switch (token->type) {
        case TokenType_Identifier:
            if (token->details.identifier != NULL) free(token->details.identifier);
            break;
        case TokenType_Constant:
            if (token->details.constant.dt.type == BaseType_Character) {
                free(token->details.constant.value.chr.string);
            }
            break;
        case TokenType_Operator:
            freeToken(token->details.operator.leftArg);
            freeToken(token->details.operator.rightArg);
            break;
        default:
            break;
        }
        free(token);
    }
}

static DataType undefinedType = { BaseType_Undefined };

static DataType *getDataType(OperatorArgument *arg) {
    Symbol *sym;

    switch (arg->class) {
    case ArgClass_Constant:
        return &arg->details.constant.dt;
    case ArgClass_Local:
    case ArgClass_Global:
    case ArgClass_Argument:
    case ArgClass_Pointee:
    case ArgClass_Function:
        sym = arg->details.symbol;
        switch (sym->class) {
        case SymClass_Function:
        case SymClass_Local:
        case SymClass_Global:
        case SymClass_Argument:
            return &sym->details.variable.dt;
        case SymClass_Parameter:
            return &sym->details.param.dt;
        case SymClass_Pointee:
            return &sym->details.pointee.dt;
        default:
            return &undefinedType;
        }
        break;
    case ArgClass_Calculation:
        return &arg->details.calculation;
    default:
        fprintf(stderr, "Unrecognized operator argument class: %d\n", arg->class);
        exit(1);
    }
}

static char *getLabel(char *s, char *label) {
    char *lp;

    lp = label;
    for (;;) {
        if (isdigit(*s)) {
            if ((lp - label) < 8) {
                *lp++ = *s++;
            }
            else {
                break;
            }
        }
        else if (isspace(*s)) {
            s += 1;
        }
        else if (lp > label) {
            *lp = '\0';
            return s;
        }
        else {
            break;
        }
    }

    return NULL;
}

static void list(char *format, ...) {
    va_list ap;

    if (listingFile != NULL) {
        va_start(ap, format);
        vfprintf(listingFile, format, ap);
        va_end(ap);
    }
}

static void parseArithmeticIF(char *s, Register reg) {
    int i;
    char *labels[3];
    char lineLabels[3][8];
    char *lp;
    Symbol *sym;

    i = 0;
    for (;;) {
        s = eatWsp(s);
        if (*s == '\0') break;
        if (i > 2) {
            err("Invalid arithmetic IF");
            return;
        }
        s = getLabel(s, &lineLabels[i++][0]);
        if (s == NULL) {
            err("Invalid line label");
            return;
        }
        if (*s == ',') s += 1;
    }
    if (i < 3) {
        err("Invalid arithmetic IF");
        return;
    }
    for (i = 0; i < 3; i++) {
        lp = &lineLabels[i][0];
        sym = findLabel(lp);
        if (sym == NULL) {
            sym = addLabel(lp);
            sym->details.label.class = StmtClass_Executable;
            sym->details.label.forwardRef = TRUE;
        }
        labels[i] = sym->details.label.label;
    }
    emitBranch3Way(reg, labels[0], labels[1], labels[2]);
}

static void parseAssignment(char *s, char *id) {
    DataType dt;
    Token *expression;
    OperatorArgument result;
    Symbol *symbol;

    s = eatWsp(s);
    if (*s != '=') {
        err("Invalid statement");
        return;
    }

    symbol = findSymbol(id);
    if (symbol == NULL) {
        symbol = defineLocalVariable(id, &implicitTypes[toupper(id[0]) - 'A']);
        localOffset -= calculateSize(symbol);
        symbol->details.variable.offset = localOffset;
    }

    s = eatWsp(parseExpression(s + 1, &expression));
    if (expression == NULL) {
        err("Expression syntax");
        return;
    }
    if (evaluateExpression(expression, &result) == FALSE) {
        if (coerceArgument(&result, getDataType(&result)->type, symbol->details.variable.dt.type) == BaseType_Undefined) {
            err("Invalid type conversion");
        }
        if (isConstant(result)) {
            emitLoadConst(&result);
        }
        else if (isLoadable(result)) {
            emitLoadVar(&result);
        }
        emitStoreArg(symbol, &result);
        freeRegister(result.reg);
    }
    freeToken(expression);
    verifyEOS(s);
}

static char *parseCharConstraint(char *s, DataType *dt) {
    char *start;
    int value;

    s = eatWsp(s);
    start = s;

    if (*s == '*') {
        s = eatWsp(s + 1);
        if (isdigit(*s)) {
            value = 0;
            while (isdigit(*s)) {
                value = (value * 10) + (*s++ - '0');
            }
            dt->constraint = value;
        }
        else if (*s == '(') {
            s = eatWsp(s + 1);
            if (*s == '*') {
                s = eatWsp(s + 1);
                if (*s == ')') {
                    dt->constraint = -1;
                    s += 1;
                }
                else {
                    s = start;
                }
            }
        }
        else {
            s = start;
        }
    }

    return s;
}

static char *parseDataType(char *s, Token *token, DataType *dt) {
    char *start;
    int value;

    dt->type = BaseType_Undefined;
    start = s;
    if (token->type == TokenType_Keyword) {
        switch (token->details.keyword.id) {
        case CHARACTER:
            dt->type = BaseType_Character;
            s = eatWsp(s);
            if (*s == '*') {
                s = parseCharConstraint(s, dt);
            }
            else {
                dt->constraint = 1;
            }
            break;
        case COMPLEX:
            dt->type = BaseType_Complex;
            break;
        case DOUBLEPRECISION:
            dt->type = BaseType_Double;
            break;
        case INTEGER:
            dt->type = BaseType_Integer;
            break;
        case LOGICAL:
            dt->type = BaseType_Logical;
            break;
        case POINTER:
            dt->type = BaseType_Pointer;
            break;
        case REAL:
            dt->type = BaseType_Real;
            break;
        default:
            s = start;
            break;
        }
    }
    else if (token->type != TokenType_Invalid) {
        s = start;
    }
    return s;
}

static char *parseDimDecl(char *s, Symbol *symbol) {
    DataType *dt;
    Token *expression;
    int lowerBound;
    OperatorArgument result;
    int upperBound;

    dt = &symbol->details.variable.dt;
    dt->rank = 0;

    for (;;) {
        s = parseExpression(s, &expression);
        if (expression == NULL) {
            err("Invalid expression in dimension declaration");
            break;
        }
        if (evaluateExpression(expression, &result) == FALSE) {
            if (!isConstant(result) || result.details.constant.dt.type != BaseType_Integer) {
                err("Dimension expression is not an integer constant");
                freeToken(expression);
                break;
            }
            lowerBound = 1;
            upperBound = result.details.constant.value.integer;
            freeToken(expression);
            s = eatWsp(s);
            if (*s == ':') {
                s = parseExpression(s + 1, &expression);
                if (expression == NULL) {
                    err("Invalid expression in dimension declaration");
                    break;
                }
                if (evaluateExpression(expression, &result) == FALSE) {
                    if (!isConstant(result) || result.details.constant.dt.type != BaseType_Integer) {
                        err("Dimension expression is not an integer constant");
                        freeToken(expression);
                        break;
                    }
                    lowerBound = upperBound;
                    upperBound = result.details.constant.value.integer;
                    freeToken(expression);
                }
                else {
                    break;
                }
            }
            if (lowerBound > upperBound) {
                err("Lower bound greater than upper bound in dimension declaration");
                break;
            }
            if (dt->rank >= 7) {
                err("Too many dimensions");
                break;
            }
            dt->bounds[dt->rank].lower = lowerBound;
            dt->bounds[dt->rank].upper = upperBound;
            dt->rank += 1;
            if (*s == ',') {
                s = eatWsp(s + 1);
            }
            else if (*s == ')') {
                s += 1;
                break;
            }
            else {
                err("Incorrect dimension declaration");
                break;
            }
        }
        else {
            freeToken(expression);
            break;
        }
    }

    return s;
}

char *parseExpression(char *s, Token **expression) {
    Token *leftArg;
    Token *op;
    Token *rightArg;
    Token token;

    s = eatWsp(s);
    if (*s == '(') {
        s = parseExpression(s + 1, &rightArg);
        if (rightArg == NULL || *s != ')') {
            freeToken(rightArg);
            *expression = NULL;
            return s;
        }
        memset(&token, 0, sizeof(Token));
        token.type = TokenType_Operator;
        token.details.operator.id = OP_SEXPR;
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
            *expression = NULL;
        }
        else {
            *expression = copyToken(&token);
        }
        break;
    case TokenType_Identifier:
    case TokenType_Constant:
        if (leftArg != NULL) {
            freeToken(leftArg);
            *expression = NULL;
        }
        else if (*s == '\0' || *s == ',' || *s == ')' || *s == ':') {
            *expression = copyToken(&token);
        }
        else {
            leftArg = copyToken(&token);
            s = getNextToken(s, &token);
            if (token.type == TokenType_Operator) {
                switch (token.details.operator.id) {
                case OP_EXP:
                case OP_ADD:
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                case OP_AND:
                case OP_OR:
                case OP_EQ:
                case OP_EQV:
                case OP_GE:
                case OP_GT:
                case OP_LE:
                case OP_LT:
                case OP_NE:
                case OP_NEQV:
                case OP_NOT:
                case OP_CAT:
                    op = copyToken(&token);
                    op->details.operator.leftArg = leftArg;
                    s = parseExpression(s, &rightArg);
                    if (rightArg != NULL) {
                        switch (rightArg->type) {
                        case TokenType_Identifier:
                        case TokenType_Constant:
                        case TokenType_Operator:
                            op->details.operator.rightArg = rightArg;
                            *expression = op;
                            break;
                        default:
                            freeToken(rightArg);
                            freeToken(op);
                            *expression = NULL;
                            break;
                        }
                    }
                    else {
                        freeToken(op);
                        *expression = NULL;
                    }
                    break;
                default:
                    freeToken(leftArg);
                    *expression = NULL;
                    break;
                }
            }
            else {
                freeToken(leftArg);
                *expression = NULL;
            }
        }
        break;
    case TokenType_Operator:
        if (leftArg == NULL) { // unary operator
            switch (token.details.operator.id) {
            case OP_SUB:
                token.details.operator.id = OP_NEG;
                token.details.operator.precedence = PREC_NEG;
                break;
            case OP_ADD:
                token.details.operator.id = OP_PLUS;
                token.details.operator.precedence = PREC_PLUS;
                break;
            default:
                // do nothing
                break;
            }
        }
        op = copyToken(&token);
        s = parseExpression(s, &rightArg);
        if (rightArg != NULL) {
            switch (rightArg->type) {
            case TokenType_Identifier:
            case TokenType_Constant:
            case TokenType_Operator:
                op->details.operator.leftArg  = leftArg;
                op->details.operator.rightArg = rightArg;
                *expression = op;
                break;
            default:
                if (leftArg != NULL) freeToken(leftArg);
                freeToken(rightArg);
                freeToken(op);
                *expression = NULL;
                break;
            }
        }
        else {
            freeToken(op);
            *expression = NULL;
        }
        break;
    default:
        *expression = NULL;
        break;
    }

    return s;
}

static char *parseFmtSpec(char *s) {
    Token *expression;
    int i;
    char label[8];
    char lineLabel[6];
    char *lp;
    Register reg;
    OperatorArgument result;
    Symbol *sym;

    s = eatWsp(s);
    if (isdigit(*s)) {
        lp = lineLabel;
        *lp++ = *s++;
        for (;;) {
            s = eatWsp(s);
            if (isdigit(*s)) {
                if (lp - lineLabel < sizeof(lineLabel)) {
                    *lp++ = *s++;
                }
                else {
                    err("Invalid line label");
                    return NULL;
                }
            }
            else {
                *lp = '\0';
                break;
            }
        }
        sym = findLabel(lineLabel);
        if (sym == NULL) {
            sym = addLabel(lineLabel);
            sym->details.label.class = StmtClass_Format;
            sym->details.label.forwardRef = TRUE;
        }
        reg = emitLoadByteAddr(sym->details.label.label);
        emitPushReg(reg);
        freeRegister(reg);
    }
    else if (*s == '*') {
        // TODO: define default format somehow
        s += 1;
    }
    else {
        s = parseExpression(s, &expression);
        if (expression == NULL) {
            err("Expression syntax");
            return NULL;
        }
        if (evaluateExpression(expression, &result) == FALSE) {
            if (getDataType(&result)->type != BaseType_Character) {
                err("Format specification is not character type or FORMAT label");
                freeRegister(result.reg);
                freeToken(expression);
                return NULL;
            }
            if (isConstant(result)) {
                generateLabel(label);
                emitString(result.details.constant.value.chr.string, label);
                reg = emitLoadByteAddr(label);
                emitPushReg(reg);
                freeRegister(reg);
            }
            else {
                if (isLoadable(result)) emitLoadVar(&result);
                emitPushReg(result.reg);
                freeRegister(result.reg);
            }
        }
        freeToken(expression);
    }

    emitPrimCall("_inifmt");

    return s;
}

static void parseInputList(char *s) {
    OperatorArgument arg;
    char *id;
    int listOrd;
    Register reg;
    Symbol *symbol;
    Token token;

    listOrd = 0;
    arg.class = ArgClass_Constant;
    arg.details.constant.dt.type = BaseType_Integer;
    arg.details.constant.dt.rank = 0;
    for (;;) {
        s = getNextToken(s, &token);
        if (token.type != TokenType_Identifier) {
            err("Input list item is not a variable");
            return;
        }
        id = token.details.identifier;
        symbol = findSymbol(id);
        if (symbol == NULL) {
            fprintf(stderr, "Symbol should have been defined: %s\n", id);
            exit(1);
        }
        arg.details.constant.value.integer = listOrd++;
        emitLoadConst(&arg);
        emitStoreStack(arg.reg, 2);
        freeRegister(arg.reg);
        reg = emitLoadVarByteAddr(symbol);
        emitStoreStack(reg, 3);
        freeRegister(reg);
        emitPrimCall("_infmt");
        s = eatWsp(s);
        if (*s == '\0') {
            break;
        }
        else if (*s == ',') {
            s += 1;
        }
        else {
            err("Input list syntax");
            return;
        }
    }
}

static void parseLogicalIF(char *s, Register reg, bool isFromLogIf) {
    IfStackEntry *entry;
    char label[8];
    Token token;

    s = getNextToken(s, &token);
    if (token.type == TokenType_Identifier && strcasecmp(token.details.identifier, "THEN") == 0) {
        if (isFromLogIf) {
            err("Block IF not allowed from logical IF");
            return;
        }
        if (ifStackPtr >= MAX_IF_STACK_SIZE) {
            err("Block IF nested too deeply");
            return;
        }
        entry = &ifStack[ifStackPtr++];
        generateLabel(entry->ifEndLabel);
        generateLabel(entry->blockEndLabel);
        emitBranchOnFalse(reg, entry->blockEndLabel);
        verifyEOS(s);
    }
    else {
        generateLabel(label);
        emitBranchOnFalse(reg, label);
        freeRegister(reg);
        if (token.type == TokenType_Keyword) {
// TODO: finish implementation
            switch (token.details.keyword.id) {
            case ASSIGN:
                parseASSIGN(s);
                break;
            case BACKSPACE:
                break;
            case BUFFERIN:
                break;
            case BUFFEROUT:
                break;
            case CALL:
                break;
            case CLOSE:
                break;
            case CONTINUE:
                break;
            case DECODE:
                break;
            case ENCODE:
                break;
            case ENDFILE:
                break;
            case GOTO:
                parseGOTO(s);
                break;
            case IF:
                parseIF(s, TRUE);
                break;
            case OPEN:
                break;
            case PAUSE:
                break;
            case PRINT:
                parsePRINT(s);
                break;
            case PUNCH:
                break;
            case READ:
                parseREAD(s);
                break;
            case RETURN:
                break;
            case REWIND:
                break;
            case SAVE:
                break;
            case WRITE:
                parseWRITE(s);
                break;
            default:
                break;
            }
        }
        else if (token.type == TokenType_Identifier) {
            parseAssignment(s, token.details.identifier);
        }
        else {
            err("Invalid IF syntax");
        }
        emitLabel(label);
    }
}

static void parseOutputList(char *s) {
    OperatorArgument arg;
    Token *expression;
    int listOrd;
    OperatorArgument result;

    listOrd = 0;
    arg.class = ArgClass_Constant;
    arg.details.constant.dt.type = BaseType_Integer;
    arg.details.constant.dt.rank = 0;
    for (;;) {
        s = parseExpression(s, &expression);
        if (expression == NULL) {
            err("Expression syntax");
            return;
        }
        arg.details.constant.value.integer = listOrd++;
        emitLoadConst(&arg);
        emitStoreStack(arg.reg, 2);
        freeRegister(arg.reg);
        if (evaluateExpression(expression, &result) == FALSE) {
            if (isConstant(result)) {
                emitLoadConst(&result);
            }
            else if (isLoadable(result)) {
                emitLoadVar(&result);
            }
            emitStoreStack(result.reg, 3);
            freeRegister(result.reg);
            emitPrimCall("_outfmt");
        }
        freeToken(expression);
        s = eatWsp(s);
        if (*s == '\0') {
            break;
        }
        else if (*s == ',') {
            s += 1;
        }
        else {
            err("Output list syntax");
            return;
        }
    }
}

static char *parseTypeDecl(char *s, DataType *dt) {
    char *id;
    Symbol *symbol;
    Token token;

    for (;;) {
        s = getNextToken(s, &token);
        if (token.type == TokenType_Identifier) {
            id = token.details.identifier;
            symbol = findSymbol(id);
            if (symbol != NULL) {
                err("Duplicate declaration of %s", id);
                break;
            }
            symbol = defineLocalVariable(id, dt);
            if (dt->type == BaseType_Character) {
                s = eatWsp(s);
                if (*s == '*') {
                    s = parseCharConstraint(s, &symbol->details.variable.dt);
                }
            }
            symbol->details.variable.dt.rank = 0;
            s = eatWsp(s);
            if (*s == '\0') {
                break;
            }
            else if (*s == ',') {
                s = eatWsp(s + 1);
            }
            else if (*s == '(') {
                s = parseDimDecl(s + 1, symbol);
                s = eatWsp(s);
                if (*s == ',')
                    s = eatWsp(s + 1);
                else if (*s == '\0')
                    break;
            }
            else {
                err("Invalid type declaration");
                break;
            }
        }
        else {
            err("Invalid type declaration");
            break;
        }
    }
    return s;
}

static char *parseUnitAndFmt(char *s, int defaultUnit) {
    Token *expression;
    OperatorArgument unit;

    s = eatWsp(s);
    if (*s != '(') {
        err("Missing '('");
        return NULL;
    }
    s = eatWsp(s + 1);
    if (*s == '*') {
        unit.class = ArgClass_Constant;
        unit.details.constant.dt.type = BaseType_Integer;
        unit.details.constant.dt.rank = 0;
        unit.details.constant.value.integer = defaultUnit;
        s += 1;
    }
    else {
        s = parseExpression(s, &expression);
        if (expression == NULL) {
            err("Invalid unit number expression");
            return NULL;
        }
        if (evaluateExpression(expression, &unit)) {
            freeToken(expression);
            return NULL;
        }
        freeToken(expression);
        if (getDataType(&unit)->type != BaseType_Integer) {
            err("Unit must be type integer");
            return NULL;
        }
    }
    emitAdjustSP(-4);
    if (isConstant(unit)) {
        emitLoadConst(&unit);
    }
    else if (isLoadable(unit)) {
        emitLoadVar(&unit);
    }
    emitStoreStack(unit.reg, 1);
    freeRegister(unit.reg);

    s = eatWsp(s);
    if (*s != ',') {
        err("Invalid format specification");
        return NULL;
    }

    s = parseFmtSpec(s + 1);
    emitAdjustSP(1);

    if (s == NULL) return NULL;

    emitStoreStack(RESULT_REG, 0);

    s = eatWsp(s);
    if (*s != ')') {
        err("Missing ')' after format specifier");
        return NULL;
    }

    return s + 1;
}

static void parseASSIGN(char *s) {
    DataType dt;
    char *id;
    Symbol *labelSym;
    char lineLabel[8];
    Register reg;
    Symbol *sym;
    char to[2];
    Token token;
    BaseType type;

    s = getLabel(s, lineLabel);
    if (s == NULL) {
        err("Invalid line label");
        return;
    }
    labelSym = findLabel(lineLabel);
    if (labelSym != NULL) {
        if (labelSym->details.label.class != StmtClass_Executable && labelSym->details.label.class != StmtClass_Format) {
            err("Label does not reference executable or FORMAT statement");
            return;
        }
    }
    else {
        labelSym = addLabel(lineLabel);
        labelSym->details.label.class = StmtClass_None;
        labelSym->details.label.forwardRef = TRUE;
    }
    s = getNextChar(s);
    to[0] = *s;
    if (*s != '\0') s = getNextChar(s + 1);
    to[1] = *s;
    if (strncasecmp(to, "TO", 2) != 0) {
        err("Invalid ASSIGN syntax");
        return;
    }
    s = getNextToken(s + 1, &token);
    if (token.type != TokenType_Identifier) {
        err("Invalid target of ASSIGN");
        return;
    }
    id = token.details.identifier;
    sym = findSymbol(id);
    if (sym == NULL) {
        dt = implicitTypes[toupper(id[0]) - 'A'];
        if (dt.type != BaseType_Integer) {
            err("Invalid type of ASSIGN variable: %s", baseTypeToStr(dt.type));
            return;
        }
        dt.type = BaseType_Label;
        sym = defineLocalVariable(id, &dt);
        localOffset -= calculateSize(sym);
        sym->details.variable.offset = localOffset;
    }
    switch (sym->class) {
    case SymClass_Local:
    case SymClass_Global:
    case SymClass_Argument:
        type = sym->details.variable.dt.type;
        break;
    case SymClass_Pointee:
        type = sym->details.pointee.dt.type;
        break;
    default:
        err("Invalid ASSIGN target");
        return;
    }
    if (type != BaseType_Label && type != BaseType_Integer) {
        err("Invalid type of ASSIGN variable: %s", baseTypeToStr(dt.type));
        return;
    }
    reg = emitLoadVarAddr(labelSym);
    emitStoreReg(sym, reg);
    freeRegister(reg);
    verifyEOS(s);
}

static void parseBLOCKDATA(char *s) {
}

static void parseCOMMON(char *s) {
}

static void parseDATA(char *s) {
}

static void parseDIMENSION(char *s) {
}

static void parseDO(char *s) {
    DoStackEntry *entry;
    Token *expression;
    char *id;
    bool isIncr1;
    char lineLabel[8];
    int rank;
    Register reg1, reg2;
    OperatorArgument result;
    Symbol *sym;
    Token token;
    BaseType type;

    s = getLabel(s, lineLabel);
    if (s == NULL) {
        err("Missing or invalid DO termination label");
        return;
    }
    sym = findLabel(lineLabel);
    if (sym == NULL) {
        sym = addLabel(lineLabel);
        sym->details.label.class = StmtClass_Do_Term;
        sym->details.label.forwardRef = TRUE;
    }
    else if (sym->details.label.forwardRef == FALSE || sym->details.label.class != StmtClass_Do_Term) {
        err("Invalid DO termination label");
        return;
    }
    if (doStackPtr >= MAX_DO_STACK_SIZE) {
        err("DO nested too deeply");
        return;
    }
    entry = &doStack[doStackPtr];
    entry->termLabelSym = sym;
    generateLabel(entry->startLabel);
    generateLabel(entry->endLabel);
    s = eatWsp(s);
    if (*s == ',') s += 1;
    s = getNextToken(s, &token);
    if (token.type != TokenType_Identifier) {
        err("Missing or invalid DO loop variable");
        return;
    }
    id = token.details.identifier;
    sym = findSymbol(id);
    if (sym == NULL) {
        sym = defineLocalVariable(id, &implicitTypes[toupper(id[0]) - 'A']);
        localOffset -= calculateSize(sym);
        sym->details.variable.offset = localOffset;
    }
    switch (sym->class) {
    case SymClass_Local:
    case SymClass_Global:
    case SymClass_Argument:
        type = sym->details.variable.dt.type;
        rank = sym->details.variable.dt.rank;
        break;
    case SymClass_Pointee:
        type = sym->details.pointee.dt.type;
        rank = sym->details.pointee.dt.rank;
        break;
    default:
        type = BaseType_Undefined;
        break;
    }
    if (type == BaseType_Undefined || rank > 0) {
        err("Invalid DO loop variable");
        return;
    }
    entry->loopVariable = sym;
    entry->loopVariableType = type;
    s = eatWsp(s);
    if (*s != '=') {
        err("Invalid DO syntax");
        return;
    }

    emitAdjustSP(-3);

    /*
     *  Parse initial value
     */
    s = parseExpression(s + 1, &expression);
    if (expression == NULL) {
        err("Expression syntax");
        return;
    }

    if (evaluateExpression(expression, &result)) {
        freeToken(expression);
        return;
    }
    if (coerceArgument(&result, getDataType(&result)->type, type) == BaseType_Undefined) {
        err("Invalid type conversion");
    }
    if (isConstant(result)) {
        emitLoadConst(&result);
    }
    else if (isLoadable(result)) {
        emitLoadVar(&result);
    }
    emitStoreStack(result.reg, 0);
    freeRegister(result.reg);
    /*
     *  Parse limit value
     */
    s = eatWsp(s);
    if (*s != ',') {
        err("Invalid DO syntax");
        return;
    }
    s = parseExpression(s + 1, &expression);
    if (expression == NULL) {
        err("Expression syntax");
        return;
    }
    if (evaluateExpression(expression, &result)) {
        freeToken(expression);
        return;
    }
    if (coerceArgument(&result, getDataType(&result)->type, type) == BaseType_Undefined) {
        err("Invalid type conversion");
    }
    if (isConstant(result)) {
        emitLoadConst(&result);
    }
    else if (isLoadable(result)) {
        emitLoadVar(&result);
    }
    emitStoreStack(result.reg, 1);
    freeRegister(result.reg);
    /*
     *  Parse increment value, if provided
     */
    s = eatWsp(s);
    if (*s == ',') {
        s = parseExpression(s + 1, &expression);
        if (expression == NULL) {
            err("Expression syntax");
            return;
        }
        if (evaluateExpression(expression, &result)) {
            freeToken(expression);
            return;
        }
    }
    else {
        result.class = ArgClass_Constant;
        result.details.constant.dt.type = BaseType_Integer;
        result.details.constant.dt.rank = 0;
        result.details.constant.value.integer = 1;
    }

    isIncr1 = (isIntegerConstant(result) && result.details.constant.value.integer == 1)
        || (isRealConstant(result) && result.details.constant.value.real == 1.0);

    if (coerceArgument(&result, getDataType(&result)->type, type) == BaseType_Undefined) {
        err("Invalid type conversion");
    }
    if (isConstant(result)) {
        emitLoadConst(&result);
    }
    else if (isLoadable(result)) {
        emitLoadVar(&result);
    }
    emitStoreStack(result.reg, 2);
    reg1 = emitLoadStack(0);
    reg2 = emitLoadStack(1);
    if (isIncr1) {
        freeRegister(result.reg);
        emitCalcTrip1(reg1, reg2, type);
        emitStoreStack(reg2, 1);
        freeRegister(reg1);
        freeRegister(reg2);
    }
    else {
        emitCalcTrip(reg1, reg2, result.reg, type);
        emitStoreStack(result.reg, 1);
        freeRegister(result.reg);
        freeRegister(reg1);
        freeRegister(reg2);
    }
    emitLabel(entry->startLabel);
    reg1 = emitLoadStack(0);
    emitStoreReg(entry->loopVariable, reg1);
    freeRegister(reg1);
    emitBranchIfEndTrips(entry->endLabel);

    doStackPtr += 1;
    verifyEOS(s);
}

static void parseELSE(char *s) {
    IfStackEntry *entry;

    if (ifStackPtr < 1) {
        err("ELSE without IF");
        return;
    }
    entry = &ifStack[ifStackPtr - 1];
    emitBranch(entry->ifEndLabel);
    emitLabel(entry->blockEndLabel);
    entry->blockEndLabel[0] = '\0';
    verifyEOS(s);
}

static void parseELSEIF(char *s) {
    IfStackEntry *entry;
    Token *expression;
    OperatorArgument result;
    Token token;

    if (ifStackPtr < 1) {
        err("ELSEIF without IF");
        return;
    }
    entry = &ifStack[ifStackPtr - 1];
    emitBranch(entry->ifEndLabel);
    emitLabel(entry->blockEndLabel);
    entry->blockEndLabel[0] = '\0';

    s = eatWsp(s);
    if (*s != '(') {
        err("Missing '(' after ELSEIF");
        return;
    }
    s = parseExpression(s + 1, &expression);
    if (expression == NULL) {
        err("ELSEIF expression syntax");
        return;
    }
    else if (*s != ')') {
        err("Missing closing ')' after ELSEIF");
        freeAllRegisters();
        freeToken(expression);
        return;
    }
    s += 1;
    if (evaluateExpression(expression, &result)) {
        freeToken(expression);
        return;
    }
    if (isConstant(result)) {
        emitLoadConst(&result);
    }
    else if (isLoadable(result)) {
        emitLoadVar(&result);
    }
    if (getDataType(&result)->type == BaseType_Logical) {
        s = getNextToken(s, &token);
        if (token.type == TokenType_Identifier && strcasecmp(token.details.identifier, "THEN") == 0) {
            generateLabel(entry->blockEndLabel);
            emitBranchOnFalse(result.reg, entry->blockEndLabel);
            verifyEOS(s);
        }
        else {
            err("Invalid ELSEIF syntax");
        }
    }
    else {
        err("Invalid type of ELSEIF expression");
    }
    freeRegister(result.reg);
    freeToken(expression);
}

static void parseEND(char *s) {

    emitEpilog(progUnitSym, -localOffset);

    if (ifStackPtr > 0) err("Missing ENDIF");
    if (doStackPtr > 0) err("Missing DO termination label %s", doStack[doStackPtr - 1].termLabelSym->identifier);

    if (errorCount + warningCount > 0 && listingFile != NULL) fputs("\n\n", listingFile);

    if (errorCount > 0) {
        list(" ***** %d error%s\n", errorCount, (errorCount > 1) ? "s" : "");
        fprintf(stderr, "%d error%s\n", errorCount, (errorCount > 1) ? "s" : "");
    }
    if (warningCount > 0) {
        list(" ***** %d warning%s\n", warningCount, (warningCount > 1) ? "s" : "");
        fprintf(stderr, "%d warning%s\n", warningCount, (warningCount > 1) ? "s" : "");
    }
    printSymbols(listingFile);
    freeAllSymbols();
}

static void parseENDIF(char *s) {
    IfStackEntry *entry;

    if (ifStackPtr < 1) {
        err("ENDIF without IF");
        return;
    }
    entry = &ifStack[--ifStackPtr];
    if (entry->blockEndLabel[0] != '\0') emitLabel(entry->blockEndLabel);
    emitLabel(entry->ifEndLabel);
    verifyEOS(s);
}

static void parseENTRY(char *s) {
}

static void parseEQUIVALENCE(char *s) {
}

static void parseEXTERNAL(char *s) {
}

static void parseFORMAT(char *s) {
    char *end;
    char *start;

    if (currentLabel == NULL) {
        err("Line label missing on FORMAT");
        return;
    }
    s = eatWsp(s);
    if (*s != '(') {
        err("FORMAT does not start with '('");
        return;
    }
    start = s;
    while (*s != '\0') {
        if (!isspace(*s)) end = s;
        s += 1;
    }
    if (*end != ')') {
        err("FORMAT does not end with '('");
        return;
    }
    emitString(start, currentLabel->details.label.label);
}

static void parseFUNCTION(char *s) {
    Symbol *symbol;
    Token token;

    s = getNextToken(s, &token);
    if (token.type == TokenType_Identifier) {
        symbol = addSymbol(token.details.identifier, SymClass_Function);
        if (symbol == NULL) {
            err("Function name not unique");
            return;
        }
        progUnitSym = symbol;
        emitProlog(progUnitSym);
    }
    else {
        err("Incorrect function name");
    }
}

static void parseGOTO(char *s) {
    DataType dt;
    Token *expression;
    char *id;
    char lineLabel[8];
    int n;
    bool ok;
    OperatorArgument result;
    Symbol *sym;
    char tableLabel[8];
    Token token;
    BaseType type;

    ok = TRUE;
    s = eatWsp(s);
    if (isdigit(*s)) {
        s = getLabel(s, lineLabel);
        if (s == NULL) {
            err("Invalid line label");
            return;
        }
        sym = findLabel(lineLabel);
        if (sym == NULL) {
            sym = addLabel(lineLabel);
            sym->details.label.class = StmtClass_Executable;
            sym->details.label.forwardRef = TRUE;
        }
        emitBranch(sym->details.label.label);
    }
    else if (*s == '(') {
        emitActivateSection("DATA", "DATA");
        generateLabel(tableLabel);
        emitLabel(tableLabel);
        n = 0;
        for (;;) {
            s = getLabel(s + 1, lineLabel);
            if (s == NULL) {
                err("Invalid line label");
                ok = FALSE;
                break;
            }
            sym = findLabel(lineLabel);
            if (sym == NULL) {
                sym = addLabel(lineLabel);
                sym->details.label.class = StmtClass_Executable;
                sym->details.label.forwardRef = TRUE;
            }
            emitLabelDatum(sym->details.label.label);
            n += 1;
            if (*s == ')') {
                break;
            }
            else if (*s != ',') {
                err("Invalid computed GOTO syntax");
                ok = FALSE;
                break;
            }
        }
        emitDeactivateSection("DATA");
        if (ok) {
            s = eatWsp(s + 1);
            if (*s == ',') s += 1;
            s = parseExpression(s, &expression);
            if (expression == NULL) {
                err("Invalid computed GOTO expression syntax");
                return;
            }
            if (evaluateExpression(expression, &result)) {
                freeToken(expression);
                return;
            }
            if (isConstant(result)) {
                emitLoadConst(&result);
            }
            else if (isLoadable(result)) {
                emitLoadVar(&result);
            }
            type = getDataType(&result)->type;
            if (type != BaseType_Integer) {
                err("Invalid type of GOTO expression: %s", baseTypeToStr(type));
                return;
            }
            emitBranchIndexed(tableLabel, n, result.reg);
            freeRegister(result.reg);
            freeToken(expression);
        }
    }
    else {
        s = getNextToken(s, &token);
        if (token.type != TokenType_Identifier) {
            err("Invalid target of GOTO");
            return;
        }
        id = token.details.identifier;
        sym = findSymbol(id);
        if (sym == NULL) {
            dt = implicitTypes[toupper(id[0]) - 'A'];
            if (dt.type != BaseType_Integer) {
                err("Invalid type of assigned GOTO variable: %s", baseTypeToStr(dt.type));
                return;
            }
            dt.type = BaseType_Label;
            sym = defineLocalVariable(id, &dt);
            localOffset -= calculateSize(sym);
            sym->details.variable.offset = localOffset;
        }
        switch (sym->class) {
        case SymClass_Local:
        case SymClass_Global:
        case SymClass_Argument:
            type = sym->details.variable.dt.type;
            break;
        case SymClass_Pointee:
            type = sym->details.pointee.dt.type;
            break;
        default:
            err("Invalid assigned GOTO target");
            return;
        }
        if (type != BaseType_Label && type != BaseType_Integer) {
            err("Invalid type of assigned GOTO variable: %s", baseTypeToStr(dt.type));
            return;
        }
        if (evaluateExpression(&token, &result)) return;
        emitLoadVar(&result);
        emitBranchReg(result.reg);
        freeRegister(result.reg);
        s = eatWsp(s);
        if (*s == ',') s = eatWsp(s + 1);
        if (*s == '(') {
            /*
             * Parse and ignore optional label list
             */
            for (;;) {
                s = getLabel(s + 1, lineLabel);
                if (s == NULL) {
                    err("Invalid line label");
                    return;
                }
                else if (*s == ',') {
                    s += 1;
                }
                else if (*s == ')') {
                    s += 1;
                    break;
                }
            }
        }
    }
    if (ok) verifyEOS(s);
}

static void parseIF(char *s, bool isFromLogIf) {
    Token *expression;
    OperatorArgument result;
    BaseType type;

    s = eatWsp(s);
    if (*s != '(') {
        err("Missing '(' after IF");
        return;
    }
    s = parseExpression(s + 1, &expression);
    if (expression == NULL) {
        err("IF expression syntax");
        return;
    }
    else if (*s != ')') {
        err("Missing closing ')' after IF");
        freeAllRegisters();
        freeToken(expression);
        return;
    }
    s += 1;

    if (evaluateExpression(expression, &result)) {
        freeToken(expression);
        return;
    }
    if (isConstant(result)) {
        emitLoadConst(&result);
    }
    else if (isLoadable(result)) {
        emitLoadVar(&result);
    }
    type = getDataType(&result)->type;
    switch (type) {
    case BaseType_Logical:
        parseLogicalIF(s, result.reg, isFromLogIf);
        break;
    case BaseType_Integer:
    case BaseType_Real:
    case BaseType_Double:
        parseArithmeticIF(s, result.reg);
        break;
    default:
        err("Invalid type of IF expression: %s", baseTypeToStr(type));
        break;
    }
    freeRegister(result.reg);
    freeToken(expression);
}

static void parseIMPLICIT(char *s) {
    char first;
    char last;
    bool ok;
    DataType dt;
    Token token;

    ok = TRUE;
    while (ok) {
        s = eatWsp(s);
    	s = getNextToken(s, &token);
        s = parseDataType(s, &token, &dt);
        if (dt.type == BaseType_Undefined) {
            err("Data type missing");
            return;
        }
        s = eatWsp(s);
        if (*s != '(') ok = FALSE;
        while (ok) {
            s = eatWsp(s + 1);
            first = toupper(*s);
            if (first < 'A' || first > 'Z') {
                ok = FALSE;
                break;
            }
            s = eatWsp(s + 1);
            if (*s == '-') {
                s = eatWsp(s + 1);
                last = toupper(*s);
                if (last < 'A' || last > 'Z') {
                    ok = FALSE;
                    break;
                }
                s += 1;
            }
            else {
                last = first;
            }
            if (last < first) {
                err("Incorrect IMPLICIT range");
                return;
            }
            while (first <= last) {
                implicitTypes[first - 'A'] = dt;
                first += 1;
            }
            s = eatWsp(s);
            if (*s == ')') {
                s = eatWsp(s + 1);
                if (*s == ',') {
                    s += 1;
                    break;
                }
                else if (*s == '\0') {
                    return;
                }
                else {
                    ok = FALSE;
                }
            }
            else if (*s != ',') {
                ok = FALSE;
            }
        }
    }

    err("Incorrect IMPLICIT declaration");
}

static void parseIMPLICITNONE(char *s) {
    int i;
    Token token;

    for (i = 0; i < 26; i++) {
        implicitTypes[i].type = BaseType_Undefined;
    }
    s = getNextToken(s, &token);
    if (token.type != TokenType_None) err("Incorrect IMPLICIT NONE declaration");
}

static void parseINCLUDE(char *s) {
}

static void parseINTRINSIC(char *s) {
}

static void parseNAMELIST(char *s) {
}

static void parsePARAMETER(char *s) {
}

static void parsePRINT(char *s) {
    OperatorArgument unit;

    s = parseFmtSpec(s);

    if (s == NULL) return;

    emitAdjustSP(-3);
    emitStoreStack(RESULT_REG, 0);

    s = eatWsp(s);
    if (*s == ',') {
        unit.class = ArgClass_Constant;
        unit.details.constant.dt.type = BaseType_Integer;
        unit.details.constant.dt.rank = 0;
        unit.details.constant.value.integer = DEFAULT_OUTPUT_UNIT;
        emitLoadConst(&unit);
        emitStoreStack(unit.reg, 1);
        freeRegister(unit.reg);
        parseOutputList(s + 1);
    }
    else {
        err("Comma missing after format specifier");
    }
    emitPrimCall("_endfmt");
    emitAdjustSP(4);
}

static void parsePROGRAM(char *s) {
    Symbol *symbol;
    Token token;

    s = getNextToken(s, &token);
    if (token.type == TokenType_Identifier) {
        symbol = addSymbol(token.details.identifier, SymClass_Program);
        if (symbol == NULL) {
            err("Program name not unique");
        }
        s = eatWsp(s);
        if (*s == '(') {
            for (;;) {
                s = getNextToken(s + 1, &token);
                if (token.type != TokenType_Identifier) {
                    err("Incorrect PROGRAM statement");
                    return;
                }
                s = eatWsp(s);
                if (*s != ',') break;
            }
            if (*s != ')') {
                err("Incorrect PROGRAM statement");
                return;
            }
        }
        else if (*s != '\0') {
            err("Incorrect PROGRAM statement");
            return;
        }
        progUnitSym = symbol;
        emitProlog(progUnitSym);
    }
    else {
        err("Incorrect program name");
    }
}

static void parseREAD(char *s) {
    char *cp;
    char *id;
    char *start;
    Symbol *symbol;
    Token token;

    start = s;
    cp = NULL;
    while (*s != '\0') {
        if (*s == ')') cp = s;
        s += 1;
    }
    if (cp != NULL) {
        /*
         *  Pre-parse the input list to allocate space for and define any undeclared
         *  local variables that might be referenced in it.
         */
        s = cp + 1;
        for (;;) {
            s = getNextToken(s, &token);
            if (token.type != TokenType_Identifier) {
                err("Input list item is not a variable");
                return;
            }
            id = token.details.identifier;
            symbol = findSymbol(id);
            if (symbol == NULL) {
                symbol = defineLocalVariable(id, &implicitTypes[toupper(id[0]) - 'A']);
                localOffset -= calculateSize(symbol);
                symbol->details.variable.offset = localOffset;
            }
            s = eatWsp(s);
            if (*s == '\0') {
                break;
            }
            else if (*s == ',') {
                s += 1;
            }
            else {
                err("Input list syntax");
                return;
            }
        }
    }
    s = parseUnitAndFmt(start, DEFAULT_INPUT_UNIT);
    if (s == NULL) return;
    parseInputList(s);
    emitPrimCall("_endfmt");
    emitAdjustSP(4);
}

static void parseSAVE(char *s) {
}

static void parseSUBROUTINE(char *s) {
    Symbol *symbol;
    Token token;

    s = getNextToken(s, &token);
    if (token.type == TokenType_Identifier) {
        symbol = addSymbol(token.details.identifier, SymClass_Subroutine);
        if (symbol == NULL) {
            err("Subroutine name not unique");
            return;
        }
        progUnitSym = symbol;
        emitProlog(progUnitSym);
    }
    else {
        err("Incorrect subroutine name");
    }
}

static void parseWRITE(char *s) {
    s = parseUnitAndFmt(s, DEFAULT_OUTPUT_UNIT);
    if (s == NULL) return;
    parseOutputList(s);
    emitPrimCall("_endfmt");
    emitAdjustSP(4);
}

static void popArg(OperatorArgument *arg) {
    if (argStkPtr > 0) {
        argStkPtr -= 1;
        if (arg != NULL) {
            *arg = argStack[argStkPtr];
        }
    }
    else {
        fputs("Argument stack underflow\n", stderr);
        exit(1);
    }
}

static void popOp(OperatorDetails *op) {
    if (opStkPtr > 0) {
        opStkPtr -= 1;
        if (op != NULL) {
            *op = opStack[opStkPtr];
        }
    }
    else {
        fputs("Operator stack underflow\n", stderr);
        exit(1);
    }
}

static void presetImplicit(void) {
    char c;

    for (c = 'A'; c < 'I'; c++) implicitTypes[c - 'A'].type = BaseType_Real;
    for (c = 'I'; c < 'O'; c++) implicitTypes[c - 'A'].type = BaseType_Integer;
    for (c = 'O'; c < 'Z'; c++) implicitTypes[c - 'A'].type = BaseType_Real;
}

static void presetProgUnit(void) {
    progUnitSym = NULL;
    state = STATE_PROG_UNIT;
    doStackPtr   = 0;
    ifStackPtr   = 0;
    errorCount   = 0;
    localOffset  = 0;
    warningCount = 0;
}

static void pushArg(OperatorArgument *arg) {
    if (argStkPtr < MAX_ARG_STACK_SIZE) {
        argStack[argStkPtr++] = *arg;
    }
    else {
        fputs("Argument stack overflow\n", stderr);
        exit(1);
    }
}

static void pushOp(OperatorDetails *op) {
    if (opStkPtr < MAX_OP_STACK_SIZE) {
        opStack[opStkPtr++] = *op;
    }
    else {
        fputs("Operator stack overflow\n", stderr);
        exit(1);
    }
}

static char *readLine(int *lineLength) {
    int c;
    int len;
    char *lp;

    lp = fgets(lineBuf, sizeof(lineBuf), sourceFile);
    if (lp == NULL) return NULL;
    len = strlen(lineBuf);
    if (len < 8) { // valid lines are at least 7 characters long
        if (len > 0 && lineBuf[len - 1] == '\n') lineBuf[len - 1] = ' ';
        while (len < 7) lineBuf[len++] = ' ';
        lineBuf[len] = '\0';
    }
    else if (lineBuf[len - 1] != '\n') { // if line is outrageously long or doesn't have EOL
        for (;;) {
            c = fgetc(sourceFile);
            if (c == -1 || c == '\n') break;
        }
    }
    else {
        lineBuf[--len] = '\0';
    }
    while (len > 0 && lineBuf[len - 1] == ' ') len -= 1; // trim trailing blanks
    if (len < 7) len = 7;
    lineBuf[len] = '\0';
    *lineLength = len;
    return lineBuf;
}

static void verifyEOS(char *s) {
    while (*s != '\0') {
        if (!isspace(*s++)) {
            err("Unexpected text at end of statement");
            return;
        }
    }
}

static void warn(char *format, ...) {
    va_list ap;
    char buf[80];

    va_start(ap, format);
    vsprintf(buf, format, ap);
    va_end(ap);
    list(" *WARNING* %s\n", buf);
    fprintf(stderr, "WARNING line %d : %s\n", lineNo, buf);
    warningCount += 1;
}

static char *baseTypeToStr(BaseType type) {
    switch (type) {
    case BaseType_Undefined: return "Undefined";
    case BaseType_Character: return "Character";
    case BaseType_Logical:   return "Logical";
    case BaseType_Integer:   return "Integer";
    case BaseType_Real:      return "Real";
    case BaseType_Double:    return "Double";
    case BaseType_Complex:   return "Complex";
    case BaseType_Label:     return "Label";
    case BaseType_Pointer:   return "Pointer";
    default:                 return "unknown";
    }
}

static char *opIdToStr(OperatorId id) {
    switch (id) {
    case OP_DIV:          return "/";
    case OP_SUB:          return "-";
    case OP_NEG:          return "-";
    case OP_ADD:          return "+";
    case OP_PLUS:         return "+";
    case OP_EXP  :        return "**";
    case OP_MUL:          return "*";
    case OP_CAT:          return "//";
    case OP_AND:          return ".AND.";
    case OP_EQ:           return ".EQ.";
    case OP_EQV:          return ".EQV.";
    case OP_GE:           return ".GE.";
    case OP_GT:           return ".GT.";
    case OP_LE:           return ".LE.";
    case OP_LT:           return ".LT.";
    case OP_NE:           return ".NE.";
    case OP_NEQV:         return ".NEQV.";
    case OP_NOT:          return ".NOT.";
    case OP_OR:           return ".OR.";
    case OP_SEXPR:        return "(";
    default:              return "unknown";
    }
}

#if DEBUG

static void printExpression(FILE *f, Token *expression) {
    if (expression != NULL) {
        if (expression->type == TokenType_Operator) {
            printExpression(f, expression->details.operator.leftArg);
            printToken(f, expression);
            printExpression(f, expression->details.operator.rightArg);
        }
        else {
            printToken(f, expression);
        }
    }
}

static void printToken(FILE *f, Token *token) {
    if (token != NULL) {
        switch (token->type) {
        case TokenType_Keyword:
            fputs(tokenIdToStr(token->details.keyword.id), f);
            break;
        case TokenType_Identifier:
            fputs(token->details.identifier, f);
            break;
        case TokenType_Operator:
            fputs(opIdToStr(token->details.operator.id), f);
            break;
        case TokenType_Constant:
            switch (token->details.constant.dt.type) {
            case BaseType_Character:
                fprintf(f, "'%s'", token->details.constant.value.chr.string);
                break;
            case BaseType_Logical:
                break;
            case BaseType_Integer:
                fprintf(f, "%ld", token->details.constant.value.integer);
                break;
            case BaseType_Real:
                fprintf(f, "%f", token->details.constant.value.real);
                break;
            case BaseType_Double:
            case BaseType_Complex:
            case BaseType_Label:
            case BaseType_Pointer:
            default:
                break;
            }
            break;
        case TokenType_None:
            break;
        case TokenType_Invalid:
            fputs("-- invalid --", f);
            break;
        default:
            break;
        }
    }
}

static char *argClassToStr(ArgumentClass class) {
    switch (class) {
    case ArgClass_Constant:    return "Constant";
    case ArgClass_Calculation: return "Calculation";
    case ArgClass_Function:    return "Function";
    case ArgClass_Local:       return "Local";
    case ArgClass_Global:      return "Global";
    case ArgClass_Argument:    return "Argument";
    case ArgClass_Pointee:     return "Pointee";
    default:                   return "unknown";
    }
}

static char *tokenIdToStr(TokenId id) {
    switch (id) {
    case UNDEFINED:       return "";
    case ASSIGN:          return "ASSIGN";
    case BACKSPACE:       return "BACKSPACE";
    case BLOCKDATA:       return "BLOCKDATA";
    case BUFFERIN:        return "BUFFERIN";
    case BUFFEROUT:       return "BUFFEROUT";
    case CALL:            return "CALL";
    case CHARACTER:       return "CHARACTER";
    case CLOSE:           return "CLOSE";
    case COMMON:          return "COMMON";
    case COMPLEX:         return "COMPLEX";
    case CONTINUE:        return "CONTINUE";
    case DATA:            return "DATA";
    case DECODE:          return "DECODE";
    case DIMENSION:       return "DIMENSION";
    case DO:              return "DO";
    case DOUBLEPRECISION: return "DOUBLEPRECISION";
    case ELSE:            return "ELSE";
    case ELSEIF:          return "ELSEIF";
    case ENCODE:          return "ENCODE";
    case END:             return "END";
    case ENDFILE:         return "ENDFILE";
    case ENDIF:           return "ENDIF";
    case ENTRY:           return "ENTRY";
    case EQUIVALENCE:     return "EQUIVALENCE";
    case EXTERNAL:        return "EXTERNAL";
    case FORMAT:          return "FORMAT";
    case FUNCTION:        return "FUNCTION";
    case GOTO:            return "GOTO";
    case IF:              return "IF";
    case IMPLICIT:        return "IMPLICIT";
    case IMPLICITNONE:    return "IMPLICITNONE";
    case INCLUDE:         return "INCLUDE";
    case INQUIRE:         return "INQUIRE";
    case INTEGER:         return "INTEGER";
    case INTRINSIC:       return "INTRINSIC";
    case LOGICAL:         return "LOGICAL";
    case NAMELIST:        return "NAMELIST";
    case OPEN:            return "OPEN";
    case PARAMETER:       return "PARAMETER";
    case PAUSE:           return "PAUSE";
    case POINTER:         return "POINTER";
    case PRINT:           return "PRINT";
    case PROGRAM:         return "PROGRAM";
    case PUNCH:           return "PUNCH";
    case READ:            return "READ";
    case REAL:            return "REAL";
    case RETURN:          return "RETURN";
    case REWIND:          return "REWIND";
    case SAVE:            return "SAVE";
    case STOP:            return "STOP";
    case SUBROUTINE:      return "SUBROUTINE";
    case WRITE:           return "WRITE";
    default:              return "unknown";
    }
}

static char *tokenTypeToStr(TokenType type) {
    switch (type) {
    case TokenType_Keyword:    return "keyword";
    case TokenType_Identifier: return "identifier";
    case TokenType_Operator:   return "operator";
    case TokenType_Constant:   return "constant";
    case TokenType_None:       return "none";
    case TokenType_Invalid:    return "invalid";
    default:                   return "unknown";
    }
}

#endif
