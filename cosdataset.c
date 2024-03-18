/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: cosdataset.c
**
**  Description:
**      This file privides functions for managing COS structured datasets.
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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cosdataset.h"

#ifdef __cos

int cosDsClose(Dataset *ds) {
    return ds == NULL ? 0 : close(ds->fd);
}

Dataset *cosDsCreate(char *pathname) {
    Dataset *ds;
    int fd;

    fd = open(pathname, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY);

    return fd == -1 ? NULL : _ftPtr(fd);
}

bool cosDsIsBCW(u64 cw) {
    return 0;
}

bool cosDsIsEOD(u64 cw) {
    return cw == COS_EOD;
}

bool cosDsIsEOF(u64 cw) {
    return cw == COS_EOF;
}

bool cosDsIsEOR(u64 cw) {
    return cw == COS_EOR;
}

Dataset *cosDsOpen(char *pathname) {
    Dataset *ds;
    int fd;

    fd = open(pathname, O_RDONLY|O_BINARY);

    return fd == -1 ? NULL : _ftPtr(fd);
}

int cosDsRead(Dataset *ds, u8 *buffer, int len) {
    return read(ds->fd, buffer, len);
}

u64 cosDsReadCW(Dataset *ds) {
    u64 cw;

    cw = ds->status;
    ds->status = 0;

    return cw;
}

int cosDsRewind(Dataset *ds) {
    return _reopen(ds->fd);
}

int cosDsWrite(Dataset *ds, u8 *buffer, int len) {
    return write(ds->fd, buffer, len);
}

int cosDsWriteEOD(Dataset *ds) {
    return _coswed(ds);
}

int cosDsWriteEOF(Dataset *ds) {
    return _coswef(ds);
}

int cosDsWriteEOR(Dataset *ds) {
    return _coswer(ds);
}

int cosDsWriteWord(Dataset *ds, u64 word) {
    return write(ds->fd, &word, 8) == 8 ? 0 : -1;
}

#else

static int appendCW(Dataset *ds, u64 cw);
static int flushBuffer(Dataset *ds);
static u64 getWord(Dataset *ds);
static void setFWI(Dataset *ds);

static int appendCW(Dataset *ds, u64 cw) {
    int i;
    int shiftCount;

    ds->lastCtrlWordIndex = ds->cursor;
    for (i = 0, shiftCount = 56; i < 8; i++, shiftCount -= 8) {
         ds->buffer[ds->cursor++] = (cw >> shiftCount) & 0xff;
    }
    return (ds->cursor >= COS_BLOCK_SIZE) ? flushBuffer(ds) : 0;

}

/*
 *  cosDsClose - close a dataset
 */
int cosDsClose(Dataset *ds) {
    int n;

    if (ds == NULL) return 0;
    if (ds->isWritable && ds->cursor > 0) {
        n = write(ds->fd, ds->buffer, ds->cursor);
        if (n != ds->cursor) return -1;
    }
    close(ds->fd);
    free(ds);
    return 0;
}

/*
 *  cosDsCreate - create a dataset
 */
Dataset *cosDsCreate(char *pathname) {
    Dataset *ds;

    ds = (Dataset *)malloc(sizeof(Dataset));
    if (ds == NULL) return NULL;
    memset(ds, 0, sizeof(Dataset));
    ds->fd = open(pathname, O_CREAT|O_WRONLY|O_TRUNC, 0644);
    if (ds->fd == -1) {
        free(ds);
        return NULL;
    }
    ds->cursor = 8;
    ds->isWritable = 1;
    return ds;
}

/*
 *  cosDsIsBCW - test a control word for block control word indication
 */
bool cosDsIsBCW(u64 cw) {
    return (cw >> 60) == COS_CW_BCW;
}

/*
 *  cosDsIsEOD - test a control word for end of data indication
 */
bool cosDsIsEOD(u64 cw) {
    return (cw >> 60) == COS_CW_EOD;
}

/*
 *  cosDsIsEOF - test a control word for end of file indication
 */
bool cosDsIsEOF(u64 cw) {
    return (cw >> 60) == COS_CW_EOF;
}

/*
 *  cosDsIsEOR - test a control word for end of record indication
 */
bool cosDsIsEOR(u64 cw) {
    return (cw >> 60) == COS_CW_EOR;
}

