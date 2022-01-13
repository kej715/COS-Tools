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

static void addExternalEntry(Section *section, Value *val, bool isParcelRelocation, u32 bitAddress, u8 fieldLength);
static void addRelocationEntry(Section *section, Value *val, bool isParcelRelocation);
static int countEntries(Module *module);
static int countExternals(Module *module);
static u64 extractSubfield(u64 word, int startingBitPos, int len);
static u64 getWord(Section *section, u32 parcelAddress);
static void putHalfWord(Section *section, u32 parcelAddress, u32 halfWord);
static void putParcel(Section *section, u32 parcelAddress, u16 parcel);
static void putWord(Section *section, u32 parcelAddress, u64 word);
static int writeEntryEntries(Module *module, Dataset *ds);
static int writeExternalEntries(Module *module, Dataset *ds);
static int writeName(Module *module, Dataset *ds, char *name);
static int writePDT(Module *module, Dataset *ds);
static int writeProgramEntry(Module *module, Dataset *ds);
static int writeString(Module *module, Dataset *ds, char *s);
static int writeTrailer(Module *module, Dataset *ds);
static int writeTXT(Module *module, Dataset *ds);

/*
 *  addExternalEntry - add a relocation table entry to a referenced object block
 */
static void addExternalEntry(Section *section, Value *val, bool isParcelRelocation, u32 bitAddress, u8 fieldLength) {
    ObjectBlock *block;
    ExternalTableEntry *entry;
    u16 targetBlockIndex;

    if (pass == 1) return;
    block = section->objectBlock;
    if (block->externalTableIndex >= block->externalTableSize) {
        block->externalTable = (ExternalTableEntry *)reallocate(block->externalTable,
            block->externalTableSize * sizeof(ExternalTableEntry),
            (block->externalTableSize + EXTERN_TABLE_INCREMENT) * sizeof(ExternalTableEntry));
    }
    entry = &block->externalTable[block->externalTableIndex++];
    entry->externalIndex = val->externalSymbol->externalIndex;
    entry->bitAddress = bitAddress;
    entry->fieldLength = fieldLength;
    entry->isParcelRelocation = isParcelRelocation;
}

/*
 *  addRelocationEntry - add a relocation table entry to a referenced object block
 */
static void addRelocationEntry(Section *section, Value *val, bool isParcelRelocation) {
    ObjectBlock *baseBlock;
    RelocationTableEntry *entry;
    u16 targetBlockIndex;

    if (pass == 1) return;
    baseBlock = val->section->objectBlock;
    if (baseBlock->relocationTableIndex >= baseBlock->relocationTableSize) {
        baseBlock->relocationTable = (RelocationTableEntry *)reallocate(baseBlock->relocationTable,
            baseBlock->relocationTableSize * sizeof(RelocationTableEntry),
            (baseBlock->relocationTableSize + RELOC_TABLE_INCREMENT) * sizeof(RelocationTableEntry));
    }
    entry = &baseBlock->relocationTable[baseBlock->relocationTableIndex++];
    entry->blockIndex = section->objectBlock->index;
    entry->offset = section->originCounter;
    entry->isParcelRelocation = isParcelRelocation;
}

/*
 *  advanceBitPosition - advance to the next bit position at which to emit code
 */
void advanceBitPosition(Section *section, int count) {
    section->parcelBitPosCounter += count;
    while (section->parcelBitPosCounter > 15) {
        section->originCounter += 1;
        section->locationCounter += 1;
        section->parcelBitPosCounter -= 16;
    }
    section->wordBitPosCounter = ((section->locationCounter & 0x03) * 16) + section->parcelBitPosCounter;
    if (pass == 1 && section->originCounter > section->size) {
        section->size = section->originCounter;
    }
}

static int countEntries(Module *module) {
    int count;
    Symbol *symbol;

    for (count = 0, symbol = module->entryPoints; symbol != NULL; symbol = symbol->next) count += 1;
    return count;
}

static int countExternals(Module *module) {
    int count;
    Symbol *symbol;

    for (count = 0, symbol = module->firstExternal; symbol != NULL; symbol = symbol->next) count += 1;
    return count;
}

/*
 *  emit_g_h_i_jkm - emit an instruction with 4-bit op code, 3-bit index register, 3-bit source or result register,
 *  and 22-bit address or displacement
 */
