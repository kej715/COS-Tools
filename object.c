/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: object.c
**
**  Description:
**      This file privides functions for creating COS format object files.
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

#include <string.h>
#include "const.h"
#include "cosdataset.h"
#include "cosldr.h"
#include "proto.h"
#include "types.h"

static int countEntries(Module *module);
static int countExternals(Module *module);
static int writeEntryEntries(Module *module, Dataset *ds);
static int writeExternalEntries(Module *module, Dataset *ds);
static int writeName(Module *module, Dataset *ds, char *name);
static int writePDT(Module *module, Dataset *ds);
static int writeProgramEntry(Module *module, Dataset *ds);
static int writeString(Module *module, Dataset *ds, char *s);
static int writeTrailer(Module *module, Dataset *ds);
static int writeTXT(Module *module, Dataset *ds);

static int countEntries(Module *module) {
    int count;
    Symbol *symbol;

    for (count = 0, symbol = module->entryPoints; symbol != NULL; symbol = symbol->next) count += 1;
    return count;
}

static int countExternals(Module *module) {
    int count;
    Symbol *symbol;

    for (count = 0, symbol = module->externals; symbol != NULL; symbol = symbol->next) count += 1;
    return count;
}

static int writeEntryEntries(Module *module, Dataset *ds) {
    Symbol *symbol;
    u64 word;

    for (symbol = module->entryPoints; symbol != NULL; symbol = symbol->next) {
        if (writeName(module, ds, symbol->id) == -1) return -1;
        word = 1; // relocation mode
        if (symbol == module->start) word |= 0x100; // primary entry point
        if (cosDsWriteWord(ds, word) == -1) return -1;
        word = (symbol->value.attributes & SYM_WORD_ADDRESS) == 0 ? symbol->value.intValue : symbol->value.intValue << 2;
        if (cosDsWriteWord(ds, word) == -1) return -1;
    }
    return 0;
}

static int writeExternalEntries(Module *module, Dataset *ds) {
    Symbol *symbol;

    for (symbol = module->externals; symbol != NULL; symbol = symbol->next) {
        if (writeName(module, ds, symbol->id) == -1) return -1;
    }
    return 0;
}

static int writeName(Module *module, Dataset *ds, char *name) {
    int i;
    int shiftCount;
    u64 word;

    word = 0;
    for (i = 0, shiftCount = 56; i < 8; i++, shiftCount -= 8) {
        if (*name != '\0') {
            word |= ((u64)*name++) << shiftCount;
        }
        else {
            word |= (u64)' ' << shiftCount;
        }
    }
    if (cosDsWriteWord(ds, word) == -1) return -1;
    return 0;
}

int writeObjectFile(Module *module, Dataset *ds) {
    if (writePDT(module, ds) == -1) return -1;
    if (writeTXT(module, ds) == -1) return -1;
    cosDsWriteEOR(ds);
    cosDsWriteEOF(ds);
    return 0;
}

static int writePDT(Module *module, Dataset *ds) {
    u64 blockCount;
    u64 entryCount;
    u64 externalCount;
    int i;
    static u8 machineType[] = {
        'C','R','A','Y','-','X','M','P'
    };
    u64 pdtLen;
    u64 word;

    if (ds == NULL) return 0;

    blockCount = 1;
    entryCount = countEntries(module);
    externalCount = countExternals(module);

    pdtLen = 1                 // header word
           + 20                // header entry
           + (blockCount * 2)
           + (entryCount * 3)
           + externalCount
           + 11;               // fixed portion of trailer
    if (module->comment != NULL) {
        pdtLen += (strlen(module->comment) + 7) / 8;
    }
    //
    //  Write headser word
    //
    word = ((u64)LDR_TT_PDT << 60)
         | (pdtLen << 36)
         | (externalCount << 22)
         | ((entryCount * 3) << 8)
         | (blockCount * 2);
    if (cosDsWriteWord(ds, word) == -1) return -1;
    //
    //  Write header entry
    //
    if (cosDsWriteWord(ds, (u64)20) == -1) return -1;  // HL field
    word = 0x0980000000000000; // machine type extensions, calling sequence, PDT type
    if (cosDsWriteWord(ds, word) == -1) return -1;
    for (i = 0; i < 10; i++) {
        if (cosDsWriteWord(ds, 0) == -1) return -1;
    }
    if (cosDsWriteWord(ds, (u64)(module->size - module->origin)) == -1) return -1; // HLM for binary (program length)
    for (i = 0; i < 4; i++) {
        if (cosDsWriteWord(ds, 0) == -1) return -1;
    }
    word = 0x0000000000000003; // machine characteristics entry length
    if (cosDsWriteWord(ds, word) == -1) return -1;
    if (cosDsWrite(ds, machineType, sizeof(machineType)) == -1) return -1;
    if (cosDsWriteWord(ds, 0) == -1) return -1; // machine characteristics flags

    if (writeProgramEntry(module, ds) == -1) return -1;

    if (writeEntryEntries(module, ds) == -1) return -1;

    if (writeExternalEntries(module, ds) == -1) return -1;

    if (writeTrailer(module, ds) == -1) return -1;

    return 0;
}

static int writeProgramEntry(Module *module, Dataset *ds) {
    u64 word;

    if (writeName(module, ds, module->id) == -1) return -1;
    word = 0;
    if (module->isAbsolute)  word |= (u64)1 << 63;
    if (getErrorCount() > 0) word |= (u64)1 << 62;
    word |= (u64)module->origin << 24;
    word |= (u64)(module->size - module->origin);
    if (cosDsWriteWord(ds, word) == -1) return -1;
    return 0;
}

static int writeString(Module *module, Dataset *ds, char *s) {
    int i;
    int shiftCount;
    u64 word;

    if (s == NULL) return 0;

    while (*s != '\0') {
        word = 0;
        for (i = 0, shiftCount = 56; i < 8; i++, shiftCount -= 8) {
            if (*s != '\0') {
                word |= ((u64)*s++) << shiftCount;
            }
            else {
                word |= (u64)' ' << shiftCount;
            }
        }
        if (cosDsWriteWord(ds, word) == -1) return -1;
    }
    return 0;
}

static int writeTrailer(Module *module, Dataset *ds) {
    int i;
    u64 word;

    if (writeName(module, ds, currentDate) == -1) return -1;
    if (writeName(module, ds, currentTime) == -1) return -1;
    if (writeName(module, ds, osName) == -1) return -1;
    if (writeName(module, ds, osDate) == -1) return -1;
    if (cosDsWriteWord(ds, 0) == -1) return -1; // reserved
    if (writeName(module, ds, calName) == -1) return -1;
    if (writeName(module, ds, calVersion) == -1) return -1;
    for (i = 0; i < 4; i++) { // reserved
        if (cosDsWriteWord(ds, 0) == -1) return -1; // reserved
    }
    if (writeString(module, ds, module->comment) == -1) return -1;
    return 0;
}

static int writeTXT(Module *module, Dataset *ds) {
    u64 word;
    u64 imageLength;

    //
    //  Write headser word
    //
    imageLength = module->size - module->origin;
    word = ((u64)LDR_TT_TXT << 60) | ((imageLength + 1) << 36) | module->origin;
    if (cosDsWriteWord(ds, word) == -1) return -1;
    cosDsWrite(ds, module->image + (module->origin * 8), imageLength * 8);
    return 0;
}
