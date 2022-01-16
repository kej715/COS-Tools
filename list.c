/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "proto.h"
#include "types.h"

static bool isListSuppressed(void);
static void listPageHeader(Section *section);
static void listQualifiers(Qualifier *qualifier);
static void listSymbols(Symbol *symbol);
static void resetHeaderLine(void);
static void resetListingLine(void);

#define LINES_PER_PAGE      55
#define LISTING_LINE_LENGTH 132
#define COL_CAL_VERSION     76
#define COL_CPU_TYPE        66
#define COL_DATE            96
#define COL_FORMAT_EFFECTOR 0
#define COL_ERRORS          1
#define COL_LOCATION        9
#define COL_CODE            19
#define COL_PAGE            115
#define COL_QUALIFIER       96
#define COL_SECTION         74
#define COL_SOURCE          44
#define COL_SUBTITLE        1
#define COL_TIME            105
#define COL_TITLE           1

static char *cpuType = "Cray X-MP";
static Section dummySection;
static char headerLine[LISTING_LINE_LENGTH+2];
static int  lineNumber = 0;
static char listingLine[LISTING_LINE_LENGTH+2];
static char parcelIndicator[4] = {'a', 'b', 'c', 'd'};

static bool isListSuppressed(void) {
    if (pass == 1 || listingFile == NULL) return TRUE;
    return (currentListControl & listControlMask) != listControlMask && hasErrorRegistrations() == FALSE;
}

void listClearSource(void) {
    char *cp;
    char *limit;

    if (isListSuppressed()) return;
    cp = &listingLine[COL_SOURCE];
    limit = &listingLine[LISTING_LINE_LENGTH];
    while (cp < limit) *cp++ = ' ';
}

void listCode(u64 bits, int count, int lastCol) {
    if (isListSuppressed()) return;
    listingLine[lastCol--] = '0' + (bits & 07);
    bits >>= 3;
    count -= 3;
    while (count > 0) {
        listingLine[lastCol--] = '0' + (bits & 07);
        bits >>= 3;
        count -= 3;
    }
}

void listCode16(u16 bits) {
    if (isListSuppressed()) return;
    listCode(bits, 16, COL_CODE + 5);
}

void listCode10_22(u32 bits, u16 attributes) {
    u32 jkm;

    if (isListSuppressed()) return;
    jkm = bits & 0x3fffff;
    listCode(bits >> 22, 10, COL_CODE + 3);
    if ((attributes & SYM_PARCEL_ADDRESS) != 0) {
        listCode(jkm >> 2, 22, COL_CODE + 12);
        listingLine[COL_CODE + 13] = parcelIndicator[jkm & 0x03];
        if ((attributes & SYM_RELOCATABLE) != 0)
            listingLine[COL_CODE + 14] = '+';
    }
    else if ((attributes & SYM_WORD_ADDRESS) != 0) {
        listCode(jkm, 22, COL_CODE + 12);
        listingLine[COL_CODE + 13] = 'a';
        if ((attributes & SYM_RELOCATABLE) != 0)
            listingLine[COL_CODE + 14] = '+';
    }
    else {
        listCode(bits & 0x3fffff, 22, COL_CODE + 12);
    }
}

void listCode7_24(u32 bits, u16 attributes) {
    u32 jkm;

    if (isListSuppressed()) return;
    jkm = bits & 0xffffff;
    listCode(bits >> 25, 7, COL_CODE + 2);
    if ((attributes & SYM_PARCEL_ADDRESS) != 0) {
        listCode(jkm >> 2, 22, COL_CODE + 11);
        listingLine[COL_CODE + 12] = parcelIndicator[bits & 0x03];
        if ((attributes & SYM_RELOCATABLE) != 0)
            listingLine[COL_CODE + 13] = '+';
    }
    else if ((attributes & SYM_WORD_ADDRESS) != 0) {
        listCode(jkm, 24, COL_CODE + 11);
        listingLine[COL_CODE + 12] = parcelIndicator[bits & 0x03];
        if ((attributes & SYM_RELOCATABLE) != 0)
            listingLine[COL_CODE + 13] = '+';
    }
    else {
        listCode(jkm, 24, COL_CODE + 11);
    }
}

void listCodeLocation(Section *section) {
    listLocation(section->originCounter);
}

void listEject(void) {
    int page;

    if (isListSuppressed()) return;
    if ((lineNumber % LINES_PER_PAGE) != 0) {
        page = (lineNumber / LINES_PER_PAGE);
        lineNumber = (page + 1) * LINES_PER_PAGE;
    }
}

void listErrorIndications(void) {
    char *cp;
    char *indications;

    if (isListSuppressed()) return;
    indications = getErrorIndications();
    cp = &listingLine[COL_ERRORS];
    while (*indications != '\0') *cp++ = *indications++;
}

