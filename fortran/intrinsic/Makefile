#--------------------------------------------------------------------------
#
#  Copyright 2024 Kevin E. Jordan
#
#  Name: Makefile
#
#  Description:
#      This is a makefile for building the intrinsic functions of the
#      FORTRAN compiler.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#--------------------------------------------------------------------------

CFLAGS  = -O3
#CFLAGS  = -O0 -g
AR      ?= ar
ARFLAGS ?= -r
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
ACKROOT = $(PREFIX)/share/ack

COBJS =	aint.o amax0.o amin0.o anint.o chari.o ichar.o imax.o imax1.o imin.o imin1.o index.o len.o lge.o lgt.o lle.o llt.o nint.o rmax.o rmin.o \
        iabs.o iacos.o iasin.o iatan.o iatan2.o icos.o icosh.o idim.o iexp.o iint.o ilog.o ilog10.o imod.o ireal.o isign.o isin.o isinh.o isqrt.o itan.o itanh.o \
	pgmarg.o rabs.o racos.o rasin.o ratan.o ratan2.o rcos.o rcosh.o rdim.o rexp.o rint.o rlog.o rlog10.o rmod.o rreal.o rsign.o rsin.o rsinh.o rsqrt.o rtan.o rtanh.o \
	rtc.o

CALOBJS = cosclk.o cputime.o date.o jdate.o irtc.o loc.o mask.o shift.o shiftl.o shiftr.o \
	  vlen.o vload.o vsaddi.o vsaddr.o vsdivr.o vsmulr.o vssubi.o vssubr.o vstore.o vvaddi.o vvaddr.o vvdivr.o vvmulr.o vvsubi.o vvsubr.o

all: libintf.lib

cos:
	CC=ack AR=lib ARFLAGS="-o" $(MAKE)

libintf.lib: $(COBJS) $(CALOBJS)
	$(AR) $(ARFLAGS) $@ $+

install: cos
	mkdir -p $(ACKROOT)/cos/fortran ; \
	install -b -o root -m 755 libintf.lib $(ACKROOT)/cos/fortran

clean:
	rm -f *.o *.a *.lib *.lst

aint.o: aint.c
	$(CC) $(CFLAGS) -c $<

amax0.o: amax0.c
	$(CC) $(CFLAGS) -c $<

amin0.o: amin0.c
	$(CC) $(CFLAGS) -c $<

anint.o: anint.c
	$(CC) $(CFLAGS) -c $<

chari.o: chari.c
	$(CC) $(CFLAGS) -c $<

iabs.o: iabs.c
	$(CC) $(CFLAGS) -c $<

iacos.o: iacos.c
	$(CC) $(CFLAGS) -c $<

iasin.o: iasin.c
	$(CC) $(CFLAGS) -c $<

iatan.o: iatan.c
	$(CC) $(CFLAGS) -c $<

iatan2.o: iatan2.c
	$(CC) $(CFLAGS) -c $<

ichar.o: ichar.c
	$(CC) $(CFLAGS) -c $<

icos.o: icos.c
	$(CC) $(CFLAGS) -c $<

icosh.o: icosh.c
	$(CC) $(CFLAGS) -c $<

idim.o: idim.c
	$(CC) $(CFLAGS) -c $<

iexp.o: iexp.c
	$(CC) $(CFLAGS) -c $<

iint.o: iint.c
	$(CC) $(CFLAGS) -c $<

ilog.o: ilog.c
	$(CC) $(CFLAGS) -c $<

ilog10.o: ilog10.c
	$(CC) $(CFLAGS) -c $<

imax.o: imax.c
	$(CC) $(CFLAGS) -c $<

imax1.o: imax1.c
	$(CC) $(CFLAGS) -c $<

imin.o: imin.c
	$(CC) $(CFLAGS) -c $<

imin1.o: imin1.c
	$(CC) $(CFLAGS) -c $<

imod.o: imod.c
	$(CC) $(CFLAGS) -c $<

index.o: index.c
	$(CC) $(CFLAGS) -c $<

ireal.o: ireal.c
	$(CC) $(CFLAGS) -c $<

isign.o: isign.c
	$(CC) $(CFLAGS) -c $<

isin.o: isin.c
	$(CC) $(CFLAGS) -c $<

isinh.o: isinh.c
	$(CC) $(CFLAGS) -c $<

isqrt.o: isqrt.c
	$(CC) $(CFLAGS) -c $<

itan.o: itan.c
	$(CC) $(CFLAGS) -c $<

itanh.o: itanh.c
	$(CC) $(CFLAGS) -c $<

len.o: len.c
	$(CC) $(CFLAGS) -c $<

lge.o: lge.c
	$(CC) $(CFLAGS) -c $<

