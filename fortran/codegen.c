/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: codegen.c
**
**  Description:
**      This file contains functions that generate code for the
**      target machine.
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

#define DEBUG 0

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "const.h"
#include "fnv.h"
#include "proto.h"
#include "types.h"

static void emit(char *format, ...);
static void emitBranchTarget(char *label);
static void emitPopAddrReg(Register reg);
static void emitPushAddrReg(Register reg);
static void normalizeLabel(char *label, char *result);

static u8 addrRegMap = 0xE3;
static int emissionInhibitDepth = 0;
static Register lastReg = 0;
static u8 registerMap = 0x81;

Register allocateAddrReg(void) {
    u8 mask;
    Register reg;

    if (addrRegMap == 0xff) {
        fputs("All -A- registers allocated\n", stderr);
#if DEBUG
        emit("* All -A- registers allocated\n");
#endif
        printStackTrace(stderr);
        exit(1);
    }
    reg = 2;
    for (;;) {
        mask = 1 << reg;
        if ((addrRegMap & mask) == 0) {
            addrRegMap |= mask;
#if DEBUG
            emit("*  allocateAddrReg: A%o -> %02X%02X\n", reg, addrRegMap, registerMap);
#endif
            return reg;
        }
        reg = (reg + 1) & 0x07;
    }
}

Register allocateRegister(void) {
    u8 mask;

    if (registerMap == 0xff) {
        fputs("All -S- registers allocated\n", stderr);
#if DEBUG
        emit("* All -S- registers allocated\n");
#endif
        printStackTrace(stderr);
        exit(1);
    }
    for (;;) {
        mask = 1 << lastReg;
        if ((registerMap & mask) == 0) {
            registerMap |= mask;
#if DEBUG
            emit("* allocateRegister: S%o -> %02X%02X\n", lastReg, addrRegMap, registerMap);
#endif
            return lastReg;
        }
        lastReg = (lastReg + 1) & 0x07;
    }
}

void checkRegisterMap(void) {
    if (registerMap != 0x81) {
        emit("* -S- registers remain allocated, map is %02X\n", registerMap);
        fprintf(stderr, "WARNING: -S- registers remain allocated, map is %02X\n", registerMap);
    }
    if (addrRegMap != 0xE3) {
        emit("* -A- registers remain allocated, map is %02X\n", addrRegMap);
        fprintf(stderr, "WARNING: -A- registers remain allocated, map is %02X\n", addrRegMap);
    }
}

static void emit(char *format, ...) {
    va_list ap;

    if (objectFile != NULL && emissionInhibitDepth < 1) {
        va_start(ap, format);
        vfprintf(objectFile, format, ap);
        va_end(ap);
    }
}

void emitActivateSection(char *name, char *type) {
    emit("%-8s SECTION   %s\n", name, type);
}

void emitAddInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S%o        S%o+S%o\n", rightArg->reg, leftArg->reg, rightArg->reg);
}

void emitAddOffsets(Register reg1, Register reg2) {
    emit("         A%o        A%o+A%o\n", reg2, reg2, reg1);
}

void emitAddOffset(Register reg, int offset) {
    if (offset == 1) {
        emit("         A%o        A%o+1\n", reg, reg);
    }
    else if (offset == -1) {
        emit("         A%o        A%o-1\n", reg, reg);
    }
    else if (offset > 0) {
        emit("         A1        %d\n", offset);
        emit("         A%o        A%o+A1\n", reg, reg);
    }
    else if (offset < 0) {
        emit("         A1        %d\n", -offset);
        emit("         A%o        A%o-A1\n", reg, reg);
    }
}

void emitAddReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S%o        S%o+FS%o\n", rightArg->reg, leftArg->reg, rightArg->reg);
}

void emitAddReg(Register reg1, Register reg2, BaseType type) {
    switch (type) {
    case BaseType_Integer:
        emit("         S%o        S%o+S%o\n", reg1, reg1, reg2);
        break;
    case BaseType_Double:
    case BaseType_Real:
        emit("         S%o        S%o+FS%o\n", reg1, reg1, reg2);
        break;
    default:
       fprintf(stderr, "emitAddReg unexpected type: %d\n", type);
       exit(1);
    }
}

void emitAdjustSP(int delta) {
    if (delta < 0) {
        delta = -delta;
        if (delta < 3) {
            while (delta-- > 0) {
                emit("         A7        A7-1\n");
            }
        }
        else if (delta > 0) {
            emit("         A1        %d\n", delta);
            emit("         A7        A7-A1\n");
        }
    }
    else if (delta < 3) {
        while (delta-- > 0) {
            emit("         A7        A7+1\n");
        }
    }
    else if (delta > 0) {
        emit("         A1        %d\n", delta);
        emit("         A7        A7+A1\n");
    }
}

void emitAndInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S%o        S%o&S%o\n", rightArg->reg, leftArg->reg, rightArg->reg);
}

void emitBranch(char *label) {
    emit("         J         ");
    emitBranchTarget(label);
    emit("\n");
}

void emitBranch3Way(Register reg, char *label1, char *label2, char *label3) {
    if (reg != NO_REG) emit("         S0        S%o\n", reg);
    if (label1 != NULL) {
        emit("         JSM       ");
        emitBranchTarget(label1);
        emit("\n");
    }
    if (label3 != NULL) {
        emit("         JSN       ");
        emitBranchTarget(label3);
        emit("\n");
    }
    if (label2 != NULL) {
        emit("         JSZ       ");
        emitBranchTarget(label2);
        emit("\n");
    }
}

void emitBranchIfEndTrips(char *label) {
    emit("         S0        1,A7\n");
    emit("         JSZ       %s\n", label);
    emit("         JSM       %s\n", label);
}

void emitBranchIndexed(char *tableLabel, int tableSize, Register reg) {
    char endLabel[8];

    generateLabel(endLabel);
    emit("         A1        S%o\n", reg);
    emit("         A2        %d\n", tableSize);
    emit("         A0        A2-A1\n");
    emit("         JAM       %s\n", endLabel);
    emit("         A1        A1-1\n");
    emit("         A0        A1\n");
    emit("         JAM       %s\n", endLabel);
    emit("         A0        %s,A1\n", tableLabel);
    emit("         B00       A0\n");
    emit("         J         B00\n");
    emitLabel(endLabel);
}

void emitBranchOnFalse(Register reg, char *label) {
    if (reg != NO_REG) emit("         S0        S%o\n", reg);
    emit("         JSZ       %s\n", label);
}