void emit_g_h_i_jkm(Section *section, u8 g, u8 h, u8 i, Value *jkm) {
    u32 instr;

    instr = (g << 28) | (h << 25) | (i << 22) | (jkm->intValue & MASK22);
    putHalfWord(section, section->originCounter, instr);
    if (isExternal(jkm)) {
       addExternalEntry(section, jkm, FALSE, section->originCounter * 16 + 31, 22);
    }
    else if (isRelocatable(jkm)) {
        addRelocationEntry(section, jkm, FALSE);
    }
    listCodeLocation(section);
    listCode10_22(instr, jkm->attributes);
    advanceBitPosition(section, 32);
}

/*
 *  emit_gh_i_j_k - emit an instruction with 7-bit op code and 3 3-bit register designators
 */
void emit_gh_i_j_k(Section *section, u8 gh, u8 i, u8 j, u8 k) {
    u16 instr;

    instr = (gh << 9) | (i << 6) | (j << 3) | k;
    putParcel(section, section->originCounter, instr);
    listCodeLocation(section);
    listCode16(instr);
    advanceBitPosition(section, 16);
}

/*
 *  emit_gh_i_jk - emit an instruction with 7-bit op code, 3-bit result register, and 6-bit constant or register designator
 */
void emit_gh_i_jk(Section *section, u8 gh, u8 i, u8 jk) {
    u16 instr;

    instr = (gh << 9) | (i << 6) | (jk & MASK6);
    putParcel(section, section->originCounter, instr);
    listCodeLocation(section);
    listCode16(instr);
    advanceBitPosition(section, 16);
}

/*
 *  emit_gh_ijk - emit an instruction with 7-bit op code and 9-bit constant
 */
void emit_gh_ijk(Section *section, u8 gh, u16 ijk) {
    u16 instr;

    instr = (gh << 9) | (ijk & MASK9);
    putParcel(section, section->originCounter, instr);
    listCodeLocation(section);
    listCode16(instr);
    advanceBitPosition(section, 16);
}

/*
 *  emit_gh_i_jkm - emit an instruction with 7-bit op code, 3-bit result register, and 22-bit constant
 */
void emit_gh_i_jkm(Section *section, u8 gh, u8 i, Value *jkm) {
    u32 instr;

    instr = (gh << 25) | (i << 22) | (jkm->intValue & MASK22);
    putHalfWord(section, section->originCounter, instr);
    if (isExternal(jkm)) {
       addExternalEntry(section, jkm, FALSE, section->originCounter * 16 + 31, 22);
    }
    else if (isRelocatable(jkm)) {
        addRelocationEntry(section, jkm, FALSE);
    }
    listCodeLocation(section);
    listCode10_22(instr, jkm->attributes);
    advanceBitPosition(section, 32);
}

/*
 *  emit_gh_ijkm - emit an instruction with 7-bit op code, and 24-bit parcel address
 */
void emit_gh_ijkm(Section *section, u8 gh, Value *ijkm) {
    u32 instr;

    instr = (gh << 25) | (ijkm->intValue & MASK24);
    putHalfWord(section, section->originCounter, instr);
    if (isExternal(ijkm)) {
       addExternalEntry(section, ijkm, TRUE, section->originCounter * 16 + 31, 24);
    }
    else if (isRelocatable(ijkm)) {
        addRelocationEntry(section, ijkm, TRUE);
    }
    listCodeLocation(section);
    listCode7_24(instr, ijkm->attributes);
    advanceBitPosition(section, 32);
}

/*
 *  emitFieldBits - emit a field of bits
 */
static int startingBitPos = 0;

void emitFieldBits(Section *section, u64 bits, int len, u16 attributes, bool doListFlush) {
    u64 currentWord;
    int emptyBitCount;
    int shiftCount;
    u64 subfield;
    int subfieldLen;

    currentWord = getWord(section, section->originCounter);
    emptyBitCount = 64 - section->wordBitPosCounter;
    while (len > emptyBitCount) {
        shiftCount = len - emptyBitCount;
        currentWord |= bits >> shiftCount;
        putWord(section, section->originCounter, currentWord);
        subfieldLen = 64 - startingBitPos;
        subfield = extractSubfield(currentWord, startingBitPos, subfieldLen);
        listField(subfield, subfieldLen, attributes, 21);
        listFlush(section);
        listCodeLocation(section);
        len = shiftCount;
        bits = extractSubfield(bits, 64 - len, len);
        advanceBitPosition(section, emptyBitCount);
        currentWord = getWord(section, section->originCounter);
        startingBitPos = 0;
        emptyBitCount = 64 - section->wordBitPosCounter;
    }
    if (len > 0) {
        shiftCount = 64 - (section->wordBitPosCounter + len);
        currentWord |= bits << shiftCount;
        putWord(section, section->originCounter, currentWord);
        advanceBitPosition(section, len);
        if (section->wordBitPosCounter == 0) {
            subfieldLen = 64 - startingBitPos;
            subfield = extractSubfield(currentWord, startingBitPos, subfieldLen);
            listField(subfield, subfieldLen, attributes, 21);
            if (doListFlush) {
                listFlush(section);
                listCodeLocation(section);
            }
            startingBitPos = 0;
        }
    }
}

