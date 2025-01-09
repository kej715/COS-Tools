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
static bool       doPlusSigns;
static FormatDesc *firstDesc;
static char       fmtBuf[MAX_FMT_LEN+1];
static bool       isBlankZero;
static bool       isLastChr;
static char       *limit;
static FormatDesc *nextDesc;
static char       record[MAX_FMT_RECL];
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

static f64 powers10[20] = {
    1E00,1E01,1E02,1E03,1E04,1E05,1E06,1E07,1E08,1E09,
    1E10,1E11,1E12,1E13,1E14,1E15,1E16,1E17,1E18,1E19
};

static FormatDesc *allocDesc(void);
static char *eatWsp(char *s, char *limit);
static void endfmtHelper(FormatDesc *fdp);
static char *fmtClassToStr(FormatClass class);
static void fmtInt(u64 value, FormatDesc *fdp, int radix);
static void fmtReal(f64 value, FormatDesc *fdp);
static void fmtRealE(f64 value, FormatDesc *fdp);
static void fmtRealF(f64 value, FormatDesc *fdp);
static void fmtRealG(f64 value, FormatDesc *fdp);
static char *getPrecision(char *s, char *limit, FormatDesc *fdp);
static i64 inpInt(FormatDesc *fdp, int base);
static f64 inpReal(FormatDesc *fdp);
static void outfmtHelper(void *value, int doEndOnRep, int *eor);
static char *parseFloat(char *s, char *limit, f64 *value);
static char *parseInteger(char *s, char *limit, i64 *value);
static char *prsfmtHelper(char *s, char *limit, FormatDesc **list);
static void resetIterations(FormatDesc *fdp);
static void showWhere(char *fmt, char *cursor, char *limit);

void _endfmt(void) {
    endfmtHelper(firstDesc);
}

void _inircd(void) {
    memset(currentRecord, ' ', limit - currentRecord);
    cursor = currentRecord;
}

FormatDesc *_getfdl(void) {
    return firstDesc;
}

unsigned long _getrcd(void) {
    return ((unsigned long)currentRecord) | ((limit - currentRecord) << 32);
}

void _inpchr(int unitNum, unsigned long ref) {
    int len;
    char *s;

    s = (char *)(ref & 0xffffffff);
    len = ref >> 32;
    while (cursor < limit && *cursor != '\'') cursor += 1;
    if (cursor < limit) {
        cursor += 1;
        while (cursor < limit) {
            if (*cursor == '\'') {
                cursor += 1;
                if (cursor >= limit || *cursor != '\'') break;
            }
            if (len > 0) {
                *s++ = *cursor;
                len -= 1;
            }
            cursor += 1;
        }
    }
    while (len-- > 0) *s++ = ' ';
    cursor = eatWsp(cursor, limit);
    if (cursor < limit && *cursor == ',') cursor += 1;
}

void _inpdbl(int unitNum, f64 *value) {
    cursor = eatWsp(cursor, limit);
    cursor = parseFloat(cursor, limit, value);
    cursor = eatWsp(cursor, limit);
    if (cursor < limit && *cursor == ',') cursor += 1;
}

