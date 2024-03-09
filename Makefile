#--------------------------------------------------------------------------
#
#  Copyright 2021 Kevin E. Jordan
#
#  Name: Makefile
#
#  Description:
#      This is a makefile for building the assembler.
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
PREFIX = /usr/local
COSOBJS = $(PREFIX)/share/ack/cos/boot.o $(PREFIX)/share/ack/cos/c-ansi.o

CALHDRS = basetypes.h    \
          calconst.h     \
          calproto.h     \
          caltypes.h     \
          cosdataset.h   \
          cosldr.h       \
          fnv.h          \
          services.h

CALOBJS = cal.o          \
          cosdataset.o   \
          error.o        \
          fnv32a.o       \
          global.o       \
          inst.o         \
          io.o           \
          list.o         \
          object.o       \
          parse.o        \
          re.o           \
          services.o     \
          trees.o

LDRHDRS = basetypes.h    \
          cosdataset.h   \
          cosldr.h       \
          fnv.h          \
          ldrconst.h     \
          ldrproto.h     \
          ldrtypes.h     \
          services.h

LIBHDRS = basetypes.h    \
          cosdataset.h   \
          cosldr.h       \
          fnv.h          \
          libconst.h     \
          libproto.h     \
          libtypes.h     \
          services.h

LDROBJS = ldr.o          \
          cosdataset.o   \
          fnv32a.o       \
          services.o

LIBOBJS = lib.o          \
          cosdataset.o   \
          fnv32a.o       \
          services.o

all: cal ldr lib

cos:
	CC=ack EXTRAOBJS="$(COSOBJS)" $(MAKE)

cal: $(CALOBJS)
	$(CC) $(LDFLAGS) -o $@ $+ $(EXTRAOBJS)

ldr: $(LDROBJS)
	$(CC) $(LDFLAGS) -o $@ $+ $(EXTRAOBJS)

lib: $(LIBOBJS)
	$(CC) $(LDFLAGS) -o $@ $+ $(EXTRAOBJS)

clean:
	rm -f *.o *.abs cal ldr lib

install: cal ldr lib
	install -b -g wheel -o root -m 755 cal ldr lib $(PREFIX)/bin

cal.o:  cal.c $(CALHDRS)
	$(CC) $(CFLAGS) -c $<
cosdataset.o: cosdataset.c cosdataset.h
	$(CC) $(CFLAGS) -c $<
error.o: error.c $(CALHDRS)
	$(CC) $(CFLAGS) -c $<
fnv32a.o: fnv32a.c fnv.h
	$(CC) $(CFLAGS) -c $<
global.o: global.c $(CALHDRS)
	$(CC) $(CFLAGS) -c $<
inst.o: inst.c $(CALHDRS)
	$(CC) $(CFLAGS) -c $<
io.o:   io.c $(CALHDRS)
	$(CC) $(CFLAGS) -c $<
ldr.o:  ldr.c $(LDRHDRS)
	$(CC) $(CFLAGS) -c $<
list.o: list.c $(CALHDRS)
	$(CC) $(CFLAGS) -c $<
object.o: object.c $(CALHDRS)
	$(CC) $(CFLAGS) -c $<
parse.o: parse.c $(CALHDRS)
	$(CC) $(CFLAGS) -c $<
re.o:   re.c $(CALHDRS)
	$(CC) $(CFLAGS) -c $<
services.o: services.c $(CALHDRS)
	$(CC) $(CFLAGS) -c $<
trees.o: trees.c $(CALHDRS)
	$(CC) $(CFLAGS) -c $<

#---------------------------  End Of File  --------------------------------
