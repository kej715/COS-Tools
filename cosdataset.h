#ifndef COSFILE_H
#define COSFILE_H
/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: cosdataset.h
**
**  Description:
**      This file defines constants associated with COS file structures.
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
#include "basetypes.h"

/*
 *  Segment size
 *
 *  This is the number of bytes contained in each segment transmitted between
 *  the NOS and COS operating systems via the Cray Station software interface.
 *  The number of bits in a segment must be a multiple *  of 60 and a multiple
 *   of 64. The normal value is 28800 bytes (230400 bits).
 */
#define COS_SEGMENT_SIZE 28800

/*
 *  Control Word types
 */
#define COS_CW_BCW 000
#define COS_CW_EOR 010
#define COS_CW_EOF 016
#define COS_CW_EOD 017

/*
 *  BCW (Block Control Word) masks
 */
#define COS_BCW_M_MASK    0xf000000000000000
#define COS_BCW_BDF_MASK  0x0010000000000000
#define COS_BCW_BN_MASK   0x00000001fffffe00
#define COS_BCW_FWI_MASK  0x00000000000001ff

/*
 *  RCW (Record Control Word) masks
 */
#define COS_RCW_M_MASK    0xf000000000000000
#define COS_RCW_UBC_MASK  0x0fc0000000000000
#define COS_RCW_TRAN_MASK 0x0020000000000000
#define COS_RCW_BDF_MASK  0x0010000000000000
#define COS_RCW_SRS_MASK  0x0008000000000000
#define COS_RCW_PFI_MASK  0x00000fffff000000
#define COS_RCW_PRI_MASK  0x0000000000fffe00
#define COS_RCW_FWI_MASK  0x00000000000001ff

/*
 *  Dataset management structure
 */
#define COS_BLOCK_SIZE 4096
typedef struct dataset {
    int fd;
    bool isAtCW;
    bool isWritable;
    int cursor;
    int limit;
    int nextCtrlWordIndex;
    u64 controlWord;
    int bytesRead;
    int currentBlock;
    int lastFileBlock;
    int lastRecordBlock;
    int lastCtrlWordIndex;
    int bytesWritten;
    u8 buffer[COS_BLOCK_SIZE];
} Dataset;

/*
 *  Function prototypes
 */
int cosDsClose(Dataset *ds);
Dataset *cosDsCreate(char *pathname);
bool cosDsIsBCW(u64 cw);
bool cosDsIsEOD(u64 cw);
bool cosDsIsEOF(u64 cw);
bool cosDsIsEOR(u64 cw);
Dataset *cosDsOpen(char *pathname);
int cosDsRead(Dataset *ds, u8 *buffer, int len);
u64 cosDsReadCW(Dataset *ds);
int cosDsRewind(Dataset *ds);
int cosDsWrite(Dataset *ds, u8 *buffer, int len);
int cosDsWriteEOD(Dataset *ds);
int cosDsWriteEOF(Dataset *ds);
int cosDsWriteEOR(Dataset *ds);
int cosDsWriteWord(Dataset *ds, u64 word);

#endif
