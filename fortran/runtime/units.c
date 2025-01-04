/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: units.c
**
**  Description:
**      This file contains functions supporting management of I/O units.
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
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "fmt.h"
#include "units.h"

#define DEBUG 1

static char unitName[8] = {'U','N','I','T','0','0','0',0};

static Unit units[MAX_ALLOCATED_UNITS];

static void closeUnit(Unit *up, int doDelete);
static void copyStrToRef(char *s1, ulong ref);
static Unit *getUnit(int unitNum, int flags, int recLen);
static int openUnit(char *fileName, int unitNum, int flags, int recLen);
static void refToCharPtr(ulong ref, char **s, int *len);
static int trim(char *s, int len);
static int writer(Unit *up, void *buf, size_t nbyte);

extern int _exists(char *path);

Unit *_allocu(int unitNum) {
    Unit *up;

    up = _findu(unitNum);
    if (up == NULL) up = _findu(0);
    if (up != NULL) {
        if ((up->flags & MASK_OPEN) != 0) closeUnit(up, (up->flags & MASK_SCRATCH) != 0);
        memset(up, 0, sizeof(Unit));
        up->number = unitNum;
    }
    else {
        fputs("I/O unit pool exhausted\n", stderr);
    }
    return up;
}

void _closeu(int unitNum, ulong statusRef) {
    int len;
    char *s;
    Unit *up;

    up = _findu(unitNum);
    if (up == NULL || (up->flags & MASK_OPEN) == 0) return;

    refToCharPtr(statusRef, &s, &len);
    if (s == NULL) {
        closeUnit(up, (up->flags & MASK_SCRATCH) != 0);
    }
    else if (len == 4 && strncasecmp(s, "KEEP", len) == 0) {
        closeUnit(up, 0);
    }
    else if (len == 6 && strncasecmp(s, "DELETE", len) == 0) {
        closeUnit(up, 1);
    }
    else {
        fputs("Invalid file STATUS: ", stderr);
        fwrite(s, 1, len, stderr);
        fputs("\n", stderr);
        exit(1);
    }
}

void _clrios(int unitNum) {
    Unit *up;

    up = _findu(unitNum);
    if (up != NULL) {
        up->ioStat = 0;
    }
}

void _endfio(void) {
    int i;
    Unit *up;

    for (i = 0; i < MAX_ALLOCATED_UNITS; i++) {
        up = &units[i];
        closeUnit(up, (up->flags & MASK_SCRATCH) != 0);
    }
}

Unit *_findu(int unitNum) {
    int i;
    Unit *up;

    if (unitNum == 0) {
        for (i = 0; i < MAX_ALLOCATED_UNITS; i++) {
            up = &units[i];
            if ((up->flags & MASK_OPEN) == 0) return up;
        }
    }
    else {
        for (i = 0; i < MAX_ALLOCATED_UNITS; i++) {
            up = &units[i];
            if (up->number == unitNum) return up;
        }
    }
    return NULL;
}

void _flufmt(int unitNum) {
    int eor;
    int len;
    ulong ref;
    char *s;
    Unit *up;

    up = getUnit(unitNum, 0, MAX_FMT_RECL);
    while (up->ioStat == 0) {
        _outfin(&eor);
        ref = _getrcd();
        refToCharPtr(ref, &s, &len);
        if (writer(up, s, len) != len || eor == 0) break;
    }
}

void _flulst(int unitNum) {
    int len;
    ulong ref;
    char *s;
    Unit *up;

    up = getUnit(unitNum, 0, MAX_FMT_RECL);
    if (up->ioStat == 0) {
        ref = _getrcd();
        refToCharPtr(ref, &s, &len);
        writer(up, s, len);
    }
}

void _flustr(void) {
    int eor;

    _outfin(&eor);
}

