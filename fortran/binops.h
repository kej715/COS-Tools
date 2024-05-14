#ifndef BINOPS_H
#define BINOPS_H
/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: binops.h
**
**  Description:
**      This file contains function prototype definitions for implementations
**      of binary operators.
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

#include "types.h"

void cstAddInt(OperatorArgument *left, OperatorArgument *right);
void cstAddReal(OperatorArgument *left, OperatorArgument *right);
void cstDivInt(OperatorArgument *left, OperatorArgument *right);
void cstDivReal(OperatorArgument *left, OperatorArgument *right);
void cstExpInt(OperatorArgument *left, OperatorArgument *right);
void cstExpReal(OperatorArgument *left, OperatorArgument *right);
void cstMulInt(OperatorArgument *left, OperatorArgument *right);
void cstMulReal(OperatorArgument *left, OperatorArgument *right);
void cstSubInt(OperatorArgument *left, OperatorArgument *right);
void cstSubReal(OperatorArgument *left, OperatorArgument *right);

void cstAndLog(OperatorArgument *left, OperatorArgument *right);
void cstAndInt(OperatorArgument *left, OperatorArgument *right);
void cstOrLog(OperatorArgument *left, OperatorArgument *right);
void cstOrInt(OperatorArgument *left, OperatorArgument *right);
void cstEqvLog(OperatorArgument *left, OperatorArgument *right);
void cstEqvInt(OperatorArgument *left, OperatorArgument *right);
void cstNeqvLog(OperatorArgument *left, OperatorArgument *right);
void cstNeqvInt(OperatorArgument *left, OperatorArgument *right);

void cstEqChar(OperatorArgument *left, OperatorArgument *right);
void cstEqLog(OperatorArgument *left, OperatorArgument *right);
void cstEqInt(OperatorArgument *left, OperatorArgument *right);
void cstEqReal(OperatorArgument *left, OperatorArgument *right);
void cstGeChar(OperatorArgument *left, OperatorArgument *right);
void cstGeLog(OperatorArgument *left, OperatorArgument *right);
void cstGeInt(OperatorArgument *left, OperatorArgument *right);
void cstGeReal(OperatorArgument *left, OperatorArgument *right);
void cstGtChar(OperatorArgument *left, OperatorArgument *right);
void cstGtLog(OperatorArgument *left, OperatorArgument *right);
void cstGtInt(OperatorArgument *left, OperatorArgument *right);
void cstGtReal(OperatorArgument *left, OperatorArgument *right);
void cstLeChar(OperatorArgument *left, OperatorArgument *right);
void cstLeLog(OperatorArgument *left, OperatorArgument *right);
void cstLeInt(OperatorArgument *left, OperatorArgument *right);
void cstLeReal(OperatorArgument *left, OperatorArgument *right);
void cstLtChar(OperatorArgument *left, OperatorArgument *right);
void cstLtLog(OperatorArgument *left, OperatorArgument *right);
void cstLtInt(OperatorArgument *left, OperatorArgument *right);
void cstLtReal(OperatorArgument *left, OperatorArgument *right);
void cstNeChar(OperatorArgument *left, OperatorArgument *right);
void cstNeLog(OperatorArgument *left, OperatorArgument *right);
void cstNeInt(OperatorArgument *left, OperatorArgument *right);
void cstNeReal(OperatorArgument *left, OperatorArgument *right);

void cstCatChar(OperatorArgument *left, OperatorArgument *right);

extern void (*cstBinOps[(OP_CAT-OP_ADD)+1][BaseType_Pointer+1])(OperatorArgument *left, OperatorArgument *right);
extern void (*genBinOps[(OP_CAT-OP_ADD)+1][BaseType_Pointer+1])(OperatorArgument *left, OperatorArgument *right);

#endif
