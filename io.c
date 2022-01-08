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
#include "const.h"
#include "proto.h"
#include "types.h"

static char *getMacroParamValue(MacroCall *call, char *name);

static void generateMacroLine(void) {
    MacroCall *call;
    MacroFragment *frag;
    char *limit;
    MacroLine *line;
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
                fputs("Failed to read source file\n", stderr);
                exit(1);
            }
        }
        else if (c == '\n') {
            break;
        }
        else if (i < MAX_SOURCE_LINE_LENGTH) {
            sourceLine[i++] = c;
            if (c != ' ') lineEnd = i;
        }
    }
    sourceLine[lineEnd] = '\0';
}
