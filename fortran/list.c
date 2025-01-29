/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: list.c
**
**  Description:
**      This file provides functions for generating the output listing.
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
#include "const.h"
#include "proto.h"
#include "types.h"

static char *baseTypeToStr(BaseType type);
static char *dataTypeToStr(DataType *dt);
static void listSymbol(Symbol *symbol);
static void listTree(Symbol *symbol);
static void resetHeaderLine(void);

#define LINES_PER_PAGE      55
#define LISTING_LINE_LENGTH 132
#define COL_VERSION         76
#define COL_CPU_TYPE        66
#define COL_DATE            96
#define COL_FORMAT_EFFECTOR 0
#define COL_PAGE            115
#define COL_SUBTITLE        1
#define COL_TIME            105
#define COL_TITLE           1

static char *cpuType = "Cray X-MP";
static char *ftcName = "kFTC";
static char *ftcVersion = "1.9.11";
static char headerLine[LISTING_LINE_LENGTH+2];
static int  lineNumber = LINES_PER_PAGE;
static int  pageNumber = 0;

static char *baseTypeToStr(BaseType type) {
    switch (type) {
    case BaseType_Undefined: return "Undefined";
    case BaseType_Logical:   return "Logical";
    case BaseType_Integer:   return "Integer";
    case BaseType_Real:      return "Real";
    case BaseType_Double:    return "Double";
    case BaseType_Complex:   return "Complex";
    case BaseType_Character: return "Character";
    case BaseType_Label:     return "Label";
    case BaseType_Pointer:   return "Pointer";
    default:                 return "Unknown";
    }
}

static char *dataTypeToStr(DataType *dt) {
    static char buf[32];
    char *name;

    name = baseTypeToStr(dt->type);
    if (dt->type != BaseType_Character) {
        return name;
    }
    else {
        if (dt->constraint > 0) {
            sprintf(buf, "%s*%d", name, dt->constraint);
            return buf;
        }
        else if (dt->constraint == 0) {
            return name;
        }
        else {
            sprintf(buf, "%s*(*)", name);
            return buf;
        }
    }
}

void list(char *format, ...) {
    va_list ap;

    if (listingFile != NULL && doList) {
        if (++lineNumber > LINES_PER_PAGE) {
            listEject();
            lineNumber = 1;
        }
        va_start(ap, format);
        vfprintf(listingFile, format, ap);
        va_end(ap);
        fputc('\n', listingFile);
    }
}

void listEject(void) {
    char buf[20];
    char *cp;
    char *hp;
    char *limit;

    if (listingFile != NULL && doList) {
        pageNumber += 1;
        resetHeaderLine();
        headerLine[0] = '1';
        if (cpuType != NULL) {
            hp = &headerLine[COL_CPU_TYPE];
            limit = &headerLine[COL_VERSION-1];
            cp = cpuType;
            while (*cp != '\0' && hp < limit) *hp++ = *cp++;
        }
        hp = &headerLine[COL_VERSION];
        limit = &headerLine[COL_DATE-2];
        cp = ftcName;
        while (*cp != '\0' && hp < limit) *hp++ = *cp++;
        hp += 1;
        cp = ftcVersion;
        while (*cp != '\0' && hp < limit) *hp++ = *cp++;
        hp = &headerLine[COL_DATE];
        cp = currentDate;
        while (*cp != '\0') *hp++ = *cp++;
        hp = &headerLine[COL_TIME];
        cp = currentTime;
        while (*cp != '\0') *hp++ = *cp++;
        sprintf(buf, "PAGE %4d", pageNumber);
        hp = &headerLine[COL_PAGE];
        cp = buf;
        while (*cp != '\0') *hp++ = *cp++;
        *hp++ = '\n';
        *hp   = '\0';
        fputs(headerLine, listingFile);
        fputs("\n\n\n", listingFile);
    }
}

void listSetPageEnd(void) {
    lineNumber = LINES_PER_PAGE;
}