/*
 *  emitFieldEnd - complete the emission of a field of bits
 */
void emitFieldEnd(Section *section, u16 attributes) {
    int lastBitPos;
    int lastCol;
    int len;
    int shiftCount;
    u64 subfield;

    len = section->wordBitPosCounter - startingBitPos;
    if (len > 0) {
        lastCol = (section->wordBitPosCounter + 1) / 3;
        subfield = extractSubfield(getWord(section, section->originCounter), startingBitPos, len);
        lastBitPos = (section->wordBitPosCounter - 1) % 3;
        if (lastBitPos > 0) {
            shiftCount = 3 - lastBitPos;
            subfield <<= shiftCount;
            len += shiftCount;
        }
        listField(subfield, len, attributes, lastCol);
    }
}

/*
 *  emitFieldStart - begin the emission of a field of bits
 */
void emitFieldStart(Section *section) {
    startingBitPos = section->wordBitPosCounter;
}

/*
 *  emitLiterals - emit literals into the literals section
 */
void emitLiterals(Module *module) {
    Literal *literal;
    Section *section;
    u16 savedListControl;
    Token *str;
    Value val;

    savedListControl = currentListControl;
    section = module->firstSection->next; // Literals section is always 2nd in module
    currentListControl = 0; // suppress listing completely
    for (literal = module->literals; literal != NULL; literal = literal->next) {
        forceWordBoundary(section);
        literal->offset = section->locationCounter;
        if (literal->expression->type == TokenType_String) {
            emitString(section, literal->expression->details.string.ptr, literal->expression->details.string.len,
                       literal->expression->details.string.count, literal->expression->details.string.justification);
        }
        else {
            (void)evaluateExpression(literal->expression, &val);
            emitFieldStart(section);
            emitFieldBits(section, (val.type == NumberType_Integer) ? val.intValue : toCrayFloat(val.intValue), 64, val.attributes, FALSE);
            emitFieldEnd(section, val.attributes);
        }
    }
    currentListControl = savedListControl;
}

/*
 *  emitString - emit a string of text
 */
void emitString(Section *section, char *s, int len, int count, JustifyType justification) {
    int fillCount;

    if (len > count) len = count;
    fillCount = count - len;

    emitFieldStart(section);
    switch (justification) {
    case Justify_LeftBlankFill:
        while (len-- > 0) emitFieldBits(section, *s++, 8, 0, len > 0 || fillCount > 0);
        while (fillCount-- > 0) emitFieldBits(section, 0x20, 8, 0, fillCount > 0);
        break;
    case Justify_LeftZeroFill:
        while (len-- > 0) emitFieldBits(section, *s++, 8, 0, len > 0 || fillCount > 0);
        while (fillCount-- > 0) emitFieldBits(section, 0, 8, 0, fillCount > 0);
        break;
    case Justify_RightZeroFill:
        while (fillCount-- > 0) emitFieldBits(section, 0, 8, 0, TRUE);
        while (len-- > 0) emitFieldBits(section, *s++, 8, 0, len > 0);
        break;
    case Justify_LeftZeroEnd:
        if (len >= count) len -= 1;
        while (len-- > 0) emitFieldBits(section, *s++, 8, 0, TRUE);
        emitFieldBits(section, 0, 8, 0, fillCount > 0);
        while (fillCount-- > 0) emitFieldBits(section, 0, 8, 0, fillCount > 0);
        break;
    }
    emitFieldEnd(section, 0);
}

/*
 *  extractSubfield - extract a subfield of bits from a word
 */
