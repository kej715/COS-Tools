#ifndef PROTO_H
#define PROTO_H
/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: proto.h
**
**  Description:
**      This file defines function and variable prototypes.
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
#include "const.h"
#include "cosdataset.h"
#include "types.h"

int baseStack[BASE_STACK_SIZE];
int baseStackPtr;
Block *blockStack[BLOCK_STACK_SIZE];
int blockStackPtr;
char *calName;
char *calVersion;
u32 column;
int currentBase;
Block *currentBlock;
char currentDate[9];
char currentJDate[7];
u16 currentListControl;
Module *currentModule;
Qualifier *currentQualifier;
char currentTime[9];
Name *duplicateds;
u32 errorCount;
u64 errorUnion;
Module *firstModule;
Module *lastModule;
u16 listControlMask;
u16 listControlStack[LIST_CONTROL_STACK_SIZE];
int listControlStackPtr;
FILE *listingFile;
char *locationField;
Token *locationFieldToken;
MacroCall *macroStack[MACRO_STACK_SIZE];
int macroStackPtr;
Name *moduleNames;
Dataset *objectFile;
char *operandField;
char *osDate;
char *osName;
int pass;
Qualifier *qualifierStack[QUALIFIER_STACK_SIZE];
int qualifierStackPtr;
char *resultField;
FILE *sourceFile;
char sourceLine[MAX_SOURCE_LINE_LENGTH+1];
char subtitle[MAX_TITLE_LENGTH+1];
char title[MAX_TITLE_LENGTH+1];
u32 warningCount;

Literal *addLiteral(Token *expression);
ErrorCode addLocationSymbol(char *id, int len, u16 attributes);
Module *addModule(char *id, int len);
Name *addName(Name **root, char *id, int len);
Qualifier *addQualifier(char *id, int len);
Symbol *addSymbol(char *id, int len, Qualifier *qualifier, Value *value);
void adjustSymbolValues(Module *module);
void advanceBitPosition(int count);
void *allocate(int size);
void calculateBlockOffsets(Module *module);
ErrorCode callMacro(MacroDefn *defn, Token *locationFieldToken);
void clearErrorIndications(void);
Token *copyToken(Token *token);
void emitLiterals(void);
bool equalTokens(Token *t1, Token *t2);
ErrorCode evaluateExpression(Token *expression, Value *value);
NamedInstruction *findInstruction(char *id, int len);
Module *findModule(char *id, int len);
Name *findName(Name *root, char *id, int len);
Symbol *findQualifiedSymbol(Token *token);
Qualifier *findQualifier(char *id);
Qualifier *findQualifierWithLen(char *id, int len);
Symbol *findSymbol(char *id, int len, Qualifier *qualifier);
void freeMacroCall(MacroCall *call);
void freeToken(Token *token);
ErrorCode getErrorCode(char *s, int len);
int getErrorCount(void);
char *getErrorIndications(void);
char *getErrorIndicator(ErrorCode code);
char *getErrorMessage(ErrorCode code);
char *getNextToken(char *s, Token *token);
char *getNextValue(char *s, Value *value, ErrorCode *err);
ErrorCode getRegisterNumber(Token *regster, int *number);
u16 getValueType(Value *value);
int getWarningCount(void);
int handleExp(void);
bool hasErrorRegistrations(void);
void instInit(void);
int isEof(void);
bool isNameChar(char c);
bool isNameChar1(char c);
bool isParcelType(Value *value);
bool isUnqualifiedName(Token *token);
bool isValueType(Value *value);
bool isWordType(Value *value);
void listClearSource(void);
void listCode(u64 bits, int count, int lastCol);
void listCode16(u16 bits);
void listCode10_22(u32 bits, u16 attributes);
void listCode7_24(u32 bits, u16 attributes);
void listCodeLocation(void);
void listEject(void);
void listErrorIndications(void);
void listErrorSummary(void);
void listField(u64 bits, int len, u16 attributes, int colOffset);
void listFlush(void);
void listInit(void);
void listLocation(u32 location);
void listSource(void);
void listSpace(int lines);
void listSymbolTable(void);
void listValue(Value *val);
void listWord(u64 bits, u16 attributes);
char *parseExpression(char *s, Token **expression);
ErrorCode parseSourceLine(void);
void printToken(FILE *file, Token *token);
ErrorCode processMachineInstruction(void);
void readNextLine(void);
void *reallocate(void *old, int oldSize, int newSize);
ErrorCode registerError(ErrorCode code);
void resetBase(void);
void resetModule(Module *module);
void resetErrorRegistrations(void);
int writeObjectFile(Module *module, Dataset *ds);
#endif