void _inbchr(int unitNum, unsigned long ref) {
    int len;
    int n;
    char *s;
    Unit *up;

    up = getUnit(unitNum, MASK_UNFORMATTED, MAX_FMT_RECL);
    if (up->ioStat != 0) return;
    s = (char *)(ref & 0xffffffff);
    len = ref >> 32;
    n = read(up->fd, s, len);
    if (n == len) {
        up->ioStat = 0;
    }
    else if (n == 0) {
        up->ioStat = -1;
    }
    else {
        up->ioStat = errno;
        if (errno == 0) {
            s += n;
            while (n++ < len) *s++ = ' ';
        }
    }
}

void _inbwrd(int unitNum, u64 *value) {
    int n;
    Unit *up;

    up = getUnit(unitNum, MASK_UNFORMATTED, MAX_FMT_RECL);
    if (up->ioStat != 0) return;
    n = read(up->fd, (u8 *)value, sizeof(u64));
    if (n == sizeof(u64)) {
        up->ioStat = 0;
    }
    else if (n == 0) {
        up->ioStat = -1;
    }
    else {
        up->ioStat = errno;
    }
}

void _inifio(void) {
    int rc;
    Unit *up;

    up = _allocu(100);
    up->fd = 0;
    up->flags = MASK_IMMUTABLE | MASK_OPEN;
    strcpy(up->fileName, "$IN");

    up = _allocu(101);
    up->fd = 1;
    up->flags = MASK_IMMUTABLE | MASK_OPEN;
    strcpy(up->fileName, "$OUT");

    up = _allocu(103);
    up->fd = 2;
    up->flags = MASK_OPEN;
    strcpy(up->fileName, "$ERR");

    up = _allocu(5);
    up->fd = 0;
    up->flags = MASK_OPEN;
    strcpy(up->fileName, "$IN");

    up = _allocu(6);
    up->fd = 1;
    up->flags = MASK_OPEN;
    strcpy(up->fileName, "$OUT");

    up = _allocu(7);
    up->fd = 2;
    up->flags = MASK_OPEN;
    strcpy(up->fileName, "$ERR");
}

int _iostat(int unitNum) {
    Unit *up;

    up = _findu(unitNum);
    return (up != NULL) ? up->ioStat : EBADF;
}

void _openu(int unitNum, ulong fileNameRef, ulong statusRef, ulong accessRef, ulong formattingRef, ulong blankRef, int recLen) {
    char *fileName;
    char fileNameBuf[MAX_FILE_NAME_LEN+1];
    int flags;
    bool isNameDefined;
    int len;
    int rc;
    char *s;
    Unit *up;

    flags = 0;
    refToCharPtr(fileNameRef, &fileName, &len);
    isNameDefined = fileName != NULL;
    if (isNameDefined) {
        len = trim(fileName, len);
        if (len > MAX_FILE_NAME_LEN) {
            fputs("File name too long: ", stderr);
            fwrite(fileName, 1, len, stderr);
            fputs("\n", stderr);
            exit(1);
        }
        memcpy(fileNameBuf, fileName, len);
        fileNameBuf[len] = '\0';
    }
    else {
        sprintf(fileNameBuf, "UNIT%d", unitNum);
        flags |= MASK_SCRATCH;
    }
    refToCharPtr(statusRef, &s, &len);
    len = trim(s, len);
    if (len == 3 && strncasecmp(s, "OLD", len) == 0) {
        flags |= MASK_OLD;
    }
    else if (len == 3 && strncasecmp(s, "NEW", len) == 0) {
        flags |= MASK_NEW;
    }
    else if (len == 6 && strncasecmp(s, "SCRATCH", len) == 0) {
        flags |= MASK_SCRATCH;
    }
    else if (s != NULL) {
        if (len != 7 || strncasecmp(s, "UNKNOWN", len) != 0) {
            fputs("Invalid file STATUS: ", stderr);
            fwrite(s, 1, len, stderr);
            fputs("\n", stderr);
            exit(1);
        }
    }
    refToCharPtr(accessRef, &s, &len);
    len = trim(s, len);
    if (len == 6 && strncasecmp(s, "DIRECT", len) == 0) {
        flags |= MASK_DIRECT;
    }
    else if (s != NULL && (len != 10 || strncasecmp(s, "SEQUENTIAL", len) != 0)) {
        fputs("Invalid file ACCESS: ", stderr);
        fwrite(s, 1, len, stderr);
        fputs("\n", stderr);
        exit(1);
    }
    refToCharPtr(formattingRef, &s, &len);
    len = trim(s, len);
    if (len == 11 && strncasecmp(s, "UNFORMATTED", len) == 0) {
        flags |= MASK_UNFORMATTED;
    }
    else if (s != NULL && (len != 9 || strncasecmp(s, "FORMATTED", len) != 0)) {
        fputs("Invalid file FORM: ", stderr);
        fwrite(s, 1, len, stderr);
        fputs("\n", stderr);
        exit(1);
    }
    refToCharPtr(blankRef, &s, &len);
    len = trim(s, len);
    if (len == 4 && strncasecmp(s, "ZERO", len) == 0) {
        flags |= MASK_ZERO;
    }
    else if (s != NULL && (len != 4 || strncasecmp(s, "NULL", len) != 0)) {
        fputs("Invalid file BLANK: ", stderr);
        fwrite(s, 1, len, stderr);
        fputs("\n", stderr);
        exit(1);
    }
    rc = openUnit(fileNameBuf, unitNum, flags, recLen);
    if (unitNum != 0) {
        up = _findu(unitNum);
        if (up != NULL) up->ioStat = rc;
    }
    if (rc != 0) {
        fprintf(stderr, "%s: %s\n", fileNameBuf, strerror(rc));
    }
}

