/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: fmt.c
**
**  Description:
**      This file contains functions supporting formatted I/O.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "const.h"
#include "fmt.h"

#if defined(__ACK)
char *_ecvt(long double value, int ndigit, int* decpt, int* sign);
char *_fcvt(long double value, int ndigit, int* decpt, int* sign);
#define ecvt _ecvt
#define fcvt _fcvt
#endif

#define DEBUG 1

static FormatDesc *currentParent;
static char       *currentRecord;
static char       *cursor;
static int        descIdx;
static FormatDesc descriptors[MAX_FMT_DESC];
static int        doPlusSigns;
static FormatDesc *firstDesc;
static char       fmtBuf[MAX_FMT_LEN+1];
static int        isBlankZero;
static char       *limit;
static FormatDesc *nextDesc;
static char       record[MAX_FMT_RECL+1];
static FormatDesc *revertDesc;
static int        scaleFactor;

static FormatClass alphaToFmtClass[] = {
    Fmt_A,Fmt_B,    0,Fmt_D,Fmt_E,Fmt_F,Fmt_G,    0,Fmt_I,    0,    0,Fmt_L,    0,    0,Fmt_O,    0,
        0,Fmt_R,Fmt_S,Fmt_T,    0,    0,    0,Fmt_X,    0,Fmt_Z,    0,    0,    0,    0,    0,    0,
    Fmt_A,Fmt_B,    0,Fmt_D,Fmt_E,Fmt_F,Fmt_G,    0,Fmt_I,    0,    0,Fmt_L,    0,    0,Fmt_O,    0,
        0,Fmt_R,Fmt_S,Fmt_T,    0,    0,    0,Fmt_X,    0,Fmt_Z,    0,    0,    0,    0,    0,    0
};

static char *fmtClasses[] = {
    "A","B","BN","BZ", "D", "E","F", "G","I","L","P",
    "O","R", "S","SP","SS","T","TL","TR","X","Z",
    "/", ":", "$",
    "String", "Embedded"
};

