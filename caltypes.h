#ifndef CALTYPES_H
#define CALTYPES_H
/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: caltypes.h
**
**  Description:
**      This file defines constants, types, and macros.
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

#include "basetypes.h"

/*
 *  Error indications
 */
typedef enum errorCode {
    Err_None = 0,
    Err_DataItem,
    Err_DoubleDefinition,
    Err_IllegalNesting,
    Err_TooManyEntries,
    Err_InstructionPlacement,
    Err_LocationField,
    Err_RelocatableField,
    Err_OperandField,
    Err_Programmer,
    Err_ResultField,
    Err_Syntax,
    Err_Type,
    Err_Undefined,
    Err_FieldWidth,
    Err_Expression,
    Warn_Programmer,
    Warn_IgnoredLocationSymbol,
    Warn_BadLocationSymbol,
    Warn_ExpressionElement,
    Warn_MachineInstruction,
    Warn_Truncation,
    Warn_UndefinedLocationSymbol,
    Warn_MicroSubstitution,
    Warn_AddressCounter,
    Warn_ExternalDeclaration,
    Warn_RedefinedMacro,
    Info_ModuleEnd
} ErrorCode;

/*
 *  Edit control modes
 */
typedef enum editControl {
    EditControl_Off = 0,
    EditControl_On
} EditControl;

/*
 *  Listing flags
 */
#define LIST_ON     0x001
#define LIST_XRF    0x002
#define LIST_XNS    0x004
#define LIST_DUP    0x008
#define LIST_MAC    0x010
#define LIST_MIF    0x020
#define LIST_MIC    0x040
#define LIST_LIS    0x080
#define LIST_WEM    0x100
#define LIST_TXT    0x200
#define LIST_WRP    0x400
#define LIST_WMR    0x800

/*
 *  Source code formats
 */
typedef enum sourceFormatType {
    SourceFormat_New = 0,
    SourceFormat_Old
} SourceFormatType;

/*
 *  Types supporting macro definitions
 */
typedef enum macroParamType {
    MacroParamType_Positional = 0,
    MacroParamType_Keyword
} MacroParamType;

typedef struct macroParam {
    struct macroParam *next;
    MacroParamType type;
    char *name;
    char *value;
} MacroParam;

typedef enum macroFragType {
    MacroFragType_Text = 0,
    MacroFragType_ParamRef,
    MacroFragType_Regex
} MacroFragType;

typedef struct macroFragment {
    struct macroFragment *next;
    MacroFragType type;
    char *text;
} MacroFragment;

typedef struct macroLine {
    struct macroLine *next;
    MacroFragment *fragments;
} MacroLine;

typedef struct macroDefn {
    int creationPass;
    MacroParam *locationParam;
    MacroParam *params;
    MacroLine *body;
} MacroDefn;

typedef struct macroCall {
    MacroDefn *defn;
    MacroParam *params;
    MacroLine *nextLine;
} MacroCall;

/*
 *  Instances of Name are associated with sections, duplicateds,
 *  macros, micros, and modules.
 */

typedef struct name {
    struct name *left;
    struct name *right;
    char *id;
    void *value;
} Name;

/*
 *  Attributes of symbols
 */
#define SYM_REDEFINABLE     0x001
#define SYM_WORD_ADDRESS    0x002
#define SYM_PARCEL_ADDRESS  0x004
#define SYM_BYTE_ADDRESS    0x008
#define SYM_LITERAL         0x010
#define SYM_RELOCATABLE     0x020
#define SYM_IMMOBILE        0x040
#define SYM_EXTERNAL        0x080
#define SYM_ENTRY           0x100
#define SYM_COUNTER         0x200
#define SYM_UNDEFINED       0x400
#define SYM_DEFINED_P2      0x800

/*
 *  Data types of numeric values
 */
typedef enum numberType {
    NumberType_Integer = 0,
    NumberType_Float
} NumberType;