void _inpfmt(void *value) {
    unsigned long charRef;
    int fieldWidth;
    int len;
    char *s;

    if (nextDesc == NULL) return;

    for (;;) {
        if (++nextDesc->currentIteration > nextDesc->repeatCount) {
            if (nextDesc->sibling != NULL) {
                nextDesc = nextDesc->sibling;
            }
            else if (nextDesc->parent != NULL) {
                nextDesc = nextDesc->parent;
            }
            else {
                nextDesc = revertDesc;
                resetIterations(nextDesc);
            }
            continue;
        }
        switch (nextDesc->class) {
        case Fmt_A:
            charRef = (unsigned long)value;
            s = (char *)(charRef & 0xffffffff);
            len = charRef >> 32;
            if (len == 0) { // value is not type CHARACTER
                len = 8;
            }
            fieldWidth = (nextDesc->width == 0) ? len : nextDesc->width;
            while (fieldWidth > 0 && len > 0) {
                if (cursor < limit) {
                    *s++ = *cursor++;
                    len -= 1;
                }
                fieldWidth -= 1;
            }
            while (len-- > 0) *s++ = ' ';
            return;
        case Fmt_R:
            charRef = (unsigned long)value;
            s = (char *)(charRef & 0xffffffff);
            len = charRef >> 32;
            if (len == 0) { // value is not type CHARACTER
                len = 8;
            }
            fieldWidth = (nextDesc->width == 0) ? len : nextDesc->width;
            while (len > fieldWidth) {
                *s++ = ' ';
                len -= 1;
            }
            while (fieldWidth > 0 && len > 0) {
                if (cursor < limit) {
                    *s++ = *cursor++;
                    len -= 1;
                }
                fieldWidth -= 1;
            }
            while (len-- > 0) *s++ = ' ';
            return;
        case Fmt_B:
        case Fmt_BN:
            isBlankZero = FALSE;
            break;
        case Fmt_BZ:
            isBlankZero = TRUE;
            break;
        case Fmt_D:
        case Fmt_E:
        case Fmt_F:
        case Fmt_G:
            *(f64 *)value = inpReal(nextDesc);
            return;
        case Fmt_I:
            *(i64 *)value = inpInt(nextDesc, 10);
            return;
        case Fmt_L:
            fieldWidth = (nextDesc->width == 0) ? 1 : nextDesc->width;
            *(u64 *)value = (cursor < limit && *cursor == 'T') ? ~0L : 0;
            cursor += fieldWidth;
            return;
        case Fmt_O:
            *(i64 *)value = inpInt(nextDesc, 8);
            return;
        case Fmt_P:
            scaleFactor = nextDesc->repeatCount;
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
            *(i64 *)value = inpInt(nextDesc, 16);
            return;
        case Fmt_Embedded:
            nextDesc = nextDesc->child;
            resetIterations(nextDesc);
            break;
        case Fmt_Nospace:
        case Fmt_S:
        case Fmt_SS:
        case Fmt_SP:
        case Fmt_String:
        case Fmt_Term:
        case Fmt_EOR:
            /* do nothing */
            break;
        default:
            break;
        }
    }
}

void _inpint(int unitNum, i64 *value) {
    cursor = eatWsp(cursor, limit);
    cursor = parseInteger(cursor, limit, value);
    cursor = eatWsp(cursor, limit);
    if (cursor < limit && *cursor == ',') cursor += 1;
}

void _inplog(int unitNum, u64 *value) {
    *value = 0;
    cursor = eatWsp(cursor, limit);
    if (cursor < limit && toupper(*cursor) == 'T') {
        *value = ~0L;
        cursor += 1;
    }
    while (cursor < limit && *cursor != ',') cursor += 1;
    if (cursor < limit) cursor += 1;
}

void _lstchr(int unitNum, unsigned long ref) {
    int len;
    char *s;

    s = (char *)(ref & 0xffffffff);
    len = ref >> 32;
    while (len-- > 0) {
        if (cursor < limit) *cursor++ = *s++;
    }
    isLastChr = TRUE;
}