void emitBranchReg(Register reg) {
    emit("         A0        S%o\n", reg);
    emit("         B00       A0\n");
    emit("         J         B00\n");
}

static void emitBranchTarget(char *label) {
    while (*label != '\0') {
        emit("%c", (*label == '_') ? '%' : *label);
        label += 1;
    }
}

/*
 *  emitCalcTrip - emit code to calculate initial trip count for DO loop
 *
 *  The formula for trip count is:
 *    (lim - init + incr) / incr
 *
 *  Result returned in incr
 */
void emitCalcTrip(Register init, Register lim, Register incr, BaseType type) {
    switch (type) {
    case BaseType_Integer:
        emit("         S%o        S%o-S%o\n", lim, lim, init);
        emit("         S%o        S%o+S%o\n", lim, lim, incr);
        emitDivIntReg(lim, incr);
        break;
    case BaseType_Double:
    case BaseType_Real:
        emit("         S%o        S%o-FS%o\n", lim, lim, init);
        emit("         S%o        S%o+FS%o\n", lim, lim, incr);
        emitDivRealReg(lim, incr);
        emitRealToIntReg(incr);
        break;
    default:
       fprintf(stderr, "emitCalcTrip unexpected type: %d\n", type);
       exit(1);
    }
}

/*
 *  emitCalcTrip1 - emit code to calculate initial trip count for DO loop
 *                  when increment is 1
 *
 *  The formula for trip count is:
 *    (lim - init + incr) / incr
 *
 *  Result returned in lim
 */
void emitCalcTrip1(Register init, Register lim, BaseType type) {
    switch (type) {
    case BaseType_Integer:
        emit("         S%o        S%o-S%o\n", lim, lim, init);
        emit("         S%o        1\n", init);
        emit("         S%o        S%o+S%o\n", lim, lim, init);
        break;
    case BaseType_Double:
    case BaseType_Real:
        emit("         S%o        S%o-FS%o\n", lim, lim, init);
        emit("         S%o        =1.0,\n", init);
        emit("         S%o        S%o+FS%o\n", lim, lim, init);
        emitRealToIntReg(lim);
        break;
    default:
       fprintf(stderr, "emitCalcTrip unexpected type: %d\n", type);
       exit(1);
    }
}

void emitCatChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u16 mask;

    mask = getRegisterMap() & ~((1 << rightArg->reg) | (1 << leftArg->reg));
    emitSaveRegs(mask);
    emitPushReg(rightArg->reg);
    emitPushReg(leftArg->reg);
    emitPrimCall("@_catstr");
    emitAdjustSP(2);
    if (rightArg->reg != RESULT_REG) emit("         S%o        S7\n", rightArg->reg);
    emitRestoreRegs(mask);
}

void emitConvertToByteAddress(Register reg) {
    emit("         S%o        S%o<3\n", reg, reg);
}

void emitCopyRegister(Register r1, Register r2) {
    emit("         S%o        S%o\n", r1, r2);
}

void emitCopyToOffset(Register r1, Register r2) {
    emit("         A%o        S%o\n", r1, r2);
}

void emitDeactivateSection(char *name) {
    emit("         SECTION   *\n");
}

void emitDecrTrip(void) {
    emit("         A1        1,A7\n");
    emit("         A1        A1-1\n");
    emit("         1,A7      A1\n");
}

void emitDivInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emitDivIntReg(leftArg->reg, rightArg->reg);
}

void emitDivIntReg(Register leftArg, Register rightArg) {
    u16 mask;

    mask = getRegisterMap() & ~((1 << rightArg) | (1 << leftArg));
    emitSaveRegs(mask);
    emitPushReg(leftArg);
    emitPushReg(rightArg);
    emit("         R         %%dvi\n");
    if (rightArg != RESULT_REG) emit("         S%o        S7\n", rightArg);
    emitRestoreRegs(mask);
}

void emitDivReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emitDivRealReg(leftArg->reg, rightArg->reg);
}

void emitDivRealReg(Register leftArg, Register rightArg) {
    u16 mask;

    mask = getRegisterMap() & ~((1 << rightArg) | (1 << leftArg));
    emitSaveRegs(mask);
    emitPushReg(leftArg);
    emitPushReg(rightArg);
    emit("         R         %%dvf\n");
    if (rightArg != RESULT_REG) emit("         S%o        S7\n", rightArg);
    emitRestoreRegs(mask);
}

void emitEnd(void) {
    emitDeactivateSection("TEXT");
    emit("         EDIT      *\n");
    emit("         END\n");
}

void emitEpilog(Symbol *sym, int frameSize, int staticDataSize) {
    if (sym->class != SymClass_BlockData) {
        emitLabel(sym->details.progUnit.exitLabel);
        if (sym->class == SymClass_Program) {
            emitPrimCall("@_endfio");
            emit("         S7        0\n");
        }
        else if (sym->class == SymClass_Function) {
            if (sym->details.progUnit.dt.type != BaseType_Character || sym->details.progUnit.dt.constraint == -1) {
                emit("         S7        %d,A6\n", sym->details.progUnit.offset);
            }
            else {
                emit("         A1        %d\n", sym->details.progUnit.offset);
                emit("         A1        A1+A6\n");
                emit("         S7        A1\n");
                emit("         S7        S7<3\n");
                emit("         S1        %d\n", sym->details.progUnit.dt.constraint);
                emit("         S1        S1<32\n");
                emit("         S7        S7!S1\n");
            }
        }
        emit("         A7        A6\n");
        emit("         A0        ,A7\n");
        emit("         A7        A7+1\n");
        emit("         B00       A0\n");
        emit("         A6        ,A7\n");
        emit("         A7        A7+1\n");
        emit("         J         B00\n");
        emitActivateSection("DATA", "DATA");
        emit("%-8s CON       %d\n", sym->details.progUnit.frameSizeLabel, frameSize);
        emitDeactivateSection("DATA");
    }
    if (staticDataSize > 0) {
        emitActivateSection("DATA", "DATA");
        emitWordBlock(sym->details.progUnit.staticDataLabel, staticDataSize);
        emitDeactivateSection("DATA");
    }
}

void emitEqChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u16 mask;

    mask = getRegisterMap() & ~((1 << rightArg->reg) | (1 << leftArg->reg));
    emitSaveRegs(mask);
    emitPushReg(rightArg->reg);
    emitPushReg(leftArg->reg);
    emitPrimCall("@_cmpstr");
    emitAdjustSP(2);
    emitRestoreRegs(mask);
    emit("         S0        S7\n");
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSZ       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitEqInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-S%o\n", leftArg->reg, rightArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSZ       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitEqLog(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o\\S%o\n", leftArg->reg, rightArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSZ       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitEqReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-FS%o\n", leftArg->reg, rightArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSZ       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitEqvInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S%o        S%o\\S%o\n", rightArg->reg, leftArg->reg, rightArg->reg);
    emit("         S%o        #S%o\n", rightArg->reg, rightArg->reg);
}

void emitExit(int status) {
    emit("         S%o        %d\n", RESULT_REG, status);
    emitPushReg(RESULT_REG);
    emitPrimCall("@exit");
}

void emitExpInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u16 mask;

    mask = getRegisterMap() & ~((1 << rightArg->reg) | (1 << leftArg->reg));
    emitSaveRegs(mask);
    emitPushReg(leftArg->reg);
    emitPushReg(rightArg->reg);
    emit("         R         %%cif\n");
    emit("         S1        ,A7\n");
    emit("         ,A7       S7\n");
    emit("         A7        A7-1\n");
    emit("         ,A7       S1\n");
    emit("         R         %%cif\n");
    emitPushReg(RESULT_REG);
    emitPrimCall("@pow");
    emitAdjustSP(2);
    emitPushReg(RESULT_REG);
    emit("         R         %%cfi\n");
    if (rightArg->reg != RESULT_REG) emit("         S%o        S7\n", rightArg->reg);
    emitRestoreRegs(mask);
}

void emitExpReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u16 mask;

    mask = getRegisterMap() & ~((1 << rightArg->reg) | (1 << leftArg->reg));
    emitSaveRegs(mask);
    emitPushReg(rightArg->reg);
    emitPushReg(leftArg->reg);
    emitPrimCall("@pow");
    emitAdjustSP(2);
    if (rightArg->reg != RESULT_REG) emit("         S%o        S7\n", rightArg->reg);
    emitRestoreRegs(mask);
}

void emitGeChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u16 mask;

    mask = getRegisterMap() & ~((1 << rightArg->reg) | (1 << leftArg->reg));
    emitSaveRegs(mask);
    emitPushReg(rightArg->reg);
    emitPushReg(leftArg->reg);
    emitPrimCall("@_cmpstr");
    emitAdjustSP(2);
    emitRestoreRegs(mask);
    emit("         S0        S7\n");
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSP       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitGeInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-S%o\n", leftArg->reg, rightArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSP       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitGeLog(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o\n", rightArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSZ       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitGeReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-FS%o\n", leftArg->reg, rightArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSP       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitGtChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u16 mask;

    mask = getRegisterMap() & ~((1 << rightArg->reg) | (1 << leftArg->reg));
    emitSaveRegs(mask);
    emitPushReg(leftArg->reg);
    emitPushReg(rightArg->reg);
    emitPrimCall("@_cmpstr");
    emitAdjustSP(2);
    emitRestoreRegs(mask);
    emit("         S0        S7\n");
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSM       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitGtInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-S%o\n", rightArg->reg, leftArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSM       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitGtLog(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-S%o\n", rightArg->reg, leftArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSP       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitGtReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-FS%o\n", rightArg->reg, leftArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSM       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitIntToReal(OperatorArgument *arg) {
    u16 mask;

    mask = getRegisterMap() & ~(1 << arg->reg);
    emitSaveRegs(mask);
    emitPushReg(arg->reg);
    emit("         R         %%cif\n");
    if (arg->reg != RESULT_REG) emit("         S%o        S7\n", arg->reg);
    emitRestoreRegs(mask);
}

void emitLabel(char *label) {
    emit("%-8s =         *\n", label);
}

void emitLabelDatum(char *label) {
    emit("         DATA      %s\n", label);
}

void emitLabeledString(CharacterValue *cvp, char *label, bool hasZByte) {
    CharacterValue cv;
    int len;

    emitActivateSection("DATA", "DATA");
    if (label != NULL) emitWordLabel(label);
    cv.string = cvp->string;
    len = cvp->length;
    while (len > 0) {
        emit("         DATA      ");
        if (len > 16) {
            cv.length = 16;
            emitString(&cv, FALSE);
            cv.string += 16;
            len -= 16;
        }
        else {
            cv.length = len;
            emitString(&cv, hasZByte);
            len = 0;
        }
        emit("\n");
    }
    emitDeactivateSection("DATA");
}

Register emitLabelReference(Symbol *sym) {
    Register reg;

    reg = allocateRegister();
    emit("         S%o        %s\n", reg, sym->details.label.label);
    return reg;
}

void emitLeChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u16 mask;

    mask = getRegisterMap() & ~((1 << rightArg->reg) | (1 << leftArg->reg));
    emitSaveRegs(mask);
    emitPushReg(leftArg->reg);
    emitPushReg(rightArg->reg);
    emitPrimCall("@_cmpstr");
    emitAdjustSP(2);
    emitRestoreRegs(mask);
    emit("         S0        S7\n");
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSP       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitLeInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-S%o\n", rightArg->reg, leftArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSP       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitLeLog(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o\n", leftArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSZ       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitLeReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-FS%o\n", rightArg->reg, leftArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSP       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitLoadByteReference(OperatorArgument *subject, OperatorArgument *object) {
    DataType *dt;

    dt = getSymbolType(subject->details.reference.symbol);
    emitLoadReference(subject, object);
    if (dt->type != BaseType_Character) {
        emit("         S%o        S%o<3\n", subject->reg, subject->reg);
    }
}

