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
static Token *createIntegerConstant(int value);
static void defineType(Symbol *symbol);
static char *eatWsp(char *s);
static void err(char *format, ...);
static bool evaluateArrayRef(Symbol *symbol, TokenListItem *subscriptList, OperatorArgument *offset);
static bool evaluateExpression(Token *expression, OperatorArgument *result);
static bool evaluateExprHelper(Token *expression);
static bool evaluateFmtSpec(ControlInfoList *ciList);
static bool evaluateFunction(Token *fn, Symbol *symbol);
static bool evaluateIdentifier(Token *id);
static bool evaluateStorageReference(StorageReference *reference, OperatorArgument *target, bool *isScalar);
static bool evaluateStringIndex(Token *expression, OperatorArgument *index);
static bool evaluateStringRange(Symbol *symbol, StringRange *range, OperatorArgument *offset, OperatorArgument *length);
static bool evaluateSubscript(Symbol *symbol, TokenListItem *subscriptList, int idx, OperatorArgument *subscript);
static bool executeOperator(OperatorId op);
static void freeControlInfoList(ControlInfoList *list);
static void freeStorageReference(StorageReference *reference);
static void freeStringRange(StringRange *range);
static void freeToken(Token *token);
static void freeTokenList(TokenListItem *item);
static DataType *getDataType(OperatorArgument *arg);
static char *getIntValue(char *s, int *value);
static char *getLabel(char *s, char *label);
static TokenListItem *getQualifier(TokenListItem *qualifierList, int idx);
static void inputFini(ControlInfoList *ciList);
static void inputInit(ControlInfoList *ciList);
static void list(char *format, ...);
static void loadValue(OperatorArgument *value);
static char *opIdToStr(OperatorId id);
static void outputFini(ControlInfoList *ciList);
static void outputInit(ControlInfoList *ciList);
static char *parseActualArguments(char *s, int *frameSize);
static void parseArithmeticIF(char *s, Register reg);
static void parseAssignment(char *s, Token *id);
static char *parseCharConstraint(char *s, Token *token, DataType *dt);
static char *parseControlInfoList(char *s, ControlInfoList **ciList, int defaultUnit);
static char *parseDataType(char *s, Token *token, DataType *type);
static char *parseDimDecl(char *s, Symbol *symbol);
static char *parseExpression(char *s, Token **expression);
static char *parseExpressionList(char *s, TokenListItem **list);
static char *parseFmtSpec(char *s, ControlInfoList *ciList);
static char *parseFormalArguments(char *s);
static void parseInputList(char *s, ControlInfoList *ciList);
static void parseLogicalIF(char *s, Register reg, bool isFromLogIf);
static void parseOutputList(char *s, ControlInfoList *ciList);
static void parseOutputStmt(char *s, int unitNum);
static char *parseStorageReference(char *s, Token *id, StorageReference *reference);
static char *parseStringRange(char *s, StringRange **range);
static char *parseTypeDecl(char *s, DataType *dt);
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
static void parseCALL(char *s);
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
static void parseFUNCTION(char *s, DataType *dt);
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
static void parsePUNCH(char *s);
static void parseREAD(char *s);
static void parseWRITE(char *s);

#if DEBUG
static char *argClassToStr(ArgumentClass class);
static void printExpression(FILE *f, Token *expression);
static void printToken(FILE *f, Token *token);
static char *tokenIdToStr(TokenId id);
static char *tokenTypeToStr(TokenType type);
#endif

