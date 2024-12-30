#--------------------------------------------------------------------------
#
#  Copyright 2024 Kevin E. Jordan
#
#  Name: Makefile
#
#  Description:
#      This is a makefile for building the InterLISP interpreter for COS
#      using the FORTRAN compiler.
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

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
ACKDIR = /usr/local/share/ack
COSDIR = $(ACKDIR)/cos
FTNDIR = $(COSDIR)/fortran

all: lispf4.abs

lispf4.abs: HEART.o FLOSOR.o COS.o
	$(BINDIR)/ldr -o $@ -m lisp.map $+ $(FTNDIR)/libio.lib $(FTNDIR)/librt.lib $(FTNDIR)/libintf.lib $(COSDIR)/libem.a $(COSDIR)/libsys.a $(COSDIR)/libc.a

COS.o: COS.FOR
	$(BINDIR)/kftc -o $<.cal -l $<.lst -w -s $< ; \
	$(BINDIR)/cal -x -o $@ -l 0 -i $<.cal

FLOSOR.o: FLOSOR.FOR
	$(BINDIR)/kftc -o $<.cal -l $<.lst -w -s $< ; \
	$(BINDIR)/cal -x -o $@ -l 0 -i $<.cal

HEART.o: HEART.FOR
	$(BINDIR)/kftc -o $<.cal -l $<.lst -w -s $< ; \
	$(BINDIR)/cal -x -o $@ -l 0 -i $<.cal

clean:
	rm -f *.abs *.cal *.o *.lst *.map
