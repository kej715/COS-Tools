/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: types.h
**
**  Description:
**      This file provides type definitions used by the compiler.
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

#ifndef TYPES_H
#define TYPES_H

#include "../basetypes.h"

typedef enum tokenId {
    UNDEFINED = 0,
    ASSIGN,
    BACKSPACE,
    BLOCKDATA,
    CALL,
    CHARACTER,
    CLOSE,
    COMMON,
    COMPLEX,
    CONTINUE,
    DATA,
    DIMENSION,
    DO,
    DOUBLEPRECISION,
    ELSE,
    ELSEIF,
    END,
    ENDFILE,
    ENDIF,
    ENTRY,
    EQUIVALENCE,
    EXTERNAL,
    FORMAT,
    FUNCTION,
    GOTO,
    IF,
    IMPLICIT,
    IMPLICITNONE,
    INCLUDE,
    INQUIRE,
    INTEGER,
    INTRINSIC,
    LOGICAL,
    OPEN,
    PARAMETER,
    PAUSE,
    POINTER,
    PRINT,
    PROGRAM,
    PUNCH,
    READ,
    REAL,
    RETURN,
    REWIND,
    SAVE,
    STOP,
    SUBROUTINE,
    WRITE
} TokenId;

typedef enum operatorId {
 /*
  *  Binary operators
  */
    OP_ADD = 0,
    OP_DIV,
    OP_EXP,
    OP_MUL,
    OP_SUB,
    OP_AND,
    OP_OR,
    OP_EQV,
    OP_NEQV,
    OP_EQ,
    OP_GE,
    OP_GT,
    OP_LE,
    OP_LT,
    OP_NE,
    OP_CAT,
 /*
  *  Unary operators
  */
    OP_NEG,
    OP_NOT,
    OP_PLUS,
 /*
  *  Special subexpression "operator"
  */
    OP_SEXPR
} OperatorId;

#define PREC_SEXPR 0
#define PREC_EXP   1
#define PREC_MUL   2
#define PREC_DIV   2
#define PREC_NEG   3
#define PREC_PLUS  3
#define PREC_ADD   3
#define PREC_SUB   3
#define PREC_CAT   4
#define PREC_EQ    5
#define PREC_GT    5
#define PREC_GE    5
#define PREC_LT    5
#define PREC_LE    5
#define PREC_NE    5
#define PREC_NOT   6
#define PREC_AND   7
#define PREC_OR    8
#define PREC_EQV   9
#define PREC_NEQV  9

typedef enum statementClass {
    StmtClass_None = 0,
    StmtClass_Nonexecutable,
    StmtClass_Format,
    StmtClass_Executable,
    StmtClass_Do_Term
} StatementClass;

typedef enum argumentClass {
    ArgClass_Undefined = 0,
    ArgClass_Constant,
    ArgClass_Calculation,
    ArgClass_Function,
    ArgClass_Auto,
    ArgClass_Static,
    ArgClass_Global,
    ArgClass_Argument,
    ArgClass_Pointee
} ArgumentClass;

typedef enum baseType {
    BaseType_Undefined = 0,
    BaseType_Character,
    BaseType_Logical,
    BaseType_Integer,
    BaseType_Real,
    BaseType_Double,
    BaseType_Complex,
    BaseType_Label,
    BaseType_Pointer
} BaseType;

typedef enum symbolClass {
    SymClass_Undefined = 0,
    SymClass_Program,
    SymClass_Subroutine,
    SymClass_Function,
    SymClass_Intrinsic,
    SymClass_BlockData,
    SymClass_NamedCommon,
    SymClass_Auto,
    SymClass_Static,
    SymClass_Global,
    SymClass_Argument,
    SymClass_Parameter,
    SymClass_Pointee,
    SymClass_Label
} SymbolClass;

typedef enum tokenType {
    TokenType_None = 0,
    TokenType_Keyword,
    TokenType_Identifier,
    TokenType_Operator,
    TokenType_Constant,
    TokenType_Invalid
} TokenType;

typedef struct keyword {
    char *name;
    TokenId id;
    StatementClass class;
} Keyword;

typedef struct bounds {
    int lower;
    int upper;
} Bounds;

typedef struct characterValue {
    u64 length;
    char *string;
} CharacterValue;

typedef struct complexValue {
    f64 real;
    f64 imaginary;
} ComplexValue;

typedef union dataValue {
    i64 integer;
    u64 logical;
    f64 real;
    CharacterValue character;
    u64 charRef;
    ComplexValue complex;
} DataValue;

typedef struct dataType {
    BaseType type;
    int constraint;
    int rank;
    Bounds bounds[7];
} DataType;

typedef struct invalidDetails {
    int lineNo;
    int column;
} InvalidDetails;

typedef struct constantDetails {
    DataType dt;
    DataValue value;
} ConstantDetails;

typedef struct constantListItem {
    struct constantListItem *next;
    int repeatCount;
    ConstantDetails details;
} ConstantListItem;

typedef struct identifierDetails {
    char *name;
    struct tokenListItem *qualifiers; // subscripts or function arguments
    struct stringRange *range;
} IdentifierDetails;

typedef struct keywordDetails {
    TokenId id;
    StatementClass class;
} KeywordDetails;

typedef struct operatorDetails {
    OperatorId id;
    int precedence;
    struct token *leftArg;
    struct token *rightArg;
} OperatorDetails;

typedef union tokenDetails {
    KeywordDetails keyword;
    IdentifierDetails identifier;
    OperatorDetails operator;
    ConstantDetails constant;
    InvalidDetails invalid;
} TokenDetails;