#define MAX_DIGITS 14
void _lstdbl(int unitNum, f64 value) {
    char buf[16];
    int decpt;
    char *ep;
    int exp;
    int ignore;
    int isNegative;
    int len;
    char *s;
    char *s2;

    if (value < 0) {
        isNegative = 1;
        value = -value;
    }
    else {
        isNegative = 0;
    }
    if (cursor < limit && isLastChr == FALSE) *cursor++ = ' ';
    if (isNegative && cursor < limit) *cursor++ = '-';
    if (value == 0.0) {
        if (cursor < limit) *cursor++ = '0';
        if (cursor < limit) *cursor++ = '.';
    }
    else if (value >= 1.0E-6 && value <= 1.0E+9) {
        s = fcvt(value, MAX_DIGITS, &decpt, &ignore);
        len = strlen(s);
        if (len > MAX_DIGITS) {
            len = MAX_DIGITS;
            *(s + len) = '\0';
        }
        s2 = s + len - 1;
        if (decpt <= 0) {
            if (cursor < limit) *cursor++ = '0';
            if (cursor < limit) *cursor++ = '.';
            while (decpt < 0) {
                if (cursor < limit) *cursor++ = '0';
                decpt += 1;
            }
        }
        else {
            while (decpt > 0 && *s != '\0') {
                if (cursor < limit) *cursor++ = *s++;
                decpt -= 1;
            }
            if (cursor < limit) *cursor++ = '.';
        }
        while (s2 > s && *s2 == '0') *s2-- = '\0';
        while (cursor < limit && *s != '\0') *cursor++ = *s++;
    }
    else {
        s = ecvt(value, MAX_DIGITS, &decpt, &ignore);
        len = strlen(s);
        if (len > MAX_DIGITS) {
            len = MAX_DIGITS;
            *(s + len) = '\0';
        }
        s2 = s + len - 1;
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
        while (s2 > s && *s2 == '0') *s2-- = '\0';
        while (cursor < limit && *s != '\0') *cursor++ = *s++;
        while (cursor < limit && *ep != '\0') *cursor++ = *ep++;
    }
    isLastChr = FALSE;
}

void _lstint(int unitNum, i64 value) {
    char buf[33];
    int isNegative;
    char *s;

    isNegative = 0;
    if (value < 0) {
        isNegative = 1;
        value = -value;
    }
    s = buf + sizeof(buf) - 1;
    *s-- = '\0';
    do {
        *s-- = '0' + (value % 10);
        value /= 10;
    }
    while (value != 0);
    if (isNegative) {
        *s = '-';
    }
    else {
        s += 1;
    }
    if (cursor < limit && isLastChr == FALSE) *cursor++ = ' ';
    while (cursor < limit && *s != '\0') *cursor++ = *s++;
    isLastChr = FALSE;
}

void _lstlog(int unitNum, u64 value) {
    char lc;

    lc = (value == 0) ? 'F' : 'T';
    if (cursor < limit && isLastChr == FALSE) *cursor++ = ' ';
    if (cursor < limit) *cursor++ = lc;
    isLastChr = FALSE;
}

void _outfin(int *eor) {
    outfmtHelper(NULL, 1, eor);
}

void _outfmt(void *value, int *eor) {
    outfmtHelper(value, 0, eor);
}

void _przfmt(char *s) {
    char *limit;

    descIdx = 0;
    currentParent = NULL;
    revertDesc = NULL;
    doPlusSigns = FALSE;
    isBlankZero = FALSE;
    scaleFactor = 0;
    limit = s + strlen(s);
    s = prsfmtHelper(s, limit, &firstDesc);
    s = eatWsp(s, limit);
    if (s < limit) {
        fputs("Cruft after closing ')' of FORMAT list\n", stderr);
        exit(1);
    }
    nextDesc = descriptors;
    if (revertDesc == NULL) revertDesc = descriptors;
}

void _prslst(void) {
    descIdx = 0;
    currentParent = NULL;
    nextDesc = NULL;
    revertDesc = NULL;
    doPlusSigns = FALSE;
    isBlankZero = FALSE;
    isLastChr = TRUE;
    scaleFactor = 0;
}

void _setdrc(void) {
    memset(record, ' ', MAX_FMT_RECL);
    _setrcd(((unsigned long)record) | ((long)MAX_FMT_RECL << 32));
}

void _setfdl(FormatDesc *fdp) {
    firstDesc = fdp;
}

