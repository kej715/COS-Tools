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
#include "calconst.h"
#include "calproto.h"
#include "caltypes.h"
#include "cosdataset.h"
#include "cosldr.h"
#include "services.h"

static void addExternalEntry(Section *section, Value *val, bool isParcelRelocation, u32 bitAddress, u8 fieldLength);
static void addRelocationEntry(Section *section, Value *val, bool isParcelRelocation);
static int countBlocks(Module *module);
static int countEntries(Module *module);
static int countExternals(Module *module);
static u64 extractSubfield(u64 word, int startingBitPos, int len);
static u64 getWord(Section *section, u32 parcelAddress);
static void putHalfWord(Section *section, u32 parcelAddress, u32 halfWord);
static void putParcel(Section *section, u32 parcelAddress, u16 parcel);
static void putWord(Section *section, u32 parcelAddress, u64 word);
static int writeBRT(ObjectBlock *block, Dataset *ds);
static int writeCommonBlockEntry(ObjectBlock *block, Dataset *ds);
static int writeEntryEntries(Module *module, Dataset *ds);
static int writeExternalEntries(Module *module, Dataset *ds);
static int writeName(char *name, Dataset *ds);
static int writePDT(Module *module, Dataset *ds);
static int writeProgramEntry(Module *module, Dataset *ds);
static int writeString(char *s, Dataset *ds);
static int writeTrailer(Module *module, Dataset *ds);
static int writeTXT(ObjectBlock *block, bool isAbsolute, Dataset *ds);
static int writeXRT(Module *module, Dataset *ds);

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
        block->externalTableSize += EXTERN_TABLE_INCREMENT;
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
        baseBlock->relocationTableSize += RELOC_TABLE_INCREMENT;
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
    int bitPosCtr;

    bitPosCtr = (int)section->parcelBitPosCounter + count;
    while (bitPosCtr > 15) {
        section->originCounter += 1;
        section->locationCounter += 1;
        bitPosCtr -= 16;
    }
    section->parcelBitPosCounter = (u8)bitPosCtr;
    section->wordBitPosCounter = ((section->locationCounter & 0x03) * 16) + section->parcelBitPosCounter;
    if (pass == 1 && section->originCounter > section->size) {
        section->size = section->originCounter;
    }
}

