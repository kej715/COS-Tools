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
    BUFFERIN,
    BUFFEROUT,
    CALL,
    CHARACTER,
    CLOSE,
    COMMON,
    COMPLEX,
    CONTINUE,
    DATA,
    DECODE,
    DIMENSION,
    DO,
    DOUBLEPRECISION,
    ELSE,
    ELSEIF,
    ENCODE,
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
    NAMELIST,
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
  *  Unary operators
  */
    OP_NEG,
    OP_NOT,
    OP_PLUS,
 /*
  *  Binary operators
  */
    OP_ADD,
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
  *  Special subexpression "operator"
  */
    OP_SEXPR
} OperatorId;

#define PREC_SEXPR 0
#define PREC_EXP   1
#define PREC_NEG   2
#define PREC_PLUS  2
#define PREC_MUL   3
#define PREC_DIV   3
#define PREC_ADD   4
#define PREC_SUB   4
#define PREC_CAT   4
#define PREC_NOT   5
#define PREC_AND   6
#define PREC_OR    7
#define PREC_EQV   8
#define PREC_NEQV  8
#define PREC_EQ    9
#define PREC_GT    9
#define PREC_GE    9
#define PREC_LT    9
#define PREC_LE    9
#define PREC_NE    9

typedef enum statementClass {
    StmtClass_None = 0,
    StmtClass_Nonexecutable,
    StmtClass_Format,
    StmtClass_Executable,
    StmtClass_Do_Term
} StatementClass;

typedef struct keyword {
    char *name;
    TokenId id;
    StatementClass class;
} Keyword;

typedef enum argumentClass {
    ArgClass_Constant = 0,
    ArgClass_Calculation,
    ArgClass_Function,
    ArgClass_Local,
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
    SymClass_BlockData,
    SymClass_NamedCommon,
    SymClass_Local,
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

typedef struct dataType {
    BaseType type;
    int constraint;
    int rank;
    Bounds bounds[7];
} DataType;

typedef struct doubleValue {
   f64 high;
   f64 low;
} DoubleValue;

typedef struct invalidDetails {
    int lineNo;
    int column;
} InvalidDetails;

typedef struct constantDetails {
    DataType dt;
    union val {
        i64 integer;
        u64 logical;
        f64 real;
        CharacterValue chr;
        DoubleValue dp;
        ComplexValue complex;
    } value;
} ConstantDetails;

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
    char *identifier;
    OperatorDetails operator;
    ConstantDetails constant;
    InvalidDetails invalid;
} TokenDetails;

typedef struct token {
    TokenType type;
    TokenDetails details;
} Token;

typedef struct formalParam {
    struct formalParam *next;
    struct symbol *sym;
} FormalParam;

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
    char frameSizeLabel[8];
    FormalParam *params;
    DataType dt;
    int offset;
} ProgUnitDetails;

typedef struct variableDetails {
    DataType dt;
    int offset;
} VariableDetails;

typedef union symbolDetails {
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

typedef union argumentDetails {
    ConstantDetails constant;
    DataType calculation;
    Symbol *symbol;
} ArgumentDetails;

typedef struct operatorArgument {
    ArgumentClass class;
    ArgumentDetails details;
    Register reg;
} OperatorArgument;

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
#define isUnaryOp(op)   ((op) < OP_ADD)

#endif
