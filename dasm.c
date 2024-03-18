/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: dasm.c
**
**  Description:
**      This file is the main module of the COS disassembler.
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
#include <unistd.h>
#include "cosdataset.h"
#include "cosldr.h"
#include "ldrconst.h"
#include "ldrproto.h"
#include "ldrtypes.h"
#include "services.h"

static void disassemble(Dataset *ds, u32 start, u32 limit);
static u64 getWord(u8 *bytes);
static u32 parseParcelAddr(char *s);
static void printInvInst(int parcel);
static void print_gh_ijkm(int parcel1, int parcel2);
static void print_ghi_jkm(int parcel1, int parcel2);
static void print_ghijk(int parcel);
static int readNextParcel(Dataset *ds);
static int skipBytes(Dataset *ds, int count);
static char *toParcelAddr(u32 addr, bool isParcelAddress);
static void usage(void);

#define BUFSIZE (512*8)

static u8 buffer[BUFSIZE];
static int cursor = BUFSIZE;

int main(int argc, char *argv[]) {
    u8 buf[8];
    u64 cw;
    Dataset *ds;
    u64 hdr;
    u32 limit;
    int n;
    char *path;
    u32 start;
    u8 *table;
    int tableLength;
    u8 tableType;
    u64 wc;

    if (argc < 2) usage();
    path = argv[1];
    start = 01000;
    limit = 077777777;
    if (argc > 2) {
        start = parseParcelAddr(argv[2]);
    }
    if (argc > 3) {
        limit = parseParcelAddr(argv[3]);
    }

    ds = cosDsOpen(path);
    if (ds == NULL) {
        eprintf("Failed to open %s", path);
        exit(1);
    }

    for (;;) {
        n = cosDsRead(ds, buf, 8);
        if (n == -1) {
            eprintf("Failed to read %s", path);
            exit(1);
        }
        if (n == 0) {
            cw = cosDsReadCW(ds);
            if (cosDsIsEOF(cw) || cosDsIsEOD(cw)) break;
            continue; // EOR
        }
        hdr = getWord(buf);
        tableType = hdr >> 60;
        wc = (hdr >> 36) & 0xffffff; // word count for most table types
        tableLength = (wc - 1) * 8;
        if (tableType == LDR_TT_TXT) {
            cursor = BUFSIZE;
            if (((0200 + wc) * 4) - 1 < limit) limit = ((0200 + wc) * 4) - 1;
            break;
        }
        else if (tableType == LDR_TT_DFT) {
            wc = (hdr >> 24) & 0xffffff;
            tableLength = (wc - 1) * 8;
        }
        if (skipBytes(ds, tableLength) == -1) {
            eprintf("Failed to read %s", path);
            exit(1);
        }
    }

    disassemble(ds, start, limit);

    exit(0);
}

