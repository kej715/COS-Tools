/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: binops.c
**
**  Description:
**      This file contains functions that implement binary operators for
**      the compiler. It includes implementations for constant arguments
**      and implementations that generate code for the target machine.
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

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "binops.h"
#include "codegen.h"
#include "proto.h"
#include "types.h"

static u64 truth[2] = {0, ~(u64)0};

void cstAddInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.integer += left->details.constant.value.integer;
}

void cstAddReal(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.real += left->details.constant.value.real;
}

void cstDivInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.integer = left->details.constant.value.integer / right->details.constant.value.integer;
}

void cstDivReal(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.real = left->details.constant.value.real / right->details.constant.value.real;
}

void cstExpInt(OperatorArgument *left, OperatorArgument *right) {
    i64 leftInt;
    i64 rightInt;
    i64 res;

    leftInt = left->details.constant.value.integer;
    rightInt = right->details.constant.value.integer;
    if (rightInt > 1 && rightInt < 5) {
        res = leftInt;
        while (rightInt-- > 1) res *= leftInt;
    }
    else {
        res = (i64)pow((double)leftInt, (double)rightInt);
    }
    right->details.constant.value.integer = res;
}

void cstExpReal(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.real = pow(left->details.constant.value.real, right->details.constant.value.real);
}

void cstMulInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.integer *= left->details.constant.value.integer;
}

void cstMulReal(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.real *= left->details.constant.value.real;
}

void cstSubInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.integer = left->details.constant.value.integer - right->details.constant.value.integer;
}

void cstSubReal(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.real = left->details.constant.value.real - right->details.constant.value.real;
}

void cstAndLog(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical &= left->details.constant.value.logical;
}

void cstAndInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.integer &= left->details.constant.value.integer;
}

void cstOrLog(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical |= left->details.constant.value.logical;
}

void cstOrInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.integer |= left->details.constant.value.integer;
}

void cstEqvLog(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = ~(left->details.constant.value.logical ^ right->details.constant.value.logical);
}

void cstEqvInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.integer = ~(left->details.constant.value.integer ^ right->details.constant.value.integer);
}

void cstNeqvLog(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical ^= left->details.constant.value.logical;
}

void cstNeqvInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.integer ^= left->details.constant.value.integer;
}

void cstEqChar(OperatorArgument *left, OperatorArgument *right) {
    char *rightStr;

    rightStr = right->details.constant.value.character.string;
    right->details.constant.value.logical = truth[strcmp(left->details.constant.value.character.string, rightStr) == 0];
    free(left->details.constant.value.character.string);
    free(rightStr);
}

void cstEqLog(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.logical == right->details.constant.value.logical];
}

void cstEqInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.integer == right->details.constant.value.integer];
}

void cstEqReal(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.real == right->details.constant.value.real];
}

void cstGeChar(OperatorArgument *left, OperatorArgument *right) {
    char *rightStr;

    rightStr = right->details.constant.value.character.string;
    right->details.constant.value.logical = truth[strcmp(left->details.constant.value.character.string, rightStr) >= 0];
    free(left->details.constant.value.character.string);
    free(rightStr);
}

void cstGeLog(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.logical >= right->details.constant.value.logical];
}

void cstGeInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.integer >= right->details.constant.value.integer];
}

void cstGeReal(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.real >= right->details.constant.value.real];
}

void cstGtChar(OperatorArgument *left, OperatorArgument *right) {
    char *rightStr;

    rightStr = right->details.constant.value.character.string;
    right->details.constant.value.logical = truth[strcmp(left->details.constant.value.character.string, rightStr) > 0];
    free(left->details.constant.value.character.string);
    free(rightStr);
}

void cstGtLog(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.logical > right->details.constant.value.logical];
}

void cstGtInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.integer > right->details.constant.value.integer];
}

void cstGtReal(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.real > right->details.constant.value.real];
}

