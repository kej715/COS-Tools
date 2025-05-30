#--------------------------------------------------------------------------
#
#  Copyright 2024 Kevin E. Jordan
#
#  Name: Makefile
#
#  Description:
#      This is a makefile for building the runtime library of the
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

CFLAGS  = -O3 $(INCL) $(EXTRACFLAGS)
#CFLAGS  = -O0 -g $(INCL) $(EXTRACFLAGS)
AR      ?= ar
ARFLAGS ?= -r
PREFIX = /usr/local
BINDIR = ../..
COSDIR = ../../cos-interface
ACKROOT = $(PREFIX)/share/ack
COSOBJS = $(ACKROOT)/cos/boot.o $(ACKROOT)/cos/c-ansi.o

HDRS = const.h \
       fmt.h   \
       units.h

IO_OBJS = catstr.o cmpstr.o cpystr.o fmt.o fmt2.o getstr.o units.o

all: libio.lib librt.lib

cos:
	CC=ack AR=lib ARFLAGS="-o" $(MAKE)

libio.a: $(IO_OBJS)
	$(AR) $(ARFLAGS) $@ $+

libio.lib: $(IO_OBJS) fio.o
	$(AR) $(ARFLAGS) $@ $+

librt.lib: calb01.o daryof.o darysz.o
	$(AR) $(ARFLAGS) $@ $+ $(COSOBJS)

install: cos
	mkdir -p $(ACKROOT)/cos/fortran ; \
	install -b -o root -m 755 libio.lib $(ACKROOT)/cos/fortran ; \
	install -b -o root -m 755 librt.lib $(ACKROOT)/cos/fortran

clean:
	rm -f *.o *.lib *.a *.lst test

test: test.o libio.a
	$(CC) $(LDFLAGS) -o $@ $+

calb01.o: calb01.cal
	$(BINDIR)/cal -o calb01.o $<

test.o: test.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

catstr.o: catstr.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

cmpstr.o: cmpstr.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

cpystr.o: cpystr.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

daryof.o: daryof.cal
	$(BINDIR)/cal -o daryof.o $<

darysz.o: darysz.cal
	$(BINDIR)/cal -o darysz.o $<

fio.o:	fio.cal
	$(BINDIR)/cal -t $(COSDIR)/systxt -o fio.o $<

fmt.o: fmt.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

fmt2.o: fmt2.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

getstr.o: getstr.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

units.o: units.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

#---------------------------  End Of File  --------------------------------