static void disassemble(Dataset *ds, u32 start, u32 limit) {
    u32 addr;
    char buf[16];
    u8  g, h, i, j, k;
    u16 jk;
    u32 jkm;
    int m;
    char *op;
    int parcel;

    addr = 01000;
    while (addr < start) {
        parcel = readNextParcel(ds);
        if (parcel == -1) break;
        addr += 1;
    }
    
    while (addr <= limit) {
        parcel = readNextParcel(ds);
        if (parcel == -1) break;
        printf("%8s  ", toParcelAddr(addr, TRUE));
        addr += 1;
        g = parcel >> 12;
        h = (parcel >> 9) & 7;
        i = (parcel >> 6) & 7;
        j = (parcel >> 3) & 7;
        k = parcel & 7;
        switch (g) {
        case 00:
            switch(h) {
            case 0:
                print_ghijk(parcel);
                if (i == 0 && j == 0 && k == 0) {
                    fputs("ERR", stdout);
                }
                else {
                    printf("ERR       %o", parcel & 0777);
                }
                break;
            case 1:
                print_ghijk(parcel);
                if (i == 0 && j == 0 && k == 0) {
                    fputs("PASS", stdout);
                }
                else if (i == 0) {
                    printf("CA,A%o     A%o", j, k);
                }
                else if (i == 1) {
                    printf("CL,A%o     A%o", j, k);
                }
                else if (i == 2 && k == 0) {
                    printf("CI,A%o", j);
                }
                else if (i == 2 && k == 1) {
                    printf("MC,A%o", j);
                }
                else if (i == 3 && k == 0) {
                    printf("XA        A%o", j);
                }
                else if (i == 4) {
                    switch (k) {
                    case 0:
                        printf("RT        S%o", j);
                        break;
                    case 1:
                        printf("SIPI      %o", j);
                        break;
                    case 2:
                        if (j == 0) {
                            fputs("CIPI", stdout);
                        }
                        else {
                            printInvInst(parcel);
                        }
                        break;
                    case 3:
                        printf("CLN       %o", j);
                        break;
                    case 4:
                        printf("PCI       S%o", j);
                        break;
                    case 5:
                        if (j == 0) {
                            fputs("CCI", stdout);
                        }
                        else {
                            printInvInst(parcel);
                        }
                        break;
                    case 6:
                        if (j == 0) {
                            fputs("ECI", stdout);
                        }
                        else {
                            printInvInst(parcel);
                        }
                        break;
                    case 7:
                        if (j == 0) {
                            fputs("DCI", stdout);
                        }
                        else {
                            printInvInst(parcel);
                        }
                        break;
                    }
                }
                else {
                    printInvInst(parcel);
                }
                break;
            case 2:
                print_ghijk(parcel);
                switch (i) {
                case 0:
                    if (j == 0) {
                        if (k == 0) {
                            fputs("VL        1", stdout);
                        }
                        else {
                            printf("VL        A%o", k);
                        }
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                case 1:
                    if (j == 0 && k == 0) {
                        fputs("EFI", stdout);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                case 2:
                    if (j == 0 && k == 0) {
                        fputs("DFI", stdout);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                case 3:
                    if (j == 0 && k == 0) {
                        fputs("ERI", stdout);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                case 4:
                    if (j == 0 && k == 0) {
                        fputs("DRI", stdout);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                case 5:
                    if (j == 0 && k == 0) {
                        fputs("DBM", stdout);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                case 6:
                    if (j == 0 && k == 0) {
                        fputs("EBM", stdout);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                case 7:
                    if (j == 0 && k == 0) {
                        fputs("CMR", stdout);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                }
                break;
            case 3:
                print_ghijk(parcel);
                switch (i) {
                case 0:
                    if (j == 0 && k == 0) {
                        fputs("VM        0", stdout);
                    }
                    else if (k == 0) {
                        printf("VM        S%o", j);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                case 4:
                    printf("SM%o%o      1,TS", j, k);
                    break;
                case 6:
                    printf("SM%o%o      0", j, k);
                    break;
                case 7:
                    printf("SM%o%o      1", j, k);
                    break;
                default:
                    printInvInst(parcel);
                    break;
                }
                break;
            case 4:
                print_ghijk(parcel);
                if (i == 0 && j == 0 && k == 0) {
                    fputs("EX", stdout);
                }
                else {
                    printf("EX        %o", parcel & 0777);
                }
                break;
            case 5:
                print_ghijk(parcel);
                if (i == 0) {
                    printf("J         B%o%o", j, k);
                }
                else {
                    printInvInst(parcel);
                }
                break;
            case 6:
                m = readNextParcel(ds);
                if (m == -1) break;
                addr += 1;
                print_gh_ijkm(parcel, m);
                printf("J         %s", toParcelAddr(((parcel & 0777) << 16) | m, TRUE));
                break;
            case 7:
                m = readNextParcel(ds);
                if (m == -1) break;
                addr += 1;
                print_gh_ijkm(parcel, m);
                printf("R         %s", toParcelAddr(((parcel & 0777) << 16) | m, TRUE));
                break;
            }
            break;
        case 01:
            m = readNextParcel(ds);
            if (m == -1) break;
            addr += 1;
            print_gh_ijkm(parcel, m);
            if (i >= 4) {
                printf("A%o        %o", h, ((parcel & 0377) << 16) | m);
            }
            else {
                switch (h) {
                case 0:
                    op = "JAZ";
                    break;
                case 1:
                    op = "JAN";
                    break;
                case 2:
                    op = "JAP";
                    break;
                case 3:
                    op = "JAM";
                    break;
                case 4:
                    op = "JSZ";
                    break;
                case 5:
                    op = "JSN";
                    break;
                case 6:
                    op = "JSP";
                    break;
                case 7:
                    op = "JSM";
                    break;
                }
                printf("%s       %s", op, toParcelAddr(((parcel & 0777) << 16) | m, TRUE));
            }
            break;
        case 02:
            switch (h) {
            case 0:
                m = readNextParcel(ds);
                if (m == -1) break;
                addr += 1;
                print_ghi_jkm(parcel, m);
                printf("A%o        %o", i, ((parcel & 077) << 16) | m);
                break;
            case 1:
                m = readNextParcel(ds);
                if (m == -1) break;
                addr += 1;
                print_ghi_jkm(parcel, m);
                printf("A%o        #%o", i, (((parcel & 077) << 16) | m) ^ 017777777);
                break;
            case 2:
                print_ghijk(parcel);
                printf("A%o        %o", i, parcel & 077);
                break;
            case 3:
                print_ghijk(parcel);
                if (k == 0) {
                    printf("A%o        S%o", i, j);
                }
                else if (j == 0 && k == 1) {
                    printf("A%o        VL", i);
                }
                else {
                    printInvInst(parcel);
                }
                break;
            case 4:
                print_ghijk(parcel);
                printf("A%o        B%o%o", i, j, k);
                break;
            case 5:
                print_ghijk(parcel);
                printf("B%o%o       A%o", j, k, i);
                break;
            case 6:
                print_ghijk(parcel);
                if (k == 0) {
                    printf("A%o        PS%o", i, j);
                }
                else if (k == 1) {
                    printf("A%o        QS%o", i, j);
                }
                else if (k == 7) {
                    printf("A%o        SB%o", i, j);
                }
                else {
                    printInvInst(parcel);
                }
                break;
            case 7:
                print_ghijk(parcel);
                if (k == 0) {
                    printf("A%o        ZS%o", i, j);
                }
                else if (k == 7) {
                    printf("SB%o       A%o", j, i);
                }
                else {
                    printInvInst(parcel);
                }
                break;
            }
            break;
        case 03:
            print_ghijk(parcel);
            switch (h) {
            case 0:
                if (k == 0) {
                    printf("A%o        A%o+1", i, j);
                }
                else if (j == 0) {
                    printf("A%o        A%o", i, k);
                }
                else {
                    printf("A%o        A%o+A%o", i, j, k);
                }
                break;
            case 1:
                if (j == 0 && k == 0) {
                    printf("A%o        -1", i);
                }
                else if (k == 0) {
                    printf("A%o        A%o-1", i, j);
                }
                else if (j == 0) {
                    printf("A%o        -A%o", i, k);
                }
                else {
                    printf("A%o        A%o-A%o", i, j, k);
                }
                break;
            case 2:
                printf("A%o        A%o*A%o", i, j, k);
                break;
            case 3:
                if (j == 0 && k == 0) {
                    printf("A%o        CI", i);
                }
                else if (k == 0) {
                    printf("A%o        CA,A%o", i, j);
                }
                else if (k == 1) {
                    printf("A%o        CE,A%o", i, j);
                }
                else {
                    printInvInst(parcel);
                }
                break;
            case 4:
                printf("B%o%o,A%o    ,A0", j, k, i);
                break;
            case 5:
                printf(",A0       B%o%o,A%o", j, k, i);
                break;
            case 6:
                printf("T%o%o,A%o    ,A0", j, k, i);
                break;
            case 7:
                printf(",A0       T%o%o,A%o", j, k, i);
                break;
            }
            break;
        case 04:
            switch (h) {
            case 0:
                m = readNextParcel(ds);
                if (m == -1) break;
                addr += 1;
                print_ghi_jkm(parcel, m);
                printf("S%o        %o", i, ((parcel & 077) << 16) | m);
                break;
            case 1:
                m = readNextParcel(ds);
                if (m == -1) break;
                addr += 1;
                print_ghi_jkm(parcel, m);
                printf("S%o        #%o", i, ((parcel & 077) << 16) | m);
                break;
            case 2:
                print_ghijk(parcel);
                if (j == 0 && k == 0) {
                    printf("S%o        -1", i);
                }
                else if (j == 7 && k == 7) {
                    printf("S%o        1", i);
                }
                else {
                    printf("S%o        <D'%d", i, 64 - (parcel & 077));
                }
                break;
            case 3:
                print_ghijk(parcel);
                if (j == 0 && k == 0) {
                    printf("S%o        0", i);
                }
                else {
                    jk = parcel & 077;
                    if (jk == 0) jk = 64;
                    printf("S%o        >D'%d", i, jk);
                }
                break;
            case 4:
                print_ghijk(parcel);
                if (k == 0) {
                    printf("S%o        SB&S%o", i, j);
                }
                else {
                    printf("S%o        S%o&S%o", i, j, k);
                }
                break;
            case 5:
                print_ghijk(parcel);
                if (k == 0) {
                    printf("S%o        #SB&S%o", i, j);
                }
                else {
                    printf("S%o        #S%o&S%o", i, k, j);
                }
                break;
            case 6:
                print_ghijk(parcel);
                if (k == 0) {
                    printf("S%o        SB\\S%o", i, j);
                }
                else {
                    printf("S%o        S%o\\S%o", i, j, k);
                }
                break;
            case 7:
                print_ghijk(parcel);
                if (j == 0 && k == 0) {
                    printf("S%o        #SB", i);
                }
                else if (j == 0) {
                    printf("S%o        #S%o", i, k);
                }
                else if (k == 0) {
                    printf("S%o        #SB\\S%o", i, j);
                }
                else {
                    printf("S%o        #S%o\\S%o", i, j, k);
                }
                break;
            }
            break;
        case 05:
            print_ghijk(parcel);
            switch (h) {
            case 0:
                if (k == 0) {
                    printf("S%o        S%o!S%o&SB", i, j, i);
                }
                else {
                    printf("S%o        S%o!S%o&S%o", i, j, i, k);
                }
                break;
            case 1:
                if (j == 0 && k == 0) {
                    printf("S%o        SB", i);
                }
                else if (j == 0) {
                    printf("S%o        S%o", i, k);
                }
                else if (k == 0) {
                    printf("S%o        S%o!SB", i, j);
                }
                else {
                    printf("S%o        S%o!S%o", i, j, k);
                }
                break;
            case 2:
                printf("S0        S%o<D'%d", i, parcel & 077);
                break;
            case 3:
                printf("S0        S%o>D'%d", i, parcel & 077);
                break;
            case 4:
                printf("S%o        S%o<D'%d", i, i, parcel & 077);
                break;
            case 5:
                printf("S%o        S%o>D'%d", i, i, parcel & 077);
                break;
            case 6:
                if (j == 0) {
                    printf("S%o        S%o<A%o", i, i, k);
                }
                else if (k == 0) {
                    printf("S%o        S%o,S%o<1", i, i, j);
                }
                else {
                    printf("S%o        S%o,S%o<A%o", i, i, j, k);
                }
                break;
            case 7:
                if (j == 0) {
                    printf("S%o        S%o>A%o", i, i, k);
                }
                else if (k == 0) {
                    printf("S%o        S%o,S%o>1", i, j, i);
                }
                else {
                    printf("S%o        S%o,S%o>A%o", i, j, i, k);
                }
                break;
            }
            break;
        case 06:
            print_ghijk(parcel);
            switch (h) {
            case 0:
                printf("S%o        S%o+S%o", i, j, k);
                break;
            case 1:
                if (j == 0) {
                    printf("S%o        -S%o", i, k);
                }
                else {
                    printf("S%o        S%o-S%o", i, j, k);
                }
                break;
            case 2:
                if (j == 0) {
                    printf("S%o        +FS%o", i, k);
                }
                else {
                    printf("S%o        S%o+FS%o", i, j, k);
                }
                break;
            case 3:
                if (j == 0) {
                    printf("S%o        -FS%o", i, k);
                }
                else {
                    printf("S%o        S%o-FS%o", i, j, k);
                }
                break;
            case 4:
                printf("S%o        S%o*FS%o", i, j, k);
                break;
            case 5:
                printf("S%o        S%o*HS%o", i, j, k);
                break;
            case 6:
                printf("S%o        S%o*RS%o", i, j, k);
                break;
            case 7:
                printf("S%o        S%o*IS%o", i, j, k);
                break;
            }
            break;
        case 07:
            print_ghijk(parcel);
            switch (h) {
            case 0:
                if (k == 0) {
                    printf("S%o        /HS%o", i, j);
                }
                else {
                    printInvInst(parcel);
                }
                break;
            case 1:
                switch (j) {
                case 0:
                    printf("S%o        A%o", i, k);
                    break;
                case 1:
                    printf("S%o        +A%o", i, k);
                    break;
                case 2:
                    printf("S%o        +FA%o", i, k);
                    break;
                case 3:
                    if (k != 0) {
                        printf("S%o        0.6", i);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                case 4:
                    if (k != 0) {
                        printf("S%o        0.4", i);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                case 5:
                    if (k != 0) {
                        printf("S%o        1.0", i);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                case 6:
                    if (k != 0) {
                        printf("S%o        2.0", i);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                case 7:
                    if (k != 0) {
                        printf("S%o        4.0", i);
                    }
                    else {
                        printInvInst(parcel);
                    }
                    break;
                }
                break;
            case 2:
                if (j == 0 && k == 0) {
                    printf("S%o        RT", i);
                }
                else if (j == 0 && k == 2) {
                    printf("S%o        SM", i);
                }
                else if (k == 3) {
                    printf("S%o        ST%o", i, j);
                }
                else {
                    printInvInst(parcel);
                }
                break;
            case 3:
                if (j == 0 && k == 0) {
                    printf("S%o        VM", i);
                }
                else if (j == 0 && k == 2) {
                    printf("SM        S%o", i);
                }
                else if (k == 1) {
                    printf("S%o        SR%o", i, j);
                }
                else if (k == 3) {
                    printf("ST%o       S%o", j, i);
                }
                else {
                    printInvInst(parcel);
                }
                break;
            case 4:
                printf("S%o        T%o%o", i, j, k);
                break;
            case 5:
                printf("T%o%o       S%o", j, k, i);
                break;
            case 6:
                printf("S%o        V%o,A%o", i, j, k);
                break;
            case 7:
                if (j == 0) {
                    printf("V%o,A%o     0", i, k);
                }
                else {
                    printf("V%o,A%o     S%o", i, k, j);
                }
                break;
            }
            break;
        case 010:
            m = readNextParcel(ds);
            if (m == -1) break;
            addr += 1;
            print_ghi_jkm(parcel, m);
            jkm = ((parcel & 077) << 16) | m;
            if (h == 0) {
                printf("A%o        %o,", i, jkm);
            }
            else if (jkm == 0) {
                printf("A%o        ,A%o", i, h);
            }
            else {
                printf("A%o        %o,A%o", i, jkm, h);
            }
            break;
        case 011:
            m = readNextParcel(ds);
            if (m == -1) break;
            addr += 1;
            print_ghi_jkm(parcel, m);
            jkm = ((parcel & 077) << 16) | m;
            if (h == 0) {
                sprintf(buf, "%o,", jkm);
                printf("%-9s A%o", buf, i);
            }
            else if (jkm == 0) {
                printf(",A%o       A%o", h, i);
            }
            else {
                sprintf(buf, "%o,A%o", jkm, h);
                printf("%-9s A%o", buf, i);
            }
            break;
        case 012:
            m = readNextParcel(ds);
            if (m == -1) break;
            addr += 1;
            print_ghi_jkm(parcel, m);
            jkm = ((parcel & 077) << 16) | m;
            if (h == 0) {
                printf("S%o        %o,", i, jkm);
            }
            else if (jkm == 0) {
                printf("S%o        ,A%o", i, h);
            }
            else {
                printf("S%o        %o,A%o", i, jkm, h);
            }
            break;
        case 013:
            m = readNextParcel(ds);
            if (m == -1) break;
            addr += 1;
            print_ghi_jkm(parcel, m);
            jkm = ((parcel & 077) << 16) | m;
            if (h == 0) {
                sprintf(buf, "%o,", jkm);
                printf("%-9s S%o", buf, i);
            }
            else if (jkm == 0) {
                printf(",A%o       S%o", h, i);
            }
            else {
                sprintf(buf, "%o,A%o", jkm, h);
                printf("%-9s S%o", buf, i);
            }
            break;
        case 014:
            print_ghijk(parcel);
            switch (h) {
            case 0:
                printf("V%o        S%o&V%o", i, j, k);
                break;
            case 1:
                printf("V%o        V%o&V%o", i, j, k);
                break;
            case 2:
                if (j == 0) {
                    printf("V%o        V%o", i, k);
                }
                else {
                    printf("V%o        S%o!V%o", i, j, k);
                }
                break;
            case 3:
                printf("V%o        V%o!V%o", i, j, k);
                break;
            case 4:
                printf("V%o        S%o\\V%o", i, j, k);
                break;
            case 5:
                if (i == j && i == k) {
                    printf("V%o        0", i);
                }
                else {
                    printf("V%o        V%o\\V%o", i, j, k);
                }
                break;
            case 6:
                if (j == 0) {
                    printf("V%o        #VM&V%o", i, k);
                }
                else {
                    printf("V%o        S%o!V%o&VM", i, j, k);
                }
                break;
            case 7:
                printf("V%o        V%o!V%o&VM", i, j, k);
                break;
            }
            break;
        case 015:
            print_ghijk(parcel);
            switch (h) {
            case 0:
                if (k == 0) {
                    printf("V%o        V%o<1", i, j);
                }
                else {
                    printf("V%o        V%o<A%o", i, j, k);
                }
                break;
            case 1:
                if (k == 0) {
                    printf("V%o        V%o>1", i, j);
                }
                else {
                    printf("V%o        V%o>A%o", i, j, k);
                }
                break;
            case 2:
                if (k == 0) {
                    printf("V%o        V%o,V%o<1", i, j, j);
                }
                else {
                    printf("V%o        V%o,V%o<A%o", i, j, j, k);
                }
                break;
            case 3:
                if (k == 0) {
                    printf("V%o        V%o,V%o>1", i, j, j);
                }
                else {
                    printf("V%o        V%o,V%o>A%o", i, j, j, k);
                }
                break;
            case 4:
                printf("V%o        S%o+V%o", i, j, k);
                break;
            case 5:
                printf("V%o        V%o+V%o", i, j, k);
                break;
            case 6:
                if (j == 0) {
                    printf("V%o        -V%o", i, k);
                }
                else {
                    printf("V%o        S%o-V%o", i, j, k);
                }
                break;
            case 7:
                printf("V%o        V%o-V%o", i, j, k);
                break;
            }
            break;
        case 016:
            print_ghijk(parcel);
            switch (h) {
            case 0:
                printf("V%o        S%o*FV%o", i, j, k);
                break;
            case 1:
                printf("V%o        V%o*FV%o", i, j, k);
                break;
            case 2:
                printf("V%o        S%o*HV%o", i, j, k);
                break;
            case 3:
                printf("V%o        V%o*HV%o", i, j, k);
                break;
            case 4:
                printf("V%o        S%o*RV%o", i, j, k);
                break;
            case 5:
                printf("V%o        V%o*RV%o", i, j, k);
                break;
            case 6:
                printf("V%o        S%o*IV%o", i, j, k);
                break;
            case 7:
                printf("V%o        V%o*IV%o", i, j, k);
                break;
            }
            break;
        case 017:
            print_ghijk(parcel);
            switch (h) {
            case 0:
                if (j == 0) {
                    printf("V%o        +FV%o", i, k);
                }
                else {
                    printf("V%o        S%o+FV%o", i, j, k);
                }
                break;
            case 1:
                printf("V%o        V%o+FV%o", i, j, k);
                break;
            case 2:
                if (j == 0) {
                    printf("V%o        -FV%o", i, k);
                }
                else {
                    printf("V%o        S%o-FV%o", i, j, k);
                }
                break;
            case 3:
                printf("V%o        V%o-FV%o", i, j, k);
                break;
            case 4:
                if (k == 0) {
                    printf("V%o        /HV%o", i, j);
                }
                else if (k == 1) {
                    printf("V%o        PV%o", i, j);
                }
                else if (k == 2) {
                    printf("V%o        QV%o", i, j);
                }
                else {
                    printInvInst(parcel);
                }
                break;
            case 5:
                if (i == 0 && k == 0) {
                    printf("VM        V%o,Z", j);
                }
                else if (i == 0 && k == 1) {
                    printf("VM        V%o,N", j);
                }
                else if (i == 0 && k == 2) {
                    printf("VM        V%o,P", j);
                }
                else if (i == 0 && k == 3) {
                    printf("VM        V%o,M", j);
                }
                else if (k == 4) {
                    printf("V%o,VM     V%o,Z", i, j);
                }
                else if (k == 5) {
                    printf("V%o,VM     V%o,N", i, j);
                }
                else if (k == 6) {
                    printf("V%o,VM     V%o,P", i, j);
                }
                else if (k == 7) {
                    printf("V%o,VM     V%o,M", i, j);
                }
                else {
                    printInvInst(parcel);
                }
                break;
            case 6:
                if (j == 0 && k == 0) {
                    printf("V%o        ,A0,1", i);
                }
                else if (j == 0) {
                    printf("V%o        ,A0,A%o", i, k);
                }
                else if (j == 1) {
                    printf("V%o        ,A0,V%o", i, k);
                }
                else {
                    printInvInst(parcel);
                }
                break;
            case 7:
                if (i == 0 && k == 0) {
                    printf(",A0,1     V%o", j);
                }
                else if (i == 0) {
                    printf(",A0,A%o    V%o", k, j);
                }
                else if (i == 1) {
                    printf(",A0,V%o    V%o", k, j);
                }
                else {
                    printInvInst(parcel);
                }
                break;
            }
            break;
        }
        puts("");
    }
}

static u64 getWord(u8 *bytes) {
    int i;
    u64 word;

    word = 0;
    for (i = 0; i < 8; i++)
        word = (word << 8) | *bytes++;
    return word;
}

static u32 parseParcelAddr(char *s) {
    u32 addr;

    addr = 0;
    while (*s != '\0') {
        if (*s >= '0' && *s <= '7') {
            addr = (addr << 3) | (*s++ - '0');
        }
        else if (*(s + 1) == '\0') {
            if (*s >= 'a' && *s <= 'd') {
                addr = (addr * 4) + (*s++ - 'a');
            }
            else if (*s >= 'A' && *s <= 'D') {
                addr = (addr * 4) + (*s++ - 'A');
            }
            else {
                usage();
            }
        }
        else {
            usage();
        }
    }
    return addr;
}

static void printInvInst(int parcel) {
    fputs("----------", stdout);
}

static void print_gh_ijkm(int parcel1, int parcel2) {
    int ijkm;

    ijkm = ((parcel1 & 0777) << 16) | parcel2;
    printf("%03o  %08o%c  ", parcel1 >> 9, ijkm >> 2, 'a' + (ijkm & 3));
}

static void print_ghi_jkm(int parcel1, int parcel2) {
    printf("%04o %08o   ", parcel1 >> 6, ((parcel1 & 077) << 16) | parcel2);
}

static void print_ghijk(int parcel) {
    printf("%06o          ", parcel);
}

static int readNextParcel(Dataset *ds) {
    int n;
    int parcel;

    if (cursor + 1 >= BUFSIZE) {
        n = cosDsRead(ds, buffer, BUFSIZE);
        if (n == -1) {
            eputs("Failed to read text block");
            exit(1);
        }
        else if (n == 0) {
            return -1;
        }
        cursor = 0;
    }
    parcel = (buffer[cursor] << 8) | buffer[cursor + 1];
    cursor += 2;

    return parcel;
}

static int skipBytes(Dataset *ds, int count) {
    u8 buf[512*8];
    int n;

    while (count > 0) {
        n = (count > sizeof(buf)) ? sizeof(buf) : count;
        n = cosDsRead(ds, buf, n);
        if (n < 1) return -1;
        count -= n;
    }
    return 0;
}

static char *toParcelAddr(u32 address, bool isParcelAddress) {
    static char buf[16];
    int parcelNumber;

    if (isParcelAddress) {
        parcelNumber = address & 0x03;
        address >>= 2;
    }
    else {
        parcelNumber = 0;
    }
    sprintf(buf, "%o%c", address, 'a' + parcelNumber);
    return buf;
}

static void usage(void) {
    eputs("Usage: dasm path [start] [limit]");
    eputs("  path  - COS executable file");
    eputs("  start - parcel address at which to start disassembly (default: 0200a)");
    eputs("  limit - parcel address at which to end disassembly (default: end of executable)");
    exit(1);
}