void cstLeChar(OperatorArgument *left, OperatorArgument *right) {
    char *rightStr;

    rightStr = right->details.constant.value.character.string;
    right->details.constant.value.logical = truth[strcmp(left->details.constant.value.character.string, rightStr) <= 0];
    free(left->details.constant.value.character.string);
    free(rightStr);

}

void cstLeLog(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.logical <= right->details.constant.value.logical];
}

void cstLeInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.integer <= right->details.constant.value.integer];
}

void cstLeReal(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.real <= right->details.constant.value.real];
}

void cstLtChar(OperatorArgument *left, OperatorArgument *right) {
    char *rightStr;

    rightStr = right->details.constant.value.character.string;
    right->details.constant.value.logical = truth[strcmp(left->details.constant.value.character.string, rightStr) < 0];
    free(left->details.constant.value.character.string);
    free(rightStr);
}

void cstLtLog(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.logical < right->details.constant.value.logical];
}

void cstLtInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.integer < right->details.constant.value.integer];
}

void cstLtReal(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.real < right->details.constant.value.real];
}

void cstNeChar(OperatorArgument *left, OperatorArgument *right) {
    char *rightStr;

    rightStr = right->details.constant.value.character.string;
    right->details.constant.value.logical = truth[strcmp(left->details.constant.value.character.string, rightStr) != 0];
    free(left->details.constant.value.character.string);
    free(rightStr);
}

void cstNeLog(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.logical != right->details.constant.value.logical];
}

void cstNeInt(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.integer != right->details.constant.value.integer];
}

void cstNeReal(OperatorArgument *left, OperatorArgument *right) {
    right->details.constant.value.logical = truth[left->details.constant.value.real != right->details.constant.value.real];
}

void cstCatChar(OperatorArgument *left, OperatorArgument *right) {
    char *cp;
    char *dp;
    int len;
    char *s;

    len = left->details.constant.value.character.length + right->details.constant.value.character.length;
    s = (char *)allocate(len + 1);
    dp = s;
    cp = left->details.constant.value.character.string;
    while (*cp != '\0') *dp++ = *cp++;
    cp = right->details.constant.value.character.string;
    while (*cp != '\0') *dp++ = *cp++;
    *dp = '\0';
    free(left->details.constant.value.character.string);
    free(right->details.constant.value.character.string);
    right->details.constant.value.character.string = s;
    right->details.constant.value.character.length = len;
}

/*
 *  Table of functions implementing binary operators that operate on constants to produce constants.
 *  The table is indexed by operator and data type of operands.
 */
void (*cstBinOps[(OP_CAT-OP_ADD)+1][BaseType_Pointer+1])(OperatorArgument *left, OperatorArgument *right) = {
/*             Undefined   Character   Logical     Integer     Real        Double      Complex     Pointer  */
/* OP_ADD  */ {NULL,       NULL,       NULL,       cstAddInt,  cstAddReal, cstAddReal, NULL,       NULL      },
/* OP_DIV  */ {NULL,       NULL,       NULL,       cstDivInt,  cstDivReal, cstDivReal, NULL,       NULL      },
/* OP_EXP  */ {NULL,       NULL,       NULL,       cstExpInt,  cstExpReal, cstExpReal, NULL,       NULL      },
/* OP_MUL  */ {NULL,       NULL,       NULL,       cstMulInt,  cstMulReal, cstMulReal, NULL,       NULL      },
/* OP_SUB  */ {NULL,       NULL,       NULL,       cstSubInt,  cstSubReal, cstSubReal, NULL,       NULL      },
/* OP_AND  */ {NULL,       NULL,       cstAndLog,  cstAndInt,  NULL,       NULL,       NULL,       NULL      },
/* OP_OR   */ {NULL,       NULL,       cstOrLog,   cstOrInt,   NULL,       NULL,       NULL,       NULL      },
/* OP_EQV  */ {NULL,       NULL,       cstEqvLog,  cstEqvInt,  NULL,       NULL,       NULL,       NULL      },
/* OP_NEQV */ {NULL,       NULL,       cstNeqvLog, cstNeqvInt, NULL,       NULL,       NULL,       NULL      },
/* OP_EQ   */ {NULL,       cstEqChar,  cstEqLog,   cstEqInt,   cstEqReal,  cstEqReal,  NULL,       cstEqLog  },
/* OP_GE   */ {NULL,       cstGeChar,  cstGeLog,   cstGeInt,   cstGeReal,  cstGeReal,  NULL,       cstGeLog  },
/* OP_GT   */ {NULL,       cstGtChar,  cstGtLog,   cstGtInt,   cstGtReal,  cstGtReal,  NULL,       cstGtLog  },
/* OP_LE   */ {NULL,       cstLeChar,  cstLeLog,   cstLeInt,   cstLeReal,  cstLeReal,  NULL,       cstLeLog  },
/* OP_LT   */ {NULL,       cstLtChar,  cstLtLog,   cstLtInt,   cstLtReal,  cstLtReal,  NULL,       cstLtLog  },
/* OP_NE   */ {NULL,       cstNeChar,  cstNeLog,   cstNeInt,   cstNeReal,  cstNeReal,  NULL,       cstNeLog  },
/* OP_CAT  */ {NULL,       cstCatChar, NULL,       NULL,       NULL,       NULL,       NULL,       NULL      }
};

