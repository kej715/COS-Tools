/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: global.c
**
**  Description:
**      This file defines global variables used by the assembler.
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
int baseStackPtr = 0;
char *calName = "CALX";
char *calVersion = "0.4";
int currentBase = 10;
Section *currentSection = NULL;
char currentDate[9];
char currentJDate[7];
u16 currentListControl = LIST_ON;
Module *currentModule = NULL;
Qualifier *currentQualifier = NULL;
SourceFormatType currentSourceFormat = SourceFormat_New;
char currentTime[9];
Module *defaultModule = NULL;
SourceFormatType defaultSourceFormat = SourceFormat_New;
u32 errorCount = 0;
u64 errorUnion = 0;
Module *firstModule = NULL;
Module *lastModule = NULL;
u16 listControlMask = LIST_ON;
u16 listControlStack[LIST_CONTROL_STACK_SIZE];
int listControlStackPtr = 0;
FILE *listingFile = NULL;
u32 locationCounter = 0;
Token *locationFieldToken = NULL;
char *locationField = NULL;
MacroCall *macroStack[MACRO_STACK_SIZE];
int macroStackPtr = 0;
Name *moduleNames = NULL;
Dataset *objectFile = NULL;
char *operandField = NULL;
char *osDate = "02/28/89";
char *osName = "COS 1.17";
u32 parcelCounter = 0;
int pass = 1;
Qualifier *qualifierStack[QUALIFIER_STACK_SIZE];
int qualifierStackPtr = 0;
char *resultField = NULL;
Section *sectionStack[BLOCK_STACK_SIZE];
int sectionStackPtr = 0;
FILE *sourceFile = NULL;
SourceFormatType sourceFormatStack[SOURCE_FORMAT_STACK_SIZE];
int sourceFormatStackPtr = 0;
char sourceLine[MAX_SOURCE_LINE_LENGTH+1];
char subtitle[MAX_TITLE_LENGTH+1];
char title[MAX_TITLE_LENGTH+1];
u32 warningCount = 0;