/*
 *  cosDsOpen - open a dataset
 */
Dataset *cosDsOpen(char *pathname) {
    u64 cw;
    Dataset *ds;
    u64 fwi;
    int n;

    ds = (Dataset *)malloc(sizeof(Dataset));
    if (ds == NULL) return NULL;
    memset(ds, 0, sizeof(Dataset));
    ds->fd = open(pathname, O_RDONLY, 0644);
    if (ds->fd == -1) {
        free(ds);
        return NULL;
    }
    ds->limit = read(ds->fd, ds->buffer, COS_BLOCK_SIZE);
    if (ds->limit < 8) {
        free(ds);
        return NULL;
    }
    cw = getWord(ds);
    fwi = cw & COS_BCW_FWI_MASK;
    if (cosDsIsBCW(cw)) {
        ds->cursor = 8;
        ds->bytesRead = 8;
        ds->nextCtrlWordIndex = (fwi + 1) * 8;
    }
    ds->isWritable = 0;
    return ds;
}

/*
 *  cosDsRead - read a sequence of bytes from a dataset
 */
int cosDsRead(Dataset *ds, u8 *buffer, int len) {
    int cursor;
    u64 cw;
    int cwn;
    u64 fwi;
    int n;

    if (ds == NULL || ds->isWritable) return -1;
    if (ds->isAtCW) return 0;
    n = 0;
    while (n < len) {
        if (ds->bytesRead == ds->nextCtrlWordIndex) {
            while (ds->limit - ds->cursor < 8) {
                cursor = 0;
                while (ds->cursor < ds->limit)
                    ds->buffer[cursor++] = ds->buffer[ds->cursor++];
                ds->limit = cursor;
                ds->cursor = 0;
                cwn = read(ds->fd, &ds->buffer[ds->limit], COS_BLOCK_SIZE - ds->limit);
                if (cwn < 1) return -1;
                ds->limit += cwn;
            }
            cw = getWord(ds);
            ds->cursor += 8;
            ds->bytesRead += 8;
            fwi = cw & COS_BCW_FWI_MASK;
            ds->nextCtrlWordIndex = ds->bytesRead + (fwi * 8);
            if (cosDsIsBCW(cw)) continue;
            ds->isAtCW = 1;
            ds->controlWord = cw;
            return n;
        }
        if (ds->cursor >= ds->limit) {
            ds->cursor = 0;
            ds->limit = read(ds->fd, ds->buffer, COS_BLOCK_SIZE);
            if (ds->limit == -1)
                return -1;
            else if (ds->limit == 0)
                return n;
        }
        buffer[n++] = ds->buffer[ds->cursor++];
        ds->bytesRead += 1;
    }
    return n;
}

/*
 *  cosDsReadCW - read a control word from a dataset
 */
u64 cosDsReadCW(Dataset *ds) {
    if (ds == NULL || ds->isWritable || ds->isAtCW == 0) return 0;
    ds->isAtCW = 0;
    return ds->controlWord;
}

/*
 *  cosDsRewind - rewind a dataset
 */
int cosDsRewind(Dataset *ds) {
    u64 cw;
    u64 fwi;

    if (ds == NULL || ds->isWritable) return -1;
    if (lseek(ds->fd, 0, SEEK_SET) == -1) return -1;
    ds->cursor = ds->bytesRead = ds->nextCtrlWordIndex = 0;
    ds->limit = read(ds->fd, ds->buffer, COS_BLOCK_SIZE);
    if (ds->limit < 8) return -1;
    cw = getWord(ds);
    fwi = cw & COS_BCW_FWI_MASK;
    if (cosDsIsBCW(cw)) {
        ds->cursor = 8;
        ds->bytesRead = 8;
        ds->nextCtrlWordIndex = (fwi + 1) * 8;
    }
    return 0;
}

/*
 *  cosDsWrite - write a sequence of bytes to a dataset
 */
int cosDsWrite(Dataset *ds, u8 *buffer, int len) {
    int residue;
    int written;

    if (ds == NULL || ds->isWritable == 0) return -1;
    residue = COS_BLOCK_SIZE - ds->cursor;
    written = 0;
    while (len >= residue) {
        memcpy(ds->buffer + ds->cursor, buffer, residue);
        ds->cursor += residue;
        if (flushBuffer(ds) == -1) return -1;
        written += residue;
        buffer += residue;
        len -= residue;
        residue = COS_BLOCK_SIZE - 8;
    }
    memcpy(ds->buffer + ds->cursor, buffer, len);
    ds->cursor += len;
    written += len;
    return written;
}

