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

#CFLAGS  = -O3 $(INCL) $(EXTRACFLAGS)
CFLAGS  = -O0 -g $(INCL) $(EXTRACFLAGS)
PREFIX = /usr/local
COSOBJS = $(PREFIX)/share/ack/cos/boot.o $(PREFIX)/share/ack/cos/c-ansi.o

HDRS = binops.h        \
       codegen.h       \
       coercion.h      \
       const.h         \
       proto.h         \
       types.h         \
       ../basetypes.h

OBJS = binops.o   \
       codegen.o  \
       coercion.o \
       compile.o  \
       global.o   \
       lex.o      \
       main.o     \
       services.o \
       symbols.o

all: kftc

cos:
	CC=ack EXTRAOBJS="$(COSOBJS)" $(MAKE)

kftc: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $+ $(EXTRAOBJS)

kftc.abs:
	CC=ack $(MAKE) kftc

clean:
	rm -f *.o *.abs kftc

install: kftc
	install -b -o root -m 755 kftc $(PREFIX)/bin

binops.o: binops.c $(HDRS)
	$(CC) $(CFLAGS) -c $<
coercion.o: coercion.c $(HDRS)
	$(CC) $(CFLAGS) -c $<
codegen.o: codegen.c $(HDRS)
	$(CC) $(CFLAGS) -c $<
compile.o: compile.c $(HDRS)
	$(CC) $(CFLAGS) -c $<
global.o: global.c $(HDRS)
	$(CC) $(CFLAGS) -c $<
lex.o: lex.c $(HDRS)
	$(CC) $(CFLAGS) -c $<
main.o: main.c $(HDRS)
	$(CC) $(CFLAGS) -c $<
services.o: services.c $(HDRS)
	$(CC) $(CFLAGS) -c $<
symbols.o: symbols.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

#---------------------------  End Of File  --------------------------------