typedef union numericValue {
    i64 intValue;
    f64 floatValue;
} NumericValue;

typedef struct value {
    NumberType type;
    u16 attributes;
    struct section *section;
    struct symbol *externalSymbol;
    u32 coefficient;
    NumericValue value;
} Value;

typedef struct symbol {
    struct symbol *left;
    struct symbol *right;
    struct symbol *next;
    char *id;
    u16 externalIndex;
    Value value;
} Symbol;

typedef struct qualifier {
    struct qualifier *left;
    struct qualifier *right;
    char *id;
    Symbol *symbols;
} Qualifier;

/*
 *  Section and object block definitions
 */
typedef enum sectionType {
    SectionType_Mixed = 0,
    SectionType_Code,
    SectionType_Data,
    SectionType_Stack,
    SectionType_Common,
    SectionType_Dynamic,
    SectionType_TaskCom,
    SectionType_None
} SectionType;

typedef enum sectionLocation {
    SectionLocation_CM = 0,
    SectionLocation_EM,
    SectionLocation_LM,
    SectionLocation_None
} SectionLocation;

typedef struct externalTableEntry {
    u16 externalIndex;
    u32 bitAddress;
    u8 fieldLength;
    bool isParcelRelocation;
} ExternalTableEntry;

typedef enum relocationEntryType {
    RelocEntryType_Standard = 0,
    RelocEntryType_Extended
} RelocationEntryType;

typedef struct relocationTableEntry {
    RelocationEntryType type;
    u16 blockIndex;
    u32 offset;
    u8 fieldLength;
    bool isParcelRelocation;
} RelocationTableEntry;

typedef struct objectBlock {
    struct objectBlock *next;
    char *id;
    u16 index;
    SectionType type;
    SectionLocation location;
    u8 *image;
    u32 imageSize;
    u32 offset;
    int isNotEmpty;
    u32 lowestParcelAddress;
    u32 highestParcelAddress;
    RelocationTableEntry *relocationTable;
    int relocationTableIndex;
    int relocationTableSize;
    ExternalTableEntry *externalTable;
    int externalTableIndex;
    int externalTableSize;
} ObjectBlock;

typedef struct section {
    struct section *next;
    char *id;
    struct module *module;
    SectionType type;
    SectionLocation location;
    u32 originOffset;
    u32 size;
    u32 originCounter;
    u32 locationCounter;
    u8  wordBitPosCounter;
    u8  parcelBitPosCounter;
    u32 relocationCoefficient;
    u32 immobileCoefficient;
    ObjectBlock *objectBlock;
} Section;

/*
 *  Named instruction definitions
 */
#define INST_MACHINE 0x01

typedef struct namedInstruction {
    struct namedInstruction *left;
    struct namedInstruction *right;
    char *id;
    u8 attributes;
    ErrorCode (*handler)(void);
} NamedInstruction;

/*
 *  Token definitions used in parsing fields
 */
typedef enum tokenType {
    TokenType_None = 0,
    TokenType_Register,
    TokenType_Name,
    TokenType_Number,
    TokenType_String,
    TokenType_Operator,
    TokenType_Error
} TokenType;

typedef struct errorToken {
    ErrorCode code;
} ErrorToken;

typedef struct nameToken {
    char *ptr;
    int len;
    char *qualPtr;
    int qualLen;
} NameToken;

typedef struct numberToken {
    NumberType type;
    NumericValue value;
} NumberToken;

typedef enum operatorType {
    Op_SubExpr = 0,
    // unary
    Op_Negate,
    Op_Plus,
    Op_Complement,
    Op_Reciprocate,
    Op_MaskRight,
    Op_CmplMaskRight,
    Op_MaskLeft,
    Op_CmplMaskLeft,
    Op_Byte,
    Op_ByteOffset,
    Op_Parcel,
    Op_Word,
    Op_Literal,
    // binary
    Op_Add,
    Op_Subtract,
    Op_Multiply,
    Op_Divide,
    Op_FloatAdd,
    Op_FloatSubtract,
    Op_FloatMultiply,
    Op_HalfMultiply,
    Op_RoundedMultiply,
    Op_2_FloatMultiply,
    Op_HalfDivide,
    Op_ShiftRight,
    Op_ShiftLeft,
    Op_And,
    Op_Or,
    Op_Xor
} OperatorType;