/*
 *  cosDsWriteEOD - write an end of data indication to a dataset
 */
int cosDsWriteEOD(Dataset *ds) {
    u64 rcw;

    if (ds == NULL) return -1;

    if (ds->cursor >= COS_BLOCK_SIZE && flushBuffer(ds) == -1) return -1;
    setFWI(ds);
    rcw = (u64)COS_CW_EOD << 60;
    return appendCW(ds, rcw);
}

/*
 *  cosDsWriteEOF - write an end of file indication to a dataset
 */
int cosDsWriteEOF(Dataset *ds) {
    u64 rcw;

    if (ds == NULL) return -1;

    if (ds->cursor >= COS_BLOCK_SIZE && flushBuffer(ds) == -1) return -1;
    setFWI(ds);
    rcw = ((u64)COS_CW_EOF << 60) | ((ds->currentBlock - ds->lastFileBlock) << 24);
    if (appendCW(ds, rcw) == -1) return -1;
    ds->lastFileBlock = ds->lastRecordBlock = ds->currentBlock;
    return 0;
}

/*
 *  cosDsWriteEOR - write an end of record indication to a dataset
 */
int cosDsWriteEOR(Dataset *ds) {
    int incr;
    u64 rcw;
    int ubc;

    if (ds == NULL) return -1;

    incr = (8 - (ds->cursor & 7)) & 7;
    ds->cursor += incr;
    ubc = incr * 8;
    if (ds->cursor >= COS_BLOCK_SIZE && flushBuffer(ds) == -1) return -1;
    setFWI(ds);
    rcw = ((u64)COS_CW_EOR << 60)
        | ((u64)ubc << 50)
        | ((ds->currentBlock - ds->lastFileBlock) << 24)
        | ((ds->currentBlock - ds->lastRecordBlock) << 9);
    if (appendCW(ds, rcw) == -1) return -1;
    ds->lastRecordBlock = ds->currentBlock;
    return 0;
}

/*
 *  cosDsWriteWord - write a word to a dataset
 */
int cosDsWriteWord(Dataset *ds, u64 word) {
    int i;
    int incr;
    int shiftCount;

    if ((ds->cursor & 7) != 0) { // advance to start of next word
        incr = (8 - (ds->cursor & 7)) & 7;
        ds->cursor += incr;
    }
    if (ds->cursor >= COS_BLOCK_SIZE && flushBuffer(ds) == -1) return -1;
    for (i = 0, shiftCount = 56; i < 8; i++, shiftCount -= 8) {
         ds->buffer[ds->cursor++] = (word >> shiftCount) & 0xff;
    }
    return 0;
}

static int flushBuffer(Dataset *ds) {
    u64 bn;
    int i;
    int n;
    int shiftCount;

    setFWI(ds);
    n = write(ds->fd, ds->buffer, ds->cursor);
    if (n < ds->cursor) return -1;
    ds->bytesWritten += n;
    memset(ds->buffer, 0, 8);
    ds->lastCtrlWordIndex = 0;
    ds->currentBlock += 1;
    bn = ds->currentBlock << 9;
    for (i = 3, shiftCount = 32; i < 8; i++, shiftCount -= 8) {
         ds->buffer[i] = (bn >> shiftCount) & 0xff;
    }
    ds->cursor = 8;
    return 0;
}

static u64 getWord(Dataset *ds) {
    int i;
    u64 word;

    for (word = 0, i = 0; i < 8; i++) {
        word = (word << 8) | ds->buffer[ds->cursor + i];
    }
    return word;
}

static void setFWI(Dataset *ds) {
    int cwi;
    int fwi;

    cwi = ds->lastCtrlWordIndex;
    fwi = ((ds->cursor - cwi) / 8) - 1;
    ds->buffer[cwi + 6] = (ds->buffer[cwi + 6] & 0xfe) | ((fwi >> 8) & 1);
    ds->buffer[cwi + 7] = fwi & 0xff;
}

#endif /* __cos */
