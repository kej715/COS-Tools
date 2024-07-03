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

static int fdForUnit(int unitNum, int flags, int recLen);

Unit *_allocu(int unitNum) {
    Unit *up;

    up = _findu(0);
    if (up != NULL) {
        memset(up, 0, sizeof(Unit));
        up->number = unitNum;
    }
    return up;
}

int _closeu(int unitNum, int doDelete) {
    Unit *up;

    up = _findu(unitNum);
    if (up != NULL && (up->flags & MASK_IMMUTABLE) == 0) {
        if (up->fd > 2) {
            close(up->fd);
            if (doDelete) {
                unlink(up->fileName);
            }
        }
        _freeu(unitNum);
    }
    return 0;
}

void _endfio(void) {
    int i;
    Unit *up;

    for (i = 0; i < MAX_ALLOCATED_UNITS; i++) {
        up = &units[i];
        if (up->number != 0 && (up->flags & MASK_IMMUTABLE) == 0)
            _closeu(up->number, (up->flags & MASK_SCRATCH) != 0);
    }
}

Unit *_findu(int unitNum) {
    int i;

    for (i = 0; i < MAX_ALLOCATED_UNITS; i++) {
        if (units[i].number == unitNum) return &units[i];
    }
    return NULL;
}

void _flufmt(int unitNum) {
    int eor;
    int fd;
    int len;
    char *s;
    Unit *up;

    fd = fdForUnit(unitNum, MASK_NEW, MAX_FMT_RECL);
    for (;;) {
        _outfin(&eor);
        s = _getrcd();
        len = strlen(s);
        if (write(fd, s, len) != len || write(fd, "\n", 1) != 1) {
            up = _findu(unitNum);
            perror(up->fileName);
            exit(1);
        }
        if (eor == 0) break;
    }
}

void _flulst(int unitNum) {
    int fd;
    int len;
    char *s;
    Unit *up;

    fd = fdForUnit(unitNum, MASK_NEW, MAX_FMT_RECL);
    s = _getrcd();
    len = strlen(s);
    if (write(fd, s, len) != len || write(fd, "\n", 1) != 1) {
        up = _findu(unitNum);
        perror(up->fileName);
        exit(1);
    }
}

void _freeu(int unitNum) {
    Unit *up;

    up = _findu(unitNum);
    if (up != NULL) up->number = 0;
}

void _inifio(void) {
    int rc;
    Unit *up;

    up = _allocu(100);
    up->fd = 0;
    up->flags = MASK_IMMUTABLE;
    strcpy(up->fileName, "$IN");

    up = _allocu(101);
    up->fd = 1;
    up->flags = MASK_IMMUTABLE;
    strcpy(up->fileName, "$OUT");

    up = _allocu(5);
    up->fd = 0;
    strcpy(up->fileName, "$IN");

    up = _allocu(6);
    up->fd = 1;
    strcpy(up->fileName, "$OUT");
}

int _openu(char *fileName, int unitNum, int flags, int recLen) {
    Unit *up;

    if (unitNum < 1 || unitNum > MAX_UNIT_NUMBER) {
        fputs("Invalid unit number\n", stderr);
        exit(1);
    }
    up = _allocu(unitNum);
    if (up == NULL) return EMFILE;

    if (strlen(fileName) > MAX_FILE_NAME_LEN) return EINVAL;

    strcpy(up->fileName, fileName);

    if ((flags & MASK_NEW) != 0) {
        up->fd = open(fileName, O_CREAT|O_TRUNC|O_RDWR, 0640);
    }
    else {
        up->fd = open(fileName, O_RDWR);
    }
    if (up->fd == -1) return errno;

    up->flags = flags;

    return 0;
}

void _wrufmt(int unitNum, DataValue *value) {
    int eor;
    int fd;
    int len;
    char *s;
    Unit *up;

    _outfmt(value, &eor);
    if (eor) {
        fd = fdForUnit(unitNum, MASK_NEW, MAX_FMT_RECL);
        while (eor) {
            eor = 0;
            s = _getrcd();
            len = strlen(s);
            if (write(fd, s, len) != len || write(fd, "\n", 1) != 1) {
                up = _findu(unitNum);
                perror(up->fileName);
                exit(1);
            }
            _inircd();
            _outfmt(value, &eor);
        }
    }
}

static int fdForUnit(int unitNum, int flags, int recLen) {
    char *cp;
    int i;
    char name[8];
    int rc;
    int u;
    Unit *up;

    up = _findu(unitNum);
    if (up == NULL) {
        if (unitNum == 102) {
            rc = _openu("$PUNCH", unitNum, MASK_NEW, 0);
            if (rc != 0) {
                fputs("$PUNCH: ", stderr);
                fputs(strerror(rc), stderr);
                fputs("\n", stderr);
                exit(1);
            }
            up = _findu(102);
            up->flags |= MASK_IMMUTABLE;
        }
        else {
            cp = unitName + 6;
            u = unitNum;
            for (i = 0; i < 3; i++) {
                *cp-- = '0' + (u % 10);
                u /= 10;
            }
            rc = _openu(unitName, unitNum, flags, recLen);
            if (rc != 0) {
                fputs(unitName, stderr);
                fputs(": ", stderr);
                fputs(strerror(rc), stderr);
                fputs("\n", stderr);
                exit(1);
            }
            up = _findu(unitNum);
        }
    }
    return up->fd;
}
