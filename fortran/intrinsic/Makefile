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
BINDIR = /usr/local/bin

OBJS =	aint.o amax0.o amin0.o anint.o chari.o ichar.o imax.o imax1.o imin.o imin1.o index.o len.o lge.o lgt.o lle.o llt.o nint.o rmax.o rmin.o \
        iabs.o iacos.o iasin.o iatan.o iatan2.o icos.o icosh.o idim.o iexp.o iint.o ilog.o ilog10.o imod.o ireal.o isign.o isin.o isinh.o isqrt.o itan.o itanh.o \
	rabs.o racos.o rasin.o ratan.o ratan2.o rcos.o rcosh.o rdim.o rexp.o rint.o rlog.o rlog10.o rmod.o rreal.o rsign.o rsin.o rsinh.o rsqrt.o rtan.o rtanh.o \
	cloc.o wloc.o

all: libintf.lib

cos:
	CC=ack AR=lib ARFLAGS="-o" $(MAKE)

libintf.lib: $(OBJS)
	$(AR) $(ARFLAGS) $@ $+

clean:
	rm -f *.o *.a *.lib

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

cloc.o: cloc.c
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

nint.o: nint.c
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

wloc.o: wloc.c
	$(CC) $(CFLAGS) -c $<

#---------------------------  End Of File  --------------------------------
