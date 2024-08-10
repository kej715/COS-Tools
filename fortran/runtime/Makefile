#--------------------------------------------------------------------------
#
#  Copyright 2024 Kevin E. Jordan
#
#  Name: Makefile
#
#  Description:
#      This is a makefile for building the FORTRAN compiler.
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
BINDIR = /usr/local/bin
COSDIR = ../../cos-interface
COSOBJS = $(PREFIX)/share/ack/cos/boot.o $(PREFIX)/share/ack/cos/c-ansi.o

HDRS = const.h \
       fmt.h   \
       units.h

OBJS = catstr.o cmpstr.o cpystr.o fio.o fmt.o fmt2.o units.o

all: libio.a

cos:
	CC=ack AR=lib ARFLAGS="-o" $(MAKE)

libio.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $+

clean:
	rm -f *.o *.lib libio.a

test: test.o libio.a
	$(CC) $(LDFLAGS) -o $@ $+

test.o: test.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

catstr.o: catstr.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

cmpstr.o: cmpstr.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

cpystr.o: cpystr.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

fio.o:	fio.cal
	$(BINDIR)/cal -t $(COSDIR)/systxt -o fio.o $<

fmt.o: fmt.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

fmt2.o: fmt2.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

units.o: units.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

#---------------------------  End Of File  --------------------------------