void _setrcd(unsigned long strRef) {
    currentRecord = (char *)(strRef & 0xffffffff);
    cursor = currentRecord;
    limit  = currentRecord + (strRef >> 32);
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

static char *eatWsp(char *s, char *limit) {
    while (s < limit && isspace(*s)) s += 1;
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

static void fmtInt(u64 value, FormatDesc *fdp, int radix) {
    char buf[32];
    char *cp;
    int fieldWidth;
    bool isNegative;
    int minDigits;
    int n;
    char *s;
    char *s2;
    int sign;
    long sval;

    isNegative = FALSE;
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
        sign = doPlusSigns ? 1 : 0;
        sval = (i64)value;
        if (sval < 0) {
            sval = -sval;
            isNegative = TRUE;
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
    if (fdp->minDigits > minDigits) minDigits = fdp->minDigits;
    fieldWidth = minDigits + sign;
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

static void fmtReal(f64 value, FormatDesc *fdp) {
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

static void fmtRealE(f64 value, FormatDesc *fdp) {
    char ebuf[16];
    char *ebufLim;
    char *cp;
    int decpt;
    char *ep;
    int exp;
    int expLength;
    int fieldWidth;
    int isNegative;
    int len;
    int maxDigits;
    int n;
    char *s;
    char *s2;
    int sign;

    s = ecvt(value, 32, &decpt, &isNegative);
    expLength = (fdp->expLength == 0) ? 2 : fdp->expLength;
    sign = isNegative || doPlusSigns;
    fieldWidth = fdp->minDigits + sign + expLength + 4;
    ep = ebufLim = ebuf + sizeof(ebuf) - 1;
    exp = decpt;
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
        || ((expLength + 2) < (ebufLim - ep)))  {
        fieldWidth = fdp->width;
        for (n = fieldWidth; n > 0; n--) {
            if (cp < limit) *cp = '*';
            cp -= 1;
        }
    }
    else {
        s2 = ebufLim;
        while (s2 > ep) {
            if (cp < limit) *cp = *s2;
            cp -= 1;
            s2 -= 1;
        }
        len = strlen(s);
        if (len > fdp->minDigits) len = fdp->minDigits;
        n = fdp->minDigits - len;
        while (n-- > 0) {
            if (cp < limit) *cp = '0';
            cp -= 1;
        }
        s2 = s + len - 1;
        while (s2 >= s) {
            if (cp < limit) *cp = *s2;
            cp -= 1;
            s2 -= 1;
        }
        if (cp < limit) *cp = '.';
        cp -= 1;
        if (cp < limit) *cp = '0';
        cp -= 1;
        if (sign && cp < limit) {
            *cp = isNegative ? '-' : '+';
        }
    }
    cursor += fieldWidth;
}

static void fmtRealF(f64 value, FormatDesc *fdp) {
    char *cp;
    int decpt;
    int fieldWidth;
    int isNegative;
    int len;
    int lim;
    int n;
    char *s;
    char *s2;
    char *s3;
    int sign;

    s = fcvt(value, fdp->minDigits, &decpt, &isNegative);
    len = strlen(s);
    sign = isNegative || doPlusSigns;
    fieldWidth = sign + 1;
    if (decpt > 0) {
        lim = decpt + fdp->minDigits;
        if (lim < len) {
            len = lim;
            *(s + len) = '\0';
        }
    }
    else if (decpt == 0) {
        fieldWidth += 1;
        lim = fdp->minDigits;
        if (lim < len) {
            len = lim;
            *(s + len) = '\0';
        }
    }
    else if (decpt < 0) {
        n = -decpt;
        fieldWidth += n + 1;
        if (n < fdp->minDigits) {
            len = fdp->minDigits - n;
            *(s + len) = '\0';
        }
        else {
            len = 0;
            *s = '\0';
        }
    }
    fieldWidth += len;
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

static void fmtRealG(f64 value, FormatDesc *fdp) {
    if (value >= 0.1 && fdp->minDigits < 20 && value < powers10[fdp->minDigits]) {
        fmtRealF(value, fdp);
    }
    else {
        fmtRealE(value, fdp);
    }
}

static char *getPrecision(char *s, char *limit, FormatDesc *fdp) {
    if (isdigit(*s)) {
        s = parseInteger(s, limit, &fdp->width);
        switch (fdp->class) {
        case Fmt_D:
        case Fmt_E:
        case Fmt_F:
        case Fmt_G:
        case Fmt_I:
        case Fmt_O:
        case Fmt_Z:
            if (fdp->width < 1) {
                fprintf(stderr, "Invalid width specified for '%s' format descriptor\n", fmtClassToStr(fdp->class));
                exit(1);
            }
            if (s < limit && *s == '.') {
                s += 1;
                if (s < limit && isdigit(*s)) {
                    s = parseInteger(s, limit, &fdp->minDigits);
                    if (s < limit && (fdp->class == Fmt_E || fdp->class == Fmt_G) && (*s == 'E' || *s == 'e')) {
                        s += 1;
                        if (s < limit && isdigit(*s)) {
                            s = parseInteger(s, limit, &fdp->expLength);
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

static i64 inpInt(FormatDesc *fdp, int base) {
    char c;
    bool isNegative;
    char *lim;
    i64 res;
    char *s;

    res = 0;
    s = cursor;
    lim = s + fdp->width;
    if (lim > limit) lim = limit;
    if (isBlankZero == FALSE) s = eatWsp(s, lim);
    isNegative = FALSE;
    if (s < lim) {
        if (*s == '-') {
            isNegative = TRUE;
            s += 1;
        }
        else if (*s == '+') {
            s += 1;
        }
    }
    if (isBlankZero == FALSE) s = eatWsp(s, lim);
    switch (base) {
    case 8:
        while (s < lim) {
            c = *s++;
            if (c == ' ' && isBlankZero) c = '0';
            if (c >= '0' && c <= '7') {
                res = (res << 3) + (c - '0');
            }
            else {
                break;
            }
        }
        break;
    case 16:
        while (s < lim) {
            c = *s++;
            if (c == ' ' && isBlankZero) c = '0';
            if (c >= '0' && c <= '9') {
                res = (res << 4) + (c - '0');
            }
            else if (c >= 'A' && c <= 'F') {
                res = (res << 4) + ((c - 'A') + 10);
            }
            else if (c >= 'a' && c <= 'f') {
                res = (res << 4) + ((c - 'a') + 10);
            }
            else {
                break;
            }
        }
        break;
    default:
        while (s < lim) {
            c = *s++;
            if (c == ' ' && isBlankZero) c = '0';
            if (c >= '0' && c <= '9') {
                res = (res * 10) + (c - '0');
            }
            else {
                break;
            }
        }
        break;
    }
    cursor += fdp->width;

    return isNegative ? -res : res;
}

static f64 inpReal(FormatDesc *fdp) {
    f64 divisor;
    char *dp;
    char *ep;
    f64 frac;
    bool isNegative;
    char *lim;
    f64 res;
    char *s;
    i64 valE;

    dp = NULL;
    ep = NULL;
    lim = cursor + fdp->width;
    if (lim > limit) lim = limit;
    for (s = cursor; s < lim; s++) {
        if (*s == '.')
            dp = s;
        else if (*s == 'E' || *s == 'e' || *s == 'D' || *s == 'd')
            ep = s;
    }
    if (dp != NULL || ep != NULL) {
        s = parseFloat(cursor, lim, &res);
    }
    else {
        lim = cursor + fdp->width;
        ep = (fdp->expLength > 0) ? lim - fdp->expLength : lim;
        dp = (fdp->minDigits > 0) ? ep - fdp->minDigits : ep;
        if (ep > limit) ep = limit;
        if (dp > limit) dp = limit;
        res = 0.0;
        s = cursor;
        if (s >= dp) return res;
        isNegative = FALSE;
        if (*s == '-') {
            isNegative = TRUE;
            s += 1;
        }
        else if (*s == '+') {
            s += 1;
        }
        /*
         *  Process whole number part
         */
        while (s < dp && *s >= '0' && *s <= '9') {
            res = (res * 10.0) + (f64)(*s - '0');
            s += 1;
        }
        /*
         *  Process fraction part
         */
        if (s < ep) {
            frac = 0.0;
            s += 1;
            divisor = 10.0;
            while (s < ep && *s >= '0' && *s <= '9') {
                frac += (f64)(*s - '0') / divisor;
                divisor *= 10.0;
                s += 1;
            }
            res += frac;
        }
        /*
         *  Process power of 10 indication
         */
        if (s < lim) {
            s = parseInteger(s, lim, &valE);
            while (valE > 0) {
                res *= 10.0;
                valE -= 1;
            }
            while (valE < 0) {
                res /= 10.0;
                valE += 1;
            }
        }
        if (isNegative) res = -res;
    }
    cursor += fdp->width;

    return res;
}

static void outfmtHelper(void *value, int doEndOnRep, int *eor) {
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
            else if (doEndOnRep) {
                return;
            }
            else {
                nextDesc = revertDesc;
                resetIterations(nextDesc);
            }
            continue;
        }
        switch (nextDesc->class) {
        case Fmt_A:
        case Fmt_R:
            if (doEndOnRep == 0) {
                charRef = (unsigned long)value;
                s = (char *)(charRef & 0xffffffff);
                len = charRef >> 32;
                if (len == 0) { // value is not type CHARACTER
                    len = 8;
                }
                fieldWidth = (nextDesc->width == 0) ? len : nextDesc->width;
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
            isBlankZero = FALSE;
            break;
        case Fmt_BZ:
            isBlankZero = TRUE;
            break;
        case Fmt_D:
        case Fmt_E:
        case Fmt_F:
        case Fmt_G:
            if (doEndOnRep == 0) {
                fmtReal(*(f64 *)value, nextDesc);
            }
            return;
        case Fmt_I:
            if (doEndOnRep == 0) {
                fmtInt(*(u64 *)value, nextDesc, 10);
            }
            return;
        case Fmt_L:
            if (doEndOnRep == 0) {
                fieldWidth = (nextDesc->width == 0) ? 1 : nextDesc->width;
                s = cursor + fieldWidth - 1;
                if (s < limit) *s = (*(u64 *)value == 0) ? 'F' : 'T';
                cursor += fieldWidth;
            }
            return;
        case Fmt_O:
            if (doEndOnRep == 0) {
                fmtInt(*(u64 *)value, nextDesc, 8);
            }
            return;
        case Fmt_P:
            scaleFactor = nextDesc->repeatCount;
            break;
        case Fmt_S:
        case Fmt_SS:
            doPlusSigns = FALSE;
            break;
        case Fmt_SP:
            doPlusSigns = TRUE;
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
                fmtInt(*(u64 *)value, nextDesc, 16);
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
            resetIterations(nextDesc);
            break;
        default:
            break;
        }
    }
}

static char *parseFloat(char *s, char *limit, f64 *value) {
    f64 divisor;
    f64 frac;
    bool isNegative;
    i64 valE;
    f64 valS;

    *value = 0.0;
    if (s >= limit) return s;
    isNegative = FALSE;
    if (*s == '-') {
        isNegative = TRUE;
        s += 1;
    }
    else if (*s == '+') {
        s += 1;
    }
    /*
     *  Process whole number part
     */
    while (s < limit && *s >= '0' && *s <= '9') {
        *value = (*value * 10.0) + (f64)(*s - '0');
        s += 1;
    }
    /*
     *  Process fraction part
     */
    if (s < limit && *s == '.') {
        frac = 0.0;
        s += 1;
        divisor = 10.0;
        while (s < limit && *s >= '0' && *s <= '9') {
            frac += (f64)(*s - '0') / divisor;
            divisor *= 10.0;
            s += 1;
        }
        *value += frac;
    }
    /*
     *  Process power of 10 indication
     */
    if (s + 1 < limit
        && (*s == 'E' || *s == 'e' || *s == 'D' || *s == 'd')
        && ((*(s + 1) >= '0' && *(s + 1) <= '9')
            || ((*(s + 1) == '+' || *(s + 1) == '-') && (s + 2) < limit && *(s + 2) >= '0' && *(s + 2) <= '9'))) {
        s = parseInteger(s + 1, limit, &valE);
        while (valE > 0) {
            *value *= 10.0;
            valE -= 1;
        }
        while (valE < 0) {
            *value /= 10.0;
            valE += 1;
        }
    }
    if (isNegative) *value = -(*value);

    return s;
}

static char *parseInteger(char *s, char *limit, i64 *value) {
    bool isNegative;

    *value = 0;
    if (s < limit) {
        if (*s == '-') {
            isNegative = TRUE;
            s += 1;
        }
        else {
            isNegative = FALSE;
            if (*s == '+') s += 1;
        }
        while (s < limit && isdigit(*s)) {
            *value = (*value * 10) + (*s++ - '0');
        }
        if (isNegative) *value = -(*value);
    }
    return s;
}

static char *prsfmtHelper(char *s, char *limit, FormatDesc **list) {
    char *dp;
    char *fmt;
    FormatDesc *head;
    FormatDesc *next;
    FormatDesc *prev;
    char quote;
    char *start;

    fmt = s;
    s = eatWsp(s, limit);
    if (s >= limit || *s != '(') {
        fputs("FORMAT list does not begin with '('\n", stderr);
        showWhere(fmt, s, limit);
        exit(1);
    }
    s += 1;
    prev = NULL;
    for (;;) {
        next = allocDesc();
        next->parent = currentParent;
        s = eatWsp(s, limit);
        if (s < limit && isdigit(*s)) {
            s = parseInteger(s, limit, &next->repeatCount);
            if (next->repeatCount < 1) {
                fprintf(stderr, "Invalid repeat count: %d\n", next->repeatCount);
                showWhere(fmt, s, limit);
                exit(1);
            }
        }
        if (s >= limit) {
            fputs("FORMAT list does not end with ')'\n", stderr);
            showWhere(fmt, s, limit);
            exit(1);
        }
        switch (*s) {
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
            s = getPrecision(s + 1, limit, next);
            if (next->repeatCount == 0) next->repeatCount = 1;
            break;
        case 'B':
        case 'b':
            next->class = Fmt_B;
            s += 1;
            if (s < limit) {
                if (*s == 'N') {
                    next->class = Fmt_BN;
                    s += 1;
                }
                else if (*s == 'Z') {
                    next->class = Fmt_BZ;
                    s += 1;
                }
            }
            if (next->repeatCount != 0) {
                fprintf(stderr, "Invalid repeat count on '%s' descriptor\n", fmtClassToStr(next->class));
                showWhere(fmt, s, limit);
                exit(1);
            }
            break;
        case 'H':
        case 'h':
            next->class = Fmt_String;
            if (next->repeatCount == 0) {
                fprintf(stderr, "Invalid length specified on '%s' descriptor\n", fmtClassToStr(next->class));
                showWhere(fmt, s, limit);
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
                if (s >= limit) {
                    fputs("Invalid hollerith descriptor\n", stderr);
                    showWhere(fmt, s, limit);
                    exit(1);
                }
                *dp++ = *s++;
            }
            *dp = '\0';
            next->repeatCount = 1;
            break;
        case 'P':
        case 'p':
            next->class = Fmt_P;
            s += 1;
            break;
        case 'S':
        case 's':
            next->class = Fmt_S;
            s += 1;
            if (s < limit) {
                if (*s == 'P') {
                    next->class = Fmt_SP;
                    s += 1;
                }
                else if (*s == 'S') {
                    next->class = Fmt_SS;
                    s += 1;
                }
            }
            if (next->repeatCount != 0) {
                fprintf(stderr, "Invalid repeat count on '%s' descriptor\n", fmtClassToStr(next->class));
                showWhere(fmt, s, limit);
                exit(1);
            }
            next->repeatCount = 1;
            break;
        case 'T':
        case 't':
            next->class = Fmt_T;
            s += 1;
            if (s < limit) {
                if (*s == 'L') {
                    next->class = Fmt_TL;
                    s += 1;
                }
                else if (*s == 'R') {
                    next->class = Fmt_TR;
                    s += 1;
                }
            }
            if (s >= limit || !isdigit(*s)) {
                fprintf(stderr, "Position value missing from '%s' descriptor\n", fmtClassToStr(next->class));
                showWhere(fmt, s, limit);
                exit(1);
            }
            s = parseInteger(s, limit, &next->width);
            if (next->width == 0 && next->class == Fmt_T) {
                fprintf(stderr, "Invalid position value on '%s' descriptor\n", fmtClassToStr(next->class));
                showWhere(fmt, s, limit);
                exit(1);
            }
            if (next->repeatCount != 0) {
                fprintf(stderr, "Invalid repeat count on '%s' descriptor\n", fmtClassToStr(next->class));
                showWhere(fmt, s, limit);
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
                showWhere(fmt, s, limit);
                exit(1);
            }
            next->repeatCount = 1;
            quote = *s++;
            start = s;
            for (;;) {
                if (s >= limit) {
                    fputs("Unclosed string in format list\n", stderr);
                    showWhere(fmt, s, limit);
                    exit(1);
                }
                else if (*s == quote) {
                    if ((s + 1) >= limit || *(s + 1) != quote) break;
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
                showWhere(fmt, s, limit);
                exit(1);
            }
            s += 1;
            next->repeatCount = 1;
            break;
        case ':':
            next->class = Fmt_Term;
            if (next->repeatCount != 0) {
                fprintf(stderr, "Invalid repeat count on '%s' descriptor\n", fmtClassToStr(next->class));
                showWhere(fmt, s, limit);
                exit(1);
            }
            s += 1;
            next->repeatCount = 1;
            break;
        case '$':
            next->class = Fmt_Nospace;
            if (next->repeatCount != 0) {
                fprintf(stderr, "Invalid repeat count on '%s' descriptor\n", fmtClassToStr(next->class));
                showWhere(fmt, s, limit);
                exit(1);
            }
            s += 1;
            next->repeatCount = 1;
            break;
        case '(':
            next->class = Fmt_Embedded;
            if (currentParent == NULL) revertDesc = next;
            currentParent = next;
            s = prsfmtHelper(s, limit, &next->child);
            currentParent = next->parent;
            if (next->repeatCount == 0) next->repeatCount = 1;
            break;
        case ')':
            *list = head;
            return s + 1;
        default:
            fprintf(stderr, "Unrecognized format descriptor: '%c'\n", *s);
            showWhere(fmt, s, limit);
            exit(1);
        }
        if (prev != NULL) {
            prev->sibling = next;
        }
        else {
            head = next;
        }
        prev = next;
        s = eatWsp(s, limit);
        if (s < limit && *s == ',') {
            s += 1;
        }
    }
}

static void resetIterations(FormatDesc *fdp) {
    FormatDesc *next;

    if (fdp != NULL) {
        fdp->currentIteration = 0;
        resetIterations(fdp->child);
        for (next = fdp->sibling; next != NULL; next = next->sibling) {
            resetIterations(next);
        }
    }
}

static void showWhere(char *fmt, char *cursor, char *limit) {
    int n;
    char *s;
    
    for (s = fmt; s < limit; s++) fputc(*s, stderr);
    fputc('\n', stderr);
    for (s = fmt; s < cursor; s++) fputc(' ', stderr);
    fputc('^', stderr);
    fputc('\n', stderr);
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