#define PRECEDENCE_SUB_EXPR        0
#define PRECEDENCE_NEGATE          1
#define PRECEDENCE_PLUS            1
#define PRECEDENCE_COMPLEMENT      1
#define PRECEDENCE_RECIPROCATE     1
#define PRECEDENCE_MASK_RIGHT      1
#define PRECEDENCE_CMPL_MASK_RIGHT 1
#define PRECEDENCE_MASK_LEFT       1
#define PRECEDENCE_CMPL_MASK_LEFT  1
#define PRECEDENCE_BYTE            1
#define PRECEDENCE_BYTE_OFFSET     1
#define PRECEDENCE_PARCEL          1
#define PRECEDENCE_WORD            1
#define PRECEDENCE_LITERAL         1
#define PRECEDENCE_MULTIPLY        2
#define PRECEDENCE_DIVIDE          2
#define PRECEDENCE_AND             2
#define PRECEDENCE_SHIFT_RIGHT     2
#define PRECEDENCE_SHIFT_LEFT      2
#define PRECEDENCE_ADD             3
#define PRECEDENCE_SUBTRACT        3
#define PRECEDENCE_OR              3
#define PRECEDENCE_XOR             3

typedef struct operatorToken {
    OperatorType type;
    u8 precedence;
    struct token *rightArg;
    struct token *leftArg;
} OperatorToken;

typedef enum registerType {
    // register groups
    RegisterType_A = 0,
    RegisterType_B,
    RegisterType_S,
    RegisterType_PS,
    RegisterType_ZS,
    RegisterType_QS,
    RegisterType_SB,
    RegisterType_SM,
    RegisterType_SR,
    RegisterType_ST,
    RegisterType_T,
    RegisterType_V,
    RegisterType_PV,
    RegisterType_QV,
    // standalone registers
    RegisterType_Sem,
    RegisterType_Sign,
    RegisterType_CA,
    RegisterType_CE,
    RegisterType_CI,
    RegisterType_CL,
    RegisterType_MC,
    RegisterType_RT,
    RegisterType_VL,
    RegisterType_VM,
    RegisterType_XA
} RegisterType;

typedef struct registerToken {
    RegisterType type;
    char *ptr;
    int len;
    int ordinal;
} RegisterToken;

typedef enum justifyType {
    Justify_LeftBlankFill = 0,
    Justify_LeftZeroFill,
    Justify_RightZeroFill,
    Justify_LeftZeroEnd
} JustifyType;

typedef struct stringToken {
    char *ptr;
    int len;
    int count;
    JustifyType justification;
} StringToken;

typedef union tokenDetails {
    ErrorToken error;
    NameToken name;
    NumberToken number;
    OperatorToken operator;
    RegisterToken regster;
    StringToken string;
} TokenDetails;

typedef struct token {
    TokenType type;
    TokenDetails details;
} Token;

typedef struct literal {
    struct literal *next;
    Token *expression;
    u32 offset;
} Literal;

typedef struct module {
    struct module *next;
    char *id;
    char *comment;
    bool isAbsolute;
    u32  stackSize;
    Name *duplicateds;
    Name *macros;
    Name *micros;
    Qualifier *qualifiers;
    Literal *literals;
    Symbol *start;
    Symbol *entryPoints;
    Symbol *externals;
    Section *firstSection;
    Section *lastSection;
    ObjectBlock *firstObjectBlock;
    ObjectBlock *lastObjectBlock;
} Module;

#endif
