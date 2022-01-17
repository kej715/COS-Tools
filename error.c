/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: error.c
**
**  Description:
**      This file provides functions for obtaining information about
**      error indications.
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
#include <stdlib.h>
#include <string.h>
#include "calproto.h"
#include "caltypes.h"

typedef struct errorDefn {
    ErrorCode code;
    char *indicator;
    char *message;
} ErrorDefn;

/*
 *  The following table is indexed by the ErrorCode enumeration.
 */
static ErrorDefn errorDefns[] = {
    {Err_None,                     " ",  "NO ERROR"},
    {Err_DataItem,                 "C",  "NAME, SYMBOL, CONSTANT OR DATA ITEM ERROR"},
    {Err_DoubleDefinition,         "D",  "DOUBLE DEFINED SYMBOL OR DUPLICATE PARAMETER NAME"},
    {Err_IllegalNesting,           "E",  "DEFINITION OR CONDITIONAL SEQUENCE ILLEGALLY NESTED"},
    {Err_TooManyEntries,           "F",  "TOO MANY ENTRIES"},
    {Err_InstructionPlacement,     "I",  "INSTRUCTION PLACEMENT ERROR"},
    {Err_LocationField,            "L",  "LOCATION FIELD ERROR"},
    {Err_RelocatableField,         "N",  "RELOCATABLE FIELD ERROR"},
    {Err_OperandField,             "O",  "OPERAND FIELD ERROR"},
    {Err_Programmer,               "P",  "PROGRAMMER ERROR"},
    {Err_ResultField,              "R",  "RESULT FIELD ERROR"},
    {Err_Syntax,                   "S",  "SYNTAX ERROR"},
    {Err_Type,                     "T",  "TYPE ERROR"},
    {Err_Undefined,                "U",  "UNDEFINED SYMBOL OR OPERATION"},
    {Err_FieldWidth,               "V",  "REGISTER EXPRESSION OR FIELD WIDTH ERROR"},
    {Err_Expression,               "X",  "EXPRESSION ERROR"},
    {Warn_Programmer,              "W",  "PROGRAMMER WARNING ERROR"},
    {Warn_IgnoredLocationSymbol,   "W1", "LOCATION FIELD SYMBOL IGNORED"},
    {Warn_BadLocationSymbol,       "W2", "BAD LOCATION SYMBOL"},
    {Warn_ExpressionElement,       "W3", "EXPRESSION ELEMENT TYPE ERROR"},
    {Warn_MachineInstruction,      "W4", "POSSIBLE SYMBOLIC MACHINE INSTRUCTION ERROR"},
    {Warn_Truncation,              "W5", "TRUNCATION ERROR"},
    {Warn_UndefinedLocationSymbol, "W6", "LOCATION FIELD SYMBOL NOT DEFINED"},
    {Warn_MicroSubstitution,       "W7", "MICRO SUBSTITUTION ERROR"},
    {Warn_AddressCounter,          "W8", "ADDRESS COUNTER BOUNDARY ERROR"},
    {Warn_ExternalDeclaration,     "Y1", "EXTERNAL DECLARATION ERROR"},
    {Warn_RedefinedMacro,          "Y2", "MACRO OR OPDEF REDEFINED"},
    {0,                            NULL, NULL}
};

static u64 errorRegistrations = 0;

void clearErrorIndications(void) {
    errorCount = 0;
    errorRegistrations = 0;
    errorUnion = 0;
    warningCount = 0;
}

ErrorCode getErrorCode(char *s, int len) {
    ErrorDefn *defn;

    defn = &errorDefns[1];
    while (defn->indicator != NULL) {
        if (strncasecmp(s, defn->indicator, len) == 0 && defn->indicator[len] == '\0') return defn->code;
        defn += 1;
    }
    return 0;
}

int getErrorCount(void) {
    return errorCount;
}

char *getErrorIndications(void) {
    ErrorCode code;
    static char indications[MAX_ERROR_INDICATIONS+1];
    char *indicator;
    char *ip;
    int len;
    char *limit;

    ip = &indications[0];
    limit = &indications[MAX_ERROR_INDICATIONS];
    for (code = Err_DataItem; code <= Warn_RedefinedMacro; code++) {
        if ((errorRegistrations & (1 << code)) != 0) {
            indicator = getErrorIndicator(code);
            len = strlen(indicator);
            if (ip + len < limit) {
                memcpy(ip, indicator, len);
                ip += len;
            }
            else {
                *ip++ = '+';
                break;
            }
        }
    }
    *ip = '\0';
    return indications;
}

char *getErrorIndicator(ErrorCode code) {
    return errorDefns[code].indicator;
}

char *getErrorMessage(ErrorCode code) {
    return errorDefns[code].message;
}

int getWarningCount(void) {
    return warningCount;
}

bool hasErrorRegistrations(void) {
    return errorRegistrations != 0;
}

void printErrorSummary(FILE *file) {
    ErrorCode code;

    if (file != NULL) {
        if (errorCount > 0) {
            fprintf(file, "\n%d ERROR%s", errorCount, (errorCount > 1) ? "S" : "");
        }
        if (warningCount > 0) {
            fprintf(file, "\n%d WARNING%s", warningCount, (warningCount > 1) ? "S" : "");
        }
        if (errorCount + warningCount > 0) fputs("\n", file);
        for (code = Err_DataItem; code <= Warn_RedefinedMacro; code++) {
            if ((errorUnion & (1 << code)) != 0) {
                fprintf(file, "\n%2s %s", getErrorIndicator(code), getErrorMessage(code));
            }
        }
        if (errorCount + warningCount > 0) fputs("\n", file);
    }
}

ErrorCode registerError(ErrorCode code) {
    u64 mask;

    if (code != Err_None) {
        mask = 1 << code;
        if ((errorRegistrations & mask) == 0) {
            errorRegistrations |= mask;
            errorUnion |= mask;
            if (code >= Err_DataItem && code <= Err_Expression)
                errorCount += 1;
            else if (code >= Warn_Programmer && code <= Warn_RedefinedMacro)
                warningCount += 1;
        }
    }
    return code;
}

void resetErrorRegistrations(void) {
    errorRegistrations = 0;
}
