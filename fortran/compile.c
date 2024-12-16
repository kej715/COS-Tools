/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: compile.c
**
**  Description:
**      This file implements a single pass, recursive descent parser for
**      the FORTRAN 77 language.
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
#include <time.h>
#include "binops.h"
#include "codegen.h"
#include "coercion.h"
#include "const.h"
#include "fnv.h"
#include "proto.h"
#include "types.h"

#define DEBUG 1

typedef enum parsingState {
    STATE_PROG_UNIT = 0,
    STATE_IMPLICIT,
    STATE_SPECIFICATION,
    STATE_DEFINITION,
    STATE_EXECUTABLE
} ParsingState;

static void adjustDataInitializers(void);
static char *appendLine(char *sp, char *lp);
static void assignStorage(void);
static char *baseTypeToStr(BaseType type);
static int calculateConstOffset(Symbol *symbol, TokenListItem *subscriptList);
static char *collectStmt(void);
static void copyCharValue(DataValue *to, DataValue *from);
static Token *copyToken(Token *token);
static Token *createIntegerConstant(int value);
static void defineLocalVariable(Symbol *symbol);
static void defineType(Symbol *symbol);
static char *eatWsp(char *s);
static void errArgType(OperatorId op, BaseType type, OperatorArgument *arg);
static bool evaluateArrayRef(Symbol *symbol, TokenListItem *subscriptList, OperatorArgument *offset);
static bool evaluateExpression(Token *expression, OperatorArgument *result);
static bool evaluateExprHelper(Token *expression);
static bool evaluateFmtSpec(ControlInfoList *ciList);
static bool evaluateFunction(Token *fn, Symbol *symbol, Symbol *intrinsic);
static bool evaluateIdentifier(Token *id);
static bool evaluateInquireReference(StorageReference *ref, int stackOffset);
static bool evaluateStorageReference(StorageReference *reference, OperatorArgument *target, OperatorArgument *object, bool *isScalar);
static bool evaluateStringIndex(Token *expression, OperatorArgument *index);
static bool evaluateStringRange(Symbol *symbol, StringRange *range, OperatorArgument *offset, OperatorArgument *length);
static bool evaluateSubscript(Symbol *symbol, TokenListItem *subscriptList, int idx, OperatorArgument *subscript);
static bool executeOperator(OperatorId op);
static void freeCharValue(DataValue *charValue);
static void freeCloseInfoList(CloseInfoList *ciList);
static void freeConstantList(ConstantListItem *list);
static void freeControlInfoList(ControlInfoList *list);
static void freeDataInitializerList(DataInitializerItem *list);
static void freeImpliedDoList(ImpliedDoList *doList);
static void freeInquireInfoList(InquireInfoList *iiList);
static void freeIoList(IoListItem *ioList);
static void freeOpenInfoList(OpenInfoList *oiList);
static void freeStaticInitializers(void);
static void freeStorageReference(StorageReference *reference);
static void freeStringRange(StringRange *range);
static void freeToken(Token *token);
static void freeTokenList(TokenListItem *item);
static DataType *getDataType(OperatorArgument *arg);
static char *getIntValue(char *s, int *value);
static char *getLabel(char *s, char *label);
static char *getProgUnitQualifier(void);
static TokenListItem *getQualifier(TokenListItem *qualifierList, int idx);
static char *getStorageReference(char *s, char *paramName, BaseType type, StorageReference *reference);
static void inputCheckIostat(ControlInfoList *ciList);
static void inputFini(ControlInfoList *ciList);
static void inputInit(ControlInfoList *ciList);
static bool isAssignment(char *s, bool *isDefn, bool *hasError);
static void loadValue(OperatorArgument *value);
static Symbol *matchIntrinsic(Token *fn, Symbol *intrinsic);
static void notSupported(char *s);
static char *opIdToStr(OperatorId id);
static void outputCheckIostat(ControlInfoList *ciList);
static void outputFini(ControlInfoList *ciList);
static void outputInit(ControlInfoList *ciList);
static char *parseActualArguments(char *s, int *frameSize);
static void parseArithmeticIF(char *s, Register reg);
static void parseAssignment(char *s, Token *id);
static char *parseCharConstraint(char *s, Token *token, DataType *dt);
static char *parseCloseInfoList(char *s, CloseInfoList **ciList);
static char *parseControlInfoList(char *s, ControlInfoList **ciList, int defaultUnit);
static char *parseDataType(char *s, Token *token, DataType *type);
static char *parseDimDecl(char *s, Symbol *symbol);
static void parseDirective(char *s, int lineNo);
static char *parseExpression(char *s, Token **expression);
static char *parseExpressionList(char *s, TokenListItem **list);
static char *parseFmtSpec(char *s, ControlInfoList *ciList);
static char *parseFormalArguments(char *s, bool isStmtFn);
static char *parseImpliedDo(char *s, Token *doVarId, ImpliedDoList *doList);
static char *parseIoList(char *s, bool isWithinDo, IoListItem **ioList);
static void parseLogicalIF(char *s, Register reg, bool isFromLogIf);
static char *parseOpenInfoList(char *s, OpenInfoList **oiList);
static void parseOutputStmt(char *s, int unitNum);
static void parseStmtFunction(char *s, Token *id);
static char *parseStorageReference(char *s, Token *id, StorageReference *reference);
static char *parseStringRange(char *s, StringRange **range);
static char *parseTypeDecl(char *s, DataType *dt);
static void popArg(OperatorArgument *arg);
static void popOp(OperatorDetails *op);
static void presetImplicit(void);
static void presetProgUnit(void); 
static void processInputList(IoListItem *ioList, ControlInfoList *ciList);
static void processOutputList(IoListItem *ioList, ControlInfoList *ciList);
static void pushArg(OperatorArgument *arg);
static void pushOp(OperatorDetails *op);
static char *readLine(void);
static void setIntegerArg(OperatorArgument *arg, int value);
static bool setupImpliedDoList(ImpliedDoList *doList, DoStackEntry *entry);
static void transferCharValue(DataValue *to, DataValue *from);
static bool validateDataInitializers(DataInitializerItem *dList, ConstantListItem *cList);
static void verifyEOS(char *s);

static void parseASSIGN(char *s);
static void parseBLOCKDATA(char *s);
static void parseCALL(char *s);
static void parseCLOSE(char *s);
static void parseCOMMON(char *s);
static void parseDATA(char *s);
static void parseDIMENSION(char *s);
static void parseDO(char *s);
static void parseELSE(char *s);
static void parseELSEIF(char *s);
static void parseEND(char *s);
static void parseENDDO(char *s);
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
static void parseINQUIRE(char *s);
static void parsePAUSE(char *s);
static void parseSAVE(char *s);
static void parseSTOP(char *s);
static void parseSUBROUTINE(char *s);
static void parseIMPLICIT(char *s);
static void parseIMPLICITNONE(char *s);
static void parseOPEN(char *s);
static void parsePARAMETER(char *s);
static void parsePOINTER(char *s);
static void parsePRINT(char *s);
static void parsePROGRAM(char *s);
static void parsePUNCH(char *s);
static void parseREAD(char *s);
static void parseRETURN(char *s);
static void parseWRITE(char *s);

#if DEBUG
static char *argClassToStr(ArgumentClass class);
static void printExpression(FILE *f, Token *expression);
static void printToken(FILE *f, Token *token);
static char *tokenIdToStr(TokenId id);
static char *tokenTypeToStr(TokenType type);
#endif

static ArgumentClass argClassForSymClass[] = {
    ArgClass_Undefined,  /* SymClass_Undefined    */
    ArgClass_Undefined,  /* SymClass_Program      */
    ArgClass_Undefined,  /* SymClass_Subroutine   */
    ArgClass_Function,   /* SymClass_Function     */
    ArgClass_Function,   /* SymClass_StmtFunction */
    ArgClass_Undefined,  /* SymClass_Intrinsic    */
    ArgClass_Undefined,  /* SymClass_BlockData    */
    ArgClass_Undefined,  /* SymClass_NamedCommon  */
    ArgClass_Auto,       /* SymClass_Auto         */
    ArgClass_Static,     /* SymClass_Static       */
    ArgClass_Adjustable, /* SymClass_Adjustable   */
    ArgClass_Global,     /* SymClass_Global       */
    ArgClass_Argument,   /* SymClass_Argument     */
    ArgClass_Constant,   /* SymClass_Parameter    */
    ArgClass_Pointee,    /* SymClass_Pointee      */
    ArgClass_Undefined   /* SymClass_Label        */
};
static Symbol defaultProgSym = {
    NULL, NULL, NULL, NULL, "MAIN", SymClass_Program
};
static int autoOffset = 0;
static Symbol *currentLabel;
static DataType implicitTypes[26];
static char lineBuf[MAX_LINE_LENGTH+1];
static int staticOffset = 0;
static ParsingState state;

static OperatorArgument argStack[MAX_ARG_STACK_SIZE];
static int argStkPtr = 0;
static DoStackEntry doStack[MAX_DO_STACK_SIZE];
static int doStackPtr = 0;
static IfStackEntry ifStack[MAX_IF_STACK_SIZE];
static int ifStackPtr = 0;
static OperatorDetails opStack[MAX_OP_STACK_SIZE];
static int opStkBtm;
static int opStkPtr = 0;

static ConstantListItem *firstCListItem = NULL;
static DataInitializerItem *firstDListItem = NULL;
static ConstantListItem *lastCListItem = NULL;
static DataInitializerItem *lastDListItem = NULL;

static void adjustDataInitializers(void) {
    DataInitializerItem *dListItem;

    for (dListItem = firstDListItem; dListItem != NULL; dListItem = dListItem->next) {
        dListItem->blockOffset = dListItem->symbol->details.variable.offset;
    }
}

static char *appendLine(char *sp, char *lp) {
    char *nbp;
    char *qp;
    char *stmtLimit;
    char *xp;

    stmtLimit = stmtBuf + MAX_STMT_LENGTH;
    xp = NULL;
    qp = NULL;
    nbp = sp - 1;
    while (*lp != '\0') {
        if (sp >= stmtLimit) {
            *sp = '\0';
            err("Statement too long");
            return NULL;
        }
        if (!isspace(*lp)) {
            nbp = sp;
            if (*lp == '\'' || *lp == '"')
                qp = sp;
            else if (*lp == '!' && sp > (stmtBuf + 5))
                xp = sp;
        }
        *sp++ = *lp++;
    }
    sp = nbp + 1;
    if (xp != NULL && (qp == NULL || xp > qp)) sp = xp;
    *sp = '\0';

    return sp;
}

static void assignStorage(void) {
    presetOffsetCalculation();
    autoOffset = -calculateAutoOffsets();
    staticOffset = calculateStaticOffsets();
    calculateCommonOffsets();
    adjustDataInitializers();
}

static int calculateConstOffset(Symbol *symbol, TokenListItem *subscriptList) {
    int d;
    int dim;
    int rank;
    int result;
    OperatorArgument subscript;

    rank = symbol->details.variable.dt.rank;
    enableEmission(FALSE);

    /*
     *  Initialize result with last subscript
     */
    if (evaluateSubscript(symbol, subscriptList, rank - 1, &subscript)) {
        result = -1;
    }
    else if (isConstant(subscript)) {
        result = subscript.details.constant.value.integer;
        if (rank > 1) { // 2 or more dimensional array
            for (d = rank - 2; d >= 0; d--) {
                dim = (symbol->details.variable.dt.bounds[d].upper - symbol->details.variable.dt.bounds[d].lower) + 1;
                result *= dim;
                if (evaluateSubscript(symbol, subscriptList, d, &subscript)) {
                    result = -1;
                    break;
                }
                if (isConstant(subscript)) {
                    result += subscript.details.constant.value.integer;
                }
                else {
                    err("Index expression not constant");
                    if (isCalculation(subscript)) freeAddrReg(subscript.reg);
                    result = -1;
                    break;
                }
            }
        }
    }
    else {
        err("Index expression not constant");
        if (isCalculation(subscript)) freeAddrReg(subscript.reg);
        result = -1;
    }
    if (symbol->details.variable.dt.type == BaseType_Character) {
        result *= symbol->details.variable.dt.constraint;
    }
    enableEmission(TRUE);

    return result;
}

static char *collectStmt() {
    char *lp;
    char *s;
    char *stmtLimit;

    s = stmtBuf;
    *s = '\0';

    for (;;) {
        lp = lineBuf;
        if (*lp == '\0') {
            lp = readLine();
            if (lp == NULL) {
                *s = '\0';
                return (s > stmtBuf) ? stmtBuf : NULL;
            }
        }
        if (*lp == 'C' || *lp == 'c') {
            if (s == stmtBuf) {
                lineNo += 1;
                if (strncasecmp(lp, "CDIR$ ", 5) == 0) {
                    parseDirective(lp, lineNo);
                }
                else {
                    list("%6d: %s", lineNo, lineBuf);
                }
                lineBuf[0] = '\0';
            }
            *s = '\0';
            break;
        }
        else if (*lp == '*' || *lp == '!') {
            if (s == stmtBuf) {
                list("%6d: %s", ++lineNo, lp);
                lineBuf[0] = '\0';
            }
            *s = '\0';
            break;
        }
        else if (stmtBuf[0] == '\0') {
            list("%6d: %s", ++lineNo, lp);
            lineBuf[72] = '\0';
            s = appendLine(s, lineBuf);
            if (s == NULL) {
                lineBuf[0] = '\0';
                break;
            }
        }
        else if (lineBuf[5] != ' ' && lineBuf[5] != '0') {
            list("%6d: %s", ++lineNo, lp);
            lineBuf[72] = '\0';
            s = appendLine(s, lineBuf + 6);
            if (s == NULL) {
                lineBuf[0] = '\0';
                break;
            }
        }
        else {
            *s = '\0';
            break;
        }
        lineBuf[0] = '\0';
    }

    return stmtBuf;
}

void compile(char *name) {
    time_t clock;
    DataType dt;
    DoStackEntry *entry;
    bool hasError;
    int i;
    bool isAsgn;
    bool isDefn;
    char lineLabel[6];
    char *lp;
    char *s;
    char *start;
    StatementClass stmtClass;
    Symbol *sym;
    struct tm *tmp;
    Token token;
    int year;

    clock = time(NULL);
    tmp = localtime(&clock);
    year = tmp->tm_year >= 100 ? tmp->tm_year - 100 : tmp->tm_year;
    sprintf(currentDate, "%02d/%02d/%02d", tmp->tm_mon + 1, tmp->tm_mday, year);
    sprintf(currentTime, "%02d:%02d:%02d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

    lineBuf[0] = '\0';
    lineNo = 0;
    emitStart(name);
    presetProgUnit();
    listSetPageEnd();

    for (;;) {
#if DEBUG
        checkRegisterMap();
        freeAllRegisters();
#endif
        if (errorCount > MAX_ERRS_PER_UNIT) {
            list(" Too many errors, compilation terminated");
            if (progUnitSym != NULL) {
                fprintf(stderr, "Too many errors in %s\n", progUnitSym->identifier);
            }
            else {
                fputs("Too many errors\n", stderr);
            }
            exit(1);
        }
        start = s = collectStmt();
        if (s == NULL) {
            emitCommonBlocks();
            emitStaticInitializers(firstDListItem, firstCListItem);
            freeStaticInitializers();
            emitEnd();
            break;
        }
        if (*s == '\0') continue;
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
        isAsgn = isAssignment(s, &isDefn, &hasError);
        if (hasError) continue;
        s = getNextToken(s + 1, &token, isAsgn == FALSE && isDefn == FALSE);
        if (isDefn) {
            if (state < STATE_DEFINITION) assignStorage();
            stmtClass = StmtClass_Nonexecutable;
            state = STATE_DEFINITION;
        }
        else if (isAsgn) {
            if (state < STATE_DEFINITION) assignStorage();
            stmtClass = StmtClass_Executable;
            state = STATE_EXECUTABLE;
        }
        else if (token.type == TokenType_Keyword) {
            if (state == STATE_DEFINITION) state = STATE_EXECUTABLE;
            stmtClass = token.details.keyword.class;
        }
        else {
            if (state == STATE_DEFINITION) state = STATE_EXECUTABLE;
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
                case COMPLEX:
                    notSupported("COMPLEX");
                    continue;
                case CHARACTER:
                case INTEGER:
                case LOGICAL:
                case DOUBLEPRECISION:
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
                case COMPLEX:
                    notSupported("COMPLEX");
                    continue;
                case CHARACTER:
                case DOUBLEPRECISION:
                case INTEGER:
                case LOGICAL:
                case REAL:
                    s = parseDataType(s, &token, &dt);
                    if (dt.type != BaseType_Undefined) {
                        s = parseTypeDecl(s, &dt);
                    }
                    continue;
                case POINTER:
                    parsePOINTER(s);
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
            if (progUnitSym->class == SymClass_BlockData) {
                err("Misplaced statement");
                continue;
            }
            /*
             *  Token is not a specification statement, so proceed to STATE_EXECUTABLE.
             */
            assignStorage();
            state = STATE_EXECUTABLE;
            // fall through

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
                    parseCLOSE(s);
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
                case END:
                    parseEND(s);
                    presetProgUnit();
                    continue;
                case ENDDO:
                    parseENDDO(s);
                    break;
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
                case INQUIRE:
                    parseINQUIRE(s);
                    break;
                case OPEN:
                    parseOPEN(s);
                    break;
                case PAUSE:
                    parsePAUSE(s);
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
                    parseRETURN(s);
                    break;
                case REWIND:
                    break;
                case STOP:
                    parseSTOP(s);
                    break;
                case WRITE:
                    parseWRITE(s);
                    break;
                default:
                    err("Misplaced statement");
                    break;
                }
            }
            else if (isAsgn) {
                parseAssignment(s, &token);
            }
            else {
                err("Invalid statement");
            }
            break;
        case STATE_DEFINITION:
            parseStmtFunction(s, &token);
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
                emitEndDo(entry);
            }
        }
    }
    if (totalErrors > 0) exit(1);
}

