/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: global.c
**
**  Description:
**      This file contains declarations of global variables used by the
**      compiler.
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
#include "types.h"

int   autoOffset;
char  currentDate[9];
char  currentTime[9];
bool  doEchoSource          = FALSE;
bool  doSuppressWarnings    = FALSE;
bool  doList                = TRUE;
bool  doStaticLocals        = TRUE;
bool  doStaticLocalsDefault = TRUE;
int   errorCount            = 0;
int   lineNo                = 0;
FILE *listingFile           = NULL;
FILE *objectFile            = NULL;
FILE *sourceFile            = NULL;
char *sourcePath            = NULL;
int  staticOffset;
char stmtBuf[MAX_STMT_LENGTH+1];
int  totalErrors            = 0;
int warningCount            = 0;
