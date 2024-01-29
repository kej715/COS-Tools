/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: re.c
**
**  Description:
**      This file provides a simple regular expression engine..
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
#include "calconst.h"

/*
 *  This engine supports regular expressions containing:
 *
 *    .  matches any single character
 *    c  matches the character 'c'
 *    \. matches the character '.'
 *    \[ matches the charactr '['
 *    \\ matches the character '\'
 *    \d matches any digit
 *    \i matches any character that may begin an identifier
 *    \w matches any character that may occur within an identifier
 *    [c...c] matches any character in the specified class
 *    [^c...c] matches any character not in the specified class
 *    [c...c]* matches 0 or more occurrences of the specified class
 *    (...) designates a capture group
 *    p* match 0 or more occurrences of the pattern p
 *
 *  A match occurs when the entire regular expression matches, starting from
 *  the first character of the subject string to the last character of it. In
 *  other words, there is an implicit '^' before the beginning of the regular
 *  expression and an implicit '$' after its end.
 *
 *  Returns:
 *    1 if the regular expression matches the subject string fully
 *    0 if no match
 *   -1 if error (e.g., bad regular expression)
 */

static char classBuf[128];
static char *idClass  = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz$@%";
static char *digClass = "0123456789";

static char *getNextClass(char *re, char *reLimit, char **class, int *hasZeroOrMore) {
    char c;
    char *classLimit;
    char *cp;
    char *rp;

    *class = NULL;
    *hasZeroOrMore = FALSE;
    rp = re;
    c = *rp++;
    switch (c) {
    case '.':
        break;
    case '\\':
        if (rp >= reLimit) return NULL;
        c = *rp++;
        switch (c) {
        case 'D':
        case 'd':
            *class = digClass;
            break;
        case 'I':
        case 'i':
            *class = idClass + 10;
            break;
        case 'W':
        case 'w':
            *class = idClass;
            break;
        default:
            break;
        }
        break;
    case '[':
        cp = classBuf;
        classLimit = classBuf + sizeof(classBuf) - 1;
        while (rp < reLimit && *rp != ']' && cp < classLimit) {
            if (*rp == '-' && cp > classBuf && rp + 1 < reLimit && *(rp + 1) != ']') {
                rp += 1;
                c = *(cp - 1) + 1;
                while (c <= *rp) {
                    if (cp < classLimit) {
                        *cp++ = c++;
                    }
                    else {
                        return NULL;
                    }
                }
                rp += 1;
            }
            else {
                *cp++ = *rp++;
            }
        }
        if (rp >= reLimit || *rp != ']' || cp >= classLimit) return NULL;
        *cp = '\0';
        rp += 1;
        *class = classBuf;
        break;
    default:
        classBuf[0] = c;
        classBuf[1] = '\0';
        *class = classBuf;
        break;
    }
    if (*rp == '*') {
        rp += 1;
        *hasZeroOrMore = TRUE;
    }

    return rp;
}

static int isMatch(char *class, char c) {
    char *cp;

    if (class == NULL) return TRUE;

    cp = class;
    while (*cp != '\0') {
        if (*cp == c) return TRUE;
        cp += 1;
    }
    return FALSE;
}

int applyRE(char *re, int reLen, char *s, int sLen, char **captures, int *lenCaptures, int maxCaptures, int *nCaptures) {
    char *class;
    char *cpp;
    int hasZeroOrMore;
    int nc;
    char *reLimit;
    char *rp;
    char *sLimit;
    char *sp;

    cpp = NULL;
    nc = 0;
    rp = re;
    reLimit = rp + reLen;
    sp = s;
    sLimit = sp + sLen;

    while (rp < reLimit && sp < sLimit) {
        if (*rp == '(') {
            if (cpp != NULL) return -1;
            cpp = sp;
            rp += 1;
            continue;
        }
        else if (*rp == ')') {
            if (cpp == NULL || nc >= maxCaptures) return -1;
            captures[nc] = cpp;
            lenCaptures[nc] = sp - cpp;
            nc += 1;
            cpp = NULL;
            rp += 1;
            continue;
        }
        rp = getNextClass(rp, reLimit, &class, &hasZeroOrMore);
        if (rp == NULL) return -1;
        if (hasZeroOrMore) {
            if (class != NULL) {
                while (sp < sLimit && isMatch(class, *sp)) sp += 1;
            }
            else {
                while (class == NULL && hasZeroOrMore) {
                    rp = getNextClass(rp, reLimit, &class, &hasZeroOrMore);
                    if (rp == NULL) return -1;
                }
                while (sp < sLimit && isMatch(class, *sp) == FALSE) sp += 1;
                if (hasZeroOrMore) {
                    while (sp < sLimit && isMatch(class, *sp)) sp += 1;
                }
                else if (isMatch(class, *sp)) {
                    sp += 1;
                }
                else {
                    return 0;
                }
            }
        }
        else if (isMatch(class, *sp)) {
            sp += 1;
        }
        else {
            return 0;
        }
    }

    if (rp < reLimit && *rp == ')' && cpp != NULL && nc < maxCaptures) {
        captures[nc] = cpp;
        lenCaptures[nc] = sp - cpp;
        nc += 1;
        rp += 1;
    }

    if (nCaptures != NULL) *nCaptures = nc;

    return rp >= reLimit && sp >= sLimit;
}
