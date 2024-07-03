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

static Register lastReg = 0;
static u8 registerMap = 0x01;
static bool isEmissionEnabled = TRUE;

Register allocateRegister(void) {
    u8 mask;

    if (registerMap == 0xff) {
        fputs("All registers allocated\n", stderr);
        return -1;
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

    if (objectFile != NULL && isEmissionEnabled) {
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

void emitBranch(char *label) {
    emit("         J         %s\n", label);
}

void emitBranch3Way(Register reg, char *label1, char *label2, char *label3) {
    emit("         S0        S%o\n", reg);
    emit("         JSM       %s\n", label1);
    emit("         JSZ       %s\n", label2);
    emit("         J         %s\n", label3);
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

void emitCatChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    // TODO: implement
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
    // TODO: implement
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

void emitFnCall(OperatorArgument *arg) {
// TODO: emit function call
    arg->reg = allocateRegister();
}

void emitGeChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    // TODO: implement
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
    // TODO: implement
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

void emitLeChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    // TODO: implement
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

Register emitLoadByteAddr(char *label) {
    Register reg;

    reg = allocateRegister();
    emit("         S%o        %s\n", reg, label);
    emit("         S%o        S%o<3\n", reg, reg);

    return reg;
}

void emitLoadConst(OperatorArgument *arg) {
    char buf[32];
    char *cp;
    i64 i;
    u64 l;

    arg->reg = allocateRegister();
    switch (arg->details.constant.dt.type) {
    case BaseType_Character:
        generateLabel(buf);
        emitString(arg->details.constant.value.character.string, buf);
        freeRegister(arg->reg);
        arg->reg = emitLoadByteAddr(buf);
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

void emitLoadVar(OperatorArgument *arg) {
    Symbol *sym;

    arg->reg = allocateRegister();
    sym = arg->details.symbol;
    switch (sym->class) {
    case SymClass_Local:
        emit("         S%o        %d,A6\n", arg->reg, sym->details.variable.offset);
        break;
    case SymClass_Argument:
        emit("         A1        %d,A6\n", sym->details.variable.offset);
        emit("         S%o        ,A1\n", arg->reg);
        break;
    case SymClass_Global:
    case SymClass_Pointee:
        emit("         S%o        %s,\n", arg->reg, sym->identifier);
        break;
    default:
        fprintf(stderr, "Invalid class for load request: %d\n", sym->class);
        exit(1);
    }
}

Register emitLoadVarAddr(Symbol *sym) {
    Register offsetReg;
    Register resultReg;

    resultReg = allocateRegister();
    switch (sym->class) {
    case SymClass_Local:
        offsetReg = allocateRegister();
        emit("         S%o        %d\n", offsetReg, sym->details.variable.offset);
        emit("         S%o        A6\n", resultReg);
        emit("         S%o        S%o+S%o\n", resultReg, resultReg, offsetReg);
        freeRegister(offsetReg);
        break;
    case SymClass_Argument:
        emit("         S%o        %d,A6\n", resultReg, sym->details.variable.offset);
        break;
    case SymClass_Global:
    case SymClass_Pointee:
        emit("         S%o        %s\n", resultReg, sym->identifier);
        break;
    case SymClass_Label:
        emit("         S%o        %s\n", resultReg, sym->details.label.label);
        break;
    default:
        fprintf(stderr, "Invalid class for load request: %d\n", sym->class);
        exit(1);
    }

    return resultReg;
}

Register emitLoadVarByteAddr(Symbol *sym) {
    Register reg;

    reg = emitLoadVarAddr(sym);
    emit("         S%o        S%o<3\n", reg, reg);

    return reg;
}

void emitLtChar(OperatorArgument *leftArg, OperatorArgument *rightArg) {
    // TODO: implement
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
    // TODO: implement
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

    for (reg = 7; reg > 0; reg--) {
        if ((mask & (1 << reg)) != 0) {
            emitPopReg(reg);
        }
    }
}

void emitSaveRegs(u8 mask) {
    u8 reg;

    for (reg = 1; reg < 8; reg++) {
        if ((mask & (1 << reg)) != 0) {
            emitPushReg(reg);
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

void emitString(char *s, char *label) {
    emitActivateSection("DATA", "DATA");
    if (label != NULL) emitWordLabel(label);
    emit("         DATA      '");
    while (*s != '\0') {
        while (*s != '\0' && *s != '\'') {
            emit("%c", *s);
            s += 1;
        }
        if (*s == '\'') {
            emit("''");
            s += 1;
        }
    }
    emit("'Z\n");
    emitDeactivateSection("DATA");
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

void emitWordLabel(char *label) {
    emit("%-8s BSS       0\n", label);
}

void freeAllRegisters(void) {
    registerMap = 0x01;
    lastReg = 0;
}

void enableEmission(bool isEnabled) {
    isEmissionEnabled = isEnabled;
}

void freeRegister(Register reg) {
    u8 mask;

    if (reg > 0 && reg <= 7) {
        mask = 1 << reg;
        if ((registerMap & mask) != 0) {
            lastReg = reg;
            registerMap &= ~mask;
        }
    }
}
