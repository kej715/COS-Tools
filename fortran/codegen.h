/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: codegen.h
**
**  Description:
**      This file provides function prototypes for the code generator.
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

#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "types.h"

#define RESULT_REG ((Register)7)

Register allocateRegister(void);
void emitActivateSection(char *name, char *type);
void emitAddInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitAddReal(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitAddReg(Register reg1, Register reg2, BaseType type);
void emitAndInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitAdjustSP(int delta);
void emitBranch(char *label);
void emitBranch3Way(Register reg, char *label1, char *label2, char *label3);
void emitBranchIfEndTrips(char *label);
void emitBranchIndexed(char *tableLabel, int tableSize, Register reg);
void emitBranchOnFalse(Register reg, char *label);
void emitBranchReg(Register reg);
void emitCalcTrip(Register init, Register lim, Register incr, BaseType type);
void emitCalcTrip1(Register init, Register lim, BaseType type);
void emitCatChar(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitDeactivateSection(char *name);
void emitDecrTrip(void);
void emitDivInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitDivIntReg(Register leftArg, Register rightArg);
void emitDivReal(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitDivRealReg(Register leftArg, Register rightArg);
void emitEnd(void);
void emitEpilog(Symbol *sym, int frameSize);
void emitEqChar(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitEqInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitEqLog(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitEqReal(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitEqvInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitFnCall(OperatorArgument *arg);
void emitGeChar(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitGeInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitGeLog(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitGeReal(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitGtChar(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitGtInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitGtLog(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitGtReal(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitIntToReal(OperatorArgument *arg);
void emitLabel(char *label);
void emitLabelDatum(char *label);
void emitLeChar(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitLeInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitLeLog(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitLeReal(OperatorArgument *leftArg, OperatorArgument *rightArg);
Register emitLoadByteAddr(char *label);
void emitLoadConst(OperatorArgument *arg);
Register emitLoadStack(int offset);
Register emitLoadStackAddr(int offset);
Register emitLoadStackByteAddr(int offset);
void emitLoadVar(OperatorArgument *arg);
Register emitLoadVarAddr(Symbol *sym);
Register emitLoadVarByteAddr(Symbol *sym);
void emitLtChar(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitLtInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitLtLog(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitLtReal(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitMulInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitMulReal(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitNeChar(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitNeInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitNeLog(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitNeReal(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitNeqvInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitOrInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitPrimCall(char *label);
void emitPopReg(Register reg);
void emitProlog(Symbol *sym);
void emitPushReg(Register reg);
void emitRealToInt(OperatorArgument *arg);
void emitRealToIntReg(Register arg);
void emitRestoreRegs(u8 mask);
void emitSaveRegs(u8 mask);
void emitStart(char *name);
void emitStoreArg(Symbol *sym, OperatorArgument *arg);
void emitStoreReg(Symbol *sym, Register reg);
void emitStoreStack(Register reg, int offset);
void emitString(char *s, char *label);
void emitSubInt(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitSubprogramCall(char *id);
void emitSubReal(OperatorArgument *leftArg, OperatorArgument *rightArg);
void emitWordLabel(char *label);
void enableEmission(bool isEnabled);
void freeAllRegisters(void);
void freeRegister(Register register);

#endif