static void listSymbol(Symbol *symbol) {
    int size;

    fprintf(listingFile, "  %-31s", symbol->identifier);
    if (symbol->class == SymClass_Undefined && symbol->isFnRef) {
        fprintf(listingFile, " %-10s", symClassToStr(SymClass_Function));
    }
    else {
        fprintf(listingFile, " %-10s", symClassToStr(symbol->class));
    }
    switch (symbol->class) {
    case SymClass_Undefined:
    case SymClass_Function:
    case SymClass_StmtFunction:
    case SymClass_Auto:
    case SymClass_Static:
    case SymClass_Adjustable:
    case SymClass_Global:
    case SymClass_Argument:
    case SymClass_Pointee:
    case SymClass_Parameter:
        fprintf(listingFile, " %-14s", dataTypeToStr(&symbol->details.variable.dt));
        size = calculateSize(symbol);
        if (size > 0)
            fprintf(listingFile, " %-7d", size);
        else
            fputs("        ", listingFile);
        switch (symbol->class) {
        case SymClass_Auto:
        case SymClass_Static:
        case SymClass_Adjustable:
        case SymClass_Global:
        case SymClass_Argument:
            if (symbol->details.variable.dt.type == BaseType_Character && symbol->details.variable.dt.firstChrOffset != 0) {
                fprintf(listingFile, " %6d:%d", symbol->details.variable.offset, symbol->details.variable.dt.firstChrOffset);
            }
            else {
                fprintf(listingFile, " %8d", symbol->details.variable.offset);
            }
            break;
        case SymClass_Pointee:
            fprintf(listingFile, " %8s", symbol->details.pointee.pointer->identifier);
            break;
        case SymClass_Function:
            if (symbol->details.progUnit.offset != 0)
                fprintf(listingFile, " %8d", symbol->details.progUnit.offset);
            break;
        default:
            fputs("         ", listingFile);
            break;
        }
        if (symbol->class == SymClass_Global) {
            fprintf(listingFile, " /%s/", symbol->details.variable.staticBlock->identifier);
        }
        break;
    case SymClass_Intrinsic:
        fprintf(listingFile, " %-14s", baseTypeToStr(symbol->details.intrinsic.resultType));
        break;
    default:
        break;
    }
    fputc('\n', listingFile);
}

void listSymbols(void) {

    if (listingFile != NULL && doList) {
        if (lineNumber + 8 > LINES_PER_PAGE) {
            listEject();
        }
        else {
            list(" ");
            list(" ");
        }
        list("  Symbols");
        list("  Name                            Class      Type           Size    Location Common");
        list("  ------------------------------- ---------- -------------- ------- -------- --------");
        listTree(symbols);
    }
}

static void listTree(Symbol *symbol) {
    if (symbol != NULL) {
        listTree(symbol->left);
        if (symbol->isDeleted == FALSE) {
            if (++lineNumber > LINES_PER_PAGE) {
                listEject();
                lineNumber = 1;
            }
            listSymbol(symbol);
        }
        listTree(symbol->right);
    }
}

static void resetHeaderLine(void) {
    memset(headerLine, ' ', LISTING_LINE_LENGTH);
    headerLine[LISTING_LINE_LENGTH]   = '\n';
    headerLine[LISTING_LINE_LENGTH+1] = '\0';
}

char *symClassToStr(SymbolClass class) {
    switch (class) {
    case SymClass_Undefined:    return "Undefined";
    case SymClass_Program:      return "Program";
    case SymClass_BlockData:    return "Block Data";
    case SymClass_Subroutine:   return "Subroutine";
    case SymClass_Function:     return "Function";
    case SymClass_StmtFunction: return "Function";
    case SymClass_Intrinsic:    return "Intrinsic";
    case SymClass_External:     return "External";
    case SymClass_NamedCommon:  return "Common";
    case SymClass_Auto:         return "Auto";
    case SymClass_Static:       return "Static";
    case SymClass_Adjustable:   return "Adjustable";
    case SymClass_Global:       return "Common";
    case SymClass_Argument:     return "Argument";
    case SymClass_Pointee:      return "Pointee";
    case SymClass_Parameter:    return "Parameter";
    default:                    return "Unknown";
    }
}