static int countBlocks(Module *module) {
    ObjectBlock *block;
    int count;

    for (count = 0, block = module->firstObjectBlock; block != NULL; block = block->next) count += 1;
    return count;
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
    char *cp;
    int fillCount;
    char *limit;
    int n;

    n = 0;
    limit = s + len;
    for (cp = s; cp < limit; cp++) {
         if (*cp == '\'') cp += 1;
         n += 1;
    }
    if (n > count) n = count;
    fillCount = count - n;

    emitFieldStart(section);
    switch (justification) {
    case Justify_LeftBlankFill:
        while (n-- > 0) {
            if (*s == '\'') s += 1;
            emitFieldBits(section, *s++, 8, 0, n > 0 || fillCount > 0);
        }
        while (fillCount-- > 0) emitFieldBits(section, 0x20, 8, 0, fillCount > 0);
        break;
    case Justify_LeftZeroEnd:
        if (fillCount < 1) {
            fillCount = 1;
            n -= 1;
        }
    case Justify_LeftZeroFill:
        while (n-- > 0) {
            if (*s == '\'') s += 1;
            emitFieldBits(section, *s++, 8, 0, n > 0 || fillCount > 0);
        }
        while (fillCount-- > 0) emitFieldBits(section, 0, 8, 0, fillCount > 0);
        break;
    case Justify_RightZeroFill:
        while (fillCount-- > 0) emitFieldBits(section, 0, 8, 0, TRUE);
        while (n-- > 0) {
            if (*s == '\'') s += 1;
            emitFieldBits(section, *s++, 8, 0, n > 0 || n > 0);
        }
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
    ObjectBlock *block;
    u32 addr;
    u32 limit;
    u64 word;

    if (pass == 1) return 0;
    addr = (parcelAddress & 0xfffffc) * 2;
    limit = addr + 7;
    block = section->objectBlock;
    while (limit >= block->imageSize) {
        block->image = (u8 *)reallocate(block->image, block->imageSize, block->imageSize + IMAGE_INCREMENT);
        block->imageSize += IMAGE_INCREMENT;
    }
    word = 0;
    while (addr <= limit) {
        word = (word << 8) | block->image[addr++];
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
    ObjectBlock *block;

    if (pass == 1) return;
    addr = parcelAddress * 2;
    block = section->objectBlock;
    while (addr + 1 >= block->imageSize) {
        if (block->image == NULL) block->lowestParcelAddress = parcelAddress;
        block->image = (u8 *)reallocate(block->image, block->imageSize, block->imageSize + IMAGE_INCREMENT);
        block->imageSize += IMAGE_INCREMENT;
    }
    block->image[addr] = parcel >> 8;
    block->image[addr + 1] = parcel & 0xff;
    if (parcelAddress < block->lowestParcelAddress) block->lowestParcelAddress = parcelAddress;
    if (parcelAddress > block->highestParcelAddress) block->highestParcelAddress = parcelAddress;
}

/*
 *  putWord - put a word into a module image referenced by a parcel address
 */
static void putWord(Section *section, u32 parcelAddress, u64 word) {
    int shiftCount;

    if (pass == 2) {
        parcelAddress &= 0xfffffc;
        for (shiftCount = 48; shiftCount >= 0; shiftCount -= 16) {
            putParcel(section, parcelAddress++, (word >> shiftCount) & 0xffff);
        }
    }
}

/*
 *  reserveStorage - ensure that highest parcel address is not less than the origin counter
 */
void reserveStorage(Section *section, u32 firstAddress, u32 count) {
    u32 addr;
    ObjectBlock *block;
    u32 lastAddress;

    if (pass == 1) return;
    lastAddress = firstAddress + count - 1;
    addr = lastAddress * 2;
    block = section->objectBlock;
    while (addr + 1 >= block->imageSize) {
        if (block->image == NULL) block->lowestParcelAddress = firstAddress;
        block->image = (u8 *)reallocate(block->image, block->imageSize, block->imageSize + IMAGE_INCREMENT);
        block->imageSize += IMAGE_INCREMENT;
    }
    if (firstAddress < block->lowestParcelAddress) block->lowestParcelAddress = firstAddress;
    if (lastAddress > block->highestParcelAddress) block->highestParcelAddress = lastAddress;
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
        if (writeName(symbol->id, ds) == -1) return -1;
        word = (symbol->value.attributes & SYM_PARCEL_ADDRESS) != 0 ? 1 : 0; // relocation mode
        word |= symbol->value.section->objectBlock->index << 1;
        if (symbol == module->start) word |= 0x100; // primary entry point
        if (cosDsWriteWord(ds, word) == -1) return -1;
        word = symbol->value.intValue;
        if (cosDsWriteWord(ds, word) == -1) return -1;
    }
    return 0;
}

static int writeBRT(ObjectBlock *block, Dataset *ds) {
    RelocationTableEntry *entry;
    int i;
    u64 word;

    word = ((u64)LDR_TT_BRT << 60) | (((((u64)block->relocationTableIndex + 1) / 2) + 1) << 36) | ((u64)block->index << 25);
    if (cosDsWriteWord(ds, word) == -1) return -1;
    i = 0;
    while (i < block->relocationTableIndex) {
        entry = &block->relocationTable[i++];
        word = (u64)entry->blockIndex << 25;
        if (entry->isParcelRelocation) word |= (u64)1 << 24;
        word |= entry->offset;
        word <<= 32;
        if (i < block->relocationTableIndex) {
            entry = &block->relocationTable[i++];
            word |= (u64)entry->blockIndex << 25;
            if (entry->isParcelRelocation) word |= (u64)1 << 24;
            word |= entry->offset;
        }
        else {
            word |= (u64)0xffffffff;
        }
        if (cosDsWriteWord(ds, word) == -1) return -1;
    }
    return 0;
}

static int writeCommonBlockEntry(ObjectBlock *block, Dataset *ds) {
    u64 blockOrigin;
    u64 blockSize;
    u64 word;

    if (writeName(block->id, ds) == -1) return -1;
    word = 0;
    if (block->location == SectionLocation_EM)
        word |= (u64)2 << 48;
    blockOrigin = block->lowestParcelAddress & 0xfffffc;
    blockSize = (((block->highestParcelAddress + 4) & 0xfffffc) - blockOrigin) >> 2;
    word |= blockSize;
    if (cosDsWriteWord(ds, word) == -1) return -1;
    return 0;
}

static int writeExternalEntries(Module *module, Dataset *ds) {
    Symbol *symbol;

    for (symbol = module->firstExternal; symbol != NULL; symbol = symbol->next) {
        if (writeName(symbol->id, ds) == -1) return -1;
    }
    return 0;
}

static int writeName(char *name, Dataset *ds) {
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

int writeObjectRecord(Module *module, Dataset *ds) {
    ObjectBlock *block;

    /*
     *  Write the Program Description Table (PDT)
     */
    if (writePDT(module, ds) == -1) return -1;
    /*
     *  Write a Text Table (TXT) for each object block
     */
    for (block = module->firstObjectBlock; block != NULL; block = block->next) {
         if (writeTXT(block, block->type == SectionType_Mixed && module->isAbsolute, ds) == -1)
             return -1;
    }
    /*
     *  Write a Block Relocation Table (BRT) for each object block that
     *  has relocation entries.
     */
    for (block = module->firstObjectBlock; block != NULL; block = block->next) {
         if (block->relocationTableIndex > 0
             && writeBRT(block, ds) == -1)
             return -1;
    }
    /*
     *  Write the External Reference Table (XRT)
     */
    if (writeXRT(module, ds) == -1) return -1;
    if (cosDsWriteEOR(ds) == -1) return -1;
    return 0;
}

static int writePDT(Module *module, Dataset *ds) {
    ObjectBlock *block;
    u64 blockCount;
    u64 entryCount;
    u64 externalCount;
    int i;
    static u8 machineType[] = {
        'C','R','A','Y','-','X','M','P'
    };
    u32 moduleHLM;
    u64 pdtLen;
    u64 word;

    if (ds == NULL) return 0;

    blockCount = countBlocks(module);
    entryCount = countEntries(module);
    externalCount = countExternals(module);
    moduleHLM = 0;
    for (block = module->firstObjectBlock; block != NULL; block = block->next) {
        moduleHLM += (block->highestParcelAddress + 4) & 0xfffffc;
    }
    if (module->isAbsolute == FALSE) moduleHLM += 0200;

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
    if (cosDsWriteWord(ds, (u64)(moduleHLM >> 2)) == -1) return -1; // HLM for binary
    for (i = 0; i < 4; i++) {
        if (cosDsWriteWord(ds, 0) == -1) return -1;
    }
    word = 0x0000000000000003; // machine characteristics entry length
    if (cosDsWriteWord(ds, word) == -1) return -1;
    if (cosDsWrite(ds, machineType, sizeof(machineType)) == -1) return -1;
    if (cosDsWriteWord(ds, 0) == -1) return -1; // machine characteristics flags

    if (writeProgramEntry(module, ds) == -1) return -1;

    for (block = module->firstObjectBlock; block != NULL; block = block->next) {
         if (block->type != SectionType_Mixed
             && writeCommonBlockEntry(block, ds) == -1) return -1;
    }

    if (writeEntryEntries(module, ds) == -1) return -1;

    if (writeExternalEntries(module, ds) == -1) return -1;

    if (writeTrailer(module, ds) == -1) return -1;

    return 0;
}

static int writeProgramEntry(Module *module, Dataset *ds) {
    ObjectBlock *block;
    u64 programOrigin;
    u64 programSize;
    u64 word;

    for (block = module->firstObjectBlock; block != NULL; block = block->next) {
         if (block->type == SectionType_Mixed) break;
    }
    if (block == NULL) {
        return (writeName(" ", ds) == -1 || cosDsWriteWord(ds, 0) == -1) ? -1 : 0;
    }
    if (writeName(module->id, ds) == -1) return -1;
    word = 0;
    if (module->isAbsolute) {
        word |= (u64)1 << 63;
        programOrigin = block->lowestParcelAddress >> 2;
        programSize = ((block->highestParcelAddress + 4) >> 2) - programOrigin;
    }
    else {
        programOrigin = 0;
        programSize = (block->highestParcelAddress + 4) >> 2;
    }
    if (getErrorCount() > 0) word |= (u64)1 << 62;
    word |= programOrigin << 24;
    word |= programSize;
    if (cosDsWriteWord(ds, word) == -1) return -1;
    return 0;
}

static int writeString(char *s, Dataset *ds) {
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

    if (writeName(currentDate, ds) == -1) return -1;
    if (writeName(currentTime, ds) == -1) return -1;
    if (writeName(osName, ds) == -1) return -1;
    if (writeName(osDate, ds) == -1) return -1;
    if (cosDsWriteWord(ds, 0) == -1) return -1; // reserved
    if (writeName(calName, ds) == -1) return -1;
    if (writeName(calVersion, ds) == -1) return -1;
    for (i = 0; i < 4; i++) { // reserved
        if (cosDsWriteWord(ds, 0) == -1) return -1; // reserved
    }
    if (writeString(module->comment, ds) == -1) return -1;
    return 0;
}

static int writeTXT(ObjectBlock *block, bool isAbsolute, Dataset *ds) {
    int byteCount;
    u32 firstParcelAddress;
    u32 loadAddress;
    u64 parcelCount;
    u64 word;
    u64 wordCount;
    u64 imageLength;

    //
    //  Write headser word
    //
    if (block->lowestParcelAddress != block->highestParcelAddress) { // block not empty
        firstParcelAddress = block->lowestParcelAddress & 0xfffffc;
        parcelCount = ((block->highestParcelAddress + 4) & 0xfffffc) - firstParcelAddress;
        loadAddress = isAbsolute ? firstParcelAddress : 0;
        word = ((u64)LDR_TT_TXT << 60) | (((parcelCount >> 2) + 1) << 36) | (loadAddress >> 2);
    }
    else {
        word = ((u64)LDR_TT_TXT << 60) | ((u64)1 << 36);
    }
    if (cosDsWriteWord(ds, word) == -1) return -1;
    byteCount = parcelCount * 2;
    if (block->image == NULL
        || cosDsWrite(ds, block->image + (firstParcelAddress * 2), byteCount) == byteCount)
        return 0;
    else
        return -1;
}

static int writeXRT(Module *module, Dataset *ds) {
    ObjectBlock *block;
    ExternalTableEntry *entry;
    u64 entryCount;
    int i;
    u64 word;

    entryCount = 0;
    for (block = module->firstObjectBlock; block != NULL; block = block->next) {
        entryCount += block->externalTableIndex;
    }
    if (entryCount < 1) return 0;

    word = ((u64)LDR_TT_XRT << 60) | ((entryCount + 1) << 36);
    if (cosDsWriteWord(ds, word) == -1) return -1;
    
    for (block = module->firstObjectBlock; block != NULL; block = block->next) {
        for (i = 0; i < block->externalTableIndex; i++) {
            entry = &block->externalTable[i];
            word = (u64)block->index << 51;
            if (entry->isParcelRelocation) word |= (u64)1 << 50;
            word |= (u64)entry->externalIndex << 36;
            word |= (u64)entry->fieldLength << 30;
            word |= (u64)entry->bitAddress;
            if (cosDsWriteWord(ds, word) == -1) return -1;
        }
    }
    return 0;
}