void listErrorSummary(void) {
    ErrorCode code;

    if (isListSuppressed()) return;
    listEject();
    strcpy(subtitle, "ERROR SUMMARY");
    if (errorCount > 0) {
        sprintf(listingLine, " %d ERROR%s\n", errorCount, (errorCount > 1) ? "S" : "");
        listFlush(&dummySection);
    }
    if (warningCount > 0) {
        sprintf(listingLine, " %d WARNING%s\n", warningCount, (warningCount > 1) ? "S" : "");
        listFlush(&dummySection);
    }
    if (errorCount + warningCount > 0) {
        listFlush(&dummySection);
        for (code = Err_DataItem; code <= Warn_RedefinedMacro; code++) {
            if ((errorUnion & (1 << code)) != 0) {
                sprintf(listingLine, " %-2s %s\n", getErrorIndicator(code), getErrorMessage(code));
                listFlush(&dummySection);
            }
        }
    }
}

void listField(u64 bits, int len, u16 attributes, int colOffset) {
    listCode(bits, len, COL_CODE + colOffset);
}

void listFlush(Section *section) {
    if (pass == 1) return;
    if (isListSuppressed() == FALSE) {
        if ((lineNumber % LINES_PER_PAGE) == 0) listPageHeader(section);
        fputs(listingLine, listingFile);
        lineNumber += 1;
    }
    resetListingLine();
}

void listInit(void) {
    memset(&dummySection, 0, sizeof(Section));
    dummySection.id = "";
    title[0] = subtitle[0] = '\0';
    resetListingLine();
}

void listLocation(u32 location) {
    int i;
    char *cp;
    char *indications;

    if (isListSuppressed()) return;
    i = COL_LOCATION + 8;
    listingLine[i--] = parcelIndicator[location & 0x03];
    location >>= 2;
    listingLine[i--] = '0' + (location & 07);
    location >>= 3;
    while (location != 0) {
        listingLine[i--] = '0' + (location & 07);
        location >>= 3;
    }
}

static void listPageHeader(Section *section) {
    char buf[20];
    char *cp;
    char *hp;
    char *limit;

    if (listingFile != NULL && pass == 2) {
        resetHeaderLine();
        headerLine[0] = '1';
        if (title[0] != '\0') {
            hp = &headerLine[COL_TITLE];
            limit = &headerLine[COL_CPU_TYPE-2];
            cp = title;
            while (*cp != '\0' && hp < limit) *hp++ = *cp++;
        }
        if (cpuType != NULL) {
            hp = &headerLine[COL_CPU_TYPE];
            limit = &headerLine[COL_CAL_VERSION-1];
            cp = cpuType;
            while (*cp != '\0' && hp < limit) *hp++ = *cp++;
        }
        hp = &headerLine[COL_CAL_VERSION];
        limit = &headerLine[COL_DATE-2];
        cp = calName;
        while (*cp != '\0' && hp < limit) *hp++ = *cp++;
        hp += 1;
        cp = calVersion;
        while (*cp != '\0' && hp < limit) *hp++ = *cp++;
        hp = &headerLine[COL_DATE];
        cp = currentDate;
        while (*cp != '\0') *hp++ = *cp++;
        hp = &headerLine[COL_TIME];
        cp = currentTime;
        while (*cp != '\0') *hp++ = *cp++;
        sprintf(buf, "PAGE %4d", (lineNumber / LINES_PER_PAGE) + 1);
        hp = &headerLine[COL_PAGE];
        cp = buf;
        while (*cp != '\0') *hp++ = *cp++;
        *hp++ = '\n';
        *hp   = '\0';
        fputs(headerLine, listingFile);
    
        resetHeaderLine();
        headerLine[0] = ' ';
        if (subtitle[0] != '\0') {
            hp = &headerLine[COL_SUBTITLE];
            limit = &headerLine[COL_SECTION-2];
            cp = subtitle;
            while (*cp != '\0' && hp < limit) *hp++ = *cp++;
        }
        sprintf(buf, "SECTION: %s", section->id);
        hp = &headerLine[COL_SECTION];
        limit = &headerLine[COL_QUALIFIER-2];
        cp = buf;
        while (*cp != '\0' && hp < limit) *hp++ = *cp++;
        sprintf(buf, "QUALIFIER: %s", currentQualifier->id);
        hp = &headerLine[COL_QUALIFIER];
        limit = &headerLine[LISTING_LINE_LENGTH];
        cp = buf;
        while (*cp != '\0' && hp < limit) *hp++ = *cp++;
        *hp++ = '\n';
        *hp   = '\0';
        fputs(headerLine, listingFile);
        fputs("\n\n", listingFile);
    }
    lineNumber += 4;
}