typedef struct token {
    TokenType type;
    TokenDetails details;
} Token;

typedef struct tokenListItem {
    struct tokenListItem *next;
    Token *item;
} TokenListItem;

typedef struct stringRange {
    Token *first;
    Token *last;
} StringRange;

typedef struct commonBlockDetails {
    char label[8];
    int offset;
    int limit;
} CommonBlockDetails;

#define MAX_INTRINSIC_ARGS 2

typedef struct intrinsicDetails {
    bool isGeneric;
    char *externName;
    BaseType resultType;
    int argc;
    BaseType argumentTypes[MAX_INTRINSIC_ARGS];
} IntrinsicDetails;

typedef struct labelDetails {
    StatementClass class;
    bool forwardRef;
    char label[8];
} LabelDetails;

typedef struct pointeeDetails {
    DataType dt;
    struct symbol *pointer;
} PointeeDetails;

typedef struct progUnitDetails {
    DataType dt;
    int offset;
    char exitLabel[8];
    char frameSizeLabel[8];
    char staticDataLabel[8];
} ProgUnitDetails;

typedef struct variableDetails {
    DataType dt;
    int offset;
    struct symbol *staticBlock;
} VariableDetails;

typedef union symbolDetails {
    CommonBlockDetails common;
    IntrinsicDetails intrinsic;
    LabelDetails label;
    ConstantDetails param;
    PointeeDetails pointee;
    ProgUnitDetails progUnit;
    VariableDetails variable;
} SymbolDetails;

typedef struct symbol {
    struct symbol *left;
    struct symbol *right;
    struct symbol *next;
    char *identifier;
    SymbolClass class;
    int size;
    SymbolDetails details;
} Symbol;

typedef int Register;

typedef union arrayOffset {
    int constant;
    Register reg;
} ArrayOffset;

typedef struct reference {
    Symbol *symbol;
    ArgumentClass offsetClass;
    ArrayOffset offset;
} Reference;

typedef struct storageRefeerence {
    Symbol *symbol;
    TokenListItem *expressionList;
    StringRange *strRange;
} StorageReference;

typedef union argumentDetails {
    ConstantDetails constant;
    DataType calculation;
    Reference reference;
} ArgumentDetails;

typedef struct operatorArgument {
    ArgumentClass class;
    ArgumentDetails details;
    Register reg;
} OperatorArgument;

typedef struct equivMember {
    struct equivMember *next;
    Symbol *symbol;
    int offset;
} EquivMember;

typedef struct equivGroup {
    struct equivGroup *next;
    EquivMember *firstMember;
    EquivMember *lastMember;
} EquivGroup;

typedef enum fileStatus {
    FileStatus_Unknown = 0,
    FileStatus_Old,
    FileStatus_New,
    FileStatus_Scratch
} FileStatus;

typedef struct closeInfoList {
    Token *unit;
    Token *fileStatus;
    StorageReference iostat;
    Symbol *errLabel;
} CloseInfoList;

typedef struct controlInfoList {
    Token *unit;
    BaseType unitType;
    Token *format;
    Symbol *endLabel;
    Symbol *errLabel;
    Token *recordNumber;
    StorageReference iostat;
} ControlInfoList;

typedef struct inquireInfoList {
    Token *unit;
    Token *fileName;
    StorageReference existRef;
    StorageReference openedRef;
    StorageReference numberRef;
    StorageReference namedRef;
    StorageReference nameRef;
    StorageReference accessRef;
    StorageReference sequentialRef;
    StorageReference directRef;
    StorageReference formattedRef;
    StorageReference unformattedRef;
    StorageReference formRef;
    StorageReference blankRef;
    StorageReference reclRef;
    StorageReference nextRecRef;
    StorageReference iostat;
    Symbol *errLabel;
} InquireInfoList;

typedef struct openInfoList {
    Token *unit;
    Token *fileName;
    Token *fileStatus;
    Token *formatting;
    Token *access;
    Token *blankSpecifier;
    Token *recordLength;
    StorageReference iostat;
    Symbol *errLabel;
} OpenInfoList;

typedef struct dataInitializerItem {
    struct dataInitializerItem *next;
    Symbol *symbol;
    BaseType type;
    int constraint;
    char *blockName;
    char *blockType;
    char blockLabel[8];
    int blockOffset;
    int elementOffset;
    int elementCount;
    int charOffset;
    int charLength;
} DataInitializerItem;

#define isCalculation(arg) ((arg).class == ArgClass_Calculation)
#define isConstant(arg) ((arg).class == ArgClass_Constant)
#define isFunction(arg) ((arg).class == ArgClass_Function)
#define isLoadable(arg) ((arg).class > ArgClass_Function)

#define isIntegerConstant(arg) ((arg).class == ArgClass_Constant && (arg).details.constant.dt.type == BaseType_Integer)
#define isRealConstant(arg) ((arg).class == ArgClass_Constant && (arg).details.constant.dt.type == BaseType_Real)

#define isArithOp(op)   (((op) >= OP_ADD) && ((op) <= OP_SUB))
#define isBinaryOp(op)  (((op) >= OP_ADD) && ((op) <= OP_CAT))
#define isCompareOp(op) (((op) >= OP_EQ ) && ((op) <= OP_NE))
#define isLogicalOp(op) (((op) >= OP_AND) && ((op) <= OP_NEQV))
#define isUnaryOp(op)   (((op) >= OP_NEG) && ((op) <= OP_PLUS))

#endif