static void copyCharValue(DataValue *to, DataValue *from) {
    int len;

    len = from->character.length;
    if (from->character.string != NULL) {
        to->character.length = len;
        to->character.string = (char *)allocate(len + 1);
        memcpy(to->character.string, from->character.string, len);
    }
    else {
        to->character.length = 0;
        to->character.string = NULL;
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
            copyCharValue(&new->details.constant.value, &token->details.constant.value);
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

static void defineLocalVariable(Symbol *symbol) {
    defineType(symbol);
    if (doStaticLocals) {
        symbol->class = SymClass_Static;
        symbol->details.variable.offset = staticOffset;
        symbol->details.variable.staticBlock = (progUnitSym->class != SymClass_StmtFunction) ? progUnitSym : progUnitSym->details.progUnit.parentUnit;
        staticOffset += calculateSize(symbol);
    }
    else {
        symbol->class = SymClass_Auto;
        autoOffset -= calculateSize(symbol);
        symbol->details.variable.offset = autoOffset;
    }
}

static void defineType(Symbol *symbol) {
    if (symbol->details.variable.dt.type == BaseType_Undefined)
        symbol->details.variable.dt = implicitTypes[toupper(symbol->identifier[0]) - 'A'];
}

static char *eatWsp(char *s) {
    while (isspace(*s)) s += 1;
    return s;
}

static void errArgType(OperatorId op, BaseType type, OperatorArgument *arg) {
    err("Invalid argument type %s to '%s'", baseTypeToStr(type), op);
    if (arg != NULL && arg->class == ArgClass_Constant && arg->details.constant.dt.type == BaseType_Character) {
        freeCharValue(&arg->details.constant.value);
    }
}

static bool evaluateArrayRef(Symbol *symbol, TokenListItem *subscriptList, OperatorArgument *offset) {
    BaseType coercedType;
    int constraint;
    int d;
    int dim;
    DataType *dt;
    TokenListItem *qualifier;
    int rank;
    Register reg;
    u16 registerMap;
    OperatorArgument subscript;
    OperatorArgument sum;
    BaseType type;

    dt = getSymbolType(symbol);
    type = dt->type;
    rank = dt->rank;
    constraint = dt->constraint;

    if (symbol->class == SymClass_Adjustable) {
        registerMap = getRegisterMap();
        emitSaveRegs(registerMap);
        for (d = rank - 1; d >= 0; d--) {
            qualifier = getQualifier(subscriptList, d);
            if (qualifier == NULL || qualifier->item == NULL || evaluateExpression(qualifier->item, &subscript)) {
                err("Incorrect array index");
                return TRUE;
            }
            dt = getDataType(&subscript);
            if (dt->type != BaseType_Integer) {
                coercedType = coerceArgument(&subscript, dt->type, BaseType_Integer);
                if (coercedType == BaseType_Undefined) {
                    err("Incorrect subscript type");
                    if (isCalculation(subscript)) freeRegister(subscript.reg);
                    return TRUE;
                }
            }
            if (isConstant(subscript)) {
                emitPushInt(subscript.details.constant.value.integer);
            }
            else {
                loadValue(&subscript);
                emitPushReg(subscript.reg);
                freeRegister(subscript.reg);
            }
        }
        emitPushInt(rank);
        reg = emitLoadAdjBoundsRef(symbol);
        emitPushAddrReg(reg);
        freeAddrReg(reg);
        emitPrimCall("@_daryof");
        reg = allocateAddrReg();
        emitCopyAddrReg(reg, ADDR_RESULT_REG);
        emitAdjustSP(rank + 2);
        emitRestoreRegs(registerMap);
        sum.class = ArgClass_Calculation;
        sum.reg = reg;
    }
    else {
        /*
         *  Initialize sum with last subscript
         */
        if (evaluateSubscript(symbol, subscriptList, rank - 1, &sum)) return TRUE;

        if (rank > 1) { // 2 or more dimensional array
            for (d = rank - 2; d >= 0; d--) {
                dim = (dt->bounds[d].upper - dt->bounds[d].lower) + 1;
                if (isConstant(sum)) {
                    sum.details.constant.value.integer *= dim;
                }
                else {
                    emitMulOffset(sum.reg, dim);
                }
                if (evaluateSubscript(symbol, subscriptList, d, &subscript)) {
                    if (isCalculation(sum)) freeAddrReg(sum.reg);
                    return TRUE;
                }
                if (isConstant(sum)) {
                    if (isConstant(subscript)) {
                        sum.details.constant.value.integer += subscript.details.constant.value.integer;
                    }
                    else {
                        emitAddOffset(subscript.reg, sum.details.constant.value.integer);
                        sum.class = ArgClass_Calculation;
                        sum.reg = subscript.reg;
                    }
                }
                else if (isConstant(subscript)) {
                    emitAddOffset(sum.reg, subscript.details.constant.value.integer);
                }
                else {
                    emitAddOffsets(subscript.reg, sum.reg);
                    freeAddrReg(subscript.reg);
                }
            }
        }
    }
    if (type == BaseType_Character) {
        if (constraint == -1) {
            if (isConstant(sum)) emitLoadConstOffset(&sum);
            emitMulSize(sum.reg, symbol);
        }
        else if (isConstant(sum)) {
            sum.details.constant.value.integer *= dt->constraint;
        }
        else {
            emitMulOffset(sum.reg, dt->constraint);
        }
    }
    *offset = sum;

    return FALSE;
}

static bool evaluateExpression(Token *expression, OperatorArgument *result) {
    int curOpStkBtm;
    int argStkBtm;
    bool err;
    Token token;

    curOpStkBtm = opStkBtm;
    argStkBtm = argStkPtr;
    opStkBtm  = opStkPtr;
    err = evaluateExprHelper(expression);
    if (err == FALSE && argStkPtr == (argStkBtm + 1) && opStkPtr == opStkBtm) {
        popArg(result);
    }
    else {
        argStkPtr = argStkBtm;
        opStkPtr  = opStkBtm;
    }
    opStkBtm = curOpStkBtm;

    return err;
}

static bool evaluateExprHelper(Token *expression) {
    OperatorArgument arg;
    OperatorDetails op;
    Token *rightArg;

    switch (expression->type) {
    case TokenType_Identifier:
        if (evaluateIdentifier(expression)) return TRUE;
        break;
    case TokenType_Constant:
        arg.class = ArgClass_Constant;
        arg.details.constant = expression->details.constant;
        if (expression->details.constant.dt.type == BaseType_Character) {
            copyCharValue(&arg.details.constant.value, &expression->details.constant.value);
        }
        pushArg(&arg);
        break;
    case TokenType_Operator:
        if (expression->details.operator.id == OP_SEXPR) {
            pushOp(&expression->details.operator);
            if (evaluateExprHelper(expression->details.operator.rightArg)) return TRUE;
            opStkPtr -= 1;
            return FALSE;
        }
        if (expression->details.operator.leftArg != NULL) {
            if (evaluateExprHelper(expression->details.operator.leftArg)) return TRUE;
            while (opStkPtr > opStkBtm
                   && opStack[opStkPtr - 1].id != OP_SEXPR
                   && expression->details.operator.precedence >= opStack[opStkPtr - 1].precedence) {
                popOp(&op);
                if (executeOperator(op.id)) return TRUE;
            }
        }
        pushOp(&expression->details.operator);
        rightArg = expression->details.operator.rightArg;
        if (rightArg->type == TokenType_Operator
            && isUnaryOp(rightArg->details.operator.id)
            && rightArg->details.operator.precedence >= expression->details.operator.precedence) {
            err("Expression syntax");
            return TRUE;
        }
        if (evaluateExprHelper(rightArg)) return TRUE;
        while (opStkPtr > opStkBtm && opStack[opStkPtr - 1].id != OP_SEXPR) {
            popOp(&op);
            if (executeOperator(op.id)) return TRUE;
        }
        break;
    default:
        err("Expression syntax");
        return TRUE;
    }

    return FALSE;
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

static bool evaluateFunction(Token *fn, Symbol *symbol, Symbol *intrinsic) {
    DataType *dt;
    int frameSize;
    int parmIdx;
    int pass;
    TokenListItem *qualifier;
    Register reg;
    u16 registerMap;
    OperatorArgument result;
    int tempIdx;
    int varArgIncr;

    registerMap = getRegisterMap();
    emitSaveRegs(registerMap);
    enableEmission(FALSE);
    varArgIncr = 0;
    if (intrinsic != NULL) {
        intrinsic = matchIntrinsic(fn, intrinsic);
        if (intrinsic == NULL) {
            enableEmission(TRUE);
            emitRestoreRegs(registerMap);
            return TRUE;
        }
        if (intrinsic->details.intrinsic.argc < 0) varArgIncr = 1;
    }
    frameSize = varArgIncr;
    for (pass = 1; pass <= 2; pass++) {
        if (pass == 2) {
            enableEmission(TRUE);
            emitAdjustSP(-frameSize);
            tempIdx = parmIdx;
        }
        parmIdx = varArgIncr;
        for (qualifier = fn->details.identifier.qualifiers; qualifier != NULL; qualifier = qualifier->next) {
            if (qualifier->item == NULL) continue;
            if (evaluateExpression(qualifier->item, &result)) {
                if (pass == 1) {
                    enableEmission(TRUE);
                }
                else {
                    emitAdjustSP(frameSize);
                }
                emitRestoreRegs(registerMap);
                return TRUE;
            }
            dt = getDataType(&result);
            if (isConstant(result)) {
                if (dt->type == BaseType_Character) {
                    if (pass == 1) {
                        frameSize += 1;
                    }
                    else {
                        emitLoadConst(&result);
                        freeCharValue(&result.details.constant.value);
                        emitStoreStack(result.reg, parmIdx);
                        freeRegister(result.reg);
                    }
                }
                else {
                    if (pass == 1) {
                        frameSize += 2;
                    }
                    else {
                        emitLoadConst(&result);
                        emitStoreStack(result.reg, tempIdx);
                        freeRegister(result.reg);
                        if (intrinsic == NULL || intrinsic->details.intrinsic.hasCifc == FALSE) {
                            emitStoreParmAddr(tempIdx, parmIdx);
                        }
                        else {
                            reg = emitLoadStackByteAddr(tempIdx);
                            emitStoreStack(reg, parmIdx);
                            freeRegister(reg);
                        }
                        tempIdx += 1;
                    }
                }
            }
            else if (isLoadable(result)) {
                if (pass == 1) {
                    frameSize += 1;
                    freeAllRegisters();
                }
                else {
                    if (intrinsic != NULL && intrinsic->details.intrinsic.hasCifc && dt->type != BaseType_Character)
                        emitLoadByteReference(&result, NULL);
                    else
                        emitLoadReference(&result, NULL);
                    emitStoreStack(result.reg, parmIdx);
                    freeRegister(result.reg);
                }
            }
            else if (dt->type == BaseType_Character) {
                if (pass == 1) {
                    frameSize += 1;
                    freeAllRegisters();
                }
                else {
                    emitStoreStack(result.reg, parmIdx);
                    freeRegister(result.reg);
                }
            }
            else {
                if (pass == 1) {
                    frameSize += 2;
                    freeAllRegisters();
                }
                else {
                    emitStoreStack(result.reg, tempIdx);
                    freeRegister(result.reg);
                    if (intrinsic == NULL || intrinsic->details.intrinsic.hasCifc == FALSE) {
                        emitStoreParmAddr(tempIdx, parmIdx);
                    }
                    else {
                        reg = emitLoadStackByteAddr(tempIdx);
                        emitStoreStack(reg, parmIdx);
                        freeRegister(reg);
                    }
                    tempIdx += 1;
                }
            }
            parmIdx += 1;
        }
    }
    if (varArgIncr != 0) emitStoreStackInt(parmIdx - 1, 0);
    if (intrinsic != NULL) {
        emitSubprogramCall(intrinsic->details.intrinsic.externName, NULL);
    }
    else if (symbol->class != SymClass_StmtFunction) {
        emitSubprogramCall(fn->details.identifier.name, NULL);
    }
    else {
        emitSubprogramCall(fn->details.identifier.name, getProgUnitQualifier());
    }
    emitAdjustSP(frameSize);
    emitRestoreRegs(registerMap);
    reg = allocateRegister();
    emitCopyRegister(reg, RESULT_REG);
    result.class = ArgClass_Calculation;
    if (intrinsic == NULL) {
        result.details.calculation = symbol->details.variable.dt;
    }
    else {
        result.details.calculation.type = intrinsic->details.intrinsic.resultType;
        result.details.calculation.rank = 0;
    }
    result.reg = reg;
    pushArg(&result);

    return FALSE;
}

static bool evaluateIdentifier(Token *id) {
    OperatorArgument arg;
    DataType *dt;
    Symbol *intrinsic;
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
    switch (symbol->class) {
    case SymClass_Undefined:
        if (id->details.identifier.qualifiers == NULL) { // simple variable reference
            defineLocalVariable(symbol);
        }
        else {
            intrinsic = findIntrinsicFunction(name);
            if (intrinsic != NULL) {
                symbol->class = SymClass_Intrinsic;
                symbol->details.intrinsic.resultType = intrinsic->details.intrinsic.resultType;
            }
            else {
                defineType(symbol);
                symbol->isFnRef = TRUE;
            }
            return evaluateFunction(id, symbol, intrinsic);
        }
        break;
    case SymClass_Argument:
        if (symbol->details.variable.dt.type == BaseType_Undefined) {
            symbol->details.variable.dt = implicitTypes[toupper(name[0]) - 'A'];
        }
        break;
    case SymClass_Function:
        if (symbol->details.progUnit.dt.type == BaseType_Undefined) {
            symbol->details.progUnit.dt = implicitTypes[toupper(name[0]) - 'A'];
            autoOffset -= calculateSize(symbol);
            symbol->details.progUnit.offset = autoOffset;
        }
        break;
    case SymClass_StmtFunction:
        return evaluateFunction(id, symbol, NULL);
    case SymClass_Intrinsic:
        return evaluateFunction(id, symbol, findIntrinsicFunction(name));
    default:
        // do nothing
        break;
    }
    arg.class = argClassForSymClass[symbol->class];
    switch (symbol->class) {
    case SymClass_Function:
        if (symbol->details.progUnit.dt.constraint == -1) {
            err("Invalid reference to assumed-size %s", symbol->identifier);
            return TRUE;
        }
    case SymClass_Auto:
    case SymClass_Static:
    case SymClass_Adjustable:
    case SymClass_Global:
    case SymClass_Argument:
    case SymClass_Pointee:
        dt = getSymbolType(symbol);
        arg.details.reference.symbol = symbol;
        if (id->details.identifier.qualifiers == NULL) {
            arg.details.reference.offsetClass = ArgClass_Undefined;
            if (id->details.identifier.range != NULL) {
                if (evaluateStringRange(symbol, id->details.identifier.range, &strOffset, &strLength)) return TRUE;
                emitLoadReference(&arg, NULL);
                emitUpdateStringRef(&arg, &strOffset, &strLength);
            }
        }
        else if (dt->rank > 0) {
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
                    if (isCalculation(offset)) freeAddrReg(offset.reg);
                    return TRUE;
                }
                emitLoadReference(&arg, NULL);
                emitUpdateStringRef(&arg, &strOffset, &strLength);
            }
            loadValue(&arg);
        }
        else {
            err("%s is not an array", symbol->identifier);
            return TRUE;
        }
        break;
    case SymClass_Parameter:
        arg.details.constant = symbol->details.param;
        if (symbol->details.param.dt.type == BaseType_Character) {
            copyCharValue(&arg.details.constant.value, &symbol->details.param.value);
        }
        break;
    default:
        err("Invalid symbol reference");
        return TRUE;
    }

    pushArg(&arg);
    return FALSE;
}

static bool evaluateInquireReference(StorageReference *ref, int stackOffset) {
    bool isScalar;
    OperatorArgument target;

    if (ref->symbol == NULL) {
        emitLoadNullPtr(&target);
    }
    else {
        if (evaluateStorageReference(ref, &target, NULL, &isScalar)) return TRUE;
        if (isScalar) emitLoadReference(&target, NULL);
    }
    emitStoreStack(target.reg, stackOffset);
    freeRegister(target.reg);
    return FALSE;
}

static bool evaluateStorageReference(StorageReference *reference, OperatorArgument *target, OperatorArgument *object, bool *isScalar) {
    DataType *dt;
    bool isAssumedSize;
    OperatorArgument offset;
    OperatorArgument strLength;
    OperatorArgument strOffset;
    Symbol *symbol;

    symbol = reference->symbol;
    dt = getSymbolType(symbol);
    isAssumedSize = dt->type == BaseType_Character && dt->constraint == -1;
    if (isAssumedSize && (object == NULL || getDataType(object)->type != BaseType_Character)) {
        err("Invalid reference to assumed-size variable %s", symbol->identifier);
        return TRUE;
    }
    *isScalar = FALSE;
    target->class = argClassForSymClass[reference->symbol->class];
    target->details.reference.symbol = symbol;
    if (reference->expressionList == NULL) {
        target->details.reference.offsetClass = ArgClass_Undefined;
        if (reference->strRange == NULL) {
            if (dt->type == BaseType_Character) {
                emitLoadReference(target, object);
            }
            else {
                *isScalar = TRUE;
            }
            return FALSE;
        }
        else {
            if (evaluateStringRange(symbol, reference->strRange, &strOffset, &strLength)) return TRUE;
            emitLoadReference(target, object);
            emitUpdateStringRef(target, &strOffset, &strLength);
        }
    }
    else if (symbol->details.variable.dt.rank > 0) {
        if (evaluateArrayRef(symbol, reference->expressionList, &offset)) return TRUE;
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
            emitLoadReference(target, object);
        }
        else {
            if (evaluateStringRange(symbol, reference->strRange, &strOffset, &strLength)) {
                if (isCalculation(offset)) freeAddrReg(offset.reg);
                return TRUE;
            }
            emitLoadReference(target, object);
            emitUpdateStringRef(target, &strOffset, &strLength);
        }
    }
    else {
        err("%s is not an array", symbol->identifier);
        return TRUE;
    }

    return FALSE;
}

static bool evaluateStringIndex(Token *expression, OperatorArgument *index) {
    void (*binop)(OperatorArgument *leftArg, OperatorArgument *rightArg);
    DataType *dt;
    BaseType type;

    if (evaluateExpression(expression, index)) {
        err("Incorrect string index");
        return TRUE;
    }
    dt = getDataType(index);
    if (dt->type != BaseType_Integer) {
        type = coerceArgument(index, dt->type, BaseType_Integer);
        if (type == BaseType_Undefined) {
            err("Incorrent string index type");
            if (index->class == ArgClass_Calculation) freeRegister(index->reg);
            return TRUE;
        }
    }

    return FALSE;
}