/*
 *  Table of functions that generate code to implement binary operators for a target machine.
 *  The table is indexed by operator and data type of operands.
 */
void (*genBinOps[(OP_CAT-OP_ADD)+1][BaseType_Pointer+1])(OperatorArgument *left, OperatorArgument *right) = {
/*             Undefined   Character    Logical      Integer      Real         Double      Complex     Pointer  */
/* OP_ADD  */ {NULL,       NULL,        NULL,        emitAddInt,  emitAddReal, emitAddReal,NULL,       NULL      },
/* OP_DIV  */ {NULL,       NULL,        NULL,        emitDivInt,  emitDivReal, emitDivReal,NULL,       NULL      },
/* OP_EXP  */ {NULL,       NULL,        NULL,        emitExpInt,  emitExpReal, emitExpReal,NULL,       NULL      },
/* OP_MUL  */ {NULL,       NULL,        NULL,        emitMulInt,  emitMulReal, emitMulReal,NULL,       NULL      },
/* OP_SUB  */ {NULL,       NULL,        NULL,        emitSubInt,  emitSubReal, emitSubReal,NULL,       NULL      },
/* OP_AND  */ {NULL,       NULL,        emitAndInt,  emitAndInt,  NULL,        NULL,       NULL,       NULL      },
/* OP_OR   */ {NULL,       NULL,        emitOrInt,   emitOrInt,   NULL,        NULL,       NULL,       NULL      },
/* OP_EQV  */ {NULL,       NULL,        emitEqvInt,  emitEqvInt,  NULL,        NULL,       NULL,       NULL      },
/* OP_NEQV */ {NULL,       NULL,        emitNeqvInt, emitNeqvInt, NULL,        NULL,       NULL,       NULL      },
/* OP_EQ   */ {NULL,       emitEqChar,  emitEqLog,   emitEqInt,   emitEqReal,  emitEqReal, NULL,       emitEqLog },
/* OP_GE   */ {NULL,       emitGeChar,  emitGeLog,   emitGeInt,   emitGeReal,  emitGeReal, NULL,       emitGeLog },
/* OP_GT   */ {NULL,       emitGtChar,  emitGtLog,   emitGtInt,   emitGtReal,  emitGtReal, NULL,       emitGtLog },
/* OP_LE   */ {NULL,       emitLeChar,  emitLeLog,   emitLeInt,   emitLeReal,  emitLeReal, NULL,       emitLeLog },
/* OP_LT   */ {NULL,       emitLtChar,  emitLtLog,   emitLtInt,   emitLtReal,  emitLtReal, NULL,       emitLtLog },
/* OP_NE   */ {NULL,       emitNeChar,  emitNeLog,   emitNeInt,   emitNeReal,  emitNeReal, NULL,       emitNeLog },
/* OP_CAT  */ {NULL,       emitCatChar, NULL,        NULL,        NULL,        NULL,       NULL,       NULL      }
};