void emitLoadConst(OperatorArgument *arg) {
    char buf[32];
    char *cp;
    DataType dt;
    f64 f;
    i64 i;
    u64 l;

    arg->reg = allocateRegister();
    dt = arg->details.constant.dt;
    switch (arg->details.constant.dt.type) {
    case BaseType_Character:
        if (arg->details.constant.value.character.length <= 40) {
            emit("         S%o        =", arg->reg);
            emitString(&arg->details.constant.value.character, FALSE);
            emit("\n");
        }
        else {
            generateLabel(buf);
            emitLabeledString(&arg->details.constant.value.character, buf, FALSE);
            emit("         S%o        %s\n", arg->reg, buf);
        }
        emit("         S%o        S%o<3\n", arg->reg, arg->reg);
        emit("         S7        %d\n", arg->details.constant.value.character.length);
        emit("         S7        S7<32\n");
        emit("         S%o        S%o!S7\n", arg->reg, arg->reg);
        break;
    case BaseType_Logical:
        l = arg->details.constant.value.logical;
        if (l == 0)
            emit("         S%o        0\n", arg->reg);
        else if (l == ~0)
            emit("         S%o        -1\n", arg->reg);
        else if (l <= 017777777)
            emit("         S%o        %ld\n", arg->reg, l);
        else
            emit("         S%o        =%ld,\n", arg->reg, l);
        break;
    case BaseType_Integer:
        i = arg->details.constant.value.integer;
        if (i >= 0 && i <= 07777777) {
            emit("         S%o        %ld\n", arg->reg, i);
        }
        else if (i < 0 && i >= -010000000) {
            emit("         S%o        %ld\n", arg->reg, i);
        }
        else
            emit("         S%o        =%ld,\n", arg->reg, i);
        break;
    case BaseType_Double:
    case BaseType_Real:
        f = arg->details.constant.value.real;
        if (f >= 1.0E-5 && f <= 1.0E+9) {
            sprintf(buf, "%f", f);
        }
        else {
            sprintf(buf, "%g", f);
        }
        emit("         S%o        =", arg->reg);
        for (cp = buf; *cp == ' '; cp++)
             ;
        emit("%s,\n", cp);
        break;
    case BaseType_Pointer:
        emit("         S%o        O'%lo\n", arg->reg, arg->details.constant.value.integer);
        break;
    default:
        fprintf(stderr, "emitLoadConst: Invalid type: %d\n", arg->details.constant.dt.type);
        exit(1);
    }
    arg->class = ArgClass_Calculation;
    arg->details.calculation = dt;
}

void emitLoadNullPtr(OperatorArgument *arg) {
    arg->reg = allocateRegister();
    emit("         S%o        0\n", arg->reg);
}

