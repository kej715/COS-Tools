/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: coercion.c
**
**  Description:
**      This file implements type coercion for the compiler.
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

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "coercion.h"
#include "const.h"
#include "proto.h"
#include "types.h"

static BaseType arithCoercion[BaseType_Pointer+1][BaseType_Pointer+1] = {
/*               Undefined           Character           Logical             Integer             Real                Double              Complex             Label               Pointer            */
/* Undefined */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Character */ {BaseType_Undefined, BaseType_Character, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Logical   */ {BaseType_Undefined, BaseType_Undefined, BaseType_Logical,   BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Integer   */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Integer,   BaseType_Real,      BaseType_Double,    BaseType_Complex,   BaseType_Undefined, BaseType_Integer  },
/* Real      */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Real,      BaseType_Real,      BaseType_Double,    BaseType_Complex,   BaseType_Undefined, BaseType_Undefined},
/* Double    */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Double,    BaseType_Double,    BaseType_Double,    BaseType_Complex,   BaseType_Undefined, BaseType_Undefined},
/* Complex   */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Complex,   BaseType_Complex,   BaseType_Complex,   BaseType_Complex,   BaseType_Undefined, BaseType_Undefined},
/* Label     */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Pointer   */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Integer,   BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Integer  }
};

static BaseType compareCoercion[BaseType_Pointer+1][BaseType_Pointer+1] = {
/*               Undefined           Character           Logical             Integer             Real                Double              Complex             Label               Pointer            */
/* Undefined */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Character */ {BaseType_Undefined, BaseType_Character, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Logical   */ {BaseType_Undefined, BaseType_Undefined, BaseType_Logical,   BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Integer   */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Integer,   BaseType_Real,      BaseType_Double,    BaseType_Complex,   BaseType_Undefined, BaseType_Integer  },
/* Real      */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Real,      BaseType_Real,      BaseType_Double,    BaseType_Complex,   BaseType_Undefined, BaseType_Undefined},
/* Double    */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Double,    BaseType_Double,    BaseType_Double,    BaseType_Complex,   BaseType_Undefined, BaseType_Undefined},
/* Complex   */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Complex,   BaseType_Complex,   BaseType_Complex,   BaseType_Complex,   BaseType_Undefined, BaseType_Undefined},
/* Label     */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Pointer   */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Integer,   BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Pointer  }
};

static BaseType logicalCoercion[BaseType_Pointer+1][BaseType_Pointer+1] = {
/*               Undefined           Character           Logical             Integer             Real                Double              Complex             Label               Pointer            */
/* Undefined */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Character */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Logical   */ {BaseType_Undefined, BaseType_Undefined, BaseType_Logical,   BaseType_Integer,   BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Pointer  },
/* Integer   */ {BaseType_Undefined, BaseType_Undefined, BaseType_Integer,   BaseType_Integer,   BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Pointer  },
/* Real      */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Double    */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Complex   */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Label     */ {BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined},
/* Pointer   */ {BaseType_Undefined, BaseType_Undefined, BaseType_Pointer,   BaseType_Pointer,   BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined, BaseType_Undefined}
};

static BaseType coerceDoubleToComplex(OperatorArgument *arg, BaseType fromType, BaseType toType);
static BaseType coerceDoubleToInt(OperatorArgument *arg, BaseType fromType, BaseType toType);
static BaseType coerceDoubleToReal(OperatorArgument *arg, BaseType fromType, BaseType toType);
static BaseType coerceIntToComplex(OperatorArgument *arg, BaseType fromType, BaseType toType);
static BaseType coerceIntToDouble(OperatorArgument *arg, BaseType fromType, BaseType toType);
static BaseType coerceIntToReal(OperatorArgument *arg, BaseType fromType, BaseType toType);
static BaseType coerceLogicalToInt(OperatorArgument *arg, BaseType fromType, BaseType toType);
static BaseType coerceNot(OperatorArgument *arg, BaseType fromType, BaseType toType);
static BaseType coercePtrToInt(OperatorArgument *arg, BaseType fromType, BaseType toType);
static BaseType coerceRealToComplex(OperatorArgument *arg, BaseType fromType, BaseType toType);
static BaseType coerceRealToDouble(OperatorArgument *arg, BaseType fromType, BaseType toType);
static BaseType coerceRealToInt(OperatorArgument *arg, BaseType fromType, BaseType toType);
static BaseType coerceToUndefined(OperatorArgument *arg, BaseType fromType, BaseType toType);