int _queryu(int unitNum, ulong fileNameRef, long *existRef, long *openedRef, int *numberRef, long *namedRef,
            ulong nameRef, ulong accessRef, ulong sequentialRef, ulong directRef, ulong formattedRef, ulong unformattedRef, ulong formRef, ulong blankRef,
            int *reclRef, int *nextRecRef) {
    Unit dummyUnit;
    bool exists;
    char fileName[MAX_FILE_NAME_LEN+1];
    int i;
    int len;
    char *s;
    Unit *up;

    if (unitNum != 0) {
        up = _findu(unitNum);
        if (up == NULL) {
            up = &dummyUnit;
            memset(up, 0, sizeof(Unit));
            up->number = unitNum;
        }
    }
    else if (fileNameRef != 0) {
        refToCharPtr(fileNameRef, &s, &len);
        if (len > MAX_FILE_NAME_LEN) return EINVAL;
        memcpy(fileName, s, len);
        fileName[len] = '\0';
        up = NULL;
        for (i = 0; i < MAX_ALLOCATED_UNITS; i++) {
            if ((units[i].flags & MASK_OPEN) != 0 && strcmp(fileName, units[i].fileName) == 0) {
                up = &units[i];
                break;
            }
        }
        if (up == NULL) {
            up = &dummyUnit;
            memset(up, 0, sizeof(Unit));
            up->number = unitNum;
            memcpy(up->fileName, fileName, len);
        }
    }
    else {
        return EINVAL;
    }
    if (existRef != NULL) {
        exists = FALSE;
        if ((up->flags & MASK_OPEN) == 0 && *up->fileName != '\0') {
            up->fd = open(up->fileName, O_RDONLY);
            if (up->fd == -1) {
                if (errno != ENOENT) return errno;
            }
            else {
                exists = TRUE;
                close(up->fd);
            }
        }
        else {
            exists = TRUE;
        }
        *existRef = exists ? ~0L : 0;
    }
    if (openedRef      != NULL) *openedRef = (up->flags & MASK_OPEN) ? ~0L : 0;
    if (numberRef      != NULL) *numberRef = up->number;
    if (namedRef       != NULL) *namedRef = (up->fileName[0] != '\0') ? ~0L : 0;
    if (nameRef        != 0   ) copyStrToRef(up->fileName, nameRef);
    if (accessRef      != 0   ) copyStrToRef((up->flags & MASK_DIRECT) != 0 ? "DIRECT" : "SEQENTIAL", accessRef);
    if (sequentialRef  != 0   ) copyStrToRef((up->flags & MASK_DIRECT) != 0 ? "NO" : "YES", sequentialRef);
    if (directRef      != 0   ) copyStrToRef((up->flags & MASK_DIRECT) != 0 ? "YES" : "NO", directRef);
    if (formattedRef   != 0   ) copyStrToRef((up->flags & MASK_UNFORMATTED) != 0 ? "NO" : "YES", formattedRef);
    if (unformattedRef != 0   ) copyStrToRef((up->flags & MASK_UNFORMATTED) != 0 ? "YES" : "NO", unformattedRef);
    if (formRef        != 0   ) copyStrToRef((up->flags & MASK_UNFORMATTED) != 0 ? "UNFORMATTED" : "FORMATTED", formRef);
    if (blankRef       != 0   ) copyStrToRef((up->flags & MASK_ZERO) != 0 ? "ZERO" : "NULL", blankRef);
    if (reclRef        != NULL) *reclRef = up->recLen;
    if (nextRecRef     != NULL) *nextRecRef = up->nextRec;

    return up->ioStat;
}