void emitLoadReference(OperatorArgument *subject, OperatorArgument *object) {
    DataType *dt;
    u16 mask;
    Symbol *sym;

    subject->reg = allocateRegister();
    sym = subject->details.reference.symbol;
    dt = getSymbolType(sym);
    if (dt->type == BaseType_Character) {
        switch (sym->class) {
        case SymClass_Auto:
            emit("         S%o        %d\n", subject->reg, sym->details.variable.offset);
            emit("         S7        A6\n");
            emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
            emit("         S%o        S%o<3\n", subject->reg, subject->reg);
            switch (subject->details.reference.offsetClass) {
            case ArgClass_Undefined:
                /* do nothing */
                break;
            case ArgClass_Constant:
                emit("         S7        %d\n", subject->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                break;
            case ArgClass_Calculation:
                emit("         S7        A%o\n", subject->details.reference.offset.reg);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                freeAddrReg(subject->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, subject->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Static:
            emit("         S%o        %s+%d\n", subject->reg, progUnitSym->details.progUnit.staticDataLabel, sym->details.variable.offset);
            emit("         S%o        S%o<3\n", subject->reg, subject->reg);
            switch (subject->details.reference.offsetClass) {
            case ArgClass_Undefined:
                /* do nothing */
                break;
            case ArgClass_Constant:
                emit("         S7        %d\n", subject->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                break;
            case ArgClass_Calculation:
                emit("         S7        A%o\n", subject->details.reference.offset.reg);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                freeAddrReg(subject->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, subject->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Argument:
            emit("         S%o        %d,A6\n", subject->reg, sym->details.variable.offset);
            switch (subject->details.reference.offsetClass) {
            case ArgClass_Undefined:
                /* do nothing */
                break;
            case ArgClass_Constant:
                emit("         S7        %d\n", subject->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                break;
            case ArgClass_Calculation:
                emit("         S7        A%o\n", subject->details.reference.offset.reg);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                freeAddrReg(subject->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, subject->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Function:
            if (dt->constraint == -1) {
                if (object == NULL) {
                    fprintf(stderr, "No reference object for assumed-size %s\n", sym->identifier);
                    exit(1);
                }
                freeRegister(subject->reg);
                mask = getRegisterMap();
                emitSaveRegs(mask);
                emit("         S%o        S%o>32\n", object->reg, object->reg);
                emitPushReg(object->reg);
                emitPrimCall("@_getstr");
                emitAdjustSP(1);
                emitRestoreRegs(mask);
                emitStoreReg(sym, RESULT_REG);
                subject->reg = allocateRegister();
                emit("         S%o        S7\n", subject->reg);
            }
            else {
                emit("         S%o        %d\n", subject->reg, sym->details.progUnit.offset);
                emit("         S7        A6\n");
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                emit("         S%o        S%o<3\n", subject->reg, subject->reg);
            }
            switch (subject->details.reference.offsetClass) {
            case ArgClass_Undefined:
                /* do nothing */
                break;
            case ArgClass_Constant:
                emit("         S7        %d\n", subject->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                break;
            case ArgClass_Calculation:
                emit("         S7        A%o\n", subject->details.reference.offset.reg);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                freeAddrReg(subject->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, subject->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Global:
            emit("         S%o        %s+%d\n", subject->reg, sym->details.variable.staticBlock->details.common.label, sym->details.variable.offset);
            emit("         S%o        S%o<3\n", subject->reg, subject->reg);
            switch (subject->details.reference.offsetClass) {
            case ArgClass_Undefined:
                /* do nothing */
                break;
            case ArgClass_Constant:
                emit("         S7        %d\n", subject->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                break;
            case ArgClass_Calculation:
                emit("         S7        A%o\n", subject->details.reference.offset.reg);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                freeAddrReg(subject->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, subject->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Pointee:
        default:
            fprintf(stderr, "Invalid class for load request: %d\n", sym->class);
            exit(1);
        }
        if (sym->class != SymClass_Argument && dt->constraint != -1) {
            emit("         S7        %d\n", dt->constraint);
            emit("         S7        S7<32\n");
            emit("         S%o        S%o!S7\n", subject->reg, subject->reg);
        }
    }
    else { // dt->type != BaseType_Character
        switch (sym->class) {
        case SymClass_Auto:
            switch (subject->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S7        A6\n");
                emit("         S%o        %d\n", subject->reg, sym->details.variable.offset);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                break;
            case ArgClass_Constant:
                emit("         S7        A6\n");
                emit("         S%o        %d\n", subject->reg, sym->details.variable.offset + subject->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                break;
            case ArgClass_Calculation:
                emit("         A%o        A%o+A6\n", subject->details.reference.offset.reg, subject->details.reference.offset.reg);
                if (sym->details.variable.offset == 1) {
                    emit("         A%o        A%o+1\n", subject->details.reference.offset.reg, subject->details.reference.offset.reg);
                }
                else if (sym->details.variable.offset == -1) {
                    emit("         A%o        A%o-1\n", subject->details.reference.offset.reg, subject->details.reference.offset.reg);
                }
                else if (sym->details.variable.offset > 0) {
                    emit("         A1        %d\n", sym->details.variable.offset);
                    emit("         A%o        A%o+A1\n", subject->details.reference.offset.reg, subject->details.reference.offset.reg);
                }
                else if (sym->details.variable.offset < 0) {
                    emit("         A1        %d\n", -sym->details.variable.offset);
                    emit("         A%o        A%o-A1\n", subject->details.reference.offset.reg, subject->details.reference.offset.reg);
                }
                emit("         S%o        A%o\n", subject->reg, subject->details.reference.offset.reg);
                freeAddrReg(subject->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, subject->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Static:
            switch (subject->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S%o        %s+%d\n", subject->reg, progUnitSym->details.progUnit.staticDataLabel, sym->details.variable.offset);
                break;
            case ArgClass_Constant:
                emit("         S%o        %s+%d\n", subject->reg, progUnitSym->details.progUnit.staticDataLabel, sym->details.variable.offset + subject->details.reference.offset.constant);
                break;
            case ArgClass_Calculation:
                emit("         A1        %s+%d\n", progUnitSym->details.progUnit.staticDataLabel, sym->details.variable.offset);
                emit("         A%o        A%o+A1\n", subject->details.reference.offset.reg, subject->details.reference.offset.reg);
                emit("         S%o        A%o\n", subject->reg, subject->details.reference.offset.reg);
                freeAddrReg(subject->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, subject->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Argument:
            emit("         S%o        %d,A6\n", subject->reg, sym->details.variable.offset);
            switch (subject->details.reference.offsetClass) {
            case ArgClass_Undefined:
                /* do nothing */
                break;
            case ArgClass_Constant:
                emit("         S7        %d\n", subject->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                break;
            case ArgClass_Calculation:
                emit("         S7        A%o\n", subject->details.reference.offset.reg);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                freeAddrReg(subject->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, subject->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Function:
            switch (subject->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S7        A6\n");
                emit("         S%o        %d\n", subject->reg, sym->details.progUnit.offset);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                break;
            case ArgClass_Constant:
                emit("         S7        A6\n");
                emit("         S%o        %d\n", subject->reg, sym->details.progUnit.offset + subject->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", subject->reg, subject->reg);
                break;
            case ArgClass_Calculation:
                emit("         A%o        A%o+A6\n", subject->details.reference.offset.reg, subject->details.reference.offset.reg);
                if (sym->details.progUnit.offset == 1) {
                    emit("         A%o        A%o+1\n", subject->details.reference.offset.reg, subject->details.reference.offset.reg);
                }
                else if (sym->details.progUnit.offset == -1) {
                    emit("         A%o        A%o-1\n", subject->details.reference.offset.reg, subject->details.reference.offset.reg);
                }
                else if (sym->details.progUnit.offset > 0) {
                    emit("         A1        %d\n", sym->details.progUnit.offset);
                    emit("         A%o        A%o+A1\n", subject->details.reference.offset.reg, subject->details.reference.offset.reg);
                }
                else if (sym->details.progUnit.offset < 0) {
                    emit("         A1        %d\n", -sym->details.progUnit.offset);
                    emit("         A%o        A%o-A1\n", subject->details.reference.offset.reg, subject->details.reference.offset.reg);
                }
                emit("         S%o        A%o\n", subject->reg, subject->details.reference.offset.reg);
                freeAddrReg(subject->details.reference.offset.reg);

                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, subject->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Global:
            switch (subject->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S%o        %s+%d\n", subject->reg, sym->details.variable.staticBlock->details.common.label, sym->details.variable.offset);
                break;
            case ArgClass_Constant:
                emit("         S%o        %s+%d\n", subject->reg, sym->details.variable.staticBlock->details.common.label, sym->details.variable.offset + subject->details.reference.offset.constant);
                break;
            case ArgClass_Calculation:
                emit("         A1        %s+%d\n", sym->details.variable.staticBlock->details.common.label, sym->details.variable.offset);
                emit("         A%o        A%o+A1\n", subject->details.reference.offset.reg, subject->details.reference.offset.reg);
                emit("         S%o        A%o\n", subject->reg, subject->details.reference.offset.reg);
                freeAddrReg(subject->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, subject->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Pointee:
        default:
            fprintf(stderr, "Invalid class for load request: %d\n", sym->class);
            exit(1);
        }
    }
    subject->class = ArgClass_Calculation;
    subject->details.calculation = sym->details.variable.dt;
}

Register emitLoadStack(int offset) {
    Register reg;

    reg = allocateRegister();
    emit("         S%o        %d,A7\n", reg, offset);
    return reg;
}

Register emitLoadStackAddr(int offset) {
    Register reg;

    reg = allocateRegister();
    emit("         S%o        A7\n", reg);
    if (offset > 0) {
        emit("         S7        %d\n", offset);
        emit("         S%o        S%o+S7\n", reg, reg);
    }
    else if (offset < 0) {
        emit("         S7        %d\n", -offset);
        emit("         S%o        S%o-S7\n", reg, reg);
    }
    return reg;
}

Register emitLoadStackByteAddr(int offset) {
    Register reg;

    reg = emitLoadStackAddr(offset);
    emit("         S%o        S%o<3\n", reg, reg);
    return reg;
}

void emitLoadValue(OperatorArgument *arg) {
    DataType *dt;
    Symbol *sym;

    sym = arg->details.reference.symbol;
    dt = getSymbolType(sym);
    if (dt->type == BaseType_Character) {
        emitLoadReference(arg, NULL);
    }
    else {
        arg->reg = allocateRegister();
        switch (sym->class) {
        case SymClass_Auto:
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S%o        %d,A6\n", arg->reg, sym->details.variable.offset);
                break;
            case ArgClass_Constant:
                emit("         S%o        %d,A6\n", arg->reg, sym->details.variable.offset + arg->details.reference.offset.constant);
                break;
            case ArgClass_Calculation:
                emit("         S%o        %d,A%o\n", arg->reg, sym->details.variable.offset, arg->details.reference.offset.reg);
                freeAddrReg(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Static:
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S%o        %s+%d,\n", arg->reg, progUnitSym->details.progUnit.staticDataLabel, sym->details.variable.offset);
                break;
            case ArgClass_Constant:
                emit("         S%o        %s+%d,\n", arg->reg, progUnitSym->details.progUnit.staticDataLabel, sym->details.variable.offset + arg->details.reference.offset.constant);
                break;
            case ArgClass_Calculation:
                emit("         S%o        %s+%d,A%o\n", arg->reg, progUnitSym->details.progUnit.staticDataLabel, sym->details.variable.offset, arg->details.reference.offset.reg);
                freeAddrReg(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Argument:
            emit("         A1        %d,A6\n", sym->details.variable.offset);
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S%o        ,A1\n", arg->reg);
                break;
            case ArgClass_Constant:
                emit("         S%o        %d,A1\n", arg->reg, arg->details.reference.offset.constant);
                break;
            case ArgClass_Calculation:
                emit("         A1        A1+A%o\n", arg->details.reference.offset.reg);
                emit("         S%o        ,A1\n", arg->reg);
                freeAddrReg(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Function:
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S%o        %d,A6\n", arg->reg, sym->details.progUnit.offset);
                break;
            case ArgClass_Constant:
                emit("         S%o        %d,A6\n", arg->reg, sym->details.progUnit.offset + arg->details.reference.offset.constant);
                break;
            case ArgClass_Calculation:
                emit("         A1        A1+A%o\n", arg->details.reference.offset.reg);
                emit("         S%o        %d,A1\n", arg->reg, sym->details.progUnit.offset);
                freeAddrReg(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Global:
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S%o        %s+%d,\n", arg->reg, sym->details.variable.staticBlock->details.common.label, sym->details.variable.offset);
                break;
            case ArgClass_Constant:
                emit("         S%o        %s+%d,\n", arg->reg, sym->details.variable.staticBlock->details.common.label, sym->details.variable.offset + arg->details.reference.offset.constant);
                break;
            case ArgClass_Calculation:
                emit("         S%o        %s+%d,A%o\n", arg->reg, sym->details.variable.staticBlock->details.common.label, sym->details.variable.offset, arg->details.reference.offset.reg);
                freeAddrReg(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Pointee:
        default:
            fprintf(stderr, "Invalid class for load request: %d\n", sym->class);
            exit(1);
        }
        arg->class = ArgClass_Calculation;
        arg->details.calculation = *dt;
    }
}

Register emitLoadZStrAddr(char *label) {
    Register reg;

    reg = allocateRegister();
    emit("         S%o        %s\n", reg, label);
    emitPushReg(reg);
    emitPrimCall("@strlen");
    emitAdjustSP(1);
    emit("         S%o        %s\n", reg, label);
    emit("         S%o        S%o<3\n", reg, reg);
    emit("         S7        S7<32\n");
    emit("         S%o        S%o!S7\n", reg, reg);
    return reg;
}

void emitLtChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u16 mask;

    mask = getRegisterMap() & ~((1 << rightArg->reg) | (1 << leftArg->reg));
    emitSaveRegs(mask);
    emitPushReg(rightArg->reg);
    emitPushReg(leftArg->reg);
    emitPrimCall("@_cmpstr");
    emitAdjustSP(2);
    emitRestoreRegs(mask);
    emit("         S0        S7\n");
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSM       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitLtInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-S%o\n", leftArg->reg, rightArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSM       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitLtLog(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-S%o\n", leftArg->reg, rightArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSP       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitLtReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-FS%o\n", leftArg->reg, rightArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSM       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitMulInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u16 mask;

    mask = getRegisterMap() & ~((1 << rightArg->reg) | (1 << leftArg->reg));
    emitSaveRegs(mask);
    emitPushReg(leftArg->reg);
    emitPushReg(rightArg->reg);
    emit("         R         %%mli\n");
    if (rightArg->reg != RESULT_REG) emit("         S%o        S7\n", rightArg->reg);
    emitRestoreRegs(mask);
}

void emitMulOffset(Register reg, int factor) {
    emit("         A1        %d\n", factor);
    emit("         A%o        A%o*A1\n", reg, reg);
}

void emitMulReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S%o        S%o*FS%o\n", rightArg->reg, leftArg->reg, rightArg->reg);
}

void emitNeChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u16 mask;

    mask = getRegisterMap() & ~((1 << rightArg->reg) | (1 << leftArg->reg));
    emitSaveRegs(mask);
    emitPushReg(rightArg->reg);
    emitPushReg(leftArg->reg);
    emitPrimCall("@_cmpstr");
    emitAdjustSP(2);
    emitRestoreRegs(mask);
    emit("         S0        S7\n");
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSN       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitNegReg(Register reg, BaseType type) {
    switch (type) {
    case BaseType_Integer:
        emit("         S%o        -S%o\n", reg, reg);
        break;
    case BaseType_Double:
    case BaseType_Real:
        emit("         S%o        -FS%o\n", reg, reg);
        break;
    case BaseType_Logical:
        emit("         S%o        #S%o\n", reg, reg);
        break;
    default:
       fprintf(stderr, "emitNegReg unexpected type: %d\n", type);
       exit(1);
    }
}

void emitNeInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-S%o\n", leftArg->reg, rightArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSN       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitNeLog(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o\\S%o\n", leftArg->reg, rightArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSN       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitNeReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S0        S%o-FS%o\n", leftArg->reg, rightArg->reg);
    emit("         S%o        <64\n", rightArg->reg);
    emit("         JSN       *+3\n");
    emit("         S%o        0\n", rightArg->reg);
}

void emitNeqvInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S%o        S%o\\S%o\n", rightArg->reg, leftArg->reg, rightArg->reg);
}

void emitNotReg(Register reg, BaseType type) {
    switch (type) {
    case BaseType_Integer:
    case BaseType_Logical:
        emit("         S%o        #S%o\n", reg, reg);
        break;
    default:
       fprintf(stderr, "emitNegReg unexpected type: %d\n", type);
       exit(1);
    }
}

void emitOrInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S%o        S%o!S%o\n", rightArg->reg, leftArg->reg, rightArg->reg);
}

static void emitPopAddrReg(Register reg) {
    emit("         A%o        ,A7\n", reg);
    emit("         A7        A7+1\n");
}

void emitPopReg(Register reg) {
    emit("         S%o        ,A7\n", reg);
    emit("         A7        A7+1\n");
}

void emitPrimCall(char *label) {
    emit("         R         ");
    emitBranchTarget(label);
    emit("\n");
}

void emitProlog(Symbol *sym) {
    char buf[32];

    generateLabel(sym->details.progUnit.staticDataLabel);
    if (sym->class == SymClass_BlockData) {
        return;
    }
    else if (sym->class == SymClass_Program) {
        emit("         ENTRY     @main\n");
        emit("@main    BSS       0\n");
    }
    else {
        normalizeLabel(sym->identifier, buf);
        emit("         ENTRY     %s\n", buf);
        emit("%-8s BSS       0\n", buf);
    }
    generateLabel(sym->details.progUnit.exitLabel);
    generateLabel(sym->details.progUnit.frameSizeLabel);
    emit("         A7        A7-1\n");  /* push base pointer    */
    emit("         ,A7       A6\n");
    emit("         A6        B00\n");   /* push return address  */
    emit("         A7        A7-1\n");
    emit("         ,A7       A6\n");
    emit("         A6        A7\n");    /* set new base pointer */
    emit("         A1        %s,\n", sym->details.progUnit.frameSizeLabel);
    emit("         A7        A7-A1\n"); /* reserve space for local variables */
    if (sym->class == SymClass_Program) {
        emitPrimCall("@_inifio");
    }
}

static void emitPushAddrReg(Register reg) {
    emit("         A7        A7-1\n");
    emit("         ,A7       A%o\n", reg);
}

void emitPushReg(Register reg) {
    emit("         A7        A7-1\n");
    emit("         ,A7       S%o\n", reg);
}

void emitRealToInt(OperatorArgument *arg) {
    emitRealToIntReg(arg->reg);
}

void emitRealToIntReg(Register arg) {
    u16 mask;

    mask = getRegisterMap() & ~(1 << arg);
    emitSaveRegs(mask);
    emitPushReg(arg);
    emit("         R         %%cfi\n");
    if (arg != RESULT_REG) emit("         S%o        S7\n", arg);
    emitRestoreRegs(mask);
}

void emitRestoreRegs(u16 mask) {
    u16 mask2;
    u8 reg;
    u8 selector;

#if DEBUG
    emit("* restoreRegisters: mask %04X map %02X%02X\n", mask, addrRegMap, registerMap);
#endif
    mask2 = mask >> 8;
    for (reg = 4; reg > 1; reg--) {
        selector = 1 << reg;
        if ((mask2 & selector) != 0) {
            emitPopAddrReg(reg);
            addrRegMap |= selector;
        }
    }
    for (reg = 6; reg > 0; reg--) {
        selector = 1 << reg;
        if ((mask & selector) != 0) {
            emitPopReg(reg);
            registerMap |= selector;
        }
    }
#if DEBUG
    emit("* restoreRegisters:            -> %02X%02X\n", addrRegMap, registerMap);
#endif
}

void emitSaveRegs(u16 mask) {
    u16 mask2;
    u8 reg;
    u8 selector;

#if DEBUG
    emit("*    saveRegisters: mask %04X map %02X%02X\n", mask, addrRegMap, registerMap);
#endif
    for (reg = 1; reg < 7; reg++) {
        selector = 1 << reg;
        if ((mask & selector) != 0) {
            emitPushReg(reg);
            registerMap &= ~selector;
        }
    }
    mask2 = mask >> 8;
    for (reg = 2; reg < 5; reg++) {
        selector = 1 << reg;
        if ((mask2 & selector) != 0) {
            emitPushAddrReg(reg);
            addrRegMap &= ~selector;
        }
    }
#if DEBUG
    emit("*    saveRegisters:            -> %02X%02X\n", addrRegMap, registerMap);
#endif
}

void emitStart(char *name) {
    emit("         IDENT     %s\n", name);
    emit("         EDIT      OFF\n");
    emitActivateSection("TEXT", "CODE");
}

void emitStaticInitializers(DataInitializerItem *dList, ConstantListItem *cList) {
    int charOffset;
    ConstantListItem *cListItem;
    DataInitializerItem *dListItem;
    int fieldLen;
    int len;
    char *s;
    int sLen;
    int wordOffset;

    dListItem = dList;
    cListItem = cList;
    while (dListItem != NULL) {
        emitActivateSection(dListItem->blockName, dListItem->blockType);
        wordOffset = dListItem->blockOffset;
        if (dListItem->type == BaseType_Character) {
            wordOffset += dListItem->elementOffset / 8;
            charOffset = dListItem->charOffset;
        }
        else {
            wordOffset += dListItem->elementOffset;
        }
        emit("         ORG       %s+%d\n", dListItem->blockLabel, wordOffset);
        while (dListItem->elementCount-- > 0) {
            switch (dListItem->type) {
            case BaseType_Character:
                s = cListItem->details.value.character.string;
                sLen = cListItem->details.value.character.length;
                len = dListItem->charLength;
                if (charOffset > 0) {
                    emit("         BITW      %d\n", charOffset * 8);
                }
                fieldLen = 8 - charOffset;
                if (fieldLen > len) fieldLen = len;
                while (len > 0) {
                    len -= fieldLen;
                    if (fieldLen < 8) {
                        emit("         VWD       %d/'", fieldLen * 8);
                    }
                    else {
                        emit("         DATA      '");
                    }
                    while (fieldLen > 0 && sLen > 0) {
                        if (*s == '\'')
                            emit("''");
                        else
                            emit("%c", *s);
                        s += 1;
                        fieldLen -= 1;
                        sLen -= 1;
                    }
                    while (fieldLen-- > 0) emit(" ");
                    emit("'\n");
                    fieldLen = (len < 8) ? len : 8;
                }
                break;
            case BaseType_Logical:
                emit("         CON       %d\n", cListItem->details.value.logical);
                break;
            case BaseType_Integer:
                emit("         CON       %d\n", cListItem->details.value.integer);
                break;
            case BaseType_Double:
            case BaseType_Real:
                emit("         CON       %f\n", cListItem->details.value.real);
                break;
            case BaseType_Complex:
            default:
                break;
            }
            cListItem->repeatCount -= 1;
            if (cListItem->repeatCount < 1) {
                cListItem = cListItem->next;
            }
        }
        emitDeactivateSection(dListItem->blockName);
        dListItem = dListItem->next;
    }
}

void emitStoreArg(Symbol *sym, OperatorArgument *arg) {
    emitStoreReg(sym, arg->reg);
}

void emitStoreByReference(OperatorArgument *target, OperatorArgument *value) {
    if (target->details.calculation.type == BaseType_Character) {
        emitPushReg(value->reg);
        emitPushReg(target->reg);
        emitPrimCall("@_cpystr");
        emitAdjustSP(2);
    }
    else {
        emit("         A1        S%o\n", target->reg);
        emit("         ,A1       S%o\n", value->reg);
    }
}

void emitStoreReg(Symbol *sym, Register reg) {
    char buf[32];

    switch (sym->class) {
    case SymClass_Auto:
        sprintf(buf, "%d,A6", sym->details.variable.offset);
        emit("         %-9s S%o\n", buf, reg);
        break;
    case SymClass_Static:
        sprintf(buf, "%s+%d,", progUnitSym->details.progUnit.staticDataLabel, sym->details.variable.offset);
        emit("         %-9s S%o\n", buf, reg);
        break;
    case SymClass_Argument:
        emit("         A1        %d,A6\n", sym->details.variable.offset);
        emit("         ,A1       S%o\n", reg);
        break;
    case SymClass_Function:
        sprintf(buf, "%d,A6", sym->details.progUnit.offset);
        emit("         %-9s S%o\n", buf, reg);
        break;
    case SymClass_Global:
        sprintf(buf, "%s+%d,", sym->details.variable.staticBlock->details.common.label, sym->details.variable.offset);
        emit("         %-9s S%o\n", buf, reg);
        break;
    case SymClass_Parameter:
        sprintf(buf, "%s,", sym->identifier);
        emit("         %-9s S%o\n", buf, reg);
        break;
    case SymClass_Pointee:
    default:
        fprintf(stderr, "Invalid class for store request: %d\n", sym->class);
        exit(1);
    }
}

void emitStoreRegByReference(OperatorArgument *target, Register reg) {
    emit("         A1        S%o\n", target->reg);
    emit("         ,A1       S%o\n", reg);
}

void emitStoreStack(Register reg, int offset) {
    char buf[16];

    sprintf(buf, "%d,A7", offset);
    emit("         %-9s S%o\n", buf, reg);
}

void emitStoreStackInt(int value, int offset) {
    char buf[16];

    emit("         S7        %d\n", value);
    sprintf(buf, "%d,A7", offset);
    emit("         %-9s S7\n", buf);
}

void emitString(CharacterValue *cvp, bool hasZByte) {
    int len;
    char *s;

    len = cvp->length;
    s = cvp->string;
    emit("'");
    while (len > 0 && *s != '\0') {
        if (*s == '\'') emit("'");
        emit("%c", *s++);
        len -= 1;
    }
    while (len-- > 0) emit(" ");
    if (hasZByte)
        emit("'Z");
    else
        emit("'");
}

void emitSubInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S%o        S%o-S%o\n", rightArg->reg, leftArg->reg, rightArg->reg);
}

void emitSubprogramCall(char *id) {
    char buf[32];

    normalizeLabel(id, buf);
    emitPrimCall(buf);
}

void emitSubReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S%o        S%o-FS%o\n", rightArg->reg, leftArg->reg, rightArg->reg);
}

void emitUpdateStringRef(OperatorArgument *strRef, OperatorArgument *strOffset, OperatorArgument *strLength) {
    if (strOffset->class == ArgClass_Constant) {
        if (strOffset->details.constant.value.integer > 0) {
            emit("         S7        %d\n", strOffset->details.constant.value.integer);
            emit("         S%o        S%o+S7\n", strRef->reg, strRef->reg);
        }
    }
    else {
        if (strOffset->class > ArgClass_Function) emitLoadValue(strOffset);
        emit("         S%o        S%o+S%o\n", strRef->reg, strRef->reg, strOffset->reg);
        freeRegister(strOffset->reg);
    }
    emit("         S7        <32\n");
    emit("         S%o        S%o&S7\n", strRef->reg, strRef->reg);
    if (strLength->class == ArgClass_Constant) {
        emit("         S7        %d\n", strLength->details.constant.value.integer);
        emit("         S7        S7<32\n");
        emit("         S%o        S%o!S7\n", strRef->reg, strRef->reg);
    }
    else {
        if (strLength->class > ArgClass_Function) emitLoadValue(strLength);
        emit("         S%o        S%o<32\n", strLength->reg, strLength->reg);
        emit("         S%o        S%o!S%o\n", strRef->reg, strRef->reg, strLength->reg);
        freeRegister(strLength->reg);
    }
}

void emitWordBlock(char *label, int size) {
    emit("%-8s BSS       %d\n", label, size);
}

void emitWordLabel(char *label) {
    emit("%-8s BSS       0\n", label);
}

void enableEmission(bool isEnabled) {
    if (isEnabled)
        emissionInhibitDepth -= 1;
    else
        emissionInhibitDepth += 1;
}

void freeAddrReg(Register reg) {
    u8 mask;

    if (reg > 1 && reg < 5) {
        mask = 1 << reg;
        if ((addrRegMap & mask) != 0) {
            addrRegMap &= ~mask;
        }
#if DEBUG
        else {
            emit("*      freeAddrReg: A%o already free\n", reg);
        }
#endif
    }
#if DEBUG
    emit("*      freeAddrReg: A%o -> %02X%02X\n", reg, addrRegMap, registerMap);
#endif
}

void freeAllRegisters(void) {
    registerMap = 0x81;
    addrRegMap = 0xE3;
    lastReg = 0;
#if DEBUG
    emit("* freeAllRegisters: %02X%02X\n", addrRegMap, registerMap);
#endif
}

void freeRegister(Register reg) {
    u8 mask;

    if (reg > 0 && reg < 7) {
        mask = 1 << reg;
        if ((registerMap & mask) != 0) {
            lastReg = reg;
            registerMap &= ~mask;
        }
#if DEBUG
        else {
            emit("*     freeRegister: S%o already free\n", reg);
        }
#endif
    }
#if DEBUG
    emit("*     freeRegister: S%o -> %02X%02X\n", reg, addrRegMap, registerMap);
#endif
}

u16 getRegisterMap(void) {
    return ((u16)addrRegMap << 8) | (u16)registerMap;
}

static void normalizeLabel(char *label, char *result) {
    Fnv32_t hash;
    int len;
    char *lp;
    char *rp;

    lp = label;
    rp = result;
    *rp++ = '@';
    len = 1;
    while (*lp != '\0') {
        *rp = (*lp == '_') ? '%' : *lp;
        lp += 1;
        rp += 1;
        len += 1;
    }
    *rp = '\0';
    if (len > MAX_EXT_NAME_LENGTH) {
        hash = fnv32a(result, len, FNV1_32A_INIT);
        sprintf(result + 4, "%04x", hash & 0xffff);
    }
}
