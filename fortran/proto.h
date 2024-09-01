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
int calculateAutoOffsets(void);
int calculateSize(Symbol *symbol);
int calculateStaticOffsets(void);
void compile(char *name);
int countArrayElements(Symbol *symbol);
void emitCommonBlocks(void);
Symbol *findCommonBlock(char *name);
Symbol *findIntrinsicFunction(char *name);
Symbol *findLabel(char *label);
Symbol *findSymbol(char *identifier);
void freeAllSymbols(void);
void generateLabel(char *label);
char *getNextChar(char *s);
char *getIdentifier(char *s, Token *token);
char *getNextToken(char *s, Token *token, bool doMatchKeywords);
DataType *getSymbolType(Symbol *sym);
void printSymbols(FILE *f);
void *reallocate(void *old, int oldSize, int newSize);
void registerIntrinsicFunctions(void);
void resetCommonBlocks(void);

extern bool doEchoSource;
extern bool doStaticLocalsDefault;
extern int lineNo;
extern FILE *listingFile;
extern FILE *objectFile;
extern Symbol *progUnitSym;
extern FILE *sourceFile;
extern char stmtBuf[];

#endif
