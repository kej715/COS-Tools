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

extern int baseStack[];
extern int baseStackPtr;
extern char *calName;
extern char *calVersion;
extern u32 column;
extern int currentBase;
extern Section *currentSection;
extern char currentDate[];
extern char currentJDate[];
extern u16 currentListControl;
extern Module *currentModule;
extern Qualifier *currentQualifier;
extern SourceFormatType currentSourceFormat;
extern char currentTime[];
extern Module *defaultModule;
extern SourceFormatType defaultSourceFormat;
extern Name *duplicateds;
extern u32 errorCount;
extern u64 errorUnion;
extern Module *firstModule;
extern Module *lastModule;
extern u16 listControlMask;
extern u16 listControlStack[];
extern int listControlStackPtr;
extern FILE *listingFile;
extern char *locationField;
extern Token *locationFieldToken;
extern MacroCall *macroStack[];
extern int macroStackPtr;
extern Name *moduleNames;
extern Dataset *objectFile;
extern char *operandField;
extern char *osDate;
extern char *osName;
extern int pass;
extern Qualifier *qualifierStack[];
extern int qualifierStackPtr;
extern char *resultField;
extern Section *sectionStack[];
extern int sectionStackPtr;
extern FILE *sourceFile;
extern SourceFormatType sourceFormatStack[];
extern int sourceFormatStackPtr;
extern char sourceLine[];
extern char subtitle[];
extern char title[];
extern u32 warningCount;

Literal *addLiteral(Token *expression);
ErrorCode addLocationSymbol(Section *section, char *id, int len, u16 attributes);
Module *addModule(char *id, int len);
Name *addName(Name **root, char *id, int len);
Qualifier *addQualifier(char *id, int len);
Section *addSection(Module *module, char *id, int len, SectionType type, SectionLocation location);
Symbol *addSymbol(char *id, int len, Qualifier *qualifier, Value *value);
void adjustSymbolValues(Module *module);
void advanceBitPosition(Section *section, int count);
void *allocate(int size);
ErrorCode callMacro(MacroDefn *defn, Token *locationFieldToken);
void clearErrorIndications(void);
Token *copyToken(Token *token);
void createObjectBlocks(Module *module);
void emit_g_h_i_jkm(Section *section, u8 g, u8 h, u8 i, Value *jkm);
void emit_gh_i_j_k(Section *section, u8 gh, u8 i, u8 j, u8 k);
void emit_gh_i_jk(Section *section, u8 gh, u8 i, u8 jk);
void emit_gh_i_jkm(Section *section, u8 gh, u8 i, Value *jkm);
void emit_gh_ijk(Section *section, u8 gh, u16 ijk);
void emit_gh_ijkm(Section *section, u8 gh, Value *ijkm);
void emitFieldBits(Section *section, u64 bits, int len, u16 attributes, bool doListFlush);
void emitFieldEnd(Section *section, u16 attributes);
void emitFieldStart(Section *section);
void emitLiterals(Module *module);
void emitString(Section *section, char *s, int len, int count, JustifyType justification);
bool equalTokens(Token *t1, Token *t2);
ErrorCode evaluateExpression(Token *expression, Value *value);
NamedInstruction *findInstruction(char *id, int len);
Module *findModule(char *id, int len);
Name *findName(Name *root, char *id, int len);
Symbol *findQualifiedSymbol(Token *token);
Qualifier *findQualifier(char *id);
Qualifier *findQualifierWithLen(char *id, int len);
Symbol *findSymbol(char *id, int len, Qualifier *qualifier);
void forceWordBoundary(Section *section);
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
u16 getRelativeAttribute(Section *section);
u16 getValueType(Value *value);
int getWarningCount(void);
int handleExp(void);
bool hasErrorRegistrations(void);
void instInit(void);
bool isAbsolute(Value *value);
bool isCodeSection(Section *section);
bool isCommonSection(Section *section);
bool isDataSection(Section *section);
int isEof(void);
bool isExternal(Value *value);
bool isImmobile(Value *value);
bool isNameChar(char c);
bool isNameChar1(char c);
bool isNamedCommonSection(Section *section);
bool isParcelAddress(Value *value);
bool isPlainValue(Value *value);
bool isRelative(Value *val);
bool isRelocatable(Value *value);
bool isUnqualifiedName(Token *token);
bool isWordAddress(Value *value);
void listClearSource(void);
void listCode(u64 bits, int count, int lastCol);
void listCode16(u16 bits);
void listCode10_22(u32 bits, u16 attributes);
void listCode7_24(u32 bits, u16 attributes);
void listCodeLocation(Section *section);
void listEject(void);
void listErrorIndications(void);
void listErrorSummary(void);
void listField(u64 bits, int len, u16 attributes, int colOffset);
void listFlush(Section *section);
void listInit(void);
void listLocation(u32 location);
void listSource(void);
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
u64 toCrayFloat(u64 ieee);
int writeObjectRecord(Module *module, Dataset *ds);
#endif