void _rdufmt(int unitNum, void *value) {
    _inpfmt(value);
}

void _rdurec(int unitNum) {
    char c;
    int len;
    char *limit;
    int n;
    ulong ref;
    char *s;
    Unit *up;

    ref = _getrcd();
    refToCharPtr(ref, &s, &len);
    limit = s + len;
    up = getUnit(unitNum, 0, MAX_FMT_RECL);
    if (up->ioStat != 0) return;
    if (up->buf == NULL) {
        up->buf = (char *)malloc(MAX_BUF_SIZE);
        if (up->buf == NULL) {
            fputs("Out of memory\n", stderr);
            exit(1);
        }
        up->out = up->limit = up->buf + MAX_BUF_SIZE;
    }
    while (s < limit) {
        if (up->out >= up->limit) {
            n = read(up->fd, up->buf, MAX_BUF_SIZE);
            if (n > 0) {
                up->ioStat = 0;
                up->limit = up->buf + n;
                up->out = up->buf;
            }
            else if (n == 0) {
                up->ioStat = -1;
                return;
            }
            else {
                up->ioStat = errno;
                return;
            }
        }
        c = *up->out++;
        if (c == '\n') {
            while (s < limit) *s++ = ' ';
            break;
        }
        else {
            *s++ = c;
        }
    }
}

void _wrbchr(int unitNum, unsigned long ref) {
    int len;
    int n;
    char *s;
    Unit *up;

    up = getUnit(unitNum, MASK_UNFORMATTED, MAX_FMT_RECL);
    if (up->ioStat != 0) return;
    s = (char *)(ref & 0xffffffff);
    len = ref >> 32;
    n = write(up->fd, s, len);
    if (n == len) {
        up->ioStat = 0;
    }
    else if (n < 0) {
        up->ioStat = errno;
    }
    else {
        up->ioStat = EIO;
    }
}

void _wrbwrd(int unitNum, u64 *value) {
    int n;
    Unit *up;

    up = getUnit(unitNum, MASK_UNFORMATTED, MAX_FMT_RECL);
    if (up->ioStat != 0) return;
    n = write(up->fd, (u8 *)value, sizeof(u64));
    if (n == sizeof(u64)) {
        up->ioStat = 0;
    }
    else {
        up->ioStat = errno;
    }
}

void _wrsfmt(unsigned long strRef, DataValue *value) {
    int eor;

    _outfmt(value, &eor);
    while (eor) {
        eor = 0;
        _outfmt(value, &eor);
    }
}

void _wrufmt(int unitNum, DataValue *value) {
    int eor;
    int len;
    ulong ref;
    char *s;
    Unit *up;

    up = getUnit(unitNum, 0, MAX_FMT_RECL);
    _outfmt(value, &eor);
    while (eor && up->ioStat == 0) {
        eor = 0;
        ref = _getrcd();
        refToCharPtr(ref, &s, &len);
        if (writer(up, s, len) != len) break;
        _inircd();
        _outfmt(value, &eor);
    }
}