static BaseType (*coercionFns[BaseType_Pointer+1][BaseType_Pointer+1])(OperatorArgument *arg, BaseType fromType, BaseType toType) = {
/*               Undefined          Character          Logical            Integer             Real                Double              Complex                Label              Pointer            */
/* Undefined */ {coerceToUndefined, coerceToUndefined, coerceToUndefined, coerceToUndefined,  coerceToUndefined,  coerceToUndefined,  coerceToUndefined,     coerceToUndefined, coerceToUndefined},
/* Character */ {coerceToUndefined, coerceNot,         coerceToUndefined, coerceToUndefined,  coerceToUndefined,  coerceToUndefined,  coerceToUndefined,     coerceToUndefined, coerceToUndefined},
/* Logical   */ {coerceToUndefined, coerceToUndefined, coerceNot,         coerceLogicalToInt, coerceToUndefined,  coerceToUndefined,  coerceToUndefined,     coerceToUndefined, coerceToUndefined},
/* Integer   */ {coerceToUndefined, coerceToUndefined, coerceToUndefined, coerceNot,          coerceIntToReal,    coerceIntToDouble,  coerceIntToComplex,    coerceToUndefined, coerceNot        },
/* Real      */ {coerceToUndefined, coerceToUndefined, coerceToUndefined, coerceRealToInt,    coerceNot,          coerceRealToDouble, coerceRealToComplex,   coerceToUndefined, coerceToUndefined},
/* Double    */ {coerceToUndefined, coerceToUndefined, coerceToUndefined, coerceDoubleToInt,  coerceDoubleToReal, coerceNot,          coerceDoubleToComplex, coerceToUndefined, coerceToUndefined},
/* Complex   */ {coerceToUndefined, coerceToUndefined, coerceToUndefined, coerceToUndefined,  coerceToUndefined,  coerceToUndefined,  coerceNot,             coerceToUndefined, coerceToUndefined},
/* Label     */ {coerceToUndefined, coerceToUndefined, coerceToUndefined, coerceToUndefined,  coerceToUndefined,  coerceToUndefined,  coerceToUndefined,     coerceNot,         coerceToUndefined},
/* Pointer   */ {coerceToUndefined, coerceToUndefined, coerceToUndefined, coercePtrToInt,     coerceToUndefined,  coerceToUndefined,  coerceToUndefined,     coerceToUndefined, coerceNot        }
};

BaseType calculateCoercedType(OperatorId op, BaseType leftType, BaseType rightType) {
    switch (op) {
    case OP_EXP:
    case OP_ADD:
    case OP_DIV:
    case OP_MUL:
    case OP_SUB:
        return arithCoercion[leftType][rightType];
    case OP_AND:
    case OP_OR:
    case OP_EQV:
    case OP_NEQV:
        return logicalCoercion[leftType][rightType];
    case OP_EQ:
    case OP_GE:
    case OP_GT:
    case OP_LE:
    case OP_LT:
    case OP_NE:
        return compareCoercion[leftType][rightType];
    case OP_CAT:
        return (leftType == BaseType_Character && rightType == BaseType_Character) ? BaseType_Character : BaseType_Undefined;
    default:
        fprintf(stderr, "Invalid binary operator: %d\n", op);
        exit(1);
    }
}

BaseType coerceArgument(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    return (*coercionFns[fromType][toType])(arg, fromType, toType);
}

static BaseType coerceDoubleToComplex(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    return toType;
}

static BaseType coerceDoubleToInt(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    if (arg->class == ArgClass_Constant) {
        arg->details.constant.dt.type = toType;
        arg->details.constant.value.integer = arg->details.constant.value.real;
    }
    else {
        emitRealToInt(arg);
    }
    return toType;
}

static BaseType coerceDoubleToReal(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    return toType;
}

static BaseType coerceIntToComplex(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    return toType;
}

static BaseType coerceIntToDouble(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    if (arg->class == ArgClass_Constant) {
        arg->details.constant.dt.type = toType;
        arg->details.constant.value.real = arg->details.constant.value.integer;
    }
    else {
        emitIntToReal(arg);
    }
    return toType;
}

static BaseType coerceIntToReal(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    if (arg->class == ArgClass_Constant) {
        arg->details.constant.dt.type = toType;
        arg->details.constant.value.real = arg->details.constant.value.integer;
    }
    else {
        emitIntToReal(arg);
    }
    return toType;
}

static BaseType coerceLogicalToInt(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    if (arg->class == ArgClass_Constant) {
        arg->details.constant.dt.type = toType;
    }
    return toType;
}

static BaseType coerceNot(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    return toType;
}

static BaseType coercePtrToInt(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    return toType;
}

static BaseType coerceRealToComplex(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    return toType;
}

static BaseType coerceRealToDouble(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    return toType;
}

static BaseType coerceRealToInt(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    if (arg->class == ArgClass_Constant) {
        arg->details.constant.dt.type = toType;
        arg->details.constant.value.integer = arg->details.constant.value.real;
    }
    else {
        emitRealToInt(arg);
    }
    return toType;
}

static BaseType coerceToUndefined(OperatorArgument *arg, BaseType fromType, BaseType toType) {
    return BaseType_Undefined;
}
