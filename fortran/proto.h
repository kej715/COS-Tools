/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: proto.h
**
**  Description:
**      This file provides function prototypes.
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

#ifndef PROTO_H
#define PROTO_H

#include <stdio.h>
#include "types.h"

Symbol *addCommonBlock(char *name);
Symbol *addLabel(char *label);
Symbol *addSymbol(char *identifier, SymbolClass class);
void *allocate(int size);
Symbol *allocSymbol(char *identifier, SymbolClass class);
int calculateAutoOffsets(void);
void calculateCommonOffsets(void);
int calculateSize(Symbol *symbol);
int calculateStaticOffsets(void);
void compile(char *name);
Symbol *createShadow(Symbol *symbol, SymbolClass class);
int countArrayElements(Symbol *symbol);
void defineLocalVariable(Symbol *symbol);
void defineType(Symbol *symbol);
void emitCommonBlocks(void);
void err(char *format, ...);
Symbol *findCommonBlock(char *name);
Symbol *findIntrinsicFunction(char *name);
Symbol *findLabel(char *label);
Symbol *findSymbol(char *identifier);
void freeAllSymbols(void);
void generateLabel(char *label);
char *getNextChar(char *s);
char *getIdentifier(char *s, Token *token);
char *getNextToken(char *s, Token *token, bool doMatchKeywords);
Symbol *getSymbolRoot(void);
DataType *getSymbolType(Symbol *sym);
bool linkVariables(Symbol *fromSym, int fromOffset, Symbol *toSym, int toOffset);
void list(char *format, ...);
void listEject(void);
void listSetPageEnd(void);
void listSymbols(void);
void presetImplicit(void);
void presetOffsetCalculation(void);
void printStackTrace(FILE *fp);
void *reallocate(void *old, int oldSize, int newSize);
void registerIntrinsicFunctions(void);
void removeAllShadows(void);
void removeShadow(Symbol *symbol);
void reportUnresolvedLabels(void);
void resetCommonBlocks(void);
void resolveTypes(void);
char *symClassToStr(SymbolClass class);
void warn(char *format, ...);

extern int autoOffset;
extern char currentDate[];
extern char currentTime[];
extern bool doEchoSource;
extern bool doList;
extern bool doStaticLocals;
extern bool doStaticLocalsDefault;
extern bool doSuppressWarnings;
extern int errorCount;
extern DataType implicitTypes[];
extern int lineNo;
extern FILE *listingFile;
extern FILE *objectFile;
extern Symbol *progUnitSym;
extern FILE *sourceFile;
extern char *sourcePath;
extern int staticOffset;
extern char stmtBuf[];
extern Symbol *symbols;
extern int totalErrors;
extern int warningCount;

#endif