static void closeUnit(Unit *up, int doDelete) {
    if (up != NULL && (up->flags & MASK_IMMUTABLE) == 0) {
        up->ioStat = 0;
        if (up->fd > 2 && (up->flags & MASK_OPEN) != 0) {
            up->flags ^= MASK_OPEN;
            if (close(up->fd) == -1) {
                up->ioStat = errno;
            }
            else if (doDelete) {
                unlink(up->fileName);
            }
            if (up->buf != NULL) {
                free(up->buf);
                up->buf = NULL;
            }
        }
    }
}

static void copyStrToRef(char *s1, ulong ref) {
    int len;
    char *s2;

    refToCharPtr(ref, &s2, &len);
    while (*s1 != '\0' && len > 0) {
        *s2++ = *s1++;
        len -= 1;
    }
    while (len-- > 0) *s2++ = ' ';
}

static Unit *getUnit(int unitNum, int flags, int recLen) {
    char *cp;
    int i;
    char name[8];
    int rc;
    int u;
    Unit *up;

    up = _findu(unitNum);
    if (up == NULL || (up->flags & MASK_OPEN) == 0) {
        if (unitNum == 102) {
            rc = openUnit("$PUNCH", unitNum, 0, 0);
            if (rc != 0) {
                fputs("$PUNCH: ", stderr);
                fputs(strerror(rc), stderr);
                fputs("\n", stderr);
                exit(1);
            }
            up = _findu(102);
            up->ioStat = 0;
            up->flags |= MASK_IMMUTABLE;
        }
        else {
            cp = unitName + 6;
            u = unitNum;
            for (i = 0; i < 3; i++) {
                *cp-- = '0' + (u % 10);
                u /= 10;
            }
            rc = openUnit(unitName, unitNum, flags, recLen);
            if (rc != 0) {
                fputs(unitName, stderr);
                fputs(": ", stderr);
                fputs(strerror(rc), stderr);
                fputs("\n", stderr);
                exit(1);
            }
            up = _findu(unitNum);
            up->ioStat = 0;
        }
    }
    return up;
}

static int openUnit(char *fileName, int unitNum, int flags, int recLen) {
    int access;
    int mode;
    int rc;
    Unit *up;

    if (unitNum < 1 || unitNum > MAX_UNIT_NUMBER) {
        fprintf(stderr, "Invalid unit number: %d\n", unitNum);
        return EINVAL;
    }

    if (strlen(fileName) > MAX_FILE_NAME_LEN) {
        fprintf(stderr, "%s: file name too long\n", fileName);
        return EINVAL;
    }

    up = _allocu(unitNum);
    if (up == NULL) return EMFILE;

    strcpy(up->fileName, fileName);
    mode = 0;

    if ((flags & MASK_NEW) != 0) {
        access = O_CREAT|O_TRUNC|O_RDWR;
        mode = 0640;
    }
    else if ((flags & MASK_OLD) != 0) {
        if (_exists(fileName) == 0) return ENOENT;
        access = O_RDWR;
    }
    else {
        access = O_CREAT|O_RDWR;
    }
    if ((flags & MASK_UNFORMATTED) != 0) access |= O_BINARY;
    up->fd = open(fileName, access, mode);
    if (up->fd == -1) {
        rc = errno;
        perror(fileName);
        return rc;
    }

    up->flags = (flags & ~MASK_NEW) | MASK_OLD | MASK_OPEN;

    return 0;
}

static void refToCharPtr(ulong ref, char **s, int *len) {
    *s = (char *)(ref & 0xffffffff);
    *len = ref >> 32;
}

static int trim(char *s, int len) {
    char *cp;

    if (s != NULL) {
        cp = s + (len - 1);
        while (cp >= s && *cp == ' ') cp -= 1;

        return (cp - s) + 1;
    }
    else {
        return 0;
    }
}

static int writer(Unit *up, void *buf, size_t nbyte) {
    int n;

    n = write(up->fd, buf, nbyte);
    if (n == nbyte) {
        n = write(up->fd, "\n", 1);
        if (n == 1) {
            up->ioStat = 0;
            return nbyte;
        }
    }
    if (n < 0) {
        up->ioStat = errno;
    }
    else {
        up->ioStat = -1;
    }
    return n;
}