lgt.o: lgt.c
	$(CC) $(CFLAGS) -c $<

lle.o: lle.c
	$(CC) $(CFLAGS) -c $<

llt.o: llt.c
	$(CC) $(CFLAGS) -c $<

cosclk.o: cosclk.cal
	$(BINDIR)/cal -o cosclk.o $<

cputime.o: cputime.cal
	$(BINDIR)/cal -o cputime.o $<

date.o: date.cal
	$(BINDIR)/cal -o date.o $<

irtc.o: irtc.cal
	$(BINDIR)/cal -o irtc.o $<

jdate.o: jdate.cal
	$(BINDIR)/cal -o jdate.o $<

loc.o: loc.cal
	$(BINDIR)/cal -o loc.o $<

mask.o: mask.cal
	$(BINDIR)/cal -o mask.o $<

nint.o: nint.c
	$(CC) $(CFLAGS) -c $<

pgmarg.o: pgmarg.c
	$(CC) $(CFLAGS) -c $<

rabs.o: rabs.c
	$(CC) $(CFLAGS) -c $<

racos.o: racos.c
	$(CC) $(CFLAGS) -c $<

rasin.o: rasin.c
	$(CC) $(CFLAGS) -c $<

ratan.o: ratan.c
	$(CC) $(CFLAGS) -c $<

ratan2.o: ratan2.c
	$(CC) $(CFLAGS) -c $<

rcos.o: rcos.c
	$(CC) $(CFLAGS) -c $<

rcosh.o: rcosh.c
	$(CC) $(CFLAGS) -c $<

rdim.o: rdim.c
	$(CC) $(CFLAGS) -c $<

rexp.o: rexp.c
	$(CC) $(CFLAGS) -c $<

rint.o: rint.c
	$(CC) $(CFLAGS) -c $<

rlog.o: rlog.c
	$(CC) $(CFLAGS) -c $<

rlog10.o: rlog10.c
	$(CC) $(CFLAGS) -c $<

rmax.o: rmax.c
	$(CC) $(CFLAGS) -c $<

rmin.o: rmin.c
	$(CC) $(CFLAGS) -c $<

rmod.o: rmod.c
	$(CC) $(CFLAGS) -c $<

rreal.o: rreal.c
	$(CC) $(CFLAGS) -c $<

rsign.o: rsign.c
	$(CC) $(CFLAGS) -c $<

rsin.o: rsin.c
	$(CC) $(CFLAGS) -c $<

rsinh.o: rsinh.c
	$(CC) $(CFLAGS) -c $<

rsqrt.o: rsqrt.c
	$(CC) $(CFLAGS) -c $<

rtan.o: rtan.c
	$(CC) $(CFLAGS) -c $<

rtanh.o: rtanh.c
	$(CC) $(CFLAGS) -c $<

rtc.o: rtc.c
	$(CC) $(CFLAGS) -c $<

shift.o: shift.cal
	$(BINDIR)/cal -o shift.o $<

shiftl.o: shiftl.cal
	$(BINDIR)/cal -o shiftl.o $<

shiftr.o: shiftr.cal
	$(BINDIR)/cal -o shiftr.o $<

vlen.o: vlen.cal
	$(BINDIR)/cal -o vlen.o $<

vload.o: vload.cal
	$(BINDIR)/cal -o vload.o $<

vsaddi.o: vsaddi.cal
	$(BINDIR)/cal -o vsaddi.o $<

vsaddr.o: vsaddr.cal
	$(BINDIR)/cal -o vsaddr.o $<

vsdivr.o: vsdivr.cal
	$(BINDIR)/cal -o vsdivr.o $<

vsmulr.o: vsmulr.cal
	$(BINDIR)/cal -o vsmulr.o $<

vssubi.o: vssubi.cal
	$(BINDIR)/cal -o vssubi.o $<

vssubr.o: vssubr.cal
	$(BINDIR)/cal -o vssubr.o $<

vstore.o: vstore.cal
	$(BINDIR)/cal -o vstore.o $<

vvaddi.o: vvaddi.cal
	$(BINDIR)/cal -o vvaddi.o $<

vvaddr.o: vvaddr.cal
	$(BINDIR)/cal -o vvaddr.o $<

vvdivr.o: vvdivr.cal
	$(BINDIR)/cal -o vvdivr.o $<

vvmulr.o: vvmulr.cal
	$(BINDIR)/cal -o vvmulr.o $<

vvsubi.o: vvsubi.cal
	$(BINDIR)/cal -o vvsubi.o $<

vvsubr.o: vvsubr.cal
	$(BINDIR)/cal -o vvsubr.o $<

#---------------------------  End Of File  --------------------------------