static void listQualifiers(Qualifier *qualifier) {
    if (qualifier != NULL) {
        listQualifiers(qualifier->left);
        currentQualifier = qualifier;
        listEject();
        listSymbols(qualifier->symbols);
        listQualifiers(qualifier->right);
    }
}

void listSource(void) {
    char *cp;
    char *limit;
    char *sp;

    if (isListSuppressed()) return;
    sp = sourceLine;
    cp = &listingLine[COL_SOURCE];
    limit = &listingLine[LISTING_LINE_LENGTH];
    while (*sp != '\0' && cp < limit) *cp++ = *sp++;
}

static void listSymbols(Symbol *symbol) {
    int col;

    if (symbol != NULL) {
        listSymbols(symbol->left);
        if ((symbol->value.attributes & SYM_COUNTER) == 0) {
            sprintf(listingLine, " %-8s ", symbol->id);
            col = 10;
            if (symbol->value.section != NULL) {
                sprintf(&listingLine[col], " %-8s ", symbol->value.section->id);
            }
            else {
                memset(&listingLine[col], ' ', 10);
            }
            col += 10;
            listingLine[col++] = ((symbol->value.attributes & SYM_REDEFINABLE) != 0) ? 'R' : ' ';
            if ((symbol->value.attributes & SYM_WORD_ADDRESS) != 0)
                listingLine[col++] = 'W';
            else if ((symbol->value.attributes & SYM_PARCEL_ADDRESS) != 0)
                listingLine[col++] = 'P';
            else
                listingLine[col++] = 'V';
            if ((symbol->value.attributes & SYM_EXTERNAL) != 0)
                listingLine[col++] = 'X';
            else if ((symbol->value.attributes & SYM_RELOCATABLE) != 0)
                listingLine[col++] = '+';
            else if ((symbol->value.attributes & SYM_IMMOBILE) != 0)
                listingLine[col++] = 'I';
            else
                listingLine[col++] = ' ';
            listingLine[col++] = isCommonSection(symbol->value.section) ? 'C' : ' ';
            col += 2;
            if ((symbol->value.attributes & SYM_PARCEL_ADDRESS) != 0)
                sprintf(&listingLine[col], "%lo%c\n", symbol->value.intValue >> 2, parcelIndicator[symbol->value.intValue & 0x03]);
            else
                sprintf(&listingLine[col], "%lo\n", symbol->value.intValue);
            listFlush(&dummySection);
        }
        listSymbols(symbol->right);
    }
}

void listSymbolTable() {
    if (isListSuppressed()) return;
    strcpy(subtitle, " SYMBOL TABLE");
    listQualifiers(currentModule->qualifiers);
}

void listValue(Value *val) {
    int i;
    char *cp;
    int len;
    i64 n;

    if (isListSuppressed()) return;
    if (val->type == NumberType_Integer) {
        i = COL_CODE + 21;
        n = (val->intValue < 0) ? -val->intValue : val->intValue;
        listingLine[i--] = '0' + (n & 07);
        n >>= 3;
        while (n != 0) {
            listingLine[i--] = '0' + (n & 07);
            n >>= 3;
        }
        if (val->intValue < 0) listingLine[i] = '-';
    }
    else { // NumberType_Float
        char buf[20];
        len = sprintf(buf, "%g", val->floatValue);
        memcpy(&listingLine[COL_CODE + 22 - len], buf, len);
    }
}

void listWord(u64 bits, u16 attributes) {
    if ((attributes & SYM_PARCEL_ADDRESS) != 0) {
        listCode(bits >> 2, 64, COL_CODE + 21);
        listingLine[COL_CODE + 22] = parcelIndicator[bits & 0x03];
        if ((attributes & SYM_RELOCATABLE) != 0)
            listingLine[COL_CODE + 23] = '+';
    }
    else if ((attributes & SYM_WORD_ADDRESS) != 0) {
        listCode(bits, 64, COL_CODE + 21);
        listingLine[COL_CODE + 22] = parcelIndicator[bits & 0x03];
        if ((attributes & SYM_RELOCATABLE) != 0)
            listingLine[COL_CODE + 23] = '+';
    }
    else {
        listCode(bits, 64, COL_CODE + 21);
    }

}

static void resetHeaderLine(void) {
    memset(headerLine, ' ', LISTING_LINE_LENGTH);
    headerLine[LISTING_LINE_LENGTH]   = '\n';
    headerLine[LISTING_LINE_LENGTH+1] = '\0';
}

static void resetListingLine(void) {
    memset(listingLine, ' ', LISTING_LINE_LENGTH);
    listingLine[LISTING_LINE_LENGTH]   = '\n';
    listingLine[LISTING_LINE_LENGTH+1] = '\0';
}