static bool evaluateStringRange(Symbol *symbol, StringRange *range, OperatorArgument *offset, OperatorArgument *length) {
    void (*binop)(OperatorArgument *leftArg, OperatorArgument *rightArg);
    OperatorArgument negOne;

    setIntegerArg(offset, 0);
    setIntegerArg(length, symbol->details.variable.dt.constraint);
    if (range != NULL) {
        if (range->first != NULL) {
            if (evaluateStringIndex(range->first, offset)) return TRUE;
            if (offset->class == ArgClass_Constant) {
                offset->details.constant.value.integer -= 1;
            }
            else {
                setIntegerArg(&negOne, -1);
                emitLoadConst(&negOne);
                if (offset->class > ArgClass_Function) emitLoadValue(offset);
                binop = genBinOps[OP_ADD][BaseType_Integer];
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
        binop = genBinOps[OP_SUB][BaseType_Integer];
        (*binop)(length, offset);
        freeRegister(length->reg);
        length->reg = offset->reg;
        offset->reg = allocateRegister();
        emitPopReg(offset->reg);
    }
    
    return FALSE;
}

static bool evaluateSubscript(Symbol *symbol, TokenListItem *subscriptList, int idx, OperatorArgument *subscript) {
    DataType *dt;
    TokenListItem *qualifier;
    Register reg;
    BaseType type;

    qualifier = getQualifier(subscriptList, idx);
    if (qualifier == NULL || qualifier->item == NULL || evaluateExpression(qualifier->item, subscript)) {
        err("Incorrect array index");
        return TRUE;
    }
    dt = getDataType(subscript);
    if (dt->type != BaseType_Integer) {
        type = coerceArgument(subscript, dt->type, BaseType_Integer);
        if (type == BaseType_Undefined) {
            err("Incorrect subscript type");
            if (subscript->class == ArgClass_Calculation) freeRegister(subscript->reg);
            return TRUE;
        }
    }
    if (symbol->details.variable.dt.bounds[idx].lower != 0) {
        if (subscript->class == ArgClass_Constant) {
            subscript->details.constant.value.integer -= symbol->details.variable.dt.bounds[idx].lower;
        }
        else {
            loadValue(subscript);
            reg = allocateAddrReg();
            emitCopyToOffset(reg, subscript->reg);
            freeRegister(subscript->reg);
            emitAddOffset(reg, -symbol->details.variable.dt.bounds[idx].lower);
            subscript->reg = reg;
            subscript->class = ArgClass_Calculation;
        }
    }
    else if (subscript->class != ArgClass_Constant) {
        loadValue(subscript);
        reg = allocateAddrReg();
        emitCopyToOffset(reg, subscript->reg);
        freeRegister(subscript->reg);
        subscript->reg = reg;
    }
    return FALSE;
}

static bool executeOperator(OperatorId op) {
    BaseType argType;
    void (*binop)(OperatorArgument *leftArg, OperatorArgument *rightArg);
    bool isBop;
    bool isConstResult;
    OperatorArgument leftArg;
    BaseType leftType;
    OperatorArgument rightArg;
    BaseType rightType;

    popArg(&rightArg);
    rightType = getDataType(&rightArg)->type;
    isConstResult = isConstant(rightArg);
    if (isLoadable(rightArg)) loadValue(&rightArg);
    isBop = isBinaryOp(op);
    if (isBop) {
        popArg(&leftArg);
        leftType = getDataType(&leftArg)->type;
        isConstResult = isConstResult && isConstant(leftArg);
        argType = calculateCoercedType(op, leftType, rightType);
        if (argType == BaseType_Undefined) {
            err("Invalid type combination %s/%s to '%s'", baseTypeToStr(leftType), baseTypeToStr(rightType), opIdToStr(op));
            return TRUE;
        }
        if (isLoadable(leftArg)) loadValue(&leftArg);
        if (leftType  != argType) leftType  = coerceArgument(&leftArg,  leftType,  argType);
        if (rightType != argType) rightType = coerceArgument(&rightArg, rightType, argType);
        if (!isConstResult) {
            if (isConstant(leftArg)) {
                emitLoadConst(&leftArg);
                if (leftArg.details.constant.dt.type == BaseType_Character) freeCharValue(&leftArg.details.constant.value);
            }
            if (isConstant(rightArg)) {
                emitLoadConst(&rightArg);
                if (rightArg.details.constant.dt.type == BaseType_Character) freeCharValue(&rightArg.details.constant.value);
            }
        }
    }
    switch (op) {
    /*
     *  Unary operators
     */
    case OP_NEG:
        if (isConstResult) {
            switch (rightType) {
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
            default:
                errArgType(op, rightType, &rightArg);
                return TRUE;
            }
        }
        else {
            switch(rightType) {
            case BaseType_Integer:
            case BaseType_Real:
            case BaseType_Double:
            case BaseType_Logical:
                emitNegReg(rightArg.reg, rightType);
                break;
            default:
                errArgType(op, rightType, &rightArg);
                return TRUE;
            }
        }
        break;
    case OP_NOT:
        if (isConstResult) {
            switch (rightType) {
            case BaseType_Integer:
            case BaseType_Pointer:
            case BaseType_Logical:
                rightArg.details.constant.value.logical = ~rightArg.details.constant.value.logical;
                break;
            default:
                errArgType(op, rightType, &rightArg);
                return TRUE;
            }
        }
        else {
            switch (rightType) {
            case BaseType_Integer:
            case BaseType_Pointer:
            case BaseType_Logical:
                emitNotReg(rightArg.reg, rightType);
                break;
            default:
                errArgType(op, rightType, &rightArg);
                return TRUE;
            }
        }
        break;
    case OP_PLUS:
        switch (rightType) {
        case BaseType_Integer:
        case BaseType_Pointer:
        case BaseType_Real:
        case BaseType_Logical:
        case BaseType_Double:
        case BaseType_Complex:
            break;
        default:
            errArgType(op, rightType, &rightArg);
            return TRUE;
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
        binop = isConstResult ? cstBinOps[op][argType] : genBinOps[op][argType];
        if (binop != NULL) {
            (*binop)(&leftArg, &rightArg);
        }
        else {
            errArgType(op, argType, NULL);
            if (leftArg.class == ArgClass_Constant && leftArg.details.constant.dt.type == BaseType_Character) {
                freeCharValue(&leftArg.details.constant.value);
            }
            if (rightArg.class == ArgClass_Constant && rightArg.details.constant.dt.type == BaseType_Character) {
                freeCharValue(&rightArg.details.constant.value);
            }
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

static void freeCharValue(DataValue *charValue) {
    if (charValue->character.string != NULL) {
        free(charValue->character.string);
    }
    charValue->character.string = NULL;
    charValue->character.length = 0;
}

static void freeConstantList(ConstantListItem *list) {
    ConstantListItem *nextItem;

    while (list != NULL) {
        nextItem = list->next;
        if (list->details.dt.type == BaseType_Character)
            free(list->details.value.character.string);
        free(list);
        list = nextItem;
    }
}

static void freeCloseInfoList(CloseInfoList *ciList) {
    if (ciList != NULL) {
        if (ciList->unit != NULL) freeToken(ciList->unit);
        if (ciList->fileStatus != NULL) freeToken(ciList->fileStatus);
        freeStorageReference(&ciList->iostat);
        free(ciList);
    }
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

static void freeDataInitializerList(DataInitializerItem *list) {
    DataInitializerItem *nextItem;

    while (list != NULL) {
        nextItem = list->next;
        free(list);
        list = nextItem;
    }
}

static void freeImpliedDoList(ImpliedDoList *doList) {
    if (doList != NULL) {
        if (doList->ioList          != NULL) freeIoList(doList->ioList);
        if (doList->initExpression  != NULL) freeToken(doList->initExpression);
        if (doList->limitExpression != NULL) freeToken(doList->limitExpression);
        if (doList->incrExpression  != NULL) freeToken(doList->incrExpression);
        free(doList);
    }
}

static void freeInquireInfoList(InquireInfoList *iiList) {
   if (iiList != NULL) {
        if (iiList->unit != NULL) freeToken(iiList->unit);
        if (iiList->fileName != NULL) freeToken(iiList->fileName);
        freeStorageReference(&iiList->existRef);
        freeStorageReference(&iiList->openedRef);
        freeStorageReference(&iiList->numberRef);
        freeStorageReference(&iiList->namedRef);
        freeStorageReference(&iiList->nameRef);
        freeStorageReference(&iiList->accessRef);
        freeStorageReference(&iiList->sequentialRef);
        freeStorageReference(&iiList->directRef);
        freeStorageReference(&iiList->formattedRef);
        freeStorageReference(&iiList->unformattedRef);
        freeStorageReference(&iiList->formRef);
        freeStorageReference(&iiList->blankRef);
        freeStorageReference(&iiList->reclRef);
        freeStorageReference(&iiList->nextRecRef);
        freeStorageReference(&iiList->iostat);
        free(iiList);
    }
}

static void freeIoList(IoListItem *ioList) {
    IoListItem *next;

    while (ioList != NULL) {
        if (ioList->class == IoListClass_Expression) {
            freeToken(ioList->details.expression);
        }
        else {
            freeImpliedDoList(ioList->details.doList);
        }
        next = ioList->next;
        free(ioList);
        ioList = next;
    }
}

static void freeOpenInfoList(OpenInfoList *oiList) {
    if (oiList != NULL) {
        if (oiList->unit != NULL) freeToken(oiList->unit);
        if (oiList->fileName != NULL) freeToken(oiList->fileName);
        if (oiList->fileStatus != NULL) freeToken(oiList->fileStatus);
        if (oiList->formatting != NULL) freeToken(oiList->formatting);
        if (oiList->access != NULL) freeToken(oiList->access);
        if (oiList->blankSpecifier != NULL) freeToken(oiList->blankSpecifier);
        if (oiList->recordLength != NULL) freeToken(oiList->recordLength);
        freeStorageReference(&oiList->iostat);
        free(oiList);
    }
}

static void freeStaticInitializers(void) {
    freeDataInitializerList(firstDListItem);
    freeConstantList(firstCListItem);
    firstDListItem = NULL;
    firstCListItem = NULL;
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
            if (token->details.constant.dt.type == BaseType_Character
                && token->details.constant.value.character.string != NULL) {
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
    case ArgClass_Auto:
    case ArgClass_Static:
    case ArgClass_Adjustable:
    case ArgClass_Global:
    case ArgClass_Argument:
    case ArgClass_Pointee:
    case ArgClass_Function:
        return getSymbolType(arg->details.reference.symbol);
    case ArgClass_Calculation:
        return &arg->details.calculation;
    default:
        fprintf(stderr, "Unrecognized operator argument class: %d\n", arg->class);
        printStackTrace(stderr);
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

static char *getProgUnitQualifier(void) {
    int hash;
    int len;
    static char qualifier[MAX_EXT_NAME_LENGTH + 1];

    strcpy(qualifier, progUnitSym->identifier);
    len = strlen(qualifier);
    if (len > MAX_EXT_NAME_LENGTH) {
        hash = fnv32a(qualifier, len, FNV1_32A_INIT);
        sprintf(qualifier + 4, "%04x", hash & 0xffff);
    }
    return qualifier;
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

static char *getStorageReference(char *s, char *paramName, BaseType type, StorageReference *reference) {
    DataType *dt;
    Token token;

    s = getNextToken(s, &token, FALSE);
    if (token.type == TokenType_Identifier) {
        s = parseStorageReference(s, &token, reference);
        if (s == NULL) return s;
        dt = getSymbolType(reference->symbol);
        if (dt->type != type) {
            err("%s is not %s", reference->symbol->identifier, baseTypeToStr(type));
            return NULL;
        }
        return s;
    }
    else if (paramName != NULL) {
        err("%s= syntax", paramName);
        return NULL;;
    }
    else {
        err("Syntax");
        return NULL;
    }
}

static void inputCheckIostat(ControlInfoList *ciList) {
    bool isScalar;
    char label[8];
    Register reg;
    OperatorArgument target;

    emitPrimCall("@_iostat");
    reg = RESULT_REG;
    if (ciList->iostat.symbol != NULL) {
        reg = allocateRegister();
        emitCopyRegister(reg, RESULT_REG);
        if (evaluateStorageReference(&ciList->iostat, &target, NULL, &isScalar)) {
            freeRegister(reg);
            return;
        }
        if (isScalar) {
            emitStoreReg(ciList->iostat.symbol, reg);
        }
        else {
            emitStoreRegByReference(&target, reg);
            freeRegister(target.reg);
        }
        freeRegister(reg);
    }
    generateLabel(label);
    emitBranchOnFalse(reg, label);
    emitAdjustSP(2);
    emitBranch3Way(NO_REG,
                   ciList->endLabel != NULL ? ciList->endLabel->details.label.label : "@_fioeof",
                   NULL,
                   ciList->errLabel != NULL ? ciList->errLabel->details.label.label : "@_fioerr");
    emitLabel(label);
}

static void inputFini(ControlInfoList *ciList) {
    if (ciList->format != NULL) emitPrimCall("@_endfmt");
    emitAdjustSP(2);
}

static void inputInit(ControlInfoList *ciList) {
    DataType *dt;
    OperatorArgument unit;

    if (evaluateFmtSpec(ciList) || evaluateExpression(ciList->unit, &unit)) return;
    emitAdjustSP(-2);
    loadValue(&unit);
    emitStoreStack(unit.reg, 0);
    freeRegister(unit.reg);
    dt = getDataType(&unit);
    ciList->unitType = dt->type;
    if (ciList->unitType == BaseType_Character) {
        emitPrimCall("@_setrcd");
    }
    else {
        emitPrimCall("@_setdrc");
        emitPrimCall("@_rdurec");
        inputCheckIostat(ciList);
    }
}

static bool isAssignment(char *s, bool *isDefn, bool *hasError) {
    Token *expression;
    TokenListItem *expressionList;
    bool isId;
    TokenListItem *member;
    StringRange *strRange;
    Token token;

    *isDefn = FALSE;
    *hasError = FALSE;
    s = getNextToken(s, &token, FALSE);
    if (token.type != TokenType_Identifier) return FALSE;
    if (*s == '(') {
        s = parseExpressionList(s, &expressionList);
        if (s == NULL) return FALSE;
        if (state < STATE_EXECUTABLE) {
            isId = TRUE;
            member = expressionList;
            while (member != NULL && member->item != NULL && isId) {
                isId = member->item->type == TokenType_Identifier;
                member = member->next;
            }
            *isDefn = isId;
        }
        freeTokenList(expressionList);
        s = eatWsp(s);
        if (*s == '(') {
            *isDefn = FALSE;
            s = parseStringRange(s, &strRange);
            if (s == NULL) return FALSE;
            freeStringRange(strRange);
        }
    }
    s = eatWsp(s);
    if (*s != '=') {
        *isDefn = FALSE;
        return FALSE;
    }
    s = parseExpression(s + 1, &expression);
    if (expression == NULL) {
        err("Expression syntax");
        *hasError = TRUE;
        return FALSE;
    }
    freeToken(expression);
    s = eatWsp(s);
    if (*s != '\0') {
        *isDefn = FALSE;
        return FALSE;
    }

    return TRUE;
}

static void loadValue(OperatorArgument *value) {
    if (value->class == ArgClass_Constant) {
        emitLoadConst(value);
        if (value->details.constant.dt.type == BaseType_Character) freeCharValue(&value->details.constant.value);
    }
    else if (value->class >= ArgClass_Function) {
        emitLoadValue(value);
    }
}

static Symbol *matchIntrinsic(Token *fn, Symbol *intrinsic) {
    int argc;
    DataType *dt;
    int i;
    bool isGeneric;
    char *name;
    TokenListItem *qualifier;
    OperatorArgument r;
    OperatorArgument *result;
    OperatorArgument results[MAX_INTRINSIC_ARGS];
    BaseType type;

    /*
     *  Assumption: on entry, code emission is disabled and allocated registers
     *  have been saved.
     */
    name = intrinsic->identifier;
    argc = 0;
    if (intrinsic->details.intrinsic.argc != -1) { // function with fixed number of arguments
        for (qualifier = fn->details.identifier.qualifiers; qualifier != NULL; qualifier = qualifier->next) {
            if (qualifier->item == NULL) continue;
            if (argc >= intrinsic->details.intrinsic.argc) {
                err("Too many arguments for intrinsic %s", name);
                return NULL;
            }
            if (evaluateExpression(qualifier->item, &results[argc])) return NULL;
            if (isCalculation(results[argc])) freeRegister(results[argc].reg);
            argc += 1;
        }
        if (argc != intrinsic->details.intrinsic.argc) {
            err("Incorrect number of arguments for intrinsic %s", name);
            return NULL;
        }
    }
    else { // function with variable number of arguments
        for (qualifier = fn->details.identifier.qualifiers; qualifier != NULL; qualifier = qualifier->next) {
            if (qualifier->item == NULL) continue;
            if (evaluateExpression(qualifier->item, &r)) return NULL;
            if (isCalculation(r)) freeRegister(r.reg);
            if (argc == 0) {
                results[argc++] = r;
                type = getDataType(&r)->type;
            }
            else if (type != getDataType(&r)->type) {
                err("Inconsistent data types in call to intrinsic %s", name);
                return NULL;
            }
        }
    }
    isGeneric = intrinsic->details.intrinsic.isGeneric;
    for (;;) {
        i = 0;
        while (i < argc) {
            dt = getDataType(&results[i]);
            if (dt->type != intrinsic->details.intrinsic.argumentTypes[i]) break;
            i += 1;
        }
        if (i >= argc) break;
        if (isGeneric && intrinsic->next != NULL) {
            intrinsic = intrinsic->next;
        }
        else {
            err("Invalid argument type for intrinsic %s", name);
            return NULL;
        }
    }
    return intrinsic;
}

static void notSupported(char *s) {
    err("Not yet supported: %s", s);
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
    Token *expression;
    bool isScalar;
    StorageReference reference;
    OperatorArgument result;
    BaseType symType;
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
        symType = getSymbolType(reference.symbol)->type;
        if (coerceArgument(&result, getDataType(&result)->type, symType) == BaseType_Undefined) {
            err("Invalid type conversion");
            if (isCalculation(result)) freeRegister(result.reg);
            freeToken(expression);
            freeStorageReference(&reference);
            return;
        }
        loadValue(&result);
        if (evaluateStorageReference(&reference, &target, &result, &isScalar)) {
            if (isCalculation(result)) freeRegister(result.reg);
            freeToken(expression);
            freeStorageReference(&reference);
            return;
        }
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

static void outputCheckIostat(ControlInfoList *ciList) {
    bool isScalar;
    char label[8];
    Register reg;
    OperatorArgument target;

    emitPrimCall("@_iostat");
    reg = RESULT_REG;
    if (ciList->iostat.symbol != NULL) {
        reg = allocateRegister();
        emitCopyRegister(reg, RESULT_REG);
        if (evaluateStorageReference(&ciList->iostat, &target, NULL, &isScalar)) {
            freeRegister(reg);
            return;
        }
        if (isScalar) {
            emitStoreReg(ciList->iostat.symbol, reg);
        }
        else {
            emitStoreRegByReference(&target, reg);
            freeRegister(target.reg);
        }
        freeRegister(reg);
    }
    generateLabel(label);
    emitBranchOnFalse(reg, label);
    emitAdjustSP(3);
    emitBranch(ciList->errLabel != NULL ? ciList->errLabel->details.label.label : "@_fioerr");
    emitLabel(label);
}

static void outputFini(ControlInfoList *ciList) {
    if (ciList->unitType != BaseType_Character) {
        emitPrimCall((ciList->format == NULL) ? "@_flulst" : "@_flufmt");
        outputCheckIostat(ciList);
    }
    if (ciList->format != NULL) emitPrimCall("@_endfmt");
    emitAdjustSP(3);
}

static void outputInit(ControlInfoList *ciList) {
    DataType *dt;
    OperatorArgument unit;

    if (evaluateFmtSpec(ciList) || evaluateExpression(ciList->unit, &unit)) return;
    emitAdjustSP(-3);
    loadValue(&unit);
    emitStoreStack(unit.reg, 0);
    freeRegister(unit.reg);
    dt = getDataType(&unit);
    ciList->unitType = dt->type;
    if (ciList->unitType == BaseType_Character) {
        emitPrimCall("@_setrcd");
    }
    else {
        emitPrimCall("@_setdrc");
    }
}

static char *parseActualArguments(char *s, int *frameSize) {
    DataType *dt;
    Token *expression;
    int parmIdx;
    int pass;
    Register reg;
    OperatorArgument result;
    char *start;
    int tempIdx;

    *frameSize = 0;
    tempIdx = 0;
    start = eatWsp(s + 1);
    if (*start == ')') return start + 1;
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
                dt = getDataType(&result);
                if (isConstant(result)) {
                    if (dt->type == BaseType_Character) {
                        if (pass == 1) {
                            *frameSize += 1;
                        }
                        else {
                            emitLoadConst(&result);
                            freeCharValue(&result.details.constant.value);
                            emitStoreStack(result.reg, parmIdx);
                            freeRegister(result.reg);
                        }
                    }
                    else {
                        if (pass == 1) {
                            *frameSize += 2;
                        }
                        else {
                            emitLoadConst(&result);
                            emitStoreStack(result.reg, tempIdx);
                            freeRegister(result.reg);
                            emitStoreParmAddr(tempIdx, parmIdx);
                            tempIdx += 1;
                        }
                    }
                }
                else if (isLoadable(result)) {
                    if (pass == 1) {
                        *frameSize += 1;
                        freeAllRegisters();
                    }
                    else {
                        emitLoadReference(&result, NULL);
                        emitStoreStack(result.reg, parmIdx);
                        freeRegister(result.reg);
                    }
                }
                else if (dt->type == BaseType_Character) {
                    if (pass == 1) {
                        *frameSize += 1;
                        freeAllRegisters();
                    }
                    else {
                        emitStoreStack(result.reg, parmIdx);
                        freeRegister(result.reg);
                    }
                }
                else {
                    if (pass == 1) {
                        *frameSize += 2;
                        freeAllRegisters();
                    }
                    else {
                        emitStoreStack(result.reg, tempIdx);
                        freeRegister(result.reg);
                        emitStoreParmAddr(tempIdx, parmIdx);
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

    s = eatWsp(s);
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
                err("Character length syntax");
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
        err("Character length syntax");
    }
    return s;
}

static char *parseCloseInfoList(char *s, CloseInfoList **ciList) {
    DataType *dt;
    int ec;
    Token *expression;
    CloseInfoList *list;
    Symbol *labelSym;
    int len;
    char lineLabel[16];
    char keyword[16];
    int n;
    StorageReference reference;
    char *start;
    Token token;

    list = (CloseInfoList *)allocate(sizeof(CloseInfoList));
    *ciList = list;
    s = eatWsp(s);
    if (*s != '(') {
        err("Close info list syntax");
        return NULL;
    }
    ec = errorCount;
    expression = NULL;
    s = eatWsp(s + 1);
    start = s;
    n = 0;
    for (;;) {
        s = getNextToken(s, &token, FALSE);
        if (token.type == TokenType_Identifier && *s == '=') {
            s = eatWsp(s + 1);
            len = strlen(token.details.identifier.name);
            if (len >= sizeof(keyword)) len = sizeof(keyword) - 1;
            memcpy(keyword, token.details.identifier.name, len);
            keyword[len] = '\0';
            start = s;
            s = parseExpression(s, &expression);
            if (expression == NULL) {
                err("Invalid expression in close info list");
                break;
            }
            if (strcasecmp(keyword, "UNIT") == 0) {
                if (list->unit != NULL) {
                    err("UNIT specified more than once");
                    break;
                }
                list->unit = expression;
            }
            else if (strcasecmp(keyword, "STATUS") == 0) {
                if (list->fileStatus != NULL) {
                    err("STATUS specified more than once");
                    break;
                }
                list->fileStatus = expression;
            }
            else if (strcasecmp(keyword, "ERR") == 0) {
                if (list->errLabel != NULL) {
                    err("ERR specified more than once");
                    break;
                }
                else if (expression->type == TokenType_Constant && expression->details.constant.dt.type == BaseType_Integer) {
                    sprintf(lineLabel, "%ld", expression->details.constant.value.integer);
                    labelSym = findLabel(lineLabel);
                    if (labelSym != NULL) {
                        if (labelSym->details.label.class != StmtClass_Executable
                            && (labelSym->details.label.class != StmtClass_None || labelSym->details.label.forwardRef == FALSE)) {
                            err("ERR= label does not reference executable statement");
                            break;
                        }
                    }
                    else {
                        labelSym = addLabel(lineLabel);
                        labelSym->details.label.class = StmtClass_None;
                        labelSym->details.label.forwardRef = TRUE;
                    }
                    list->errLabel = labelSym;
                    freeToken(expression);
                }
                else {
                    err("Invalid statement label in ERR=");
                    break;
                }
            }
            else if (strcasecmp(keyword, "IOSTAT") == 0) {
                if (list->iostat.symbol != NULL) {
                    err("IOSTAT specified more than once");
                    break;
                }
                s = getStorageReference(start, "IOSTAT", BaseType_Integer, &reference);
                if (s == NULL) break;
                list->iostat = reference;
            }
            else {
                err("Invalid keyword: %s", token.details.identifier.name);
                break;
            }
            n += 1;
        }
        else if (n == 0) {
            s = parseExpression(start, &expression);
            if (expression == NULL) {
                err("Invalid expression in close list");
                break;
            }
            list->unit = expression;
            n += 1;
        }
        else {
            err("Close list syntax");
            break;
        }
        expression = NULL;
        s = eatWsp(s);
        if (*s == ',') {
            s += 1;
        }
        else if (*s == ')') {
            break;
        }
        else {
            err("Close list syntax");
            break;
        }
    }
    if (ec != errorCount) {
        if (expression != NULL) freeToken(expression);
        freeCloseInfoList(list);
        return NULL;
    }
    else {
        return s + 1;
    }
}

static char *parseControlInfoList(char *s, ControlInfoList **ciList, int defaultUnit) {
    DataType *dt;
    int ec;
    Token *expression;
    bool isListDirected;
    ControlInfoList *list;
    Symbol *labelSym;
    int len;
    char lineLabel[16];
    int n;
    char keyword[16];
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
    ec = errorCount;
    expression = NULL;
    n = 0;
    for (;;) {
        s = eatWsp(s + 1);
        if (*s == '*' && n < 2) {
            if (list->unit == NULL && isListDirected == FALSE) {
                list->unit = createIntegerConstant(defaultUnit);
            }
            else if (list->format == NULL && isListDirected == FALSE) {
                isListDirected = TRUE;
            }
            else {
                err("I/O control info list syntax");
                break;
            }
            s += 1;
            n += 1;
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
                        break;
                    }
                    s += 1;
                }
                else {
                    len = strlen(token.details.identifier.name);
                    if (len >= sizeof(keyword)) len = sizeof(keyword) - 1;
                    memcpy(keyword, token.details.identifier.name, len);
                    keyword[len] = '\0';
                    start = s;
                    s = parseExpression(s, &expression);
                    if (expression == NULL) {
                        err("Invalid expression in I/O control list");
                        break;
                    }
                    if (strcasecmp(keyword, "UNIT") == 0) {
                        if (list->unit != NULL) {
                            err("UNIT specified more than once");
                            break;
                        }
                        list->unit = expression;
                    }
                    else if (strcasecmp(keyword, "FMT") == 0) {
                        if (list->format != NULL || isListDirected) {
                            err("FMT specified more than once");
                            break;
                        }
                        list->format = expression;
                    }
                    else if (strcasecmp(keyword, "END") == 0) {
                        if (list->endLabel != NULL) {
                            err("END specified more than once");
                            break;
                        }
                        else if (expression->type == TokenType_Constant && expression->details.constant.dt.type == BaseType_Integer) {
                            sprintf(lineLabel, "%ld", expression->details.constant.value.integer);
                            labelSym = findLabel(lineLabel);
                            if (labelSym != NULL) {
                                if (labelSym->details.label.class != StmtClass_Executable
                                    && (labelSym->details.label.class != StmtClass_None || labelSym->details.label.forwardRef == FALSE)) {
                                    err("END= label does not reference executable statement");
                                    break;
                                }
                            }
                            else {
                                labelSym = addLabel(lineLabel);
                                labelSym->details.label.class = StmtClass_None;
                                labelSym->details.label.forwardRef = TRUE;
                            }
                            list->endLabel = labelSym;
                            freeToken(expression);
                        }
                        else {
                            err("Invalid statement label in END=");
                            break;
                        }
                    }
                    else if (strcasecmp(keyword, "ERR") == 0) {
                        if (list->errLabel != NULL) {
                            err("ERR specified more than once");
                            break;
                        }
                        else if (expression->type == TokenType_Constant && expression->details.constant.dt.type == BaseType_Integer) {
                            sprintf(lineLabel, "%ld", expression->details.constant.value.integer);
                            labelSym = findLabel(lineLabel);
                            if (labelSym != NULL) {
                                if (labelSym->details.label.class != StmtClass_Executable
                                    && (labelSym->details.label.class != StmtClass_None || labelSym->details.label.forwardRef == FALSE)) {
                                    err("ERR= label does not reference executable statement");
                                    break;
                                }
                            }
                            else {
                                labelSym = addLabel(lineLabel);
                                labelSym->details.label.class = StmtClass_None;
                                labelSym->details.label.forwardRef = TRUE;
                            }
                            list->errLabel = labelSym;
                            freeToken(expression);
                        }
                        else {
                            err("Invalid statement label in ERR=");
                            break;
                        }
                    }
                    else if (strcasecmp(keyword, "IOSTAT") == 0) {
                        if (list->iostat.symbol != NULL) {
                            err("IOSTAT specified more than once");
                            break;
                        }
                        s = getStorageReference(start, "IOSTAT", BaseType_Integer, &reference);
                        if (s == NULL) break;
                        list->iostat = reference;
                    }
                    else if (strcasecmp(keyword, "REC") == 0) {
                        if (list->recordNumber != NULL) {
                            err("REC specified more than once");
                            break;
                        }
                        list->recordNumber = expression;
                    }
                    else {
                        err("Invalid keyword: %s", token.details.identifier.name);
                        break;
                    }
                }
                n += 1;
            }
            else if (n < 2) {
                s = parseExpression(start, &expression);
                if (expression == NULL) {
                    err("Invalid expression in I/O control list");
                    break;
                }
                if (list->unit == NULL) {
                    list->unit = expression;
                }
                else if (list->format == NULL && isListDirected == FALSE) {
                    list->format = expression;
                }
                else {
                    err("I/O control info list syntax");
                    break;
                }
                n += 1;
            }
            else {
                err("I/O control info list syntax");
                break;
            }
        }
        expression = NULL;
        s = eatWsp(s);
        if (*s == ')') {
            break;
        }
        else if (*s != ',') {
            err("I/O control info list syntax");
            break;
        }
    }
    if (ec != errorCount) {
        if (expression != NULL) freeToken(expression);
        freeControlInfoList(list);
        return NULL;
    }
    else {
        return s + 1;
    }
}

static char *parseDataType(char *s, Token *token, DataType *dt) {
    memset(dt, 0, sizeof(DataType));
    dt->type = BaseType_Undefined;
    if (token->type == TokenType_Keyword) {
        switch (token->details.keyword.id) {
        case CHARACTER:
            dt->type = BaseType_Character;
            s = eatWsp(s);
            if (*s == '*') {
                s = parseCharConstraint(s + 1, token, dt);
            }
            else {
                dt->constraint = 1;
            }
            break;
        case COMPLEX:
            dt->type = BaseType_Complex;
            notSupported("COMPLEX");
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
        case REAL:
            dt->type = BaseType_Real;
            break;
        default:
            break;
        }
    }
    return s;
}

static char *parseDimDecl(char *s, Symbol *symbol) {
    DataType *bdt;
    DataType *dt;
    int ec;
    Token *expression;
    bool isAdjustable;
    OperatorArgument lowerBound;
    int rank;
    OperatorArgument result;
    char *start;
    Token token;
    OperatorArgument upperBound;

    dt = getSymbolType(symbol);

    /*
     *  Pass 1. Calculate rank and determine whether any bounds are adjustable
     */
    s = eatWsp(s);
    start = s;
    rank = 0;
    isAdjustable = FALSE;
    ec = errorCount;
    enableEmission(FALSE);
    for (;;) {
        if (*s == '*') {
            s = eatWsp(s + 1);
            if (*s != ')') {
                err("Invalid expression in dimension declaration");
                break;
            }
            if (symbol->class != SymClass_Argument) {
                err("Invalid assumed-size array declaration");
                break;
            }
            if (dt->rank >= MAX_DIMENSIONS) {
                err("Too many dimensions");
                break;
            }
            rank += 1;
            break;
        }
        s = parseExpression(s, &expression);
        if (expression == NULL) {
            err("Invalid expression in dimension declaration");
            break;
        }
        if (evaluateExpression(expression, &result)) break;
        if (!isConstant(result)) isAdjustable = TRUE;
        if (isCalculation(result)) freeRegister(result.reg);
        freeToken(expression);
        s = eatWsp(s);
        if (*s == ':') {
            s = parseExpression(s + 1, &expression);
            if (expression == NULL) {
                err("Invalid expression in dimension declaration");
                break;
            }
            if (evaluateExpression(expression, &result)) break;
            if (!isConstant(result)) isAdjustable = TRUE;
            if (isCalculation(result)) freeRegister(result.reg);
            freeToken(expression);
        }
        if (rank >= MAX_DIMENSIONS) {
            err("Too many dimensions");
            break;
        }
        rank += 1;
        if (*s == ',') {
            s = eatWsp(s + 1);
        }
        else if (*s == ')') {
            break;
        }
        else {
            err("Incorrect dimension declaration");
            break;
        }
    }
    enableEmission(TRUE);
    freeAllRegisters();
    if (errorCount > ec) return s;

    if (isAdjustable) {
        switch (symbol->class) {
        case SymClass_Argument:
            symbol->details.adjustable.argOffset = symbol->details.adjustable.offset;
            // fall through
        case SymClass_Undefined:
        case SymClass_Auto:
        case SymClass_Static:
            symbol->class = SymClass_Adjustable;
            break;
        default:
            err("Invalid adjustable array declaration: %s", symbol->identifier);
            return s;
        }
        autoOffset -= (rank * 2) + 1;
        symbol->details.adjustable.offset = autoOffset;
    }

    s = start;

    /*
     *  Pass 2. Evaluate bounds
     */
    dt->rank = rank;
    rank = 0;
    for (;;) {
        if (*s == '*') {
            s = eatWsp(s + 1);
            if (*s == ')') s += 1;
            if (isAdjustable) {
                setIntegerArg(&lowerBound, 1);
                loadValue(&lowerBound);
                emitStoreFrame(lowerBound.reg, autoOffset + (rank * 2) + 1);
                freeRegister(lowerBound.reg);
                setIntegerArg(&upperBound, 0);
                loadValue(&upperBound);
                emitStoreFrame(upperBound.reg, autoOffset + (rank * 2) + 2);
                freeRegister(upperBound.reg);
            }
            else {
                dt->bounds[rank].lower = 1;
                dt->bounds[rank].upper = 0;
            }
            break;
        }
        s = parseExpression(s, &expression);
        if (evaluateExpression(expression, &upperBound)) {
            freeToken(expression);
            break;
        }
        freeToken(expression);
        bdt = getDataType(&upperBound);
        if (bdt->type != BaseType_Integer) {
            err("Dimension expression is not integer");
            if (isCalculation(upperBound)) freeRegister(upperBound.reg);
            break;
        }
        s = eatWsp(s);
        if (*s == ':') {
            lowerBound = upperBound;
            s = parseExpression(s + 1, &expression);
            if (evaluateExpression(expression, &upperBound)) {
                freeToken(expression);
                break;
            }
            freeToken(expression);
            bdt = getDataType(&upperBound);
            if (bdt->type != BaseType_Integer) {
                err("Dimension expression is not integer");
                if (isCalculation(lowerBound)) freeRegister(lowerBound.reg);
                if (isCalculation(upperBound)) freeRegister(upperBound.reg);
                break;
            }
        }
        else {
            setIntegerArg(&lowerBound, 1);
        }
        if (isAdjustable) {
            loadValue(&lowerBound);
            emitStoreFrame(lowerBound.reg, autoOffset + (rank * 2) + 1);
            freeRegister(lowerBound.reg);
            loadValue(&upperBound);
            emitStoreFrame(upperBound.reg, autoOffset + (rank * 2) + 2);
            freeRegister(upperBound.reg);
        }
        else {
            if (lowerBound.details.constant.value.integer > upperBound.details.constant.value.integer) {
                err("Lower bound greater than upper bound in dimension declaration");
                break;
            }
            dt->bounds[rank].lower = lowerBound.details.constant.value.integer;
            dt->bounds[rank].upper = upperBound.details.constant.value.integer;
        }
        rank += 1;
        if (*s == ',') {
            s = eatWsp(s + 1);
        }
        else if (*s == ')') {
            s += 1;
            break;
        }
    }
    if (isAdjustable) {
        emitInitAdjustableRef(symbol);
    }

    return s;
}

static void parseDirective(char *s, int lineNo) {
    char *start;
    Token token;

    start = s;
    s = getNextToken(s + 6, &token, FALSE);
    if (token.type == TokenType_Identifier) {
        if (strcasecmp(token.details.identifier.name, "EJECT") == 0) {
            listEject();
            return;
        }
        else if (strcasecmp(token.details.identifier.name, "LIST") == 0) {
            doList = TRUE;
            return;
        }
        else if (strcasecmp(token.details.identifier.name, "NOLIST") == 0) {
            doList = FALSE;
            return;
        }
        else if (strcasecmp(token.details.identifier.name, "ALLOC") == 0 && *s == '=') {
            s = getNextToken(s + 1, &token, FALSE);
            if (token.type == TokenType_Identifier) {
                if (strcasecmp(token.details.identifier.name, "STATIC") == 0) {
                    doStaticLocals = TRUE;
                }
                else if (strcasecmp(token.details.identifier.name, "STACK") == 0
                         || strcasecmp(token.details.identifier.name, "AUTO") == 0) {
                    if (progUnitSym->class == SymClass_BlockData) {
                        list("%6d: %s", lineNo, start);
                        err("ALLOC=%s invalid for BLOCK DATA", token.details.identifier.name);
                        return;
                    }
                    doStaticLocals = FALSE;
                }
            }
        }
    }
    list("%6d: %s", lineNo, start);
}

static char *parseExpression(char *s, Token **expression) {
    TokenListItem *expressionList;
    Token *leftArg;
    Token *op;
    Token *rightArg;
    char *start;
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
        s = eatWsp(s + 1);
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
            start = s;
            s = eatWsp(s + 1);
            if (*s == ')') { // maybe a parameter-less function call
                expressionList = (TokenListItem *)allocate(sizeof(TokenListItem));
                s = eatWsp(s + 1);
            }
            else {
                tp = copyToken(&token);
                s = parseExpressionList(start, &expressionList);
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
        if (tp == NULL) {
            tp = copyToken(&token);
            if (token.details.constant.dt.type == BaseType_Character && token.details.constant.value.character.string != NULL) {
                free(token.details.constant.value.character.string);
            }
        }
        if (leftArg != NULL) {
            freeTokenList(expressionList);
            freeToken(tp);
            freeToken(leftArg);
            *expression = NULL;
            return s;
        }
        s = eatWsp(s);
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
            if (isUnaryOp(op->details.operator.id) && rightArg->type == TokenType_Constant) {
                if (op->details.operator.id == OP_NEG) {
                    freeToken(op);
                    *expression = rightArg;
                    switch (rightArg->details.constant.dt.type) {
                    case BaseType_Integer:
                    case BaseType_Pointer:
                        rightArg->details.constant.value.integer = -rightArg->details.constant.value.integer;
                        break;
                    case BaseType_Real:
                    case BaseType_Double:
                        rightArg->details.constant.value.real = -rightArg->details.constant.value.real;
                        break;
                    case BaseType_Logical:
                        rightArg->details.constant.value.logical = ~rightArg->details.constant.value.logical;
                        break;
                    case BaseType_Complex:
                    default:
                        err("Syntax");
                        freeToken(rightArg);
                        *expression = NULL;
                    }
                    return s;
                }
                else if (op->details.operator.id == OP_PLUS) {
                    freeToken(op);
                    *expression = rightArg;
                    return s;
                }
            }
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

static char *parseFormalArguments(char *s, bool isStmtFn) {
    int argIdx;
    char *id;
    Symbol *shadow;
    char *start;
    Symbol *symbol;
    Token token;

    argIdx = 0;
    s = eatWsp(s);
    if (*s != '(') return s;
    start = s;
    s = eatWsp(s + 1);
    if (*s == ')') return s + 1;
    s = start;
    for (;;) {
        s = getNextToken(s + 1, &token, FALSE);
        if (token.type == TokenType_Identifier) {
            id = token.details.identifier.name;
            symbol = findSymbol(id);
            if (symbol == NULL) {
                symbol = addSymbol(id, SymClass_Argument);
                symbol->isShadow = isStmtFn;
                symbol->details.variable.offset = argIdx + 2; // base pointer offset after subprogram call
            }
            else if (isStmtFn && symbol->shadow == NULL) {
                shadow = createShadow(symbol, SymClass_Argument);
                switch (symbol->class) {
                case SymClass_Auto:
                case SymClass_Static:
                case SymClass_Global:
                case SymClass_Argument:
                case SymClass_Undefined:
                    shadow->details.variable.dt = symbol->details.variable.dt;
                    break;
                case SymClass_Function:
                case SymClass_StmtFunction:
                    shadow->details.variable.dt = symbol->details.progUnit.dt;
                    break;
                case SymClass_Parameter:
                    shadow->details.variable.dt = symbol->details.param.dt;
                    break;
                case SymClass_Pointee:
                    shadow->details.variable.dt = symbol->details.pointee.dt;
                    break;
                default:
                    // do nothing
                    break;
                }
                shadow->details.variable.offset = argIdx + 2; // base pointer offset after subprogram call
            }
            else {
                err("Previously declared parameter name: %s", id);
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

static char *parseImpliedDo(char *s, Token *doVarId, ImpliedDoList *doList) {
    Token *expression;
    char *name;
    int rank;
    char *start;
    Symbol *sym;
    Token token;
    BaseType type;

    name = doVarId->details.identifier.name;
    sym = findSymbol(name);
    if (sym == NULL) {
        sym = addSymbol(name, SymClass_Undefined);
    }
    switch (sym->class) {
    case SymClass_Undefined:
        defineLocalVariable(sym);
        /* fall through */
    case SymClass_Auto:
    case SymClass_Static:
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
    if (type != BaseType_Integer || rank > 0) {
        err("Invalid implied DO loop variable: %s", name);
        return NULL;
    }
    doList->loopVariable = sym;

    s = parseExpression(s, &expression);
    if (expression == NULL) {
        err("Invalid expression in implied DO");
        return NULL;
    }
    doList->initExpression = expression;
    if (*s != ',') {
        err("Invalid implied DO syntax");
        return NULL;
    }
    s = parseExpression(s + 1, &expression);
    if (expression == NULL) {
        err("Invalid expression in implied DO");
        return NULL;
    }
    doList->limitExpression = expression;
    if (*s == ',') {
        s = parseExpression(s + 1, &expression);
        if (expression == NULL) {
            err("Invalid expression in implied DO");
            return NULL;
        }
        doList->incrExpression = expression;
    }
    if (*s == ')') {
        return s + 1;
    }
    else {
        err("Invalid implied DO syntax");
        return NULL;
    }
}

static char *parseInquireInfoList(char *s, InquireInfoList **iiList) {
    DataType *dt;
    int ec;
    Token *expression;
    InquireInfoList *list;
    Symbol *labelSym;
    int len;
    char lineLabel[16];
    char keyword[16];
    int n;
    StorageReference reference;
    char *start;
    Token token;

    list = (InquireInfoList *)allocate(sizeof(InquireInfoList));
    *iiList = list;
    s = eatWsp(s);
    if (*s != '(') {
        err("Inquiry info list syntax");
        return NULL;
    }
    ec = errorCount;
    expression = NULL;
    s = eatWsp(s + 1);
    start = s;
    n = 0;
    for (;;) {
        s = getNextToken(s, &token, FALSE);
        if (token.type == TokenType_Identifier && *s == '=') {
            s = eatWsp(s + 1);
            len = strlen(token.details.identifier.name);
            if (len >= sizeof(keyword)) len = sizeof(keyword) - 1;
            memcpy(keyword, token.details.identifier.name, len);
            keyword[len] = '\0';
            start = s;
            s = parseExpression(s, &expression);
            if (expression == NULL) {
                err("Invalid expression in inquire info list");
                break;
            }
            if (strcasecmp(keyword, "UNIT") == 0) {
                if (list->unit != NULL) {
                    err("UNIT specified more than once");
                    break;
                }
                if (list->fileName != NULL) {
                    err("Both UNIT and FILE specified");
                    break;
                }
                list->unit = expression;
            }
            else if (strcasecmp(keyword, "FILE") == 0) {
                if (list->fileName != NULL) {
                    err("FILE specified more than once");
                    break;
                }
                if (list->unit != NULL) {
                    err("Both UNIT and FILE specified");
                    break;
                }
                list->fileName = expression;
            }
            else if (strcasecmp(keyword, "ERR") == 0) {
                if (list->errLabel != NULL) {
                    err("ERR specified more than once");
                    break;
                }
                else if (expression->type == TokenType_Constant && expression->details.constant.dt.type == BaseType_Integer) {
                    sprintf(lineLabel, "%ld", expression->details.constant.value.integer);
                    labelSym = findLabel(lineLabel);
                    if (labelSym != NULL) {
                        if (labelSym->details.label.class != StmtClass_Executable
                            && (labelSym->details.label.class != StmtClass_None || labelSym->details.label.forwardRef == FALSE)) {
                            err("ERR= label does not reference executable statement");
                            break;
                        }
                    }
                    else {
                        labelSym = addLabel(lineLabel);
                        labelSym->details.label.class = StmtClass_None;
                        labelSym->details.label.forwardRef = TRUE;
                    }
                    list->errLabel = labelSym;
                    freeToken(expression);
                }
                else {
                    err("Invalid statement label in ERR=");
                    break;
                }
            }
            else if (strcasecmp(keyword, "IOSTAT") == 0) {
                if (list->iostat.symbol != NULL) {
                    err("IOSTAT specified more than once");
                    break;
                }
                s = getStorageReference(start, "IOSTAT", BaseType_Integer, &reference);
                if (s == NULL) break;
                list->iostat = reference;
            }
            else if (strcasecmp(keyword, "EXIST") == 0) {
                if (list->existRef.symbol != NULL) {
                    err("EXIST specified more than once");
                    break;
                }
                s = getStorageReference(start, "EXIST", BaseType_Logical, &reference);
                if (s == NULL) break;
                list->existRef = reference;
            }
            else if (strcasecmp(keyword, "OPENED") == 0) {
                if (list->openedRef.symbol != NULL) {
                    err("OPENED specified more than once");
                    break;
                }
                s = getStorageReference(start, "OPENED", BaseType_Logical, &reference);
                if (s == NULL) break;
                list->openedRef = reference;
            }
            else if (strcasecmp(keyword, "NAMED") == 0) {
                if (list->namedRef.symbol != NULL) {
                    err("NAMED specified more than once");
                    break;
                }
                s = getStorageReference(start, "NAMED", BaseType_Logical, &reference);
                if (s == NULL) break;
                list->namedRef = reference;
            }
            else if (strcasecmp(keyword, "NUMBER") == 0) {
                if (list->numberRef.symbol != NULL) {
                    err("NUMBER specified more than once");
                    break;
                }
                s = getStorageReference(start, "NUMBER", BaseType_Integer, &reference);
                if (s == NULL) break;
                list->numberRef = reference;
            }
            else if (strcasecmp(keyword, "RECL") == 0) {
                if (list->reclRef.symbol != NULL) {
                    err("RECL specified more than once");
                    break;
                }
                s = getStorageReference(start, "RECL", BaseType_Integer, &reference);
                if (s == NULL) break;
                list->reclRef = reference;
            }
            else if (strcasecmp(keyword, "NEXTREC") == 0) {
                if (list->nextRecRef.symbol != NULL) {
                    err("NEXTREC specified more than once");
                    break;
                }
                s = getStorageReference(start, "NEXTREC", BaseType_Integer, &reference);
                if (s == NULL) break;
                list->nextRecRef = reference;
            }
            else if (strcasecmp(keyword, "NAME") == 0) {
                if (list->nameRef.symbol != NULL) {
                    err("NAME specified more than once");
                    break;
                }
                s = getStorageReference(start, "NAME", BaseType_Character, &reference);
                if (s == NULL) break;
                list->nameRef = reference;
            }
            else if (strcasecmp(keyword, "ACCESS") == 0) {
                if (list->accessRef.symbol != NULL) {
                    err("ACCESS specified more than once");
                    break;
                }
                s = getStorageReference(start, "ACCESS", BaseType_Character, &reference);
                if (s == NULL) break;
                list->accessRef = reference;
            }
            else if (strcasecmp(keyword, "SEQUENTIAL") == 0) {
                if (list->sequentialRef.symbol != NULL) {
                    err("SEQUENTIAL specified more than once");
                    break;
                }
                s = getStorageReference(start, "SEQUENTIAL", BaseType_Character, &reference);
                if (s == NULL) break;
                list->sequentialRef = reference;
            }
            else if (strcasecmp(keyword, "DIRECT") == 0) {
                if (list->directRef.symbol != NULL) {
                    err("DIRECT specified more than once");
                    break;
                }
                s = getStorageReference(start, "DIRECT", BaseType_Character, &reference);
                if (s == NULL) break;
                list->directRef = reference;
            }
            else if (strcasecmp(keyword, "FORM") == 0) {
                if (list->formRef.symbol != NULL) {
                    err("FORM specified more than once");
                    break;
                }
                s = getStorageReference(start, "FORM", BaseType_Character, &reference);
                if (s == NULL) break;
                list->formRef = reference;
            }
            else if (strcasecmp(keyword, "FORMATTED") == 0) {
                if (list->formattedRef.symbol != NULL) {
                    err("FORMATTED specified more than once");
                    break;
                }
                s = getStorageReference(start, "FORMATTED", BaseType_Character, &reference);
                if (s == NULL) break;
                list->formattedRef = reference;
            }
            else if (strcasecmp(keyword, "UNFORMATTED") == 0) {
                if (list->unformattedRef.symbol != NULL) {
                    err("UNFORMATTED specified more than once");
                    break;
                }
                s = getStorageReference(start, "UNFORMATTED", BaseType_Character, &reference);
                if (s == NULL) break;
                list->unformattedRef = reference;
            }
            else if (strcasecmp(keyword, "BLANK") == 0) {
                if (list->blankRef.symbol != NULL) {
                    err("BLANK specified more than once");
                    break;
                }
                s = getStorageReference(start, "BLANK", BaseType_Character, &reference);
                if (s == NULL) break;
                list->blankRef = reference;
            }
            else {
                err("Invalid keyword: %s", token.details.identifier.name);
                break;
            }
            n += 1;
        }
        else if (n == 0) {
            s = parseExpression(start, &expression);
            if (expression == NULL) {
                err("Invalid expression in inquire list");
                break;
            }
            list->unit = expression;
            n += 1;
        }
        else {
            err("Inquiry list syntax");
            break;
        }
        expression = NULL;
        s = eatWsp(s);
        if (*s == ',') {
            s += 1;
        }
        else if (*s == ')') {
            break;
        }
        else {
            err("Inquiry list syntax");
            break;
        }
    }
    if (ec != errorCount) {
        if (expression != NULL) freeToken(expression);
        freeInquireInfoList(list);
        return NULL;
    }
    else {
        return s + 1;
    }
}

static char *parseIoList(char *s, bool isWithinDo, IoListItem **ioList) {
    IoListItem *currentItem;
    ImpliedDoList *doList;
    Token *expression;
    IoListItem *firstItem;
    IoListItem *lastItem;
    char *start;
    Token token;

    *ioList   = NULL;
    firstItem = NULL;
    lastItem  = NULL;
    s = eatWsp(s);
    if (*s == '\0') return s;

    for (;;) {
        if (*s == '(') {
            start = s;
            s = parseIoList(s + 1, TRUE, &currentItem);
            if (s == NULL) {
                s = parseExpression(start, &expression);
                if (s == NULL) {
                    err("Expression syntax");
                    freeIoList(firstItem);
                    return NULL;
                }
                currentItem = (IoListItem *)allocate(sizeof(IoListItem));
                currentItem->class = IoListClass_Expression;
                currentItem->details.expression = expression;
            }
        }
        else {
            if (isWithinDo) {
                start = s;
                s = getNextToken(s, &token, FALSE);
                if (token.type == TokenType_Identifier && *s == '=') { // start of implied DO
                    doList = (ImpliedDoList *)allocate(sizeof(ImpliedDoList));
                    s = parseImpliedDo(s + 1, &token, doList);
                    doList->ioList = firstItem;
                    if (s == NULL) {
                        freeImpliedDoList(doList);
                        return NULL;
                    }
                    currentItem = (IoListItem *)allocate(sizeof(IoListItem));
                    currentItem->class = IoListClass_DoList;
                    currentItem->details.doList = doList;
                    *ioList = currentItem;
                    return s;
                }
                s = start;
            }
            s = parseExpression(s, &expression);
            if (expression == NULL) {
                err("Expression syntax");
                freeIoList(firstItem);
                return NULL;
            }
            currentItem = (IoListItem *)allocate(sizeof(IoListItem));
            currentItem->class = IoListClass_Expression;
            currentItem->details.expression = expression;
        }
        if (firstItem == NULL) {
            firstItem = currentItem;
        }
        else {
            lastItem->next = currentItem;
        }
        lastItem = currentItem;
        s = eatWsp(s);
        if (*s == '\0') {
            break;
        }
        else if (*s == ',') {
            s = eatWsp(s + 1);
        }
        else {
            if (isWithinDo == FALSE) err("Syntax");
            freeIoList(firstItem);
            return NULL;
        }
    }

    *ioList = firstItem;

    return s;
}

static void parseLogicalIF(char *s, Register reg, bool isFromLogIf) {
    IfStackEntry *entry;
    bool hasError;
    bool isAsgn;
    bool isDefn;
    char label[8];
    Token token;

    isAsgn = isAssignment(s, &isDefn, &hasError);
    if (hasError) return;
    s = getNextToken(s, &token, isAsgn == FALSE);
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
                parseCLOSE(s);
                break;
            case CONTINUE:
                break;
            case ENDFILE:
                break;
            case GOTO:
                parseGOTO(s);
                break;
            case IF:
                parseIF(s, TRUE);
                break;
            case INQUIRE:
                parseINQUIRE(s);
                break;
            case OPEN:
                parseOPEN(s);
                break;
            case PAUSE:
                parsePAUSE(s);
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
                parseRETURN(s);
                break;
            case REWIND:
                break;
            case SAVE:
                break;
            case STOP:
                parseSTOP(s);
                break;
            case WRITE:
                parseWRITE(s);
                break;
            default:
                break;
            }
        }
        else if (isAsgn) {
            parseAssignment(s, &token);
        }
        else {
            err("Invalid IF syntax");
        }
        emitLabel(label);
    }
}

static char *parseOpenInfoList(char *s, OpenInfoList **oiList) {
    DataType *dt;
    int ec;
    Token *expression;
    OpenInfoList *list;
    Symbol *labelSym;
    int len;
    char lineLabel[16];
    char keyword[16];
    int n;
    StorageReference reference;
    char *start;
    Token token;

    list = (OpenInfoList *)allocate(sizeof(OpenInfoList));
    *oiList = list;
    s = eatWsp(s);
    if (*s != '(') {
        err("Open info list syntax");
        return NULL;
    }
    ec = errorCount;
    expression = NULL;
    s = eatWsp(s + 1);
    start = s;
    n = 0;
    for (;;) {
        s = getNextToken(s, &token, FALSE);
        if (token.type == TokenType_Identifier && *s == '=') {
            s = eatWsp(s + 1);
            len = strlen(token.details.identifier.name);
            if (len >= sizeof(keyword)) len = sizeof(keyword) - 1;
            memcpy(keyword, token.details.identifier.name, len);
            keyword[len] = '\0';
            start = s;
            s = parseExpression(s, &expression);
            if (expression == NULL) {
                err("Invalid expression in open info list");
                break;
            }
            if (strcasecmp(keyword, "UNIT") == 0) {
                if (list->unit != NULL) {
                    err("UNIT specified more than once");
                    break;
                }
                list->unit = expression;
            }
            else if (strcasecmp(keyword, "FILE") == 0) {
                if (list->fileName != NULL) {
                    err("FILE specified more than once");
                    break;
                }
                list->fileName = expression;
            }
            else if (strcasecmp(keyword, "STATUS") == 0) {
                if (list->fileStatus != NULL) {
                    err("STATUS specified more than once");
                    break;
                }
                list->fileStatus = expression;
            }
            else if (strcasecmp(keyword, "ERR") == 0) {
                if (list->errLabel != NULL) {
                    err("ERR specified more than once");
                    break;
                }
                else if (expression->type == TokenType_Constant && expression->details.constant.dt.type == BaseType_Integer) {
                    sprintf(lineLabel, "%ld", expression->details.constant.value.integer);
                    labelSym = findLabel(lineLabel);
                    if (labelSym != NULL) {
                        if (labelSym->details.label.class != StmtClass_Executable
                            && (labelSym->details.label.class != StmtClass_None || labelSym->details.label.forwardRef == FALSE)) {
                            err("ERR= label does not reference executable statement");
                            break;
                        }
                    }
                    else {
                        labelSym = addLabel(lineLabel);
                        labelSym->details.label.class = StmtClass_None;
                        labelSym->details.label.forwardRef = TRUE;
                    }
                    list->errLabel = labelSym;
                    freeToken(expression);
                }
                else {
                    err("Invalid statement label in ERR=");
                    break;
                }
            }
            else if (strcasecmp(keyword, "IOSTAT") == 0) {
                if (list->iostat.symbol != NULL) {
                    err("IOSTAT specified more than once");
                    break;
                }
                s = getStorageReference(start, "IOSTAT", BaseType_Integer, &reference);
                if (s == NULL) break;
                list->iostat = reference;
            }
            else if (strcasecmp(keyword, "FORM") == 0) {
                if (list->formatting != NULL) {
                    err("FORM specified more than once");
                    break;
                }
                list->formatting = expression;
            }
            else if (strcasecmp(keyword, "ACCESS") == 0) {
                if (list->access != NULL) {
                    err("ACCESS specified more than once");
                    break;
                }
                list->access = expression;
            }
            else if (strcasecmp(keyword, "BLANK") == 0) {
                if (list->blankSpecifier != NULL) {
                    err("BLANK specified more than once");
                    break;
                }
                list->blankSpecifier = expression;
            }
            else if (strcasecmp(keyword, "RECL") == 0) {
                if (list->recordLength != NULL) {
                    err("RECL specified more than once");
                    break;
                }
                list->recordLength = expression;
            }
            else {
                err("Invalid keyword: %s", token.details.identifier.name);
                break;
            }
            n += 1;
        }
        else if (n == 0) {
            s = parseExpression(start, &expression);
            if (expression == NULL) {
                err("Invalid expression in open list");
                break;
            }
            list->unit = expression;
            n += 1;
        }
        else {
            err("Open list syntax");
            break;
        }
        expression = NULL;
        s = eatWsp(s);
        if (*s == ',') {
            s += 1;
        }
        else if (*s == ')') {
            break;
        }
        else {
            err("Open list syntax");
            break;
        }
    }
    if (ec != errorCount) {
        if (expression != NULL) freeToken(expression);
        freeOpenInfoList(list);
        return NULL;
    }
    else {
        return s + 1;
    }
}

static void parseStmtFunction(char *s, Token *id) {
    DataType *dt;
    Token *expression;
    char *name;
    char qualifier[MAX_ID_LENGTH + 1];
    Symbol *parentUnitSym;
    OperatorArgument result;
    Symbol *symbol;

    name = id->details.identifier.name;
    symbol = findSymbol(name);
    if (symbol == NULL) {
        symbol = addSymbol(name, SymClass_Undefined);
    }
    if (symbol->class == SymClass_Undefined) {
        symbol->class = SymClass_StmtFunction;
        symbol->details.progUnit.parentUnit = progUnitSym;
    }
    else {
        err("Function name not unique");
        return;
    }
    if (symbol->details.progUnit.dt.type == BaseType_Undefined) {
        symbol->details.progUnit.dt = implicitTypes[toupper(name[0]) - 'A'];
        symbol->details.progUnit.offset = -1;
    }
    strcpy(qualifier, getProgUnitQualifier());
    dt = &symbol->details.progUnit.dt;
    s = parseFormalArguments(s, TRUE);
    s = eatWsp(s);
    if (*s != '=') {
        err("Syntax");
        removeAllShadows();
        return;
    }
    s = parseExpression(s + 1, &expression);
    if (expression == NULL) {
        removeAllShadows();
        err("Expression syntax");
        return;
    }
    emitActivateQualifier(qualifier);
    emitActivateSection("@STMTFN", "CODE");
    emitProlog(symbol);
    parentUnitSym = progUnitSym;
    progUnitSym = symbol;
    if (evaluateExpression(expression, &result)) {
        progUnitSym = parentUnitSym;
        freeToken(expression);
        removeAllShadows();
        emitDeactivateSection("@STMTFN");
        emitDeactivateQualifier(qualifier);
        return;
    }
    progUnitSym = parentUnitSym;
    if (coerceArgument(&result, getDataType(&result)->type, dt->type) == BaseType_Undefined) {
        err("Invalid type conversion");
        removeAllShadows();
        emitDeactivateSection("@STMTFN");
        emitDeactivateQualifier(qualifier);
        return;
    }
    loadValue(&result);
    emitStoreReg(symbol, result.reg);
    freeRegister(result.reg);
    emitEpilog(symbol, 0, 0);
    removeAllShadows();
    emitDeactivateSection("@STMTFN");
    emitDeactivateQualifier(qualifier);
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
        defineLocalVariable(symbol);
        /* fall through */
    case SymClass_Auto:
    case SymClass_Static:
    case SymClass_Global:
    case SymClass_Argument:
        dt = &symbol->details.variable.dt;
        break;
    case SymClass_Adjustable:
        dt = &symbol->details.adjustable.dt;
        break;
    case SymClass_Pointee:
        dt = &symbol->details.pointee.dt;
        break;
    case SymClass_Function:
        if (symbol->details.progUnit.dt.type == BaseType_Undefined) {
            symbol->details.progUnit.dt = implicitTypes[toupper(name[0]) - 'A'];
            autoOffset -= calculateSize(symbol);
            symbol->details.progUnit.offset = autoOffset;
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
            if (s == NULL) {
                err("Invalid array index");
                return NULL;
            }
            if (expressionList == NULL) {
                err("Invalid array index");
                return NULL;
            }
            s = eatWsp(s);
            if (*s == '(') {
                if (isChrAsgn) {
                    s = parseStringRange(s, &strRange);
                    if (s == NULL) {
                        err("Invalid character range");
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
            if (s == NULL) {
                err("Invalid character range");
                return NULL;
            }
        }
        else {
            err("Undefined array %s", name);
            return NULL;
        }
    }
    reference->symbol = symbol;
    reference->expressionList = expressionList;
    reference->strRange = strRange;

    return s;
}

static char *parseStringRange(char *s, StringRange **range) {
    Token *expression;
    Token *one;
    StringRange *sr;

    *range = NULL;
    sr = (StringRange *)allocate(sizeof(StringRange));
    s = eatWsp(s + 1);
    if (*s == ':') {
        one = (Token *)allocate(sizeof(Token));
        one->type = TokenType_Constant;
        one->details.constant.dt.type = BaseType_Integer;
        one->details.constant.value.integer = 1;
        sr->first = one;
    }
    else if (*s != ')') {
        s = parseExpression(s, &expression);
        if (s == NULL) {
            freeStringRange(sr);
            return NULL;
        }
        sr->first = copyToken(expression);
        if (*s == ')') {
            sr->last = copyToken(expression);
        }
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
                if (dt->type == BaseType_Character && dt->constraint == -1) {
                    err("Invalid assumed-length CHARACTER declaration");
                    break;
                }
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
                    s = parseCharConstraint(s + 1, &token, &symbol->details.variable.dt);
                    if (symbol->details.variable.dt.constraint == -1 && symbol->class != SymClass_Argument
                        && symbol->class != SymClass_Function) {
                        err("Invalid assumed-length CHARACTER declaration");
                        break;
                    }
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
                if (symbol->class == SymClass_Undefined) {
                    defineLocalVariable(symbol);
                }
                else {
                    defineType(symbol);
                }
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
        defineLocalVariable(sym);
        /* fall through */
    case SymClass_Auto:
    case SymClass_Static:
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
    char *name;
    Symbol *symbol;
    Token token;

    s = getNextToken(s, &token, FALSE);
    if (token.type == TokenType_Identifier) {
        name = token.details.identifier.name;
    }
    else if (token.type == TokenType_None) {
        name = "BLKDAT";
    }
    else {
        err("Incorrect Block Data name");
        return;
    }
    symbol = addSymbol(name, SymClass_BlockData);
    if (symbol == NULL) {
        err("Block Data name not unique");
    }
    s = eatWsp(s);
    if (*s != '\0') {
        err("Incorrect BLOCK DATA statement");
        return;
    }
    progUnitSym = symbol;
    emitProlog(progUnitSym);
}

static void parseCALL(char *s) {
    int frameSize;
    char name[MAX_ID_LENGTH + 1];
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
    frameSize = 0;
    s = eatWsp(s);
    if (*s == '(') {
        s = parseActualArguments(s, &frameSize);
        s = eatWsp(s);
    }
    if (*s != '\0') {
        err("Invalid CALL statement");
        return;
    }
    emitSubprogramCall(name, NULL);
    emitAdjustSP(frameSize);
}

static void parseCLOSE(char *s) {
    OperatorArgument arg;
    DataType *dt;
    int ec;
    bool isScalar;
    char label[8];
    CloseInfoList *ciList;
    Register reg;
    OperatorArgument target;

    s = eatWsp(s);
    if (*s != '(') {
        err("Syntax");
        return;
    }
    s = parseCloseInfoList(s, &ciList);
    if (s == NULL) return;
    emitAdjustSP(-2);
    ec = errorCount;
    for (;;) {
        if (ciList->unit == NULL) {
            err("UNIT missing");
            break;
        }
        if (evaluateExpression(ciList->unit, &arg)) break;
        loadValue(&arg);
        dt = getDataType(&arg);
        if (dt->type != BaseType_Integer) {
            err("UNIT not integer");
            freeRegister(arg.reg);
            break;
        }
        emitStoreStack(arg.reg, 0);
        freeRegister(arg.reg);

        if (ciList->fileStatus == NULL) {
            emitLoadNullPtr(&arg);
        }
        else {
            if (evaluateExpression(ciList->fileStatus, &arg)) break;
            loadValue(&arg);
            dt = getDataType(&arg);
            if (dt->type != BaseType_Character) {
                err("STATUS not character");
                freeRegister(arg.reg);
                break;
            }
        }
        emitStoreStack(arg.reg, 1);
        freeRegister(arg.reg);

        break;
    }
    emitPrimCall("@_closeu");
    emitPrimCall("@_iostat");
    emitAdjustSP(2);
    reg = RESULT_REG;
    if (ciList->iostat.symbol != NULL) {
        reg = allocateRegister();
        emitCopyRegister(reg, RESULT_REG);
        if (evaluateStorageReference(&ciList->iostat, &target, NULL, &isScalar)) {
            freeRegister(reg);
            return;
        }
        if (isScalar) {
            emitStoreReg(ciList->iostat.symbol, reg);
        }
        else {
            emitStoreRegByReference(&target, reg);
            freeRegister(target.reg);
        }
        freeRegister(reg);
    }
    generateLabel(label);
    emitBranchOnFalse(reg, label);
    emitBranch(ciList->errLabel != NULL ? ciList->errLabel->details.label.label : "@_fioerr");
    emitLabel(label);
    freeCloseInfoList(ciList);
    verifyEOS(s);
}

static void parseCOMMON(char *s) {
    Symbol *commonBlock;
    char *name;
    int size;
    Symbol *symbol;
    Token token;

    s = getNextChar(s);
    if (*s == '/') {
        s = getIdentifier(s + 1, &token);
        if (token.type != TokenType_Identifier) {
            err("Invalid common block name");
            return;
        }
        else if (*s != '/') {
            err("Missing '/' after common block name");
            return;
        }
        s += 1;
        name = token.details.identifier.name;
    }
    else {
        name = "";
    }
    commonBlock = findCommonBlock(name);
    if (commonBlock == NULL) {
        commonBlock = addCommonBlock(name);
        generateLabel(commonBlock->details.common.label);
    }
    for (;;) {
        s = getNextToken(s, &token, FALSE);
        if (token.type == TokenType_Identifier) {
            name = token.details.identifier.name;
            symbol = findSymbol(name);
            if (symbol == NULL) {
                symbol = addSymbol(name, SymClass_Undefined);
            }
            switch (symbol->class) {
            case SymClass_Undefined:
            case SymClass_Auto:
            case SymClass_Static:
                symbol->class = SymClass_Global;
                symbol->details.variable.staticBlock = commonBlock;
                defineType(symbol);
                symbol->details.variable.offset = commonBlock->details.common.offset;
                s = eatWsp(s);
                if (*s == '(') {
                    if (symbol->details.variable.dt.rank != 0) {
                        err("Duplicate declaration of %s", name);
                    }
                    s = parseDimDecl(s + 1, symbol);
                    s = eatWsp(s);
                }
                size = calculateSize(symbol);
                commonBlock->details.common.offset += size;
                if (commonBlock->details.common.offset > commonBlock->details.common.limit) {
                    commonBlock->details.common.limit = commonBlock->details.common.offset;
                }
                break;
            default:
                err("Duplicate declaration of %s", name);
                return;
            }
            if (*s == '\0') {
                break;
            }
            else if (*s == ',') {
                s = eatWsp(s + 1);
            }
            else {
                err("Invalid COMMON variable declaration");
                return;
            }
        }
        else {
            err("Invalid COMMON variable declaration");
            return;
        }
    }
}

static void parseDATA(char *s) {
    ConstantListItem *cListItem;
    ConstantListItem *currentCList;
    DataInitializerItem *currentDList;
    DataInitializerItem *dListItem;
    DataType *dt;
    int ec;
    int rank;
    StorageReference ref;
    int repeatCount;
    OperatorArgument result;
    char *start;
    Symbol *symbol;
    Token token;
    int totalConstantCount;
    int totalElementCount;

    ec = errorCount;
    for (;;) {
        /*
         *  Parse the next list of variable references
         */
        currentDList = NULL;
        currentCList = NULL;
        totalElementCount = 0;
        for (;;) {
            s = getNextToken(s, &token, FALSE);
            if (token.type != TokenType_Identifier) {
                err("Syntax");
                break;
            }
            s = parseStorageReference(s, &token, &ref);
            if (s == NULL) {
                freeStaticInitializers();
                return;
            }
            dListItem = (DataInitializerItem *)allocate(sizeof(DataInitializerItem));
            if (firstDListItem == NULL) {
                firstDListItem = dListItem;
            }
            else {
                lastDListItem->next = dListItem;
            }
            lastDListItem = dListItem;
            if (currentDList == NULL) currentDList = dListItem;
            symbol = ref.symbol;
            if (symbol->class == SymClass_Static) {
                dListItem->blockName = "DATA";
                dListItem->blockType = "DATA";
                memcpy(dListItem->blockLabel, symbol->details.variable.staticBlock->details.progUnit.staticDataLabel, 8);
            }
            else if (symbol->class == SymClass_Global) {
                dListItem->blockName = symbol->details.variable.staticBlock->identifier;
                dListItem->blockType = "COMMON";
                memcpy(dListItem->blockLabel, symbol->details.variable.staticBlock->details.common.label, 8);
            }
            else {
                err("%s is not static or common", symbol->identifier);
                freeStorageReference(&ref);
                break;
            }
            dt = getSymbolType(symbol);
            dListItem->symbol = symbol;
            dListItem->type = dt->type;
            dListItem->blockOffset = symbol->details.variable.offset;
            rank = symbol->details.variable.dt.rank;
            if (rank > 0) {
                if (ref.expressionList != NULL) { // array element reference
                    if (evaluateArrayRef(symbol, ref.expressionList, &result)) {
                        freeStorageReference(&ref);
                        break;
                    }
                    if (!isConstant(result)) {
                        if (isCalculation(result)) freeAddrReg(result.reg);
                        err("Non-constant array subscript");
                        break;
                    }
                    else if (result.details.constant.dt.type != BaseType_Integer) {
                        err("Non-integer array subscript");
                        break;
                    }
                    dListItem->elementOffset = result.details.constant.value.integer;
                    dListItem->elementCount = 1;
                }
                else { // whole array reference
                    dListItem->elementOffset = 0;
                    dListItem->elementCount = countArrayElements(symbol);
                }
            }
            else {
                dListItem->elementOffset = 0;
                dListItem->elementCount = 1;
            }
            if (dt->type == BaseType_Character) {
                dListItem->constraint = dt->constraint;
                if (ref.strRange != NULL) {
                    if (evaluateExpression(ref.strRange->first, &result)) {
                        freeStorageReference(&ref);
                        break;
                    }
                    if (isConstant(result) && result.details.constant.dt.type == BaseType_Integer && result.details.constant.value.integer > 0) {
                        dListItem->charOffset = result.details.constant.value.integer - 1;
                    }
                    else {
                        err("Invalid character index");
                        freeStorageReference(&ref);
                        break;
                    }
                    if (ref.strRange->last != NULL) {
                        if (evaluateExpression(ref.strRange->last, &result)) {
                            freeStorageReference(&ref);
                            break;
                        }
                        if (isConstant(result) && result.details.constant.dt.type == BaseType_Integer && result.details.constant.value.integer > 0) {
                            dListItem->charLength = result.details.constant.value.integer - dListItem->charOffset;
                        }
                        else {
                            err("Invalid character index");
                            freeStorageReference(&ref);
                            break;
                        }
                    }
                    else {
                        dListItem->charLength = dt->constraint - dListItem->charOffset;
                    }
                }
                else {
                    dListItem->charOffset = 0;
                    dListItem->charLength = dt->constraint;
                }
            }
            freeStorageReference(&ref);
            totalElementCount += dListItem->elementCount;
            s = eatWsp(s);
            if (*s != ',') break;
            s += 1;
        }
        if (*s != '/') {
            err("Syntax");
        }
        if (errorCount > ec) break;
        /*
         *  Parse the next list of constants
         */
        totalConstantCount = 0;
        for (;;) {
            start = s + 1;
            s = getNextToken(start, &token, FALSE);
            repeatCount = 1;
            if (*s == '*') { // repeat count specified
                start = s + 1;
                if (evaluateExpression(&token, &result)) break;
                if (isConstant(result) && result.details.constant.dt.type == BaseType_Integer && result.details.constant.value.integer > 0) {
                    repeatCount = result.details.constant.value.integer;
                }
                else {
                    err("Invalid repeat count");
                    break;
                }
            }
            s = getNextToken(start, &token, FALSE);
            if (evaluateExpression(&token, &result)) break;
            if (!isConstant(result)) {
                err("DATA value is not a constant");
                break;
            }
            cListItem = (ConstantListItem *)allocate(sizeof(ConstantListItem));
            cListItem->repeatCount = repeatCount;
            totalConstantCount += repeatCount;
            cListItem->details = result.details.constant;
            if (firstCListItem == NULL) {
                firstCListItem = cListItem;
            }
            else {
                lastCListItem->next = cListItem;
            }
            lastCListItem = cListItem;
            if (currentCList == NULL) currentCList = cListItem;
            s = eatWsp(s);
            if (*s == '/') {
                s += 1;
                break;
            }
            else if (*s != ',') {
                err("Syntax");
                break;
            }
        }
        if (totalElementCount > totalConstantCount) {
            err("Too few data values");
        }
        else if (totalElementCount < totalConstantCount) {
            err("Too many data values");
        }
        else if (validateDataInitializers(currentDList, currentCList) == FALSE) {
            break;
        }
        if (errorCount > ec) break;
        s = eatWsp(s);
        if (*s == '\0') {
            break;
        }
        else if (*s == ',') {
            s += 1;
        }
        else {
            err("Syntax");
            break;
        }
    }
    if (errorCount > ec) freeStaticInitializers();
}

static void parseDIMENSION(char *s) {
    char *id;
    Symbol *symbol;
    Token token;

    for (;;) {
        s = getNextToken(s, &token, FALSE);
        if (token.type != TokenType_Identifier) {
            err("Invalid array declaration");
            return;
        }
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
            if (doStaticLocals) {
                symbol->class = SymClass_Static;
                symbol->details.variable.staticBlock = progUnitSym;
            }
            else {
                symbol->class = SymClass_Auto;
            }
            defineType(symbol);
            /* fall through */
        case SymClass_Auto:
        case SymClass_Static:
        case SymClass_Global:
            if (symbol->details.variable.dt.rank != 0) {
                err("Duplicate declaration of %s", id);
                return;
            }
            break;
        case SymClass_Pointee:
            if (symbol->details.pointee.dt.rank != 0) {
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
            err("Syntax");
            return;
        }
    }
}

static void parseDO(char *s) {
    DoStackEntry *entry;
    Token *expression;
    char *id;
    i64 initValue;
    bool isIncr1;
    bool isIncrNeg1;
    bool isIntConstInit;
    bool isIntConstLimit;
    OperatorArgument limit;
    i64 limitValue;
    char lineLabel[8];
    int rank;
    Register reg;
    OperatorArgument result;
    char *start;
    Symbol *sym;
    Token token;
    BaseType type;

    start = s;
    s = getLabel(s, lineLabel);
    if (s != NULL) {
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
    }
    else {
        s = start;
        sym = NULL;
    }
    if (doStackPtr >= MAX_DO_STACK_SIZE) {
        err("DO nested too deeply");
        return;
    }
    entry = &doStack[doStackPtr];
    memset(entry, 0, sizeof(DoStackEntry));
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
        defineLocalVariable(sym);
        /* fall through */
    case SymClass_Auto:
    case SymClass_Static:
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

    autoOffset -= DO_FRAME_SIZE;
    entry->frameOffset = autoOffset;

    isIntConstInit = isIntegerConstant(result);
    if (isIntConstInit) initValue = result.details.constant.value.integer;
    loadValue(&result);
    emitStoreFrame(result.reg, entry->frameOffset + DO_CURRENT);
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
    if (evaluateExpression(expression, &limit)) {
        freeToken(expression);
        return;
    }
    if (coerceArgument(&limit, getDataType(&limit)->type, type) == BaseType_Undefined) {
        err("Invalid type conversion");
    }
    isIntConstLimit = isIntegerConstant(limit);
    if (isIntConstLimit) {
        limitValue = limit.details.constant.value.integer;
    }
    else {
        loadValue(&limit);
        emitStoreFrame(limit.reg, entry->frameOffset + DO_TRIP_COUNT);
        freeRegister(limit.reg);
    }
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
        setIntegerArg(&result, 1);
    }
    isIncr1 = (isIntegerConstant(result) && result.details.constant.value.integer == 1)
        || (isRealConstant(result) && result.details.constant.value.real == 1.0);
    isIncrNeg1 = (isIntegerConstant(result) && result.details.constant.value.integer == -1)
        || (isRealConstant(result) && result.details.constant.value.real == -1.0);

    if (coerceArgument(&result, getDataType(&result)->type, type) == BaseType_Undefined) {
        err("Invalid type conversion");
    }
    if (((isIncr1 == FALSE && isIncrNeg1 == FALSE) || isIntConstInit == FALSE || isRealConstant(result)) && isIntConstLimit) {
        loadValue(&limit);
        emitStoreFrame(limit.reg, entry->frameOffset + DO_TRIP_COUNT);
        freeRegister(limit.reg);
        isIntConstLimit = FALSE;
    }
    loadValue(&result);
    emitStoreFrame(result.reg, entry->frameOffset + DO_INCREMENT);
    freeRegister(result.reg);
    if (isIncr1) {
        if (isIntConstLimit && isIntConstInit) {
            emitStoreFrameInt((limitValue - initValue) + 1, entry->frameOffset + DO_TRIP_COUNT);
        }
        else {
            emitCalcTrip1(entry, type);
        }
    }
    else if (isIncrNeg1) {
        if (isIntConstLimit && isIntConstInit) {
            emitStoreFrameInt((initValue - limitValue) + 1, entry->frameOffset + DO_TRIP_COUNT);
        }
        else {
            emitCalcTripNeg1(entry, type);
        }
    }
    else {
        emitCalcTrip(entry, type);
    }
    emitLabel(entry->startLabel);
    reg = emitLoadFrame(entry->frameOffset + DO_CURRENT);
    emitStoreReg(entry->loopVariable, reg);
    freeRegister(reg);
    emitBranchIfEndTrips(entry);

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
    DoStackEntry *entry;

    emitEpilog(progUnitSym, -autoOffset, staticOffset);

    if (ifStackPtr > 0) err("Missing ENDIF");
    if (doStackPtr > 0) {
        entry = &doStack[doStackPtr - 1];
        if (entry->termLabelSym != NULL) {
            err("Missing DO termination label %s", entry->termLabelSym->identifier);
        }
        else {
            err("Missing ENDDO");
        }
    }
    reportUnresolvedLabels();

    if (errorCount + warningCount > 0 && listingFile != NULL) fputs("\n\n", listingFile);

    if (errorCount > 0) {
        list(" ***** %d error%s", errorCount, (errorCount > 1) ? "s" : "");
        fprintf(stderr, "%d error%s in %s\n", errorCount, (errorCount > 1) ? "s" : "", progUnitSym->identifier);
    }
    if (warningCount > 0) {
        list(" ***** %d warning%s", warningCount, (warningCount > 1) ? "s" : "");
        fprintf(stderr, "%d warning%s in %s\n", warningCount, (warningCount > 1) ? "s" : "", progUnitSym->identifier);
    }
    listSymbols();
    listSetPageEnd();
    freeAllSymbols();
}

static void parseENDDO(char *s) {
    DoStackEntry *entry;

    if (doStackPtr < 1) {
        err("ENDDO without DO");
        return;
    }
    for (;;) {
        entry = &doStack[--doStackPtr];
        if (entry->termLabelSym == NULL) break;
        err("Missing DO termination label %s", entry->termLabelSym->identifier);
        if (doStackPtr < 1) return;
    }
    emitEndDo(entry);
    verifyEOS(s);
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
    notSupported("ENTRY");
}

static void parseEQUIVALENCE(char *s) {
    int delta;
    TokenListItem *expressionList;
    char *id;
    int lastOffset;
    Symbol *lastSymbol;
    int n;
    int offset;
    Symbol *peer;
    Symbol *symbol;
    Token token;

    for (;;) {
        s = eatWsp(s);
        if (*s != '(') {
            err("Syntax");
            return;
        }
        s += 1;
        n = 0;
        lastSymbol = NULL;
        for (;;) {
            s = getNextToken(s, &token, FALSE);
            if (token.type != TokenType_Identifier) {
                err("Syntax");
                return;
            }
            id = token.details.identifier.name;
            symbol = findSymbol(id);
            if (symbol == NULL) {
                symbol = findIntrinsicFunction(id);
                if (symbol != NULL) {
                    symbol->class = SymClass_Intrinsic;
                }
                else {
                    symbol = addSymbol(id, SymClass_Undefined);
                }
            }
            switch (symbol->class) {
            case SymClass_Undefined:
                defineLocalVariable(symbol);
                break;
            case SymClass_Auto:
            case SymClass_Static:
            case SymClass_Global:
                /* do nothing */
                break;
            default:
                err("Invalid symbol class of %s: %s", id, symClassToStr(symbol->class));
                return;
            }
            offset = 0;
            if (*s == '(') {
                s = parseExpressionList(s, &expressionList);
                if (s == NULL) {
                    err("Invalid array index");
                    return;
                }
                offset = calculateConstOffset(symbol, expressionList);
                freeTokenList(expressionList);
                if (offset == -1) {
                    return;
                }
            }
            n += 1;
            if (lastSymbol != NULL) {
                if (linkVariables(lastSymbol, lastOffset, symbol, offset) == FALSE) {
                    err("Invalid equivalence: %s, %s", lastSymbol->identifier, symbol->identifier);
                }
            }
            lastSymbol = symbol;
            lastOffset = offset;
            s = eatWsp(s);
            if (*s == ')') {
                s += 1;
                break;
            }
            else if (*s == ',') {
                s += 1;
            }
            else {
                err("Syntax");
                return;
            }
        }
        if (n < 2) {
            err("Syntax");
            return;
        }
        s = eatWsp(s);
        if (*s == ',') {
            s += 1;
        }
        else if (*s == '\0') {
            break;
        }
    }
}

static void parseEXTERNAL(char *s) {
    notSupported("EXTERNAL");
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
        err("FORMAT does not end with ')'");
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
        s = parseFormalArguments(s, FALSE);
    }
    s = eatWsp(s);
    if (*s != '\0') {
        err("Function declaration syntax");
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
            defineLocalVariable(sym);
            /* fall through */
        case SymClass_Auto:
        case SymClass_Static:
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

static void parseINQUIRE(char *s) {
    OperatorArgument arg;
    DataType *dt;
    int ec;
    bool isScalar;
    char label[8];
    InquireInfoList *iiList;
    Register reg;
    OperatorArgument target;

    s = eatWsp(s);
    if (*s != '(') {
        err("Syntax");
        return;
    }
    s = parseInquireInfoList(s, &iiList);
    if (s == NULL) return;
    if (iiList->unit == NULL && iiList->fileName == NULL) {
        err("Neither UNIT nor FILE specified");
        return;
    }
    emitAdjustSP(-16);
    ec = errorCount;
    for (;;) {
        if (iiList->unit == NULL) {
            emitLoadNullPtr(&arg);
        }
        else {
            if (evaluateExpression(iiList->unit, &arg)) break;
            loadValue(&arg);
            dt = getDataType(&arg);
            if (dt->type != BaseType_Integer) {
                err("UNIT not integer");
                freeRegister(arg.reg);
                break;
            }
        }
        emitStoreStack(arg.reg, 0);
        freeRegister(arg.reg);

        if (iiList->fileName == NULL) {
            emitLoadNullPtr(&arg);
        }
        else {
            if (evaluateExpression(iiList->fileName, &arg)) break;
            loadValue(&arg);
            dt = getDataType(&arg);
            if (dt->type != BaseType_Character) {
                err("FILE not character");
                freeRegister(arg.reg);
                break;
            }
        }
        emitStoreStack(arg.reg, 1);
        freeRegister(arg.reg);

        if (evaluateInquireReference(&iiList->existRef, 2)) break;
        if (evaluateInquireReference(&iiList->openedRef, 3)) break;
        if (evaluateInquireReference(&iiList->numberRef, 4)) break;
        if (evaluateInquireReference(&iiList->namedRef, 5)) break;
        if (evaluateInquireReference(&iiList->nameRef, 6)) break;
        if (evaluateInquireReference(&iiList->accessRef, 7)) break;
        if (evaluateInquireReference(&iiList->sequentialRef, 8)) break;
        if (evaluateInquireReference(&iiList->directRef, 9)) break;
        if (evaluateInquireReference(&iiList->formattedRef, 10)) break;
        if (evaluateInquireReference(&iiList->unformattedRef, 11)) break;
        if (evaluateInquireReference(&iiList->formRef, 12)) break;
        if (evaluateInquireReference(&iiList->blankRef, 13)) break;
        if (evaluateInquireReference(&iiList->reclRef, 14)) break;
        if (evaluateInquireReference(&iiList->nextRecRef, 15)) break;

        break;
    }
    emitPrimCall("@_queryu"); // returns IOSTAT value
    emitAdjustSP(16);
    reg = RESULT_REG;
    if (iiList->iostat.symbol != NULL) {
        reg = allocateRegister();
        emitCopyRegister(reg, RESULT_REG);
        if (evaluateStorageReference(&iiList->iostat, &target, NULL, &isScalar)) {
            freeRegister(reg);
            return;
        }
        if (isScalar) {
            emitStoreReg(iiList->iostat.symbol, reg);
        }
        else {
            emitStoreRegByReference(&target, reg);
            freeRegister(target.reg);
        }
        freeRegister(reg);
    }
    generateLabel(label);
    emitBranchOnFalse(reg, label);
    emitBranch(iiList->errLabel != NULL ? iiList->errLabel->details.label.label : "@_fioerr");
    emitLabel(label);
    freeInquireInfoList(iiList);
    verifyEOS(s);
}

static void parseINTRINSIC(char *s) {
    notSupported("INTRINSIC");
}

static void parseOutputStmt(char *s, int unitNum) {
    ControlInfoList *ciList;
    IoListItem *ioList;
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
    s = parseIoList(s + 1, FALSE, &ioList);
    if (s != NULL) {
        ciList->unit = createIntegerConstant(unitNum);
        outputInit(ciList);
        processOutputList(ioList, ciList);
        freeIoList(ioList);
        outputFini(ciList);
    }
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
            symbol = addSymbol(name, SymClass_Undefined);
        }
        if (symbol->class == SymClass_Undefined) {
            defineType(symbol);
            symbol->class = SymClass_Parameter;
        }
        else {
            err("Parameter name not unique: %s", name);
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
        if (coerceArgument(&result, getDataType(&result)->type, symbol->details.variable.dt.type) == BaseType_Undefined) {
            err("Invalid type conversion");
            freeToken(expression);
            if (isCalculation(result)) freeRegister(result.reg);
            return;
        }
        if (!isConstant(result)) {
            err("Non-constant expression in declaration of %s", symbol->identifier);
            freeToken(expression);
            if (isCalculation(result)) freeRegister(result.reg);
            return;
        }
        symbol->details.param = result.details.constant;
        if (result.details.constant.dt.type == BaseType_Character) {
            transferCharValue(&symbol->details.param.value, &result.details.constant.value);
        }
        freeToken(expression);
        s = eatWsp(s);
        if (*s == ')') {
            s += 1;
            break;
        }
        else if (*s != ',') {
            err("PARAMETER statement syntax");
            return;
        }
    }
    verifyEOS(s);
}

static void parseOPEN(char *s) {
    OperatorArgument arg;
    DataType *dt;
    int ec;
    bool isScalar;
    char label[8];
    OpenInfoList *oiList;
    Register reg;
    OperatorArgument target;

    s = eatWsp(s);
    if (*s != '(') {
        err("Syntax");
        return;
    }
    s = parseOpenInfoList(s, &oiList);
    if (s == NULL) return;
    emitAdjustSP(-7);
    ec = errorCount;
    for (;;) {
        if (oiList->fileName == NULL) {
            emitLoadNullPtr(&arg);
        }
        else {
            if (evaluateExpression(oiList->fileName, &arg)) break;
            loadValue(&arg);
            dt = getDataType(&arg);
            if (dt->type != BaseType_Character) {
                err("FILE not character");
                freeRegister(arg.reg);
                break;
            }
        }
        emitStoreStack(arg.reg, 0);
        freeRegister(arg.reg);

        if (oiList->unit == NULL) {
            err("UNIT missing");
            break;
        }
        if (evaluateExpression(oiList->unit, &arg)) break;
        loadValue(&arg);
        dt = getDataType(&arg);
        if (dt->type != BaseType_Integer) {
            err("UNIT not integer");
            freeRegister(arg.reg);
            break;
        }
        emitStoreStack(arg.reg, 1);
        freeRegister(arg.reg);

        if (oiList->fileStatus == NULL) {
            emitLoadNullPtr(&arg);
        }
        else {
            if (evaluateExpression(oiList->fileStatus, &arg)) break;
            loadValue(&arg);
            dt = getDataType(&arg);
            if (dt->type != BaseType_Character) {
                err("STATUS not character");
                freeRegister(arg.reg);
                break;
            }
        }
        emitStoreStack(arg.reg, 2);
        freeRegister(arg.reg);

        if (oiList->access == NULL) {
            emitLoadNullPtr(&arg);
        }
        else {
            if (evaluateExpression(oiList->access, &arg)) break;
            loadValue(&arg);
            dt = getDataType(&arg);
            if (dt->type != BaseType_Character) {
                err("ACCESS not character");
                freeRegister(arg.reg);
                break;
            }
        }
        emitStoreStack(arg.reg, 3);
        freeRegister(arg.reg);

        if (oiList->formatting == NULL) {
            emitLoadNullPtr(&arg);
        }
        else {
            if (evaluateExpression(oiList->formatting, &arg)) break;
            loadValue(&arg);
            dt = getDataType(&arg);
            if (dt->type != BaseType_Character) {
                err("FORM not character");
                freeRegister(arg.reg);
                break;
            }
        }
        emitStoreStack(arg.reg, 4);
        freeRegister(arg.reg);

        if (oiList->blankSpecifier == NULL) {
            emitLoadNullPtr(&arg);
        }
        else {
            if (evaluateExpression(oiList->blankSpecifier, &arg)) break;
            loadValue(&arg);
            dt = getDataType(&arg);
            if (dt->type != BaseType_Character) {
                err("BLANK not character");
                freeRegister(arg.reg);
                break;
            }
        }
        emitStoreStack(arg.reg, 5);
        freeRegister(arg.reg);

        if (oiList->recordLength == NULL) {
            emitLoadNullPtr(&arg);
        }
        else {
            if (evaluateExpression(oiList->recordLength, &arg)) break;
            loadValue(&arg);
            dt = getDataType(&arg);
            if (dt->type != BaseType_Integer) {
                err("RECL not integer");
                freeRegister(arg.reg);
                break;
            }
        }
        emitStoreStack(arg.reg, 6);
        freeRegister(arg.reg);

        break;
    }
    emitPrimCall("@_openu");
    emitPrimCall("@_iostat");
    emitAdjustSP(7);
    reg = RESULT_REG;
    if (oiList->iostat.symbol != NULL) {
        reg = allocateRegister();
        emitCopyRegister(reg, RESULT_REG);
        if (evaluateStorageReference(&oiList->iostat, &target, NULL, &isScalar)) {
            freeRegister(reg);
            return;
        }
        if (isScalar) {
            emitStoreReg(oiList->iostat.symbol, reg);
        }
        else {
            emitStoreRegByReference(&target, reg);
            freeRegister(target.reg);
        }
        freeRegister(reg);
    }
    generateLabel(label);
    emitBranchOnFalse(reg, label);
    emitBranch(oiList->errLabel != NULL ? oiList->errLabel->details.label.label : "@_fioerr");
    emitLabel(label);
    freeOpenInfoList(oiList);
    verifyEOS(s);
}

static void parsePAUSE(char *s) {
    // do nothing
}

static void parsePOINTER(char *s) {
    Symbol *pteeSym;
    Symbol *ptrSym;
    Token token;

    for (;;) {
        s = eatWsp(s);
        if (*s != '(') break;
        s = getNextToken(s + 1, &token, FALSE);
        if (token.type != TokenType_Identifier) break;
        ptrSym = addSymbol(token.details.identifier.name, SymClass_Undefined);
        if (ptrSym == NULL) {
            err("Pointer name not unique: ", token.details.identifier.name);
            return;
        }
        defineLocalVariable(ptrSym);
        ptrSym->details.variable.dt.type = BaseType_Pointer;
        if (*s != ',') break;
        s = getNextToken(s + 1, &token, FALSE);
        if (token.type != TokenType_Identifier) break;
        pteeSym = findSymbol(token.details.identifier.name);
        if (pteeSym == NULL) {
            pteeSym = addSymbol(token.details.identifier.name, SymClass_Undefined);
            defineType(pteeSym);
        }
        switch (pteeSym->class) {
        case SymClass_Undefined:
        case SymClass_Auto:
        case SymClass_Static:
            pteeSym->class = SymClass_Pointee;
            pteeSym->details.pointee.pointer = ptrSym;
            break;
        default:
            err("Pointee name not unique: ", token.details.identifier.name);
            return;
        }
        if (*s != ')') break;
        s = eatWsp(s + 1);
        if (*s == '\0') {
            return;
        }
        else if (*s != ',') {
            break;
        }
        s += 1;
    }
    err("Syntax");
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
    IoListItem *ioList;

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
        s = eatWsp(s + 1);
        ciList->unit = createIntegerConstant(DEFAULT_INPUT_UNIT);
    }
    s = parseIoList(s, FALSE, &ioList);
    if (s != NULL) {
        inputInit(ciList);
        processInputList(ioList, ciList);
        freeIoList(ioList);
        inputFini(ciList);
    }
    freeControlInfoList(ciList);
}

static void parseRETURN(char *s) {
    if (progUnitSym->class != SymClass_Subroutine && progUnitSym->class != SymClass_Function && progUnitSym->class != SymClass_Program) {
        err("Misplaced statement");
        return;
    }
    s = eatWsp(s);
    if (*s != '\0') {
        notSupported("alternate RETURN");
    }
    emitBranch(progUnitSym->details.progUnit.exitLabel);
}

static void parseSAVE(char *s) {
    char *name;
    Symbol *symbol;
    Token token;

    s = eatWsp(s);
    if (*s == '\0') {
        // all variables are saved
        doStaticLocals = TRUE;
        symbol = getSymbolRoot();
        while (symbol != NULL) {
            if (symbol->class == SymClass_Auto) {
                symbol->class = SymClass_Static;
                symbol->details.variable.offset = staticOffset;
                symbol->details.variable.staticBlock = progUnitSym;
                staticOffset += calculateSize(symbol);
            }
            symbol = symbol->next;
        }
        return;
    }
    for (;;) {
        s = getNextChar(s);
        if (*s == '\0') {
            break;
        }
        else if (*s == '/') {
            s = getIdentifier(s + 1, &token);
            if (token.type != TokenType_Identifier) {
                err("Invalid common block name");
                return;
            }
            else if (*s != '/') {
                err("Missing '/' after common block name");
                return;
            }
            s += 1;
        }
        else {
            s = getNextToken(s, &token, FALSE);
            if (token.type != TokenType_Identifier) {
                err("Syntax");
                return;
            }
            name = token.details.identifier.name;
            symbol = findSymbol(name);
            if (symbol == NULL) {
                symbol = addSymbol(name, SymClass_Undefined);
                defineType(symbol);
            }
            switch (symbol->class) {
            case SymClass_Undefined:
            case SymClass_Auto:
                symbol->class = SymClass_Static;
                symbol->details.variable.offset = staticOffset;
                symbol->details.variable.staticBlock = progUnitSym;
                staticOffset += calculateSize(symbol);
                break;
            case SymClass_Static:
            case SymClass_Global:
                /* do nothing */
                break;
            default:
                err("Invalid identifier in SAVE: %s", name);
                break;
            }
        }
        s = eatWsp(s);
        if (*s == ',') {
            s += 1;
        }
        else if (*s != '\0') {
            err("Syntax");
        }
    }
}

static void parseSTOP(char *s) {
    emitExit(0);
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
        s = parseFormalArguments(s, FALSE);
    }
    s = eatWsp(s);
    if (*s != '\0') {
        err("Subroutine declaration syntax");
    }
}

static void parseWRITE(char *s) {
    ControlInfoList *ciList;
    IoListItem *ioList;

    s = eatWsp(s);
    if (*s != '(') {
        parsePRINT(s);
        return;
    }
    s = parseControlInfoList(s, &ciList, DEFAULT_OUTPUT_UNIT);
    if (s == NULL) return;
    s = parseIoList(s, FALSE, &ioList);
    if (s != NULL) {
        outputInit(ciList);
        processOutputList(ioList, ciList);
        freeIoList(ioList);
        outputFini(ciList);
    }
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

    for (c = 'A'; c <  'I'; c++) implicitTypes[c - 'A'].type = BaseType_Real;
    for (c = 'I'; c <  'O'; c++) implicitTypes[c - 'A'].type = BaseType_Integer;
    for (c = 'O'; c <= 'Z'; c++) implicitTypes[c - 'A'].type = BaseType_Real;
}

static void presetProgUnit(void) {
    doStaticLocals = doStaticLocalsDefault;
    progUnitSym    = NULL;
    state          = STATE_PROG_UNIT;
    doStackPtr     = 0;
    ifStackPtr     = 0;
    errorCount     = 0;
    warningCount   = 0;
    autoOffset     = 0;
    staticOffset   = 0;
    resetCommonBlocks();
    presetImplicit();
}

static void processInputList(IoListItem *ioList, ControlInfoList *ciList) {
    ImpliedDoList *doList;
    DataType *dt;
    DoStackEntry entry;
    Token *expression;
    bool isScalar;
    char *name;
    StorageReference reference;
    OperatorArgument result;
    Symbol *symbol;
    OperatorArgument target;

    while (ioList != NULL) {
        doList = NULL;
        if (ioList->class == IoListClass_DoList) {
            doList = ioList->details.doList;
            if (setupImpliedDoList(doList, &entry)) break;
            processInputList(doList->ioList, ciList);
            emitEndDo(&entry);
        }
        else {
            expression = ioList->details.expression;
            if (expression->type != TokenType_Identifier) {
                err("Invalid expression in input list");
                return;
            }
            name = expression->details.identifier.name;
            symbol = findSymbol(name);
            if (symbol == NULL) {
                symbol = addSymbol(name, SymClass_Undefined);
            }
            switch (symbol->class) {
            case SymClass_Undefined:
                defineLocalVariable(symbol);
                /* fall through */
            case SymClass_Auto:
            case SymClass_Static:
            case SymClass_Global:
            case SymClass_Argument:
            case SymClass_Adjustable:
            case SymClass_Pointee:
                break;
            case SymClass_Function:
                if (symbol->details.progUnit.dt.type == BaseType_Undefined) {
                    symbol->details.progUnit.dt = implicitTypes[toupper(name[0]) - 'A'];
                    autoOffset -= calculateSize(symbol);
                    symbol->details.progUnit.offset = autoOffset;
                }
                break;
            default:
                err("Invalid storage reference to %s", name);
                return;
            }
            reference.symbol = symbol;
            reference.expressionList = expression->details.identifier.qualifiers;
            reference.strRange = expression->details.identifier.range;
            if (evaluateStorageReference(&reference, &target, NULL, &isScalar)) return;
            if (isCalculation(target) == FALSE) emitLoadReference(&target, NULL);
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
                case BaseType_Complex: /* TODO */
                default:
                    err("Invalid data type of list-directed I/O element");
                    return;
                }
            }
            else {
                emitPrimCall("@_rdufmt");
            }
        }
        ioList = ioList->next;
    }
}

static void processOutputList(IoListItem *ioList, ControlInfoList *ciList) {
    ImpliedDoList *doList;
    DataType *dt;
    DoStackEntry entry;
    OperatorArgument result;

    while (ioList != NULL) {
        doList = NULL;
        if (ioList->class == IoListClass_DoList) {
            doList = ioList->details.doList;
            if (setupImpliedDoList(doList, &entry)) break;
            processOutputList(doList->ioList, ciList);
            emitEndDo(&entry);
        }
        else {
            if (evaluateExpression(ioList->details.expression, &result)) return;
            loadValue(&result);
            emitStoreStack(result.reg, 1);
            freeRegister(result.reg);
            if (ciList->format == NULL) {
                dt = getDataType(&result);
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
                case BaseType_Complex: /* TODO */
                default:
                    err("Invalid data type of list-directed I/O element");
                    return;
                }
            }
            else {
                emitPrimCall("@_wrufmt");
                outputCheckIostat(ciList);
            }
        }
        ioList = ioList->next;
    }
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

static char *readLine(void) {
    int c;
    int len;
    char *limit;
    char *lineEnd;
    char *lp;

    lp = lineBuf;
    limit = lp + (sizeof(lineBuf) - 1);
    lineEnd = lp;
    for (;;) {
        c = fgetc(sourceFile);
        if (c == EOF) {
            if (feof(sourceFile) && lp != lineEnd) break;
            return NULL;
        }
#if defined(__cos)
        else if (c == 0x1b) {
            /*
             * Handle COS blank compression indicator
             */
            c = fgetc(sourceFile);
            if (c == EOF) break;
            c -= 036; /* blank count is biased by 36 octal */
            while (c-- > 0 && lp < limit) *lp++ = ' ';
        }
#endif
        else if (c == '\n') {
            break;
        }
        else if (lp < limit) {
            *lp++ = c;
            if (c != ' ') lineEnd = lp;
        }
    }
    *lineEnd = '\0';
    len = lineEnd - lineBuf;
    if (len < 8) { // valid lines are at least 7 characters long
        while (len < 7) lineBuf[len++] = ' ';
        lineBuf[len] = '\0';
    }
    return lineBuf;
}

static void setIntegerArg(OperatorArgument *arg, int value) {
    memset(arg, 0, sizeof(OperatorArgument));
    arg->class = ArgClass_Constant;
    arg->details.constant.dt.type = BaseType_Integer;
    arg->details.constant.dt.rank = 0;
    arg->details.constant.value.integer = value;
}

static bool setupImpliedDoList(ImpliedDoList *doList, DoStackEntry *entry) {
    i64 initValue;
    bool isConstInit;
    bool isConstLimit;
    bool isIncr1;
    bool isIncrNeg1;
    OperatorArgument limit;
    i64 limitValue;
    Register reg;
    OperatorArgument result;

    memset(entry, 0, sizeof(DoStackEntry));
    entry->loopVariable = doList->loopVariable;
    entry->loopVariableType = BaseType_Integer;
    generateLabel(entry->startLabel);
    generateLabel(entry->endLabel);
    /*
     *  Evaluate initial value
     */
    if (evaluateExpression(doList->initExpression, &result)) return TRUE;
    if (getDataType(&result)->type != BaseType_Integer) {
        err("Initial value of implied DO is not integer");
        return TRUE;
    }
    autoOffset -= DO_FRAME_SIZE;
    entry->frameOffset = autoOffset;
    isConstInit = isConstant(result);
    if (isConstInit) initValue = result.details.constant.value.integer;
    loadValue(&result);
    emitStoreFrame(result.reg, entry->frameOffset + DO_CURRENT);
    freeRegister(result.reg);
    /*
     *  Evaluate limit value
     */
    if (evaluateExpression(doList->limitExpression, &limit)) return TRUE;
    if (getDataType(&result)->type != BaseType_Integer) {
        err("Limit value of implied DO is not integer");
        return TRUE;
    }
    isConstLimit = isConstant(limit);
    if (isConstLimit) {
        limitValue = limit.details.constant.value.integer;
    }
    else {
        loadValue(&limit);
        emitStoreFrame(limit.reg, entry->frameOffset + DO_TRIP_COUNT);
        freeRegister(limit.reg);
    }
    /*
     *  Evaluate increment value, if provided
     */
    if (doList->incrExpression != NULL) {
        if (evaluateExpression(doList->incrExpression, &result)) return TRUE;
        if (getDataType(&result)->type != BaseType_Integer) {
            if (isCalculation(limit)) freeRegister(limit.reg);
            err("Increment value of implied DO is not integer");
            return TRUE;
        }
    }
    else {
        setIntegerArg(&result, 1);
    }
    isIncr1    = isConstant(result) && result.details.constant.value.integer == 1;
    isIncrNeg1 = isConstant(result) && result.details.constant.value.integer == -1;
    if (((isIncr1 == FALSE && isIncrNeg1 == FALSE) || isConstInit == FALSE) && isConstLimit) {
        loadValue(&limit);
        emitStoreFrame(limit.reg, entry->frameOffset + DO_TRIP_COUNT);
        freeRegister(limit.reg);
        isConstLimit = FALSE;
    }
    loadValue(&result);
    emitStoreFrame(result.reg, entry->frameOffset + DO_INCREMENT);
    freeRegister(result.reg);
    if (isIncr1) {
        if (isConstLimit && isConstInit) {
            emitStoreFrameInt((limitValue - initValue) + 1, entry->frameOffset + DO_TRIP_COUNT);
        }
        else {
            emitCalcTrip1(entry, BaseType_Integer);
        }
    }
    else if (isIncrNeg1) {
        if (isConstLimit && isConstInit) {
            emitStoreFrameInt((initValue - limitValue) + 1, entry->frameOffset + DO_TRIP_COUNT);
        }
        else {
            emitCalcTripNeg1(entry, BaseType_Integer);
        }
    }
    else {
        emitCalcTrip(entry, BaseType_Integer);
    }
    emitLabel(entry->startLabel);
    reg = emitLoadFrame(entry->frameOffset + DO_CURRENT);
    emitStoreReg(entry->loopVariable, reg);
    freeRegister(reg);
    emitBranchIfEndTrips(entry);

    return FALSE;
}

static void transferCharValue(DataValue *to, DataValue *from) {
    to->character.length = from->character.length;
    to->character.string = from->character.string;
    from->character.length = 0;
    from->character.string = NULL;
}

static bool validateDataInitializers(DataInitializerItem *dList, ConstantListItem *cList) {
    ConstantListItem *cListItem;
    DataInitializerItem *dListItem;
    int elementCount;
    int repeatCount;

    dListItem = dList;
    cListItem = cList;
    repeatCount = cListItem->repeatCount;
    while (dListItem != NULL) {
        for (elementCount = dListItem->elementCount; elementCount > 0; elementCount--) {
            if (cListItem->details.dt.type != dListItem->type) {
                switch (dListItem->type) {
                case BaseType_Character:
                    err("Data value for %s is not CHARACTER", dListItem->symbol->identifier);
                    return FALSE;
                case BaseType_Logical:
                    switch (cListItem->details.dt.type) {
                    case BaseType_Logical:
                        break;
                    case BaseType_Integer:
                        cListItem->details.dt.type = BaseType_Logical;
                        cListItem->details.value.logical = (cListItem->details.value.integer == 0) ? 0 : ~(u64)0;
                        break;
                    case BaseType_Real:
                    case BaseType_Double:
                    case BaseType_Character:
                    case BaseType_Complex:
                    default:
                        err("Data value for %s cannot be coerced to LOGICAL", dListItem->symbol->identifier);
                        return FALSE;
                    }
                    break;
                case BaseType_Integer:
                    switch (cListItem->details.dt.type) {
                    case BaseType_Logical:
                        cListItem->details.dt.type = BaseType_Integer;
                        break;
                    case BaseType_Integer:
                        break;
                    case BaseType_Real:
                    case BaseType_Double:
                        cListItem->details.dt.type = BaseType_Integer;
                        cListItem->details.value.integer = (i64)cListItem->details.value.real;
                        break;
                    case BaseType_Character:
                    case BaseType_Complex:
                    default:
                        err("Data value for %s cannot be coerced to INTEGER", dListItem->symbol->identifier);
                        return FALSE;
                    }
                    break;
                case BaseType_Real:
                case BaseType_Double:
                    switch (cListItem->details.dt.type) {
                    case BaseType_Logical:
                        cListItem->details.dt.type = dListItem->type;
                        cListItem->details.value.real = (double)cListItem->details.value.logical;
                        break;
                    case BaseType_Integer:
                        cListItem->details.dt.type = dListItem->type;
                        cListItem->details.value.real = (double)cListItem->details.value.integer;
                        break;
                    case BaseType_Real:
                    case BaseType_Double:
                        cListItem->details.dt.type = dListItem->type;
                        break;
                    case BaseType_Character:
                    case BaseType_Complex:
                    default:
                        err("Data value for %s cannot be coerced to %s", dListItem->symbol->identifier, baseTypeToStr(dListItem->type));
                        return FALSE;
                    }
                    break;
                case BaseType_Complex:
                default:
                    break;
                }
            }
            repeatCount -= 1;
            if (repeatCount < 1) {
                cListItem = cListItem->next;
                if (cListItem != NULL) repeatCount = cListItem->repeatCount;
            }
        }
        dListItem = dListItem->next;
    }
    return TRUE;
}

static void verifyEOS(char *s) {
    while (*s != '\0') {
        if (!isspace(*s++)) {
            err("Unexpected text at end of statement");
            return;
        }
    }
}

static char *baseTypeToStr(BaseType type) {
    switch (type) {
    case BaseType_Undefined: return "Undefined";
    case BaseType_Character: return "CHARACTER";
    case BaseType_Logical:   return "LOGICAL";
    case BaseType_Integer:   return "INTEGER";
    case BaseType_Real:      return "REAL";
    case BaseType_Double:    return "DOUBLE";
    case BaseType_Complex:   return "COMPLEX";
    case BaseType_Label:     return "Label";
    case BaseType_Pointer:   return "POINTER";
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
    TokenListItem *tli;
    int n;

    if (token != NULL) {
        switch (token->type) {
        case TokenType_Keyword:
            fputs(tokenIdToStr(token->details.keyword.id), f);
            break;
        case TokenType_Identifier:
            fputs(token->details.identifier.name, f);
            if (token->details.identifier.qualifiers != NULL) {
                fputs("(", f);
                for (tli = token->details.identifier.qualifiers, n = 0; tli != NULL; tli = tli->next, n++) {
                    if (n > 0) fputs(",", f);
                    printExpression(f, tli->item);
                }
                fputs(")", f);
            }
            if (token->details.identifier.range != NULL) {
                fputs("(", f);
                printExpression(f, token->details.identifier.range->first);
                fputs(":", f);
                printExpression(f, token->details.identifier.range->last);
                fputs(")", f);
            }
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
    case ArgClass_Auto:        return "Auto";
    case ArgClass_Static:      return "Static";
    case ArgClass_Adjustable:  return "Adjustable";
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
    case DIMENSION:       return "DIMENSION";
    case DO:              return "DO";
    case DOUBLEPRECISION: return "DOUBLEPRECISION";
    case ELSE:            return "ELSE";
    case ELSEIF:          return "ELSEIF";
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