static char hexDigits[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

static double powers10[20] = {
    1E00,1E01,1E02,1E03,1E04,1E05,1E06,1E07,1E08,1E09,
    1E10,1E11,1E12,1E13,1E14,1E15,1E16,1E17,1E18,1E19
};

static FormatDesc *allocDesc(void);
static char *eatWsp(char *s);
static void endfmtHelper(FormatDesc *fdp);
static char *fmtClassToStr(FormatClass class);
static void fmtInt(unsigned long value, int radix);
static void fmtReal(double value, FormatDesc *fdp);
static void fmtRealE(double value, FormatDesc *fdp);
static void fmtRealF(double value, FormatDesc *fdp);
static void fmtRealG(double value, FormatDesc *fdp);
static char *getPrecision(char *s, FormatDesc *fdp);
static void outfmtHelper(DataValue *value, int doEndOnRep, int *eor);
static char *parseInteger(char *s, int *value);
static char *prsfmtHelper(char *s, FormatDesc **list);

void _endfmt(void) {
    endfmtHelper(firstDesc);
}

void _infmt(DataValue *value) {
}

void _inircd(void) {
    char *cp;

    for (cp = currentRecord; *cp != '\0'; cp++) *cp = ' ';
    cursor = record;
}

FormatDesc *_getfdl(void) {
    return firstDesc;
}

char *_getrcd(void) {
    return currentRecord;
}

void _inpchr(int unitNum, unsigned long ref) {
}

void _inpdbl(int unitNum, DataValue *ref) {
}

void _inpint(int unitNum, DataValue *ref) {
}

void _inplog(int unitNum, DataValue *ref) {
}

void _lstchr(int unitNum, unsigned long value) {
    int len;
    char *s;

    s = (char *)(value & 0xffffffff);
    len = value >> 32;
    while (len-- > 0) {
        if (cursor < limit) *cursor++ = *s++;
    }
}

void _lstdbl(int unitNum, DataValue *value) {
    char buf[16];
    int decpt;
    double doubleValue;
    char *ep;
    int exp;
    int ignore;
    int isNegative;
    int len;
    char *s;
    char *s2;

    doubleValue = value->real;
    if (doubleValue < 0) {
        isNegative = 1;
        doubleValue = -doubleValue;
    }
    else {
        isNegative = 0;
    }
    if (cursor < limit) *cursor++ = ' ';
    if (isNegative && cursor < limit) *cursor++ = '-';
    if (doubleValue == 0.0) {
        if (cursor < limit) *cursor++ = '0';
        if (cursor < limit) *cursor++ = '.';
    }
    else if (doubleValue >= 1.0E-6 && doubleValue <= 1.0E+9) {
        s = fcvt(doubleValue, 14, &decpt, &ignore);
        while (decpt < 0) {
            if (cursor < limit) *cursor++ = '0';
            decpt += 1;
        }
        while (decpt > 0 && *s != '\0') {
            if (cursor < limit) *cursor++ = *s++;
            decpt -= 1;
        }
        if (cursor < limit) *cursor++ = '.';
        len = strlen(s);
        s2 = s + len - 1;
        while (s2 >= s && *s2 == '0') *s2-- = '\0';
        while (cursor < limit && *s != '\0') *cursor++ = *s++;
    }
    else {
        s = ecvt(doubleValue, 14, &decpt, &ignore);
        ep = buf + sizeof(buf) - 1;
        *ep-- = '\0';
        exp = decpt - 1;
        if (exp < 0) exp = -exp;
        do {
            *ep-- = '0' + (exp % 10);
            exp /= 10;
        } while (exp != 0);
        *ep-- = (decpt >= 0) ? '+' : '-';
        *ep = 'E';
        if (cursor < limit) *cursor++ = *s;
        s += 1;
        if (cursor < limit) *cursor++ = '.';
        s2 = s + strlen(s) - 1;
        while (s2 > s && *s2 == '0') *s2-- = '\0';
        while (cursor < limit && *s != '\0') *cursor++ = *s++;
        while (cursor < limit && *ep != '\0') *cursor++ = *ep++;
    }
}

void _lstint(int unitNum, DataValue *value) {
    char buf[33];
    long intValue;
    int isNegative;
    char *s;

    intValue = value->integer;
    isNegative = 0;
    if (intValue < 0) {
        isNegative = 1;
        intValue = -intValue;
    }
    s = buf + sizeof(buf) - 1;
    *s-- = '\0';
    do {
        *s-- = '0' + (intValue % 10);
        intValue /= 10;
    }
    while (intValue != 0);
    if (isNegative) {
        *s = '-';
    }
    else {
        s += 1;
    }
    if (cursor < limit) *cursor++ = ' ';
    while (cursor < limit && *s != '\0') *cursor++ = *s++;
}

void _lstlog(int unitNum, DataValue *value) {
    char lc;

    lc = (value->logical == 0) ? 'F' : 'T';
    if (cursor < limit) *cursor++ = ' ';
    if (cursor < limit) *cursor++ = lc;
}

void _outfin(int *eor) {
    outfmtHelper((DataValue *)0, 1, eor);
    if (*eor == 0) _endfmt();
}

void _outfmt(DataValue *value, int *eor) {
    outfmtHelper(value, 0, eor);
}

void _przfmt(char *s) {
    descIdx = 0;
    currentParent = NULL;
    revertDesc = NULL;
    doPlusSigns = 0;
    isBlankZero = 0;
    scaleFactor = 0;
    s = prsfmtHelper(s, &firstDesc);
    s = eatWsp(s);
    if (*s != '\0') {
        fputs("Cruft after closing ')' of FORMAT list\n", stderr);
        exit(1);
    }
    nextDesc = descriptors;
    if (revertDesc == NULL) revertDesc = descriptors;
    memset(record, ' ', MAX_FMT_RECL);
    record[MAX_FMT_RECL] = '\0';
    _setrcd(record);
}

void _prslst(void) {
    descIdx = 0;
    currentParent = NULL;
    nextDesc = NULL;
    revertDesc = NULL;
    doPlusSigns = 0;
    isBlankZero = 0;
    scaleFactor = 0;
    memset(record, ' ', MAX_FMT_RECL);
    record[MAX_FMT_RECL] = '\0';
    _setrcd(record);
}

void _setfdl(FormatDesc *fdp) {
    firstDesc = fdp;
}

void _setrcd(char *record) {
    currentRecord = record;
    cursor = record;
    limit  = record + strlen(record);
}

void _setrfd(FormatDesc *fdp) {
    revertDesc = fdp;
}

static FormatDesc *allocDesc(void) {
    FormatDesc *fdp;

    if (descIdx < MAX_FMT_DESC) {
        fdp = &descriptors[descIdx++];
        memset(fdp, 0, sizeof(FormatDesc));
        return fdp;
    }
    else {
        fputs("Too many format descriptors\n", stderr);
        exit(1);
    }
}

static char *eatWsp(char *s) {
    while (*s != '\0' && isspace(*s)) s += 1;
    return s;
}

static void endfmtHelper(FormatDesc *fdp) {
    while (fdp != NULL) {
        if (fdp->string != NULL) free(fdp->string);
        if (fdp->child  != NULL) endfmtHelper(fdp->child);
        fdp = fdp->sibling;
    }
}

static char *fmtClassToStr(FormatClass class) {
    return (class <= Fmt_Embedded) ? fmtClasses[class] : "unknown";
}

static void fmtInt(unsigned long value, int radix) {
    char buf[32];
    char *cp;
    int fieldWidth;
    int isNegative;
    int minDigits;
    int n;
    char *s;
    char *s2;
    int sign;
    long sval;

    isNegative = 0;
    sign = 0;
    s = buf + sizeof(buf) - 1;
    switch (radix) {
    case 8:
        do {
            *s-- = '0' + (value & 0x07);
            value >>= 3;
        }
        while (value != 0);
        break;
    case 16:
        do {
            *s-- = hexDigits[value & 0x0f];
            value >>= 4;
        }
        while (value != 0);
        break;
    default:
        sign = doPlusSigns;
        sval = (long)value;
        if (sval < 0) {
            sval = -sval;
            isNegative = 1;
            sign = 1;
        }
        do {
            *s-- = '0' + (sval % 10);
            sval /= 10;
        }
        while (sval != 0);
        break;
    }
    minDigits = sizeof(buf) - ((s + 1) - buf);
    if (nextDesc->minDigits > minDigits) minDigits = nextDesc->minDigits;
    fieldWidth = minDigits + sign;
    if (fieldWidth < nextDesc->width) fieldWidth = nextDesc->width;
    cp = cursor + fieldWidth - 1;
    if (nextDesc->width != 0 && nextDesc->width < fieldWidth) {
        fieldWidth = nextDesc->width;
        for (n = fieldWidth; n > 0; n--) {
            if (cp < limit) *cp = '*';
            cp -= 1;
        }
    }
    else {
        s2 = (buf + sizeof(buf)) - 1;
        while (s2 > s) {
            if (cp < limit) *cp = *s2;
            cp -= 1;
            s2 -= 1;
            minDigits -= 1;
        }
        while (minDigits-- > 0) {
            if (cp < limit) *cp = '0';
            cp -= 1;
        }
        if (sign && cp < limit) {
            *cp = isNegative ? '-' : '+';
        }
    }
    cursor += fieldWidth;
}

static void fmtReal(double value, FormatDesc *fdp) {
    switch (fdp->class) {
    case Fmt_F:
        fmtRealF(value, fdp);
        break;
    case Fmt_D:
    case Fmt_E:
        fmtRealE(value, fdp);
        break;
    case Fmt_G:
        fmtRealG(value, fdp);
        break;
    default:
        break;
    }
}

static void fmtRealE(double value, FormatDesc *fdp) {
    char buf[16];
    char *bufLim;
    char *cp;
    int decpt;
    char *ep;
    int exp;
    int expLength;
    int fieldWidth;
    int isNegative;
    int len;
    int n;
    char *s;
    char *s2;
    int sign;

    s = ecvt(value, fdp->minDigits, &decpt, &isNegative);
    len = strlen(s);
    expLength = (fdp->expLength == 0) ? 2 : fdp->expLength;
    sign = isNegative || doPlusSigns;
    fieldWidth = len + sign + 2;
    fieldWidth += expLength;
    ep = bufLim = buf + sizeof(buf) - 1;
    exp = decpt - 1;
    if (exp < 0) exp = -exp;
    n = expLength;
    do {
        *ep-- = '0' + (exp % 10);
        exp /= 10;
        n -= 1;
    } while (exp != 0);
    while (n-- > 0) *ep-- = '0';
    *ep-- = (decpt >= 0) ? '+' : '-';
    *ep-- = 'E';
    if (fieldWidth < fdp->width) fieldWidth = fdp->width;
    cp = cursor + fieldWidth - 1;
    if ((fdp->width != 0 && fdp->width < fieldWidth)
        || ((expLength + 2) < (bufLim - ep)))  {
        fieldWidth = fdp->width;
        for (n = fieldWidth; n > 0; n--) {
            if (cp < limit) *cp = '*';
            cp -= 1;
        }
    }
    else {
        s2 = bufLim;
        while (s2 > ep) {
            if (cp < limit) *cp = *s2;
            cp -= 1;
            s2 -= 1;
        }
        s2 = s + len - 1;
        while (s2 > s) {
            if (cp < limit) *cp = *s2;
            cp -= 1;
            s2 -= 1;
        }
        if (cp < limit) *cp = '.';
        cp -= 1;
        if (cp < limit) *cp = *s;
        cp -= 1;
        if (sign && cp < limit) {
            *cp = isNegative ? '-' : '+';
        }
    }
    cursor += fieldWidth;
}

static void fmtRealF(double value, FormatDesc *fdp) {
    char *cp;
    int decpt;
    int fieldWidth;
    int isNegative;
    int len;
    int n;
    char *s;
    char *s2;
    char *s3;
    int sign;

    s = fcvt(value, fdp->minDigits, &decpt, &isNegative);
    len = strlen(s);
    sign = isNegative || doPlusSigns;
    fieldWidth = len + sign + 1;
    if (decpt == 0) {
        fieldWidth += 1;
    }
    else if (decpt < 0) {
        fieldWidth += -decpt + 1;
    }
    if (fieldWidth < fdp->width) fieldWidth = fdp->width;
    cp = cursor + fieldWidth - 1;
    if (fdp->width != 0 && fdp->width < fieldWidth) {
        fieldWidth = fdp->width;
        for (n = fieldWidth; n > 0; n--) {
            if (cp < limit) *cp = '*';
            cp -= 1;
        }
    }
    else {
        s2 = s + len - 1;
        s3 = (decpt > 0) ? s + decpt : s;
        while (s2 >= s3) {
            if (cp < limit) *cp = *s2;
            cp -= 1;
            s2 -= 1;
        }
        for (n = decpt; n < 0; n++) {
            if (cp < limit) *cp = '0';
            cp -= 1;
        }
        if (cp < limit) *cp = '.';
        cp -= 1;
        while (s2 >= s) {
            if (cp < limit) *cp = *s2;
            cp -= 1;
            s2 -= 1;
        }
        if (decpt <= 0) {
            if (cp < limit) *cp = '0';
            cp -= 1;
        }
        if (sign && cp < limit) {
            *cp = isNegative ? '-' : '+';
        }
    }
    cursor += fieldWidth;
}

static void fmtRealG(double value, FormatDesc *fdp) {
    int expLength;
    FormatDesc fd;

    if (value >= 0.1 && fdp->minDigits < 20 && value < powers10[fdp->minDigits]) {
        expLength = (fdp->expLength == 0) ? 2 : fdp->expLength;
        expLength += 2; /* E+ */
        fd = *fdp;
        fd.width -= expLength;
        fmtRealF(value, &fd);
        while (expLength-- > 0) {
            if (cursor < limit) *cursor = ' ';
            cursor += 1;
        }
    }
    else {
        fmtRealE(value, fdp);
    }
}

static char *getPrecision(char *s, FormatDesc *fdp) {
    if (isdigit(*s)) {
        s = parseInteger(s, &fdp->width);
        if (fdp->width < 1) {
            fprintf(stderr, "Invalid width specified for '%s' format descriptor\n", fmtClassToStr(fdp->class));
            exit(1);
        }
        switch (fdp->class) {
        case Fmt_D:
        case Fmt_E:
        case Fmt_F:
        case Fmt_G:
        case Fmt_I:
        case Fmt_O:
        case Fmt_Z:
            if (*s == '.') {
                s += 1;
                if (isdigit(*s)) {
                    s = parseInteger(s, &fdp->minDigits);
                    if ((fdp->class == Fmt_E || fdp->class == Fmt_G) && (*s == 'E' || *s == 'e')) {
                        s += 1;
                        if (isdigit(*s)) {
                            s = parseInteger(s, &fdp->expLength);
                        }
                        else {
                            fprintf(stderr, "Invalid '%s' format descriptor\n", fmtClassToStr(fdp->class));
                            exit(1);
                        }
                    }
                }
                else {
                    fprintf(stderr, "Invalid '%s' format descriptor\n", fmtClassToStr(fdp->class));
                    exit(1);
                }
            }
            break;
        default:
            /* do nothing */
            break;
        }
    }
    return s;
}

static void outfmtHelper(DataValue *value, int doEndOnRep, int *eor) {
    unsigned long charRef;
    int fieldWidth;
    int len;
    int n;
    char *s;

    *eor = FALSE;

    if (nextDesc == NULL) return;

    for (;;) {
        if (++nextDesc->currentIteration > nextDesc->repeatCount) {
            if (nextDesc->sibling != NULL) {
                nextDesc = nextDesc->sibling;
            }
            else if (nextDesc->parent != NULL) {
                nextDesc = nextDesc->parent;
            }
            else if (doEndOnRep == 0) {
                nextDesc = revertDesc;
            }
            else {
                return;
            }
            continue;
        }
        switch (nextDesc->class) {
        case Fmt_A:
            if (doEndOnRep == 0) {
                charRef = (unsigned long)value;
                s = (char *)(charRef & 0xffffffff);
                len = charRef >> 32;
                fieldWidth = nextDesc->width;
                if (len < fieldWidth) {
                    n = fieldWidth - len;
                    fieldWidth -= n;
                    while (n-- > 0) {
                        if (cursor < limit) *cursor++ = ' ';
                    }
                }
                while (fieldWidth > 0 && len > 0) {
                    if (cursor < limit) *cursor++ = *s++;
                    fieldWidth -= 1;
                    len -= 1;
                }
            }
            return;
        case Fmt_B:
        case Fmt_BN:
            isBlankZero = 0;
            break;
        case Fmt_BZ:
            isBlankZero = 1;
            break;
        case Fmt_D:
        case Fmt_E:
        case Fmt_F:
        case Fmt_G:
            if (doEndOnRep == 0) {
                fmtReal((double)value->real, nextDesc);
            }
            return;
        case Fmt_I:
            if (doEndOnRep == 0) {
                fmtInt(value->integer, 10);
            }
            return;
        case Fmt_L:
            if (doEndOnRep == 0) {
                fieldWidth = (nextDesc->width == 0) ? 1 : nextDesc->width;
                s = cursor + fieldWidth - 1;
                if (s < limit) *s = (value->logical == 0) ? 'F' : 'T';
                cursor += fieldWidth;
            }
            return;
        case Fmt_O:
            if (doEndOnRep == 0) {
                fmtInt(value->integer, 8);
            }
            return;
        case Fmt_P:
            scaleFactor = nextDesc->repeatCount;
            break;
        case Fmt_R:
            return;
        case Fmt_S:
        case Fmt_SS:
            doPlusSigns = 0;
            break;
        case Fmt_SP:
            doPlusSigns = 1;
            break;
        case Fmt_T:
            cursor = currentRecord + (nextDesc->width - 1);
            break;
        case Fmt_TL:
            cursor -= nextDesc->width;
            break;
        case Fmt_TR:
            cursor += nextDesc->width;
            break;
        case Fmt_X:
            cursor += 1;
            break;
        case Fmt_Z:
            if (doEndOnRep == 0) {
                fmtInt(value->integer, 16);
            }
            return;
        case Fmt_EOR:
            *eor = TRUE;
            return;
        case Fmt_Term:
            /* do nothing */
            break;
        case Fmt_Nospace:
            /* TODO */
            break;
        case Fmt_String:
            s = nextDesc->string;
            while (*s != '\0' && cursor < limit) *cursor++ = *s++;
            break;
        case Fmt_Embedded:
            nextDesc = nextDesc->child;
            break;
        default:
            break;
        }
    }
}

static char *parseInteger(char *s, int *value) {
    *value = 0;
    while (isdigit(*s)) {
        *value = (*value * 10) + (*s++ - '0');
    }
    return s;
}

static char *prsfmtHelper(char *s, FormatDesc **list) {
    char *dp;
    FormatDesc *head;
    FormatDesc *next;
    FormatDesc *prev;
    char quote;
    char *start;

    s = eatWsp(s);
    if (*s != '(') {
        fputs("FORMAT list does not begin with '('\n", stderr);
        exit(1);
    }
    s += 1;
    prev = NULL;
    for (;;) {
        next = allocDesc();
        next->parent = currentParent;
        s = eatWsp(s);
        if (isdigit(*s)) {
            s = parseInteger(s, &next->repeatCount);
            if (next->repeatCount < 1) {
                fprintf(stderr, "Invalid repeat count: %d\n", next->repeatCount);
                exit(1);
            }
        }
        switch (*s) {
        case '\0':
            fputs("FORMAT list does not end with ')'\n", stderr);
            exit(1);
        case 'A':
        case 'a':
        case 'D':
        case 'd':
        case 'E':
        case 'e':
        case 'F':
        case 'f':
        case 'G':
        case 'g':
        case 'I':
        case 'i':
        case 'L':
        case 'l':
        case 'O':
        case 'o':
        case 'R':
        case 'r':
        case 'Z':
        case 'z':
            next->class = alphaToFmtClass[*s - 'A'];
            s = getPrecision(s + 1, next);
            if (next->repeatCount == 0) next->repeatCount = 1;
            break;
        case 'B':
        case 'b':
            s += 1;
            if (*s == 'N') {
                next->class = Fmt_BN;
                s += 1;
            }
            else if (*s == 'Z') {
                next->class = Fmt_BZ;
                s += 1;
            }
            else {
                next->class = Fmt_B;
            }
            if (next->repeatCount != 0) {
                fprintf(stderr, "Invalid repeat count on '%s' descriptor\n", fmtClassToStr(next->class));
                exit(1);
            }
            break;
        case 'H':
        case 'h':
            next->class = Fmt_String;
            if (next->repeatCount == 0) {
                fprintf(stderr, "Invalid length specified on '%s' descriptor\n", fmtClassToStr(next->class));
                exit(1);
            }
            next->string = (char *)malloc(next->repeatCount + 1);
            if (next->string == NULL) {
                fputs("Memory exhausted\n", stderr);
                exit(1);
            }
            s += 1;
            dp = next->string;
            while (next->repeatCount-- > 0) {
                if (*s == '\0') {
                    fputs("Invalid hollerith descriptor\n", stderr);
                    exit(1);
                }
                *dp++ = *s++;
            }
            next->repeatCount = 1;
            break;
        case 'P':
        case 'p':
            next->class = Fmt_P;
            s += 1;
            break;
        case 'S':
        case 's':
            s += 1;
            if (*s == 'P') {
                next->class = Fmt_SP;
                s += 1;
            }
            else if (*s == 'S') {
                next->class = Fmt_SS;
                s += 1;
            }
            else {
                next->class = Fmt_S;
            }
            if (next->repeatCount != 0) {
                fprintf(stderr, "Invalid repeat count on '%s' descriptor\n", fmtClassToStr(next->class));
                exit(1);
            }
            next->repeatCount = 1;
            break;
        case 'T':
        case 't':
            s += 1;
            if (*s == 'L') {
                next->class = Fmt_TL;
                s += 1;
            }
            else if (*s == 'R') {
                next->class = Fmt_TR;
                s += 1;
            }
            else {
                next->class = Fmt_T;
            }
            if (!isdigit(*s)) {
                fprintf(stderr, "Position value missing from '%s' descriptor\n", fmtClassToStr(next->class));
                exit(1);
            }
            s = parseInteger(s, &next->width);
            if (next->width == 0 && next->class == Fmt_T) {
                fprintf(stderr, "Invalid position value on '%s' descriptor\n", fmtClassToStr(next->class));
                exit(1);
            }
            if (next->repeatCount != 0) {
                fprintf(stderr, "Invalid repeat count on '%s' descriptor\n", fmtClassToStr(next->class));
                exit(1);
            }
            next->repeatCount = 1;
            break;
        case 'X':
        case 'x':
            next->class = Fmt_X;
            s += 1;
            if (next->repeatCount == 0) next->repeatCount = 1;
            break;
        case '"':
        case '\'':
            next->class = Fmt_String;
            if (next->repeatCount != 0) {
                fprintf(stderr, "Invalid repeat count on '%s' descriptor\n", fmtClassToStr(next->class));
                exit(1);
            }
            next->repeatCount = 1;
            quote = *s++;
            start = s;
            for (;;) {
                if (*s == '\0') {
                    fputs("Unclosed string in format list\n", stderr);
                    exit(1);
                }
                else if (*s == quote) {
                    if (*(s + 1) != quote) break;
                    s += 1;
                }
                s += 1;
            }
            next->string = (char *)malloc((s - start) + 1);
            if (next->string == NULL) {
                fputs("Memory exhausted\n", stderr);
                exit(1);
            }
            dp = next->string;
            while (start < s) {
                if (*start == quote) start += 1;
                *dp++ = *start++;
            }
            *dp = '\0';
            s += 1;
            break;
        case '/':
            next->class = Fmt_EOR;
            if (next->repeatCount != 0) {
                fprintf(stderr, "Invalid repeat count on '%s' descriptor\n", fmtClassToStr(next->class));
                exit(1);
            }
            s += 1;
            next->repeatCount = 1;
            break;
        case ':':
            next->class = Fmt_Term;
            if (next->repeatCount != 0) {
                fprintf(stderr, "Invalid repeat count on '%s' descriptor\n", fmtClassToStr(next->class));
                exit(1);
            }
            s += 1;
            next->repeatCount = 1;
            break;
        case '$':
            next->class = Fmt_Nospace;
            if (next->repeatCount != 0) {
                fprintf(stderr, "Invalid repeat count on '%s' descriptor\n", fmtClassToStr(next->class));
                exit(1);
            }
            s += 1;
            next->repeatCount = 1;
            break;
        case '(':
            next->class = Fmt_Embedded;
            if (currentParent == NULL) revertDesc = next;
            currentParent = next;
            s = prsfmtHelper(s, &next->child);
            currentParent = next->parent;
            if (next->repeatCount == 0) next->repeatCount = 1;
            break;
        case ')':
            *list = head;
            return s + 1;
        default:
            fprintf(stderr, "Unrecognized format descriptor: '%c'\n", *s);
            exit(1);
        }

        if (prev != NULL) {
            prev->sibling = next;
        }
        else {
            head = next;
        }
        prev = next;
        s = eatWsp(s);
        if (*s == ',') {
            s += 1;
        }
    }
}

#if DEBUG

void printFmtList(FILE *f, FormatDesc *fdp) {
    char *s;

    fputs("(", f);

    while (fdp != NULL) {
        if (fdp->repeatCount > 1) fprintf(f, "%d", fdp->repeatCount);
        if (fdp->class < Fmt_String) {
            fputs(fmtClassToStr(fdp->class), f);
            if (fdp->width != 0) {
                fprintf(f, "%d", fdp->width);
                if (fdp->minDigits != 0) fprintf(f, ".%d", fdp->minDigits);
                if (fdp->expLength != 0) fprintf(f, "E%d", fdp->expLength);
            }
        }
        else if (fdp->class == Fmt_String) {
            fputs("'", f);
            for (s = fdp->string; *s != '\0'; s++) {
                if (*s == '\'') {
                    fputs("''", f);
                }
                else {
                    fputc(*s, f);
                }
            }
            fputs("'", f);
        }
        else if (fdp->class == Fmt_Embedded) {
            printFmtList(f, fdp->child);
        }
        fdp = fdp->sibling;
        if (fdp != NULL) fputs(",", f);
    }
    fputs(")", f);
}

#endif
