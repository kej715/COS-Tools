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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "const.h"
#include "proto.h"
#include "types.h"

static void emit(char *format, ...);

static int emissionDepth = 0;
static Register lastReg = 0;
static u8 registerMap = 0x81;

Register allocateRegister(void) {
    u8 mask;

    if (registerMap == 0xff) {
        fputs("All registers allocated\n", stderr);
        exit(1);
    }
    for (;;) {
        mask = 1 << lastReg;
        if ((registerMap & mask) == 0) {
            registerMap |= mask;
            return lastReg;
        }
        lastReg = (lastReg + 1) & 0x07;
    }
}

static void emit(char *format, ...) {
    va_list ap;

    if (objectFile != NULL && emissionDepth < 1) {
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

void emitAddReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S%o        S%o+FS%o\n", rightArg->reg, leftArg->reg, rightArg->reg);
}

void emitAddReg(Register reg1, Register reg2, BaseType type) {
    switch (type) {
    case BaseType_Integer:
        emit("         S%o        S%o+S%o\n", reg1, reg1, reg2);
        break;
    case BaseType_Real:
    case BaseType_Double:
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
    emit("         J         %s\n", label);
}

void emitBranch3Way(Register reg, char *label1, char *label2, char *label3) {
    emit("         S0        S%o\n", reg);
    if (label1 != NULL) emit("         JSM       %s\n", label1);
    if (label3 != NULL) emit("         JSN       %s\n", label3);
    if (label2 != NULL) emit("         JSZ       %s\n", label2);
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
    emit("         S0        S%o\n", reg);
    emit("         JSZ       %s\n", label);
}

void emitBranchReg(Register reg) {
    emit("         A0        S%o\n", reg);
    emit("         B00       A0\n");
    emit("         J         B00\n");
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
    case BaseType_Real:
    case BaseType_Double:
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
    case BaseType_Real:
    case BaseType_Double:
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
    u8 mask;

    mask = registerMap & ~((1 << rightArg->reg) | (1 << leftArg->reg));
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
    u8 mask;

    mask = registerMap & ~((1 << rightArg) | (1 << leftArg));
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
    u8 mask;

    mask = registerMap & ~((1 << rightArg) | (1 << leftArg));
    emitSaveRegs(mask);
    emitPushReg(leftArg);
    emitPushReg(rightArg);
    emit("         R         %%dvf\n");
    if (rightArg != RESULT_REG) emit("         S%o        S7\n", rightArg);
    emitRestoreRegs(mask);
}

void emitEnd(void) {
    emitDeactivateSection("TEXT");
    emit("         END\n");
}

void emitEpilog(Symbol *sym, int frameSize) {
    if (sym->class == SymClass_Program) {
        emitPrimCall("@_endfio");
        emit("         S7        0\n");
    }
    else if (sym->class == SymClass_Function) {
        emit("         S7        %d,A6\n", sym->details.progUnit.offset);
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

void emitEqChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u8 mask;

    mask = registerMap & ~((1 << rightArg->reg) | (1 << leftArg->reg));
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

void emitGeChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u8 mask;

    mask = registerMap & ~((1 << rightArg->reg) | (1 << leftArg->reg));
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
    u8 mask;

    mask = registerMap & ~((1 << rightArg->reg) | (1 << leftArg->reg));
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
    u8 mask;

    mask = registerMap & ~(1 << arg->reg);
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
    emitActivateSection("DATA", "DATA");
    if (label != NULL) emitWordLabel(label);
    emit("         DATA      ");
    emitString(cvp, hasZByte);
    emit("\n");
    emitDeactivateSection("DATA");
}

Register emitLabelReference(Symbol *sym) {
    Register reg;

    reg = allocateRegister();
    emit("         S%o        %s\n", reg, sym->details.label.label);
    return reg;
}

void emitLeChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u8 mask;

    mask = registerMap & ~((1 << rightArg->reg) | (1 << leftArg->reg));
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

void emitLoadConst(OperatorArgument *arg) {
    char buf[32];
    char *cp;
    DataType dt;
    i64 i;
    u64 l;

    arg->reg = allocateRegister();
    dt = arg->details.constant.dt;
    switch (arg->details.constant.dt.type) {
    case BaseType_Character:
        emit("         S%o        =", arg->reg);
        emitString(&arg->details.constant.value.character, FALSE);
        emit("\n");
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
    case BaseType_Real:
        sprintf(buf, "%f", arg->details.constant.value.real);
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

void emitLoadReference(OperatorArgument *arg) {
    DataType *dt;
    Symbol *sym;

    arg->reg = allocateRegister();
    sym = arg->details.reference.symbol;
    dt = getSymbolType(sym);
    if (dt->type == BaseType_Character) {
        switch (sym->class) {
        case SymClass_Local:
            emit("         S%o        %d\n", arg->reg, sym->details.variable.offset);
            emit("         S7        A6\n");
            emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
            emit("         S%o        S%o<3\n", arg->reg, arg->reg);
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                /* do nothing */
                break;
            case ArgClass_Constant:
                emit("         S7        %d\n", arg->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
                break;
            case ArgClass_Calculation:
                emit("         S%o        S%o+S%o\n", arg->reg, arg->reg, arg->details.reference.offset.reg);
                freeRegister(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Argument:
            emit("         S%o        %d,A6\n", arg->reg, sym->details.variable.offset);
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                /* do nothing */
                break;
            case ArgClass_Constant:
                emit("         S7        %d\n", arg->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
                break;
            case ArgClass_Calculation:
                emit("         S%o        S%o+S%o\n", arg->reg, arg->reg, arg->details.reference.offset.reg);
                freeRegister(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Function:
            emit("         S%o        %d\n", arg->reg, sym->details.progUnit.offset);
            emit("         S7        A6\n");
            emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
            emit("         S%o        S%o<3\n", arg->reg, arg->reg);
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                /* do nothing */
                break;
            case ArgClass_Constant:
                emit("         S7        %d\n", arg->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
                break;
            case ArgClass_Calculation:
                emit("         S%o        S%o+S%o\n", arg->reg, arg->reg, arg->details.reference.offset.reg);
                freeRegister(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Global:
        case SymClass_Pointee:
            emit("         S%o        %s\n", arg->reg, sym->identifier);
            emit("         S%o        S%o<3\n", arg->reg, arg->reg);
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                /* do nothing */
                break;
            case ArgClass_Constant:
                emit("         S7        %d\n", arg->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
                break;
            case ArgClass_Calculation:
                emit("         S%o        S%o+S%o\n", arg->reg, arg->reg, arg->details.reference.offset.reg);
                freeRegister(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        default:
            fprintf(stderr, "Invalid class for load request: %d\n", sym->class);
            exit(1);
        }
        emit("         S7        %d\n", dt->constraint);
        emit("         S7        S7<32\n");
        emit("         S%o        S%o!S7\n", arg->reg, arg->reg);
    }
    else { // dt->type != BaseType_Character
        switch (sym->class) {
        case SymClass_Local:
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S7        A6\n");
                emit("         S%o        %d\n", arg->reg, sym->details.variable.offset);
                emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
                break;
            case ArgClass_Constant:
                emit("         S7        A6\n");
                emit("         S%o        %d\n", arg->reg, sym->details.variable.offset + arg->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
                break;
            case ArgClass_Calculation:
                emit("         S%o        A6\n", arg->reg);
                emit("         S7        %d\n", sym->details.variable.offset);
                emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
                emit("         S%o        S%o+S%o\n", arg->reg, arg->reg, arg->details.reference.offset.reg);
                freeRegister(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Argument:
            emit("         S%o        %d,A6\n", arg->reg, sym->details.variable.offset);
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                /* do nothing */
                break;
            case ArgClass_Constant:
                emit("         S7        %d\n", arg->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
                break;
            case ArgClass_Calculation:
                emit("         S%o        S%o+S%o\n", arg->reg, arg->reg, arg->details.reference.offset.reg);
                freeRegister(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Function:
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S7        A6\n");
                emit("         S%o        %d\n", arg->reg, sym->details.progUnit.offset);
                emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
                break;
            case ArgClass_Constant:
                emit("         S7        A6\n");
                emit("         S%o        %d\n", arg->reg, sym->details.progUnit.offset + arg->details.reference.offset.constant);
                emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
                break;
            case ArgClass_Calculation:
                emit("         S%o        A6\n", arg->reg);
                emit("         S7        %d\n", sym->details.progUnit.offset);
                emit("         S%o        S%o+S7\n", arg->reg, arg->reg);
                emit("         S%o        S%o+S%o\n", arg->reg, arg->reg, arg->details.reference.offset.reg);
                freeRegister(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Global:
        case SymClass_Pointee:
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S%o        %s\n", arg->reg, sym->identifier);
                break;
            case ArgClass_Constant:
                emit("         S%o        %s+%d\n", arg->reg, sym->identifier, arg->details.reference.offset.constant);
                break;
            case ArgClass_Calculation:
                emit("         S%o        %s\n", arg->reg, sym->identifier);
                emit("         S%o        S%o+S%o\n", arg->reg, arg->reg, arg->details.reference.offset.reg);
                freeRegister(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        default:
            fprintf(stderr, "Invalid class for load request: %d\n", sym->class);
            exit(1);
        }
    }
    arg->class = ArgClass_Calculation;
    arg->details.calculation = sym->details.variable.dt;
}

Register emitLoadStack(int offset) {
    Register reg;

    reg = allocateRegister();
    emit("         S%o        %d,A7\n", reg, offset);

    return reg;
}

Register emitLoadStackAddr(int offset) {
    Register reg1;
    Register reg2;

    reg1 = allocateRegister();
    emit("         S%o        A7\n", reg1);
    if (offset > 0) {
        reg2 = allocateRegister();
        emit("         S%o        %d\n", reg2, offset);
        emit("         S%o        S%o+S%o\n", reg1, reg1, reg2);
        freeRegister(reg2);
    }
    else if (offset < 0) {
        reg2 = allocateRegister();
        emit("         S%o        %d\n", reg2, -offset);
        emit("         S%o        S%o-S%o\n", reg1, reg1, reg2);
        freeRegister(reg2);
    }

    return reg1;
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
        emitLoadReference(arg);
    }
    else {
        arg->reg = allocateRegister();
        switch (sym->class) {
        case SymClass_Local:
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S%o        %d,A6\n", arg->reg, sym->details.variable.offset);
                break;
            case ArgClass_Constant:
                emit("         S%o        %d,A6\n", arg->reg, sym->details.variable.offset + arg->details.reference.offset.constant);
                break;
            case ArgClass_Calculation:
                emit("         A1        S%o\n", arg->details.reference.offset.reg);
                emit("         A1        A1+A6\n");
                emit("         S%o        %d,A1\n", arg->reg, sym->details.variable.offset);
                freeRegister(arg->details.reference.offset.reg);
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
                emit("         A2        S%o\n", arg->details.reference.offset.reg);
                emit("         A1        A1+A2\n");
                emit("         S%o        ,A1\n", arg->reg);
                freeRegister(arg->details.reference.offset.reg);
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
                emit("         A1        S%o\n", arg->details.reference.offset.reg);
                emit("         A1        A1+A6\n");
                emit("         S%o        %d,A1\n", arg->reg, sym->details.progUnit.offset);
                freeRegister(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        case SymClass_Global:
        case SymClass_Pointee:
            switch (arg->details.reference.offsetClass) {
            case ArgClass_Undefined:
                emit("         S%o        %s,\n", arg->reg, sym->identifier);
                break;
            case ArgClass_Constant:
                emit("         S%o        %s+%d,\n", arg->reg, sym->identifier, arg->details.reference.offset.constant);
                break;
            case ArgClass_Calculation:
                emit("         A1        S%o\n", arg->details.reference.offset.reg);
                emit("         S%o        %s,A1\n", arg->reg, sym->identifier);
                freeRegister(arg->details.reference.offset.reg);
                break;
            default:
                fprintf(stderr, "Invalid offset class in reference to %s: %d\n", sym->identifier, arg->details.reference.offsetClass);
                exit(1);
            }
            break;
        default:
            fprintf(stderr, "Invalid class for load request: %d\n", sym->class);
            exit(1);
        }
        arg->class = ArgClass_Calculation;
        arg->details.calculation = sym->details.variable.dt;
    }
}

Register emitLoadZStrAddr(char *label) {
    Register reg;

    reg = allocateRegister();
    emit("         S%o        %s\n", reg, label);
    emitPushReg(reg);
    freeRegister(reg);
    emitPrimCall("@strlen");
    emitAdjustSP(1);
    reg = allocateRegister();
    emit("         S%o        %s\n", reg, label);
    emit("         S%o        S%o<3\n", reg, reg);
    emit("         S7        S7<32\n");
    emit("         S%o        S%o!S7\n", reg, reg);

    return reg;
}

void emitLtChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u8 mask;

    mask = registerMap & ~((1 << rightArg->reg) | (1 << leftArg->reg));
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
    u8 mask;

    mask = registerMap & ~((1 << rightArg->reg) | (1 << leftArg->reg));
    emitSaveRegs(mask);
    emitPushReg(leftArg->reg);
    emitPushReg(rightArg->reg);
    emit("         R         %%mli\n");
    if (rightArg->reg != RESULT_REG) emit("         S%o        S7\n", rightArg->reg);
    emitRestoreRegs(mask);
}

void emitMulReal(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S%o        S%o*FS%o\n", rightArg->reg, leftArg->reg, rightArg->reg);
}

void emitNeChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    u8 mask;

    mask = registerMap & ~((1 << rightArg->reg) | (1 << leftArg->reg));
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

void emitOrInt(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    emit("         S%o        S%o!S%o\n", rightArg->reg, leftArg->reg, rightArg->reg);
}

void emitPopReg(Register reg) {
    emit("         S%o        ,A7\n", reg);
    emit("         A7        A7+1\n");
}

void emitPrimCall(char *label) {
    emit("         R         ");
    while (*label != '\0') {
        emit("%c", (*label == '_') ? '%' : *label);
        label += 1;
    }
    emit("\n");
}

void emitProlog(Symbol *sym) {
    char buf[32];

    if (sym->class == SymClass_Program) {
        emit("         ENTRY     @main\n");
        emit("@main    BSS       0\n");
    }
    else {
        sprintf(buf, "@%s", sym->identifier);
        emit("         ENTRY     %s\n", buf);
        emit("%-8s BSS       0\n", buf);
    }
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

void emitPushReg(Register reg) {
    emit("         A7        A7-1\n");
    emit("         ,A7       S%o\n", reg);
}

void emitRealToInt(OperatorArgument *arg) {
    emitRealToIntReg(arg->reg);
}

void emitRealToIntReg(Register arg) {
    u8 mask;

    mask = registerMap & ~(1 << arg);
    emitSaveRegs(mask);
    emitPushReg(arg);
    emit("         R         %%cfi\n");
    if (arg != RESULT_REG) emit("         S%o        S7\n", arg);
    emitRestoreRegs(mask);
}

void emitRestoreRegs(u8 mask) {
    u8 reg;
    u8 selector;

    for (reg = 6; reg > 0; reg--) {
        selector = 1 << reg;
        if ((mask & selector) != 0) {
            emitPopReg(reg);
            registerMap |= selector;
        }
    }
}

void emitSaveRegs(u8 mask) {
    u8 reg;
    u8 selector;

    for (reg = 1; reg < 7; reg++) {
        selector = 1 << reg;
        if ((mask & selector) != 0) {
            emitPushReg(reg);
            registerMap &= ~selector;
        }
    }
}

void emitStart(char *name) {
    emit("         IDENT     %s\n", name);
    emitActivateSection("TEXT", "CODE");
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
    case SymClass_Local:
        sprintf(buf, "%d,A6", sym->details.variable.offset);
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
    case SymClass_Parameter:
    case SymClass_Pointee:
        sprintf(buf, "%s,", sym->identifier);
        emit("         %-9s S%o\n", buf, reg);
        break;
    default:
        fprintf(stderr, "Invalid class for store request: %d\n", sym->class);
        exit(1);
    }
}

void emitStoreStack(Register reg, int offset) {
    char buf[16];

    sprintf(buf, "%d,A7", offset);
    emit("         %-9s S%o\n", buf, reg);
}

void emitStoreString(Symbol *sym, OperatorArgument *offset, OperatorArgument *length, OperatorArgument *string) {
    char buf[32];
    Register reg;

    reg = allocateRegister();
    switch (sym->class) {
    case SymClass_Argument:
        emit("         S%o        %d,A6\n", reg, sym->details.variable.offset);
        emit("         S7        <32\n");
        emit("         S%o        S%o&S7\n", reg, reg);
        break;
    case SymClass_Local:
        emit("         A1        %d\n", sym->details.variable.offset);
        emit("         A1        A1+A6\n");
        break;
    case SymClass_Function:
        emit("         A1        %d\n", sym->details.progUnit.offset);
        emit("         A1        A1+A6\n");
        break;
    case SymClass_Global:
    case SymClass_Parameter:
    case SymClass_Pointee:
        emit("         A1        %s\n", sym->identifier);
        break;
    default:
        fprintf(stderr, "Invalid class for store request: %d\n", sym->class);
        exit(1);
    }
    if (sym->class != SymClass_Argument) {
        emit("         S%o        A1\n", reg);
        emit("         S%o         S%o<3\n", reg, reg);
    }
    if (offset->class != ArgClass_Constant || offset->details.constant.value.integer != 0) {
        if (offset->class == ArgClass_Constant) {
            emitLoadConst(offset);
        }
        else if (offset->class > ArgClass_Function) {
            emitLoadValue(offset);
        }
        emit("         S%o         S%o+S%o\n", reg, reg, offset->reg);
    }
    if (length->class == ArgClass_Constant) {
        emitLoadConst(length);
    }
    else if (length->class > ArgClass_Function) {
        emitLoadValue(length);
    }
    emit("         S%o         S%o<32\n", length->reg, length->reg);
    emit("         S%o         S%o!S%o\n", reg, reg, length->reg);
    emitPushReg(string->reg);
    emitPushReg(reg);
    freeRegister(reg);
    emitPrimCall("@_cpystr");
    emitAdjustSP(2);
}

void emitString(CharacterValue *cvp, bool hasZByte) {
    int len;
    char *s;

    len = cvp->length;
    s = cvp->string;
    emit("'");
    while (len > 0 && *s != '\0') {
        while (len > 0 && *s != '\0' && *s != '\'') {
            emit("%c", *s);
            s += 1;
            len -= 1;
        }
        if (*s == '\'') {
            emit("''");
            s += 1;
            len -= 1;
        }
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

    sprintf(buf, "@%s", id);
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

void emitWordLabel(char *label) {
    emit("%-8s BSS       0\n", label);
}

void freeAllRegisters(void) {
    registerMap = 0x01;
    lastReg = 0;
}

void enableEmission(bool isEnabled) {
    if (isEnabled)
        emissionDepth += 1;
    else
        emissionDepth -= 1;
}

void freeRegister(Register reg) {
    u8 mask;

    if (reg > 0 && reg <= 6) {
        mask = 1 << reg;
        if ((registerMap & mask) != 0) {
            lastReg = reg;
            registerMap &= ~mask;
        }
    }
}

u8 getRegisterMap(void) {
    return registerMap;
}