static u64 extractSubfield(u64 word, int startingBitPos, int len) {
    u64 mask;
    int shiftCount;

    if (len >= 64) return word;
    mask = ~((~(u64)0 >> len) << len);
    shiftCount = 64 - (startingBitPos + len);
    return (word >> shiftCount) & mask;
}

/*
 *  forceWordBoundary - advance location and origin counters to next word boundary, if necessary
 */
void forceWordBoundary(Section *section) {
    if (section->parcelBitPosCounter > 0)
        advanceBitPosition(section, 16 - section->parcelBitPosCounter);
    while ((section->locationCounter & 0x03) != 0) {
        advanceBitPosition(section, 16);
    }
}

/*
 *  getWord - get the word from a module image referenced by a parcel address
 */
static u64 getWord(Section *section, u32 parcelAddress) {
    u32 addr;
    u32 limit;
    u64 word;

    if (pass == 1) return 0;
    addr = (parcelAddress & 0xfffffc) * 2;
    limit = addr + 7;
    while (limit >= section->objectBlock->imageSize) {
        section->objectBlock->image = (u8 *)reallocate(section->objectBlock->image, section->objectBlock->imageSize,
                                                       section->objectBlock->imageSize + IMAGE_INCREMENT);
        section->objectBlock->imageSize += IMAGE_INCREMENT;
    }
    word = 0;
    while (addr <= limit) {
        word = (word << 8) | section->objectBlock->image[addr++];
    }

    return word;
}

/*
 *  putHalfWord - put two parcels into a module image referenced by a parcel address
 */
static void putHalfWord(Section *section, u32 parcelAddress, u32 halfWord) {
    putParcel(section, parcelAddress, halfWord >> 16);
    putParcel(section, parcelAddress + 1, halfWord & 0xffff);
}

/*
 *  putParcel - put a parcel into a module image referenced by a parcel address
 */
static void putParcel(Section *section, u32 parcelAddress, u16 parcel) {
    u32 addr;

    if (pass == 1) return;
    addr = parcelAddress * 2;
    while (addr + 1 >= section->objectBlock->imageSize) {
        section->objectBlock->image = (u8 *)reallocate(section->objectBlock->image, section->objectBlock->imageSize,
                                                       section->objectBlock->imageSize + IMAGE_INCREMENT);
        section->objectBlock->imageSize += IMAGE_INCREMENT;
    }
    section->objectBlock->image[addr] = parcel >> 8;
    section->objectBlock->image[addr + 1] = parcel & 0xff;
}

/*
 *  putWord - put a word into a module image referenced by a parcel address
 */
static void putWord(Section *section, u32 parcelAddress, u64 word) {
    u32 addr;
    u32 limit;
    int shiftCount;

    if (pass == 1) return;
    addr = (parcelAddress & 0xfffffc) * 2;
    limit = addr + 7;
    while (limit >= section->objectBlock->imageSize) {
        section->objectBlock->image = (u8 *)reallocate(section->objectBlock->image, section->objectBlock->imageSize,
                                                       section->objectBlock->imageSize + IMAGE_INCREMENT);
        section->objectBlock->imageSize += IMAGE_INCREMENT;
    }
    shiftCount = 56;
    while (addr <= limit) {
        section->objectBlock->image[addr++] = (word >> shiftCount) & 0xff;
        shiftCount -= 8;
    }
}

/*
 *  toCrayFloat - map IEEE 754 floating point format to Cray format
 */
u64 toCrayFloat(u64 ieee) {
    u64 cray_f;
    long exponent;
    u64 fraction;
    u64 sign;

    if (ieee == 0) return 0;
    sign = ieee & 0x8000000000000000;
    exponent = ((ieee >> 52) & 0x7ff) - 1023; // unbias the 11-bit exponent
    fraction = ieee & 0xfffffffffffff;        // 1 implied to left of binary point
    //
    //  Normalized value has 1 in most significant bit of fraction, so shift a 1 into the IEEE
    //  fraction, adjust the exponent accordingly, then add Cray bias to produce 15-bit exponent.
    //
    cray_f = sign | ((((exponent + 1) + 040000) & 0x7fff) << 48) | ((fraction >> 5) | 0x800000000000);
    return cray_f;
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

    for (symbol = module->firstExternal; symbol != NULL; symbol = symbol->next) {
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
    cosDsWrite(ds, module->firstObjectBlock->image + (module->origin * 8), imageLength * 8);
    return 0;
}