static ArgumentClass argClassForSymClass[] = {
    ArgClass_Undefined,  /* SymClass_Undefined   */
    ArgClass_Undefined,  /* SymClass_Program     */
    ArgClass_Undefined,  /* SymClass_Subroutine  */
    ArgClass_Function,   /* SymClass_Function    */
    ArgClass_Undefined,  /* SymClass_BlockData   */
    ArgClass_Undefined,  /* SymClass_NamedCommon */
    ArgClass_Local,      /* SymClass_Local       */
    ArgClass_Global,     /* SymClass_Global      */
    ArgClass_Argument,   /* SymClass_Argument    */
    ArgClass_Constant,   /* SymClass_Parameter   */
    ArgClass_Pointee,    /* SymClass_Pointee     */
    ArgClass_Undefined   /* SymClass_Label       */
};
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
        s = getNextToken(s + 1, &token, TRUE);
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
                    parseFUNCTION(s, NULL);
                    continue;
                case PROGRAM:
                    parsePROGRAM(s);
                    continue;
                case SUBROUTINE:
                    parseSUBROUTINE(s);
                    continue;
                case CHARACTER:
                case COMPLEX:
                case INTEGER:
                case LOGICAL:
                case DOUBLEPRECISION:
                case POINTER:
                case REAL:
                    start = s;
                    s = parseDataType(s, &token, &dt);
                    s = eatWsp(s);
                    if (strncasecmp(s, "FUNCTION", 8) == 0) {
                        parseFUNCTION(s + 8, &dt);
                        continue;
                    }
                    else {
                        s = start;
                    }
                    break;
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
                case CALL:
                    parseCALL(s);
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
                    parsePUNCH(s);
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
                parseAssignment(s, &token);
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
    TokenListItem *newQ;
    TokenListItem *prvQ;
    TokenListItem *qualifier;

    if (token == NULL) return NULL;

    new = (Token *)allocate(sizeof(Token));
    memcpy(new, token, sizeof(Token));
    switch (token->type) {
    case TokenType_Identifier:
        if (token->details.identifier.name != NULL) {
            len = strlen(token->details.identifier.name);
            new->details.identifier.name = (char *)allocate(len + 1);
            memcpy(new->details.identifier.name, token->details.identifier.name, len);
        }
        for (qualifier = token->details.identifier.qualifiers, prvQ = NULL; qualifier != NULL; qualifier = qualifier->next) {
            newQ = (TokenListItem *)allocate(sizeof(TokenListItem));
            newQ->item = copyToken(qualifier->item);
            if (prvQ == NULL)
                new->details.identifier.qualifiers = newQ;
            else
                prvQ->next = newQ;
            prvQ = newQ;
        }
        if (token->details.identifier.range != NULL) {
            new->details.identifier.range = (StringRange *)allocate(sizeof(StringRange));
            new->details.identifier.range->first = copyToken(token->details.identifier.range->first);
            new->details.identifier.range->last = copyToken(token->details.identifier.range->last);
        }
        break;
    case TokenType_Constant:
        if (token->details.constant.dt.type == BaseType_Character) {
            len = strlen(token->details.constant.value.character.string);
            new->details.constant.value.character.string = (char *)allocate(len + 1);
            memcpy(new->details.constant.value.character.string, token->details.constant.value.character.string, len);
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

static Token *createIntegerConstant(int value) {
    Token *token;

    token = (Token *)allocate(sizeof(Token));
    token->type = TokenType_Constant;
    token->details.constant.dt.type = BaseType_Integer;
    token->details.constant.value.integer = value;
    return token;
}

static void defineType(Symbol *symbol) {
    if (symbol->details.variable.dt.type == BaseType_Undefined)
        symbol->details.variable.dt = implicitTypes[toupper(symbol->identifier[0]) - 'A'];
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

static bool evaluateArrayRef(Symbol *symbol, TokenListItem *subscriptList, OperatorArgument *offset) {
    void (*binop)(OperatorArgument *leftArg, OperatorArgument *rightArg);
    int d;
    OperatorArgument dim;
    int rank;
    OperatorArgument subscript;
    OperatorArgument sum;

    /*
     *  Initialize sum with last subscript
     */
    rank = symbol->details.variable.dt.rank;
    if (evaluateSubscript(symbol, subscriptList, rank - 1, &sum)) return TRUE;

    if (rank > 1) { // 2 or more dimensional array
        memset(&dim, 0, sizeof(OperatorArgument));
        dim.details.constant.dt.type = BaseType_Integer;
        for (d = rank - 2; d >= 0; d--) {
            dim.class = ArgClass_Constant;
            dim.details.constant.value.integer = (symbol->details.variable.dt.bounds[d].upper - symbol->details.variable.dt.bounds[d].lower) + 1;
            if (isConstant(sum)) {
                sum.details.constant.value.integer *= dim.details.constant.value.integer;
            }
            else {
                emitLoadConst(&dim);
                binop = genBinOps[OP_MUL - OP_ADD][BaseType_Integer];
                if (isLoadable(sum)) emitLoadValue(&sum);
                (*binop)(&dim, &sum);
                freeRegister(dim.reg);
            }
            if (evaluateSubscript(symbol, subscriptList, d, &subscript)) {
                if (isCalculation(sum)) freeRegister(sum.reg);
                return TRUE;
            }
            if (isConstant(subscript) && isConstant(sum)) {
                sum.details.constant.value.integer += subscript.details.constant.value.integer;
            }
            else {
                loadValue(&subscript);
                loadValue(&sum);
                binop = genBinOps[OP_ADD - OP_ADD][BaseType_Integer];
                (*binop)(&subscript, &sum);
                freeRegister(subscript.reg);
            }
        }
    }
    if (symbol->details.variable.dt.type == BaseType_Character) {
        if (isConstant(sum)) {
            sum.details.constant.value.integer *= symbol->details.variable.dt.constraint;
        }
        else {
            dim.class = ArgClass_Constant;
            dim.details.constant.value.integer = symbol->details.variable.dt.constraint;
            emitLoadConst(&dim);
            binop = genBinOps[OP_MUL - OP_ADD][BaseType_Integer];
            if (isLoadable(sum)) emitLoadValue(&sum);
            (*binop)(&dim, &sum);
            freeRegister(dim.reg);
        }
    }
    *offset = sum;

    return FALSE;
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
        err = evaluateIdentifier(expression);
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

static bool evaluateFmtSpec(ControlInfoList *ciList) {
    DataType *dt;
    char *formatter;
    char lineLabel[16];
    Register reg;
    OperatorArgument result;
    Symbol *sym;

    if (ciList->format == NULL) {
        emitPrimCall("@_prslst");
        return FALSE;
    }
    if (evaluateExpression(ciList->format, &result)) return TRUE;
    dt = getDataType(&result);
    if (result.class == ArgClass_Constant && dt->type == BaseType_Integer) {
        sprintf(lineLabel, "%ld", result.details.constant.value.integer);
        sym = findLabel(lineLabel);
        if (sym == NULL) {
            sym = addLabel(lineLabel);
            sym->details.label.class = StmtClass_Format;
            sym->details.label.forwardRef = TRUE;
        }
        reg = emitLabelReference(sym);
        emitConvertToByteAddress(reg);
        emitPushReg(reg);
        freeRegister(reg);
        formatter = "@_przfmt";
    }
    else if (dt->type == BaseType_Character) {
        loadValue(&result);
        emitPushReg(result.reg);
        freeRegister(result.reg);
        formatter = "@_prsfmt";
    }
    else {
        err("Format specification is not character type or FORMAT label");
        if (isCalculation(result)) freeRegister(result.reg);
        return TRUE;
    }
    emitPrimCall(formatter);
    emitAdjustSP(1);

    return FALSE;
}

static bool evaluateFunction(Token *fn, Symbol *symbol) {
    bool evalStatus;
    int frameSize;
    int parmIdx;
    int pass;
    TokenListItem *qualifier;
    Register reg;
    u8 registerMap;
    OperatorArgument result;
    int tempIdx;

    registerMap = getRegisterMap();
    emitSaveRegs(registerMap);
    enableEmission(FALSE);
    frameSize = 0;
    evalStatus = FALSE;
    for (pass = 1; pass <= 2; pass++) {
        if (pass == 2) {
            enableEmission(TRUE);
            emitAdjustSP(-frameSize);
            tempIdx = parmIdx;
        }
        parmIdx = 0;
        for (qualifier = fn->details.identifier.qualifiers; qualifier != NULL; qualifier = qualifier->next) {
            if (qualifier->item == NULL) continue;
            if (evaluateExpression(qualifier->item, &result)) {
                evalStatus = TRUE;
            }
            else if (isConstant(result)) {
                if (pass == 1) {
                    frameSize += 2;
                }
                else {
                    emitLoadConst(&result);
                    emitStoreStack(result.reg, tempIdx);
                    reg = emitLoadStackAddr(tempIdx);
                    emitStoreStack(reg, parmIdx);
                    freeRegister(reg);
                    freeRegister(result.reg);
                    tempIdx += 1;
                }
            }
            else if (isLoadable(result)) {
                if (pass == 1) {
                    frameSize += 1;
                }
                else {
                    emitLoadReference(&result);
                    emitStoreStack(result.reg, parmIdx);
                    freeRegister(result.reg);
                }
            }
            else {
                if (pass == 1) {
                    frameSize += 2;
                }
                else {
                    emitStoreStack(result.reg, tempIdx);
                    reg = emitLoadStackAddr(tempIdx);
                    emitStoreStack(reg, parmIdx);
                    freeRegister(reg);
                    freeRegister(result.reg);
                    tempIdx += 1;
                }
            }
            parmIdx += 1;
        }
    }
    emitSubprogramCall(fn->details.identifier.name);
    emitAdjustSP(frameSize);
    emitRestoreRegs(registerMap);
    reg = allocateRegister();
    emitCopyRegister(reg, RESULT_REG);
    result.class = ArgClass_Calculation;
    result.details.calculation = symbol->details.variable.dt;
    result.reg = reg;
    pushArg(&result);

    return evalStatus;
}

static bool evaluateIdentifier(Token *id) {
    OperatorArgument arg;
    char *name;
    OperatorArgument offset;
    u8 registerMap;
    OperatorArgument strLength;
    OperatorArgument strOffset;
    Symbol *symbol;

    name = id->details.identifier.name;
    symbol = findSymbol(name);
    if (symbol == NULL) {
        symbol = addSymbol(name, SymClass_Undefined);
    }
    if (symbol->class == SymClass_Undefined) {
        if (id->details.identifier.qualifiers == NULL) { // simple variable reference
            symbol->class = SymClass_Local;
            defineType(symbol);
            localOffset -= calculateSize(symbol);
            symbol->details.variable.offset = localOffset;
        }
        else {
            defineType(symbol);
            return evaluateFunction(id, symbol);
        }
    }
    else if (symbol->class == SymClass_Argument && symbol->details.variable.dt.type == BaseType_Undefined) {
        symbol->details.variable.dt = implicitTypes[toupper(name[0]) - 'A'];
    }
    else if (symbol->class == SymClass_Function && symbol->details.progUnit.dt.type == BaseType_Undefined) {
        symbol->details.progUnit.dt = implicitTypes[toupper(name[0]) - 'A'];
        localOffset -= calculateSize(symbol);
        symbol->details.progUnit.offset = localOffset;
    }

    arg.class = argClassForSymClass[symbol->class];
    switch (symbol->class) {
    case SymClass_Local:
    case SymClass_Global:
    case SymClass_Argument:
    case SymClass_Pointee:
    case SymClass_Function:
        arg.details.reference.symbol = symbol;
        if (id->details.identifier.qualifiers == NULL) {
            arg.details.reference.offsetClass = ArgClass_Undefined;
            if (id->details.identifier.range != NULL) {
                if (evaluateStringRange(symbol, id->details.identifier.range, &strOffset, &strLength)) return TRUE;
                emitLoadReference(&arg);
                emitUpdateStringRef(&arg, &strOffset, &strLength);
            }
        }
        else if (symbol->details.variable.dt.rank > 0) {
            if (evaluateArrayRef(symbol, id->details.identifier.qualifiers, &offset)) return TRUE;
            arg.details.reference.offsetClass = offset.class;
            switch (offset.class) {
            case ArgClass_Constant:
                arg.details.reference.offset.constant = offset.details.constant.value.integer;
                break;
            case ArgClass_Calculation:
                arg.details.reference.offset.reg = offset.reg;
                break;
            default:
                fprintf(stderr, "Invalid class of array reference offset: %d\n", offset.class);
                exit(1);
            }
            if (id->details.identifier.range != NULL) {
                if (evaluateStringRange(symbol, id->details.identifier.range, &strOffset, &strLength)) {
                    if (isCalculation(offset)) freeRegister(offset.reg);
                    return TRUE;
                }
                emitLoadReference(&arg);
                emitUpdateStringRef(&arg, &strOffset, &strLength);
            }
        }
        else {
            err("%s is not an array", symbol->identifier);
            return TRUE;
        }
        break;
    case SymClass_Parameter:
        arg.details.constant = symbol->details.param;
        break;
    default:
        err("Invalid symbol reference");
        return TRUE;
    }

    pushArg(&arg);
    return FALSE;
}

static bool evaluateStorageReference(StorageReference *reference, OperatorArgument *target, bool *isScalar) {
    DataType *dt;
    OperatorArgument offset;
    OperatorArgument strLength;
    OperatorArgument strOffset;

    *isScalar = FALSE;
    target->class = argClassForSymClass[reference->symbol->class];
    target->details.reference.symbol = reference->symbol;
    if (reference->expressionList == NULL) {
        target->details.reference.offsetClass = ArgClass_Undefined;
        if (reference->strRange == NULL) {
            dt = getSymbolType(reference->symbol);
            if (dt->type == BaseType_Character) {
                emitLoadReference(target);
            }
            else {
                *isScalar = TRUE;
            }
            return FALSE;
        }
        else {
            if (evaluateStringRange(reference->symbol, reference->strRange, &strOffset, &strLength)) return TRUE;
            emitLoadReference(target);
            emitUpdateStringRef(target, &strOffset, &strLength);
        }
    }
    else if (reference->symbol->details.variable.dt.rank > 0) {
        if (evaluateArrayRef(reference->symbol, reference->expressionList, &offset)) return TRUE;
        target->details.reference.offsetClass = offset.class;
        switch (offset.class) {
        case ArgClass_Constant:
            target->details.reference.offset.constant = offset.details.constant.value.integer;
            break;
        case ArgClass_Calculation:
            target->details.reference.offset.reg = offset.reg;
            break;
        default:
            fprintf(stderr, "Invalid class of array reference offset: %d\n", offset.class);
            exit(1);
        }
        if (reference->strRange == NULL) {
            emitLoadReference(target);
        }
        else {
            if (evaluateStringRange(reference->symbol, reference->strRange, &strOffset, &strLength)) {
                if (isCalculation(offset)) freeRegister(offset.reg);
                return TRUE;
            }
            emitLoadReference(target);
            emitUpdateStringRef(target, &strOffset, &strLength);
        }
    }
    else {
        err("%s is not an array", reference->symbol->identifier);
        return TRUE;
    }

    return FALSE;
}

static bool evaluateStringIndex(Token *expression, OperatorArgument *index) {
    void (*binop)(OperatorArgument *leftArg, OperatorArgument *rightArg);
    DataType *dt;

    if (evaluateExpression(expression, index)) {
        err("Incorrect string index");
        return TRUE;
    }
    dt = getDataType(index);
    if (dt->type != BaseType_Integer) dt->type = coerceArgument(index, dt->type, BaseType_Integer);

    return dt->type == BaseType_Undefined;
}

static bool evaluateStringRange(Symbol *symbol, StringRange *range, OperatorArgument *offset, OperatorArgument *length) {
    void (*binop)(OperatorArgument *leftArg, OperatorArgument *rightArg);
    OperatorArgument negOne;

    memset(offset, 0, sizeof(OperatorArgument));
    offset->class = ArgClass_Constant;
    offset->details.constant.dt.type = BaseType_Integer;
    offset->details.constant.value.integer = 0;
    memset(length, 0, sizeof(OperatorArgument));
    length->class = ArgClass_Constant;
    length->details.constant.dt.type = BaseType_Integer;
    length->details.constant.value.integer = symbol->details.variable.dt.constraint;
    if (range != NULL) {
        if (range->first != NULL) {
            if (evaluateStringIndex(range->first, offset)) return TRUE;
            if (offset->class == ArgClass_Constant) {
                offset->details.constant.value.integer -= 1;
            }
            else {
                memset(&negOne, 0, sizeof(OperatorArgument));
                negOne.class = ArgClass_Constant;
                negOne.details.constant.dt.type = BaseType_Integer;
                negOne.details.constant.value.integer = -1;
                emitLoadConst(&negOne);
                if (offset->class > ArgClass_Function) emitLoadValue(offset);
                binop = genBinOps[OP_ADD - OP_ADD][BaseType_Integer];
                (*binop)(&negOne, offset);
                freeRegister(negOne.reg);
                offset->class = ArgClass_Calculation;
            }
        }
        if (range->last != NULL && evaluateStringIndex(range->last, length)) {
            if (offset->class == ArgClass_Calculation) freeRegister(offset->reg);
            return TRUE;
        }
        if (offset->class == ArgClass_Constant) {
            if (length->class == ArgClass_Constant) {
                length->details.constant.value.integer -= offset->details.constant.value.integer;
                return FALSE;
            }
            else if (offset->details.constant.value.integer == 0) {
                return FALSE;
            }
        }
        loadValue(length);
        loadValue(offset);
        emitPushReg(offset->reg);
        binop = genBinOps[OP_SUB - OP_ADD][BaseType_Integer];
        (*binop)(length, offset);
        freeRegister(length->reg);
        length->reg = offset->reg;
        offset->reg = allocateRegister();
        emitPopReg(offset->reg);
    }
    
    return FALSE;
}

static bool evaluateSubscript(Symbol *symbol, TokenListItem *subscriptList, int idx, OperatorArgument *subscript) {
    void (*binop)(OperatorArgument *leftArg, OperatorArgument *rightArg);
    DataType *dt;
    bool isConst;
    OperatorArgument lowerBound;
    TokenListItem *qualifier;

    qualifier = getQualifier(subscriptList, idx);
    if (qualifier == NULL || qualifier->item == NULL || evaluateExpression(qualifier->item, subscript)) {
        err("Incorrect array index");
        return TRUE;
    }
    dt = getDataType(subscript);
    if (dt->type != BaseType_Integer) dt->type = coerceArgument(subscript, dt->type, BaseType_Integer);
    if (symbol->details.variable.dt.bounds[idx].lower != 0) {
        if (subscript->class == ArgClass_Constant) {
            subscript->details.constant.value.integer -= symbol->details.variable.dt.bounds[idx].lower;
        }
        else {
            memset(&lowerBound, 0, sizeof(OperatorArgument));
            lowerBound.class = ArgClass_Constant;
            lowerBound.details.constant.dt.type = BaseType_Integer;
            lowerBound.details.constant.value.integer = -symbol->details.variable.dt.bounds[idx].lower;
            emitLoadConst(&lowerBound);
            if (subscript->class > ArgClass_Function) emitLoadValue(subscript);
            binop = genBinOps[OP_ADD - OP_ADD][BaseType_Integer];
            (*binop)(&lowerBound, subscript);
            freeRegister(lowerBound.reg);
            subscript->class = ArgClass_Calculation;
        }
    }

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
        emitLoadValue(&rightArg);
    }
    isBop = isBinaryOp(op);
    if (isBop) {
        popArg(&leftArg);
        leftType = getDataType(&leftArg);
        isConstResult = isConstResult && isConstant(leftArg);
        argType = calculateCoercedType(op, leftType->type, rightType->type);
        if (argType == BaseType_Undefined) {
            err("Invalid argument type to '%s'", opIdToStr(op));
            return TRUE;
        }
        if (isLoadable(leftArg)) {
            emitLoadValue(&leftArg);
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
            case BaseType_Double:
                rightArg.details.constant.value.real = -rightArg.details.constant.value.real;
                break;
            case BaseType_Logical:
                rightArg.details.constant.value.logical = ~rightArg.details.constant.value.logical;
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

static void freeControlInfoList(ControlInfoList *ciList) {
    if (ciList != NULL) {
        if (ciList->unit != NULL) freeToken(ciList->unit);
        if (ciList->format != NULL) freeToken(ciList->format);
        if (ciList->recordNumber != NULL) freeToken(ciList->recordNumber);
        freeStorageReference(&ciList->iostat);
        free(ciList);
    }
}

static void freeStorageReference(StorageReference *reference) {
    if (reference != NULL) {
        freeTokenList(reference->expressionList);
        freeStringRange(reference->strRange);
    }
}

static void freeStringRange(StringRange *range) {
    if (range != NULL) {
        freeToken(range->first);
        freeToken(range->last);
        free(range);
    }
}

static void freeToken(Token *token) {
    if (token != NULL) {
        switch (token->type) {
        case TokenType_Identifier:
            if (token->details.identifier.name != NULL) free(token->details.identifier.name);
            freeTokenList(token->details.identifier.qualifiers);
            freeStringRange(token->details.identifier.range);
            break;
        case TokenType_Constant:
            if (token->details.constant.dt.type == BaseType_Character) {
                free(token->details.constant.value.character.string);
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

static void freeTokenList(TokenListItem *item) {
    TokenListItem *nextItem;

    while (item != NULL) {
        nextItem = item->next;
        freeToken(item->item);
        free(item);
        item = nextItem;
    }
}

static DataType *getDataType(OperatorArgument *arg) {
    switch (arg->class) {
    case ArgClass_Constant:
        return &arg->details.constant.dt;
    case ArgClass_Local:
    case ArgClass_Global:
    case ArgClass_Argument:
    case ArgClass_Pointee:
    case ArgClass_Function:
        return getSymbolType(arg->details.reference.symbol);
    case ArgClass_Calculation:
        return &arg->details.calculation;
    default:
        fprintf(stderr, "Unrecognized operator argument class: %d\n", arg->class);
        exit(1);
    }
}

static char *getIntValue(char *s, int *value) {
    *value = 0;
    for (;;) {
        if (isdigit(*s)) {
            *value = (*value * 10) + (*s++ - '0');
        }
        else if (isspace(*s)) {
            s += 1;
        }
        else {
            break;
        }
    }
    return s;
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

static TokenListItem *getQualifier(TokenListItem *qualifierList, int idx) {
    TokenListItem *qualifier;

    qualifier = qualifierList;
    while (qualifier != NULL && idx > 0) {
        qualifier = qualifier->next;
        idx -= 1;
    }

    return (idx == 0) ? qualifier : NULL;
}

static void inputFini(ControlInfoList *ciList) {
    emitAdjustSP(2);
}

static void inputInit(ControlInfoList *ciList) {
    OperatorArgument unit;

    if (evaluateFmtSpec(ciList) || evaluateExpression(ciList->unit, &unit)) return;
    emitAdjustSP(-2);
    loadValue(&unit);
    emitStoreStack(unit.reg, 0);
    freeRegister(unit.reg);
    emitPrimCall("@_rdurec");
}

static void list(char *format, ...) {
    va_list ap;

    if (listingFile != NULL) {
        va_start(ap, format);
        vfprintf(listingFile, format, ap);
        va_end(ap);
    }
}

static void loadValue(OperatorArgument *value) {
    if (value->class == ArgClass_Constant) {
        emitLoadConst(value);
    }
    else if (value->class > ArgClass_Function) {
        emitLoadValue(value);
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

static void parseAssignment(char *s, Token *id) {
    DataType *dt;
    Token *expression;
    bool isScalar;
    StorageReference reference;
    OperatorArgument result;
    OperatorArgument target;

    s = parseStorageReference(s, id, &reference);
    if (s == NULL) return;
    s = eatWsp(s);
    if (*s != '=') {
        err("Invalid statement");
        freeStorageReference(&reference);
        return;
    }

    s = eatWsp(parseExpression(s + 1, &expression));
    if (expression == NULL) {
        err("Expression syntax");
        freeStorageReference(&reference);
        return;
    }
    if (evaluateExpression(expression, &result) == FALSE) {
        dt = getSymbolType(reference.symbol);
        if (coerceArgument(&result, getDataType(&result)->type, dt->type) == BaseType_Undefined) {
            err("Invalid type conversion");
            if (isCalculation(result)) freeRegister(result.reg);
            freeToken(expression);
            freeStorageReference(&reference);
            return;
        }
        if (evaluateStorageReference(&reference, &target, &isScalar)) {
            if (isCalculation(result)) freeRegister(result.reg);
            freeToken(expression);
            freeStorageReference(&reference);
            return;
        }
        loadValue(&result);
        if (isScalar) {
            emitStoreArg(reference.symbol, &result);
        }
        else {
            emitStoreByReference(&target, &result);
            freeRegister(target.reg);
        }
        freeRegister(result.reg);
    }
    freeToken(expression);
    freeStorageReference(&reference);
    verifyEOS(s);
}

static void outputFini(ControlInfoList *ciList) {
    emitPrimCall((ciList->format == NULL) ? "@_flulst" : "@_flufmt");
    emitAdjustSP(3);
}

static void outputInit(ControlInfoList *ciList) {
    OperatorArgument unit;

    if (evaluateFmtSpec(ciList) || evaluateExpression(ciList->unit, &unit)) return;
    emitAdjustSP(-3);
    loadValue(&unit);
    emitStoreStack(unit.reg, 0);
    freeRegister(unit.reg);
}

static char *parseActualArguments(char *s, int *frameSize) {
    Token *expression;
    int parmIdx;
    int pass;
    Register reg;
    OperatorArgument result;
    char *start;
    int tempIdx;

    *frameSize = 0;
    tempIdx = 0;
    start = s + 1;
    enableEmission(FALSE);
    for (pass = 1; pass <= 2; pass++) {
        if (pass == 2) {
            enableEmission(TRUE);
            emitAdjustSP(-*frameSize);
            tempIdx = parmIdx + 1;
        }
        parmIdx = 0;
        s = start;
        for (;;) {
            s = parseExpression(s, &expression);
            if (expression == NULL) {
                err("Expression syntax");
                return s;
            }
            if (evaluateExpression(expression, &result) == FALSE) {
                if (isConstant(result)) {
                    if (pass == 1) {
                        *frameSize += 2;
                    }
                    else {
                        emitLoadConst(&result);
                        emitStoreStack(result.reg, tempIdx);
                        reg = emitLoadStackAddr(tempIdx);
                        emitStoreStack(reg, parmIdx);
                        freeRegister(reg);
                        freeRegister(result.reg);
                        tempIdx += 1;
                    }
                }
                else if (isLoadable(result)) {
                    if (pass == 1) {
                        *frameSize += 1;
                    }
                    else {
                        emitLoadReference(&result);
                        emitStoreStack(result.reg, parmIdx);
                        freeRegister(result.reg);
                    }
                }
                else {
                    if (pass == 1) {
                        *frameSize += 2;
                    }
                    else {
                        emitStoreStack(result.reg, tempIdx);
                        reg = emitLoadStackAddr(tempIdx);
                        emitStoreStack(reg, parmIdx);
                        freeRegister(reg);
                        freeRegister(result.reg);
                        tempIdx += 1;
                    }
                }
            }
            freeToken(expression);
            s = eatWsp(s);
            if (*s == '\0') {
                err("Missing )");
                return s;
            }
            else if (*s == ',') {
                s += 1;
                parmIdx += 1;
            }
            else if (*s == ')') {
                s += 1;
                break;
            }
            else {
                err("Argument list syntax");
                return s;
            }
        }
    }
    
    return s;
}

static char *parseCharConstraint(char *s, Token *token, DataType *dt) {
    Token *expression;
    OperatorArgument result;
    Token savedToken;
    char *start;

    s = eatWsp(s);
    start = s;

    if (*s == '*') {
        s = eatWsp(s + 1);
        if (isdigit(*s)) {
            s = getIntValue(s, &dt->constraint);
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
            else {
                savedToken = *token;
                s = parseExpression(s, &expression);
                *token = savedToken;
                if (expression == NULL
                    || evaluateExpression(expression, &result)
                    || !isConstant(result)
                    || result.details.constant.dt.type != BaseType_Integer
                    || result.details.constant.value.integer < 1) {
                    err("Invalid character length");
                }
                else {
                    dt->constraint = result.details.constant.value.integer;
                    s = eatWsp(s);
                    if (*s == ')') {
                        s += 1;
                    }
                    else {
                        err("Character length syntax");
                    }
                }
                freeToken(expression);
            }
        }
        else {
            s = start;
        }
    }

    return s;
}

static char *parseControlInfoList(char *s, ControlInfoList **ciList, int defaultUnit) {
    DataType *dt;
    Token *expression;
    bool isListDirected;
    ControlInfoList *list;
    Symbol *labelSym;
    char lineLabel[16];
    StorageReference reference;
    char *start;
    Token token;

    list = (ControlInfoList *)allocate(sizeof(ControlInfoList));
    *ciList = list;
    isListDirected = FALSE;
    s = eatWsp(s);
    if (*s != '(') {
        err("I/O control info list syntax");
        return NULL;
    }
    for (;;) {
        s = eatWsp(s + 1);
        if (*s == '*') {
            if (list->unit == NULL && isListDirected == FALSE) {
                list->unit = createIntegerConstant(defaultUnit);
            }
            else if (list->format == NULL && isListDirected == FALSE) {
                isListDirected = TRUE;
            }
            else {
                err("I/O control info list syntax");
                freeControlInfoList(list);
                return NULL;
            }
            s += 1;
        }
        else {
            start = s;
            s = getNextToken(s, &token, FALSE);
            s = eatWsp(s);
            if (token.type == TokenType_Identifier && *s == '=') {
                s = eatWsp(s + 1);
                if (*s == '*') {
                    if (list->unit == NULL && isListDirected == FALSE) {
                        list->unit = createIntegerConstant(defaultUnit);
                    }
                    else if (list->format == NULL && isListDirected == FALSE) {
                        isListDirected = TRUE;
                    }
                    else {
                        err("I/O control info list syntax");
                        freeControlInfoList(list);
                        return NULL;
                    }
                    s += 1;
                }
                else {
                    start = s;
                    s = parseExpression(s, &expression);
                    if (expression == NULL) {
                        err("Invalid expression in I/O control list");
                        freeControlInfoList(list);
                        return NULL;
                    }
                    if (strncasecmp(token.details.identifier.name, "UNIT", 5) == 0) {
                        if (list->unit != NULL) {
                            err("UNIT specified more than once");
                            freeToken(expression);
                            freeControlInfoList(list);
                            return NULL;
                        }
                        list->unit = expression;
                    }
                    else if (strncasecmp(token.details.identifier.name, "FMT", 4) == 0) {
                        if (list->format != NULL || isListDirected) {
                            err("FMT specified more than once");
                            freeToken(expression);
                            freeControlInfoList(list);
                            return NULL;
                        }
                        list->format = expression;
                    }
                    else if (strncasecmp(token.details.identifier.name, "END", 4) == 0) {
                        if (list->endLabel != NULL) {
                            err("END specified more than once");
                            freeToken(expression);
                            freeControlInfoList(list);
                            return NULL;
                        }
                        else if (expression->type == TokenType_Constant && expression->details.constant.dt.type == BaseType_Integer) {
                            sprintf(lineLabel, "%ld", expression->details.constant.value.integer);
                            freeToken(expression);
                            labelSym = findLabel(lineLabel);
                            if (labelSym != NULL) {
                                if (labelSym->details.label.class != StmtClass_Executable) {
                                    err("END= label does not reference executable statement");
                                    freeControlInfoList(list);
                                    return NULL;
                                }
                            }
                            else {
                                labelSym = addLabel(lineLabel);
                                labelSym->details.label.class = StmtClass_None;
                                labelSym->details.label.forwardRef = TRUE;
                            }
                            list->endLabel = labelSym;
                        }
                        else {
                            err("Invalid statement label in END=");
                            freeToken(expression);
                            freeControlInfoList(list);
                            return NULL;
                        }
                    }
                    else if (strncasecmp(token.details.identifier.name, "ERR", 4) == 0) {
                        if (list->errLabel != NULL) {
                            err("ERR specified more than once");
                            freeToken(expression);
                            freeControlInfoList(list);
                            return NULL;
                        }
                        else if (expression->type == TokenType_Constant && expression->details.constant.dt.type == BaseType_Integer) {
                            sprintf(lineLabel, "%ld", expression->details.constant.value.integer);
                            freeToken(expression);
                            labelSym = findLabel(lineLabel);
                            if (labelSym != NULL) {
                                if (labelSym->details.label.class != StmtClass_Executable) {
                                    err("ERR= label does not reference executable statement");
                                    freeControlInfoList(list);
                                    return NULL;
                                }
                            }
                            else {
                                labelSym = addLabel(lineLabel);
                                labelSym->details.label.class = StmtClass_None;
                                labelSym->details.label.forwardRef = TRUE;
                            }
                            list->errLabel = labelSym;
                        }
                        else {
                            err("Invalid statement label in ERR=");
                            freeToken(expression);
                            freeControlInfoList(list);
                            return NULL;
                        }
                    }
                    else if (strncasecmp(token.details.identifier.name, "REC", 4) == 0) {
                        if (list->recordNumber != NULL) {
                            err("REC specified more than once");
                            freeToken(expression);
                            freeControlInfoList(list);
                            return NULL;
                        }
                        list->recordNumber = expression;
                    }
                    else if (strncasecmp(token.details.identifier.name, "IOSTAT", 7) == 0) {
                        if (list->iostat.symbol != NULL) {
                            err("IOSTAT specified more than once");
                            freeToken(expression);
                            freeControlInfoList(list);
                            return NULL;
                        }
                        else if (expression->type == TokenType_Identifier) {
                            s = parseStorageReference(start, expression, &reference);
                            freeToken(expression);
                            if (s == NULL) {
                                freeControlInfoList(list);
                                return NULL;
                            }
                            dt = getSymbolType(reference.symbol);
                            if (dt->type != BaseType_Integer) {
                                err("%s is not integer", reference.symbol->identifier);
                                freeControlInfoList(list);
                                return NULL;
                            }
                            list->iostat = reference;
                        }
                        else {
                            err("IOSTAT= syntax");
                            freeToken(expression);
                            freeControlInfoList(list);
                            return NULL;
                        }
                    }
                    else {
                        err("Invalid keyword: %s", token.details.identifier.name);
                        freeControlInfoList(list);
                        return NULL;
                    }
                }
            }
            else {
                s = parseExpression(start, &expression);
                if (expression == NULL) {
                    err("Invalid expression in I/O control list");
                    freeControlInfoList(list);
                    return NULL;
                }
                if (list->unit == NULL) {
                    list->unit = expression;
                }
                else if (list->format == NULL && isListDirected == FALSE) {
                    list->format = expression;
                }
                else {
                    err("I/O control info list syntax");
                    freeToken(expression);
                    freeControlInfoList(list);
                    return NULL;
                }
            }
        }
        s = eatWsp(s);
        if (*s == ',') {
            s += 1;
        }
        else if (*s == ')') {
            break;
        }
        else {
            err("I/O control info list syntax");
            freeControlInfoList(list);
            return NULL;
        }
    }
    return s + 1;
}

static char *parseDataType(char *s, Token *token, DataType *dt) {
    char *start;
    int value;

    memset(dt, 0, sizeof(DataType));
    dt->type = BaseType_Undefined;
    start = s;
    if (token->type == TokenType_Keyword) {
        switch (token->details.keyword.id) {
        case CHARACTER:
            dt->type = BaseType_Character;
            s = eatWsp(s);
            if (*s == '*') {
                s = parseCharConstraint(s, token, dt);
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

    dt = getSymbolType(symbol);
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

static char *parseExpression(char *s, Token **expression) {
    TokenListItem *expressionList;
    Token *leftArg;
    Token *op;
    Token *rightArg;
    StringRange *strRange;
    Token token;
    Token *tp;

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
    tp = NULL;
    expressionList = NULL;
    strRange = NULL;
    s = getNextToken(s, &token, FALSE);
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
        if (*s == '(') {
            s = eatWsp(s);
            if (*s == ')') { // maybe a parameter-less function call
                expressionList = (TokenListItem *)allocate(sizeof(TokenListItem));
                s = eatWsp(s + 1);
            }
            else {
                tp = copyToken(&token);
                s = parseExpressionList(s, &expressionList);
                if (s == NULL) {
                    freeToken(tp);
                    freeToken(leftArg);
                    *expression = NULL;
                    return NULL;
                }
                s = eatWsp(s);
                if (*s == '(') {
                    s = parseStringRange(s, &strRange);
                    if (s == NULL) {
                        freeTokenList(expressionList);
                        freeToken(tp);
                        freeToken(leftArg);
                        *expression = NULL;
                        return NULL;
                    }
                }
            }
        }
        /*
         *  Fall through
         */
    case TokenType_Constant:
        if (leftArg != NULL) {
            freeTokenList(expressionList);
            freeToken(tp);
            freeToken(leftArg);
            *expression = NULL;
            return s;
        }
        if (tp == NULL) tp = copyToken(&token);
        if (*s == '\0' || *s == ',' || *s == ')' || *s == ':') {
            if (tp->type == TokenType_Identifier) {
                tp->details.identifier.qualifiers = expressionList;
                tp->details.identifier.range = strRange;
            }
            *expression = tp;
        }
        else {
            leftArg = tp;
            if (leftArg->type == TokenType_Identifier) leftArg->details.identifier.qualifiers = expressionList;
            s = getNextToken(s, &token, FALSE);
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

static char *parseExpressionList(char *s, TokenListItem **list) {
    TokenListItem *elem;
    Token *expression;
    TokenListItem *firstExpression;
    TokenListItem *lastExpression;
    int n;
    char *start;

    firstExpression = NULL;
    *list = NULL;
    start = s;
    n = 0;

    s = eatWsp(s + 1);
    if (*s == ':') { // possible character index
        return start;
    }

    for (;;) {
        s = parseExpression(s, &expression);
        if (s == NULL) {
            freeTokenList(firstExpression);
            *list = NULL;
            return NULL;
        }
        n += 1;
        elem = (TokenListItem *)allocate(sizeof(TokenListItem));
        elem->item = copyToken(expression);
        if (firstExpression == NULL)
            firstExpression = elem;
        else
            lastExpression->next = elem;
        lastExpression = elem;
        if (*s == ')') {
            s = eatWsp(s + 1);
            break;
        }
        else if (*s == ',') {
            s += 1;
        }
        else if (*s == ':' && n < 2) { // possible character index
            freeTokenList(firstExpression);
            *list = NULL;
            return start;
        }
        else {
            err("Invalid expression list");
            freeTokenList(firstExpression);
            *list = NULL;
            return NULL;
        }
    }
    *list = firstExpression;

    return s;
}

static char *parseFmtSpec(char *s, ControlInfoList *ciList) {
    Token *expression;

    s = eatWsp(s);
    if (*s == '*') {
        s += 1;
    }
    else {
        s = parseExpression(s, &expression);
        if (expression == NULL) {
            err("Invalid format specification");
            return NULL;
        }
        ciList->format = expression;
    }
    return s;
}

static char *parseFormalArguments(char *s) {
    int argIdx;
    char *id;
    Symbol *symbol;
    Token token;

    argIdx = 0;
    for (;;) {
        s = getNextToken(s + 1, &token, FALSE);
        if (token.type == TokenType_Identifier) {
            id = token.details.identifier.name;
            symbol = addSymbol(id, SymClass_Argument);
            if (symbol == NULL) {
                err("Previously declared parameter name: %s", id);
            }
            else {
                symbol->details.variable.offset = argIdx + 2; // base pointer offset after subprogram call
            }
        }
        else {
            err("Invalid parameter name");
        }
        s = eatWsp(s);
        if (*s == ')') {
            s += 1;
            break;
        }
        else if (*s == ',') {
            argIdx += 1;
        }
        else {
            err("Parameter list syntax");
            while (*s != '\0') s += 1;
            break;
        }
    }
    return s;
}

static void parseInputList(char *s, ControlInfoList *ciList) {
    DataType *dt;
    bool isScalar;
    StorageReference reference;
    Register reg;
    OperatorArgument target;
    Token token;

    for (;;) {
        s = getNextToken(s, &token, FALSE);
        if (token.type != TokenType_Identifier) {
            err("Input list item is not a variable");
            return;
        }
        s = parseStorageReference(s, &token, &reference);
        if (s == NULL) return;
        if (evaluateStorageReference(&reference, &target, &isScalar)) {
            freeStorageReference(&reference);
            return;
        }
        freeStorageReference(&reference);
        emitLoadReference(&target);
        dt = getDataType(&target);
        if (dt->type != BaseType_Character) emitConvertToByteAddress(target.reg);
        emitStoreStack(target.reg, 1);
        freeRegister(target.reg);
        if (ciList->format == NULL) {
            switch (dt->type) {
            case BaseType_Character:
                emitPrimCall("@_inpchr");
                break;
            case BaseType_Logical:
                emitPrimCall("@_inplog");
                break;
            case BaseType_Integer:
            case BaseType_Pointer:
                emitPrimCall("@_inpint");
                break;
            case BaseType_Real:
            case BaseType_Double:
                emitPrimCall("@_inpdbl");
                break;
/*  TODO
            case BaseType_Complex:
                break;
*/
            default:
                err("Invalid data type of list-directed I/O element");
                return;
            }
        }
        else {
            emitPrimCall("@_rdufmt");
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

static void parseLogicalIF(char *s, Register reg, bool isFromLogIf) {
    IfStackEntry *entry;
    char label[8];
    Token token;

    s = getNextToken(s, &token, TRUE);
    if (token.type == TokenType_Identifier && strcasecmp(token.details.identifier.name, "THEN") == 0) {
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
            case CALL:
                parseCALL(s);
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
                parsePUNCH(s);
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
            parseAssignment(s, &token);
        }
        else {
            err("Invalid IF syntax");
        }
        emitLabel(label);
    }
}

static void parseOutputList(char *s, ControlInfoList *ciList) {
    DataType *dt;
    Token *expression;
    bool isChar;
    Register reg;
    OperatorArgument result;

    for (;;) {
        s = parseExpression(s, &expression);
        if (expression == NULL) {
            err("Expression syntax");
            return;
        }
        if (evaluateExpression(expression, &result) == FALSE) {
            dt = getDataType(&result);
            isChar = dt->type == BaseType_Character;
            if (isConstant(result)) {
                emitLoadConst(&result);
                if (isChar) {
                    emitStoreStack(result.reg, 1);
                }
                else {
                    emitStoreStack(result.reg, 2);
                    reg = emitLoadStackByteAddr(2);
                    emitStoreStack(reg, 1);
                    freeRegister(reg);
                }
            }
            else if (isLoadable(result)) {
                emitLoadReference(&result);
                if (!isChar) emitConvertToByteAddress(result.reg);
                emitStoreStack(result.reg, 1);
            }
            else if (isChar) {
                emitStoreStack(result.reg, 1);
            }
            else {
                emitStoreStack(result.reg, 2);
                reg = emitLoadStackByteAddr(2);
                emitStoreStack(reg, 1);
                freeRegister(reg);
            }
            freeRegister(result.reg);
            if (ciList->format == NULL) {
                switch (dt->type) {
                case BaseType_Character:
                    emitPrimCall("@_lstchr");
                    break;
                case BaseType_Logical:
                    emitPrimCall("@_lstlog");
                    break;
                case BaseType_Integer:
                case BaseType_Pointer:
                    emitPrimCall("@_lstint");
                    break;
                case BaseType_Real:
                case BaseType_Double:
                    emitPrimCall("@_lstdbl");
                    break;
/*  TODO
                case BaseType_Complex:
                    break;
*/
                default:
                    err("Invalid data type of list-directed I/O element");
                    freeToken(expression);
                    return;
                }
            }
            else {
                emitPrimCall("@_wrufmt");
            }
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

static char *parseStorageReference(char *s, Token *id, StorageReference *reference) {
    DataType *dt;
    TokenListItem *expressionList;
    bool isChrAsgn;
    char *name;
    StringRange *strRange;
    Symbol *symbol;

    s = eatWsp(s);
    expressionList = NULL;
    strRange = NULL;
    name = id->details.identifier.name;

    symbol = findSymbol(name);
    if (symbol == NULL) {
        symbol = addSymbol(name, SymClass_Undefined);
    }
    switch (symbol->class) {
    case SymClass_Undefined:
        if (*s == '(') {
            err("Undefined array %s", name);
            return NULL;
        }
        symbol->class = SymClass_Local;
        defineType(symbol);
        localOffset -= calculateSize(symbol);
        symbol->details.variable.offset = localOffset;
        /* fall through */
    case SymClass_Local:
    case SymClass_Global:
    case SymClass_Argument:
    case SymClass_Pointee:
        dt = &symbol->details.variable.dt;
        break;
    case SymClass_Function:
        if (symbol->details.progUnit.dt.type == BaseType_Undefined) {
            symbol->details.progUnit.dt = implicitTypes[toupper(name[0]) - 'A'];
            localOffset -= calculateSize(symbol);
            symbol->details.progUnit.offset = localOffset;
        }
        dt = &symbol->details.progUnit.dt;
        break;
    default:
        err("Invalid storage reference to %s", name);
        return NULL;
    }
    isChrAsgn = dt->type == BaseType_Character;
    if (*s == '(') {
        if (dt->rank > 0) {
            s = parseExpressionList(s, &expressionList);
            if (s == NULL) return NULL;
            if (expressionList == NULL) {
                err("Invalid array index");
                return NULL;
            }
            s = eatWsp(s);
            if (*s == '(') {
                if (isChrAsgn) {
                    s = parseStringRange(s, &strRange);
                    if (s == NULL) {
                        freeTokenList(expressionList);
                        return NULL;
                    }
                }
                else {
                    err("Unexpected '('");
                    freeTokenList(expressionList);
                    return NULL;
                }
            }
        }
        else if (isChrAsgn) {
            s = parseStringRange(s, &strRange);
            if (s == NULL) return NULL;
        }
    }
    reference->symbol = symbol;
    reference->expressionList = expressionList;
    reference->strRange = strRange;

    return s;
}

static char *parseStringRange(char *s, StringRange **range) {
    Token *expression;
    StringRange *sr;

    *range = NULL;
    sr = (StringRange *)allocate(sizeof(StringRange));
    s = eatWsp(s + 1);
    if (*s != ':' && *s != ')') {
        s = parseExpression(s, &expression);
        if (s == NULL) {
            freeStringRange(sr);
            return NULL;
        }
        sr->first = copyToken(expression);
    }
    if (*s == ':') {
        s = eatWsp(s + 1);
        if (*s != ')') {
            s = parseExpression(s, &expression);
            if (s == NULL) {
                freeStringRange(sr);
                return NULL;
            }
            sr->last = copyToken(expression);
        }
    }
    if (*s != ')') {
        err("Invalid character range");
        freeStringRange(sr);
        return NULL;
    }
    *range = sr;

    return s + 1;
}

static char *parseTypeDecl(char *s, DataType *dt) {
    char *id;
    Symbol *symbol;
    Token token;

    for (;;) {
        s = getNextToken(s, &token, FALSE);
        if (token.type == TokenType_Identifier) {
            id = token.details.identifier.name;
            symbol = findSymbol(id);
            if (symbol == NULL) {
                symbol = addSymbol(id, SymClass_Undefined);
                symbol->details.variable.dt = *dt;
            }
            else if (symbol->class == SymClass_Argument && symbol->details.variable.dt.type == BaseType_Undefined) {
                symbol->details.variable.dt = *dt;
            }
            else if (symbol->class == SymClass_Function && symbol->details.progUnit.dt.type == BaseType_Undefined) {
                symbol->details.progUnit.dt = *dt;
            }
            else {
                err("Duplicate declaration of %s", id);
                break;
            }
            if (dt->type == BaseType_Character) {
                s = eatWsp(s);
                if (*s == '*') {
                    s = parseCharConstraint(s, &token, &symbol->details.variable.dt);
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
                symbol->class = SymClass_Local;
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

static void parseASSIGN(char *s) {
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
    s = getNextToken(s + 1, &token, FALSE);
    if (token.type != TokenType_Identifier) {
        err("Invalid target of ASSIGN");
        return;
    }
    id = token.details.identifier.name;
    sym = findSymbol(id);
    if (sym == NULL) {
        sym = addSymbol(id, SymClass_Undefined);
    }
    switch (sym->class) {
    case SymClass_Undefined:
        sym->class = SymClass_Local;
        defineType(sym);
        localOffset -= calculateSize(sym);
        sym->details.variable.offset = localOffset;
        /* fall through */
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
        err("Invalid type of ASSIGN variable: %s", baseTypeToStr(type));
        return;
    }
    reg = emitLabelReference(labelSym);
    emitStoreReg(sym, reg);
    freeRegister(reg);
    verifyEOS(s);
}

static void parseBLOCKDATA(char *s) {
}

static void parseCALL(char *s) {
    int frameSize;
    char name[16];
    int pass;
    char *start;
    Symbol *sym;
    Token token;

    s = getNextToken(s, &token, FALSE);
    if (token.type != TokenType_Identifier) {
        err("Invalid subroutine name");
        return;
    }
    strcpy(name, token.details.identifier.name);
    sym = findSymbol(name);
    if (sym == NULL) {
        sym = addSymbol(name, SymClass_Subroutine);
    }
    else if (sym->class != SymClass_Subroutine) {
        err("%s is not a subroutine name", name);
        return;
    }
    s = eatWsp(s);
    if (*s == '(') {
        s = parseActualArguments(s, &frameSize);
    }
    else if (*s != '\0') {
        err("Invalid CALL statement");
        return;
    }
    emitSubprogramCall(name);
    emitAdjustSP(frameSize);
}

static void parseCOMMON(char *s) {
}

static void parseDATA(char *s) {
}

static void parseDIMENSION(char *s) {
    char *id;
    Symbol *symbol;
    Token token;

    for (;;) {
        s = getNextToken(s, &token, FALSE);
        if (token.type == TokenType_Identifier) {
            id = token.details.identifier.name;
            if (*s != '(') {
                err("No dimensions specified for %s", id);
                return;
            }
            symbol = findSymbol(id);
            if (symbol == NULL) {
                symbol = addSymbol(id, SymClass_Undefined);
            }
            switch (symbol->class) {
            case SymClass_Undefined:
                symbol->class = SymClass_Local;
                defineType(symbol);
                /* fall through */
            case SymClass_Local:
            case SymClass_Global:
            case SymClass_Pointee:
                if (symbol->details.variable.dt.rank != 0) {
                    err("Duplicate declaration of %s", id);
                    return;
                }
                break;
            case SymClass_Argument:
                if (symbol->details.variable.dt.type == BaseType_Undefined) {
                    defineType(symbol);
                }
                else if (symbol->details.variable.dt.rank != 0) {
                    err("Duplicate declaration of %s", id);
                    return;
                }
                break;
            case SymClass_Function:
                if (symbol->details.progUnit.dt.type == BaseType_Undefined) {
                    defineType(symbol);
                }
                else if (symbol->details.progUnit.dt.rank != 0) {
                    err("Duplicate declaration of %s", id);
                    return;
                }
                break;
            default:
                err("Invalid array declaration");
                return;
            }
            s = parseDimDecl(s + 1, symbol);
            s = eatWsp(s);
            if (*s == ',')
                s = eatWsp(s + 1);
            else if (*s == '\0')
                break;
            else {
                err("Invalid array declaration");
                break;
            }
        }
        else {
            err("Invalid array declaration");
            break;
        }
    }
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
    s = getNextToken(s, &token, FALSE);
    if (token.type != TokenType_Identifier) {
        err("Missing or invalid DO loop variable");
        return;
    }
    id = token.details.identifier.name;
    sym = findSymbol(id);
    if (sym == NULL) {
        sym = addSymbol(id, SymClass_Undefined);
    }
    switch (sym->class) {
    case SymClass_Undefined:
        sym->class = SymClass_Local;
        defineType(sym);
        localOffset -= calculateSize(sym);
        sym->details.variable.offset = localOffset;
        /* fall through */
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
    loadValue(&result);
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
    loadValue(&result);
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
    loadValue(&result);
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
    loadValue(&result);
    if (getDataType(&result)->type == BaseType_Logical) {
        s = getNextToken(s, &token, FALSE);
        if (token.type == TokenType_Identifier && strcasecmp(token.details.identifier.name, "THEN") == 0) {
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
    CharacterValue cval;
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
    cval.length = strlen(start);
    cval.string = start;
    emitLabeledString(&cval, currentLabel->details.label.label, TRUE);
}

static void parseFUNCTION(char *s, DataType *dt) {
    Symbol *symbol;
    Token token;

    s = getNextToken(s, &token, FALSE);
    if (token.type == TokenType_Identifier) {
        symbol = addSymbol(token.details.identifier.name, SymClass_Function);
        if (symbol == NULL) {
            err("Function name not unique");
            return;
        }
        if (dt != NULL) symbol->details.progUnit.dt = *dt;
        progUnitSym = symbol;
        emitProlog(progUnitSym);
    }
    else {
        err("Incorrect function name");
    }
    s = eatWsp(s);
    if (*s == '(') {
        s = parseFormalArguments(s);
        s = eatWsp(s);
    }
    if (*s != '\0') {
        err("Subroutine declaration syntax");
    }
}

static void parseGOTO(char *s) {
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
        emitWordLabel(tableLabel);
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
            loadValue(&result);
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
        s = getNextToken(s, &token, FALSE);
        if (token.type != TokenType_Identifier) {
            err("Invalid target of GOTO");
            return;
        }
        id = token.details.identifier.name;
        sym = findSymbol(id);
        if (sym == NULL) {
            sym = addSymbol(id, SymClass_Undefined);
        }
        switch (sym->class) {
        case SymClass_Undefined:
            sym->class = SymClass_Local;
            defineType(sym);
            localOffset -= calculateSize(sym);
            sym->details.variable.offset = localOffset;
            /* fall through */
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
            err("Invalid type of assigned GOTO variable: %s", baseTypeToStr(type));
            return;
        }
        if (evaluateExpression(&token, &result)) return;
        emitLoadValue(&result);
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
    loadValue(&result);
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
    	s = getNextToken(s, &token, TRUE);
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
    s = getNextToken(s, &token, FALSE);
    if (token.type != TokenType_None) err("Incorrect IMPLICIT NONE declaration");
}

static void parseINCLUDE(char *s) {
}

static void parseINTRINSIC(char *s) {
}

static void parseNAMELIST(char *s) {
}

static void parseOutputStmt(char *s, int unitNum) {
    ControlInfoList *ciList;
    OperatorArgument unit;

    ciList = (ControlInfoList *)allocate(sizeof(ControlInfoList));
    s = parseFmtSpec(s, ciList);
    if (s == NULL) {
        freeControlInfoList(ciList);
        return;
    }
    s = eatWsp(s);
    if (*s != ',') {
        err("Comma missing after format specification");
        freeControlInfoList(ciList);
        return;
    }
    ciList->unit = createIntegerConstant(unitNum);
    outputInit(ciList);
    parseOutputList(s + 1, ciList);
    outputFini(ciList);
    freeControlInfoList(ciList);
}

static void parsePARAMETER(char *s) {
    Token *expression;
    char *name;
    OperatorArgument result;
    Symbol *symbol;
    Token token;

    s = eatWsp(s);
    if (*s != '(') {
        err("PARAMETER statement syntax");
        return;
    }
    for (;;) {
        s = getNextToken(s + 1, &token, FALSE);
        if (token.type != TokenType_Identifier) {
            err("Invalid parameter name");
            return;
        }
        name = token.details.identifier.name;
        symbol = findSymbol(name);
        if (symbol == NULL) {
            symbol = addSymbol(name, SymClass_Parameter);
        }
        if (symbol->class == SymClass_Parameter && symbol->details.variable.dt.type == BaseType_Undefined) {
            defineType(symbol);
        }
        else {
            err("Duplicate parameter: %s", name);
            return;
        }
        s = eatWsp(s);
        if (*s != '=') {
            err("Invalid parameter declaration");
            return;
        }
        s = eatWsp(parseExpression(s + 1, &expression));
        if (expression == NULL) {
            err("Expression syntax");
            return;
        }
        if (evaluateExpression(expression, &result)) {
            freeToken(expression);
            return;
        }
        freeToken(expression);
        if (coerceArgument(&result, getDataType(&result)->type, symbol->details.variable.dt.type) == BaseType_Undefined) {
            err("Invalid type conversion");
            if (isCalculation(result)) freeRegister(result.reg);
            return;
        }
        if (!isConstant(result)) {
            err("Non-constant expression in declaration of %s", symbol->identifier);
            if (isCalculation(result)) freeRegister(result.reg);
            return;
        }
        symbol->details.param = result.details.constant;
        s = eatWsp(s);
        if (*s == ')') {
            s += 1;
            break;
        }
        else if (*s == ',') {
            s += 1;
        }
        else {
            err("PARAMETER statement syntax");
            return;
        }
    }
    verifyEOS(s);
}

static void parsePRINT(char *s) {
    parseOutputStmt(s, DEFAULT_OUTPUT_UNIT);
}

static void parsePROGRAM(char *s) {
    Symbol *symbol;
    Token token;

    s = getNextToken(s, &token, FALSE);
    if (token.type == TokenType_Identifier) {
        symbol = addSymbol(token.details.identifier.name, SymClass_Program);
        if (symbol == NULL) {
            err("Program name not unique");
        }
        s = eatWsp(s);
        if (*s == '(') {
            for (;;) {
                s = getNextToken(s + 1, &token, FALSE);
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

static void parsePUNCH(char *s) {
    parseOutputStmt(s, DEFAULT_PUNCH_UNIT);
}

static void parseREAD(char *s) {
    ControlInfoList *ciList;

    s = eatWsp(s);
    if (*s == '(') {
        s = parseControlInfoList(s, &ciList, DEFAULT_INPUT_UNIT);
        if (s == NULL) return;
    }
    else {
        ciList = (ControlInfoList *)allocate(sizeof(ControlInfoList));
        s = parseFmtSpec(s, ciList);
        if (s == NULL) {
            freeControlInfoList(ciList);
            return;
        }
        s = eatWsp(s);
        if (*s != ',') {
            err("Comma missing after format specification");
            freeControlInfoList(ciList);
            return;
        }
        ciList->unit = createIntegerConstant(DEFAULT_INPUT_UNIT);
    }
    inputInit(ciList);
    parseInputList(s, ciList);
    inputFini(ciList);
    freeControlInfoList(ciList);
}

static void parseSAVE(char *s) {
}

static void parseSUBROUTINE(char *s) {
    Symbol *symbol;
    Token token;

    s = getNextToken(s, &token, FALSE);
    if (token.type == TokenType_Identifier) {
        symbol = addSymbol(token.details.identifier.name, SymClass_Subroutine);
        if (symbol == NULL) {
            err("Subroutine name not unique");
            return;
        }
        progUnitSym = symbol;
        emitProlog(progUnitSym);
    }
    else {
        err("Incorrect subroutine name");
        return;
    }
    s = eatWsp(s);
    if (*s == '(') {
        s = parseFormalArguments(s);
        s = eatWsp(s);
    }
    if (*s != '\0') {
        err("Subroutine declaration syntax");
    }
}

static void parseWRITE(char *s) {
    ControlInfoList *ciList;

    s = eatWsp(s);
    if (*s != '(') {
        parsePRINT(s);
        return;
    }
    s = parseControlInfoList(s, &ciList, DEFAULT_OUTPUT_UNIT);
    if (s == NULL) return;
    outputInit(ciList);
    parseOutputList(s, ciList);
    outputFini(ciList);
    freeControlInfoList(ciList);
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
    list("1\n");
    progUnitSym  = NULL;
    state        = STATE_PROG_UNIT;
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
            fputs(token->details.identifier.name, f);
            break;
        case TokenType_Operator:
            fputs(opIdToStr(token->details.operator.id), f);
            break;
        case TokenType_Constant:
            switch (token->details.constant.dt.type) {
            case BaseType_Character:
                fprintf(f, "'%s'", token->details.constant.value.character.string);
                break;
            case BaseType_Logical:
                break;
            case BaseType_Integer:
                fprintf(f, "%ld", token->details.constant.value.integer);
                break;
            case BaseType_Real:
            case BaseType_Double:
                fprintf(f, "%f", token->details.constant.value.real);
                break;
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
