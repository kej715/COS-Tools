/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: io.c
**
**  Description:
**      This file provides I/O utility functions.
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
#include "calproto.h"
#include "caltypes.h"
#include "services.h"

static char *getMacroParamValue(MacroCall *call, char *name);

static void generateMacroLine(void) {
    MacroCall *call;
    MacroFragment *frag;
    char *limit;
    MacroLine *line;
    int rc;
    char *reCaptures[10];
    int reCaptureLens[10];
    int reCaptureN;
    char *sp;
    char *tp;

    listControlMask = LIST_ON|LIST_MAC;
    call = macroStack[macroStackPtr - 1];
    line = call->nextLine;
    sp = sourceLine;
    limit = sp + MAX_SOURCE_LINE_LENGTH;
    frag = line->fragments;
    while (frag != NULL) {
        if (frag->type == MacroFragType_Text) {
            tp = frag->text;
        }
        else {
            tp = getMacroParamValue(call, frag->text);
            if (frag->next != NULL && frag->next->type == MacroFragType_Regex) {
                frag = frag->next;
                rc = applyRE(frag->text, strlen(frag->text), tp, strlen(tp), reCaptures, reCaptureLens, 10, &reCaptureN);
                if (rc == 1 && reCaptureN > 0) {
                    tp = reCaptures[0];
                    while (sp < limit && reCaptureLens[0]-- > 0) *sp++ = *tp++;
                }
                tp = "";
            }
        }
        while (*tp != '\0' && sp < limit) *sp++ = *tp++;
        frag = frag->next;
    }
    *sp = '\0';
    call->nextLine = line->next;
    if (call->nextLine == NULL) {
        freeMacroCall(call);
        macroStackPtr -= 1;
    }
}

static char *getMacroParamValue(MacroCall *call, char *name) {
    MacroParam *pp;

    //
    //  First, look for a matching actual parameter in the
    //  call structure.
    //
    pp = call->params;
    while (pp != NULL) {
        if (strcasecmp(name, pp->name) == 0) return pp->value;
        pp = pp->next;
    }
    //
    //  If no match against actual parameters, find a matching
    //  keyword parameter and return its default value.
    //
    pp = call->defn->params;
    while (pp != NULL) {
        if (pp->type == MacroParamType_Keyword && strcasecmp(name, pp->name) == 0)
            return pp->value;
        pp = pp->next;
    }
    return "";
}

int isEof(void) {
    return (feof(sourceFile) != 0) ? TRUE : FALSE;
}

void readNextLine(void) {
    int c;
    int i;
    int lineEnd;
    int result;

    if (macroStackPtr > 0) {
        generateMacroLine();
        return;
    }
    i = 0;
    lineEnd = 0;
    while (TRUE) {
        c = fgetc(sourceFile);
        if (c == EOF) {
            if (feof(sourceFile)) {
                sourceLine[i] = '\0';
                break;
            }
            else {
                perror(sourceFilePath);
                exit(1);
            }
        }
#if defined(__cos)
        else if (c == 0x1b) {
            /*
             * Handle COS blank compression indicator
             */
            c = fgetc(sourceFile);
            if (c == EOF) break;
            c -= 036; /* blank count is biased by 36 octal */
            while (c-- > 0 && i < MAX_SOURCE_LINE_LENGTH) sourceLine[i++] = ' ';
        }
#endif
        else if (c == '\n') {
            break;
        }
        else if (i < MAX_SOURCE_LINE_LENGTH) {
            sourceLine[i++] = c;
            if (c != ' ') lineEnd = i;
        }
    }
    sourceLine[lineEnd] = '\0';
    /*
     * If flexible syntax is enabled, then labels will be delimited by ":" and may stand alone on
     * lines, and instructions may begin in column 1 when labels are not present. In these cases,
     * transform lines into "normal" CAL syntax by removing ":" delimiters and concatenating or
     * shifting lines.
     */
    if (isFlexibleSyntax && sourceLine[0] != '*' && sourceLine[0] != ' ' && sourceLine[0] != '\0') {
        char *cp;
        char *dp;
        char *labelEnd;
        long val;

        for (cp = sourceLine; *cp != '\0' && *cp != ':' && *cp != ' '; cp++)
             ;
        if (*cp == ':') {
            labelEnd = cp;
            *cp++ = ' ';
            while (*cp == ' ' && *cp != '\0') cp += 1;
            if (*cp != '\0') return; // non-standalone label
            *labelEnd = '\0';
            if (sourceLine[0] >= '0' && sourceLine[0] <= '9') {
                val = strtol(sourceLine, NULL, 10);
                localSymbolCtrs[val] += 1;
                sprintf(sourceLine, "@%ld$%d = *", val, localSymbolCtrs[val]);
            }
            else {
                strcpy(labelEnd, " = *");
            }
        }
        else {
            for (i = lineEnd; i > 0; i--) {
                sourceLine[i] = sourceLine[i - 1];
            }
            sourceLine[0] = ' ';
            if (lineEnd < MAX_SOURCE_LINE_LENGTH) {
                sourceLine[lineEnd + 1] = '\0';
            }
            else {
                sourceLine[lineEnd] = '\0';
            }
        }
    }
}
