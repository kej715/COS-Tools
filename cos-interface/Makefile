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

BINDIR = /usr/local/bin

OBJS =	\
	args.obj  \
	chars.obj \
	dmp.obj   \
	iutil.obj \
	sysio.obj

all:	libcos.lib

libcos.lib: $(OBJS)
	rm -f libcos.lib ;\
	$(BINDIR)/lib -o libcos.lib $(OBJS)

args.obj: args.cal systxt.cal
	$(BINDIR)/cal -t systxt $<

chars.obj: chars.cal systxt.cal
	$(BINDIR)/cal -t systxt $<

dmp.obj: dmp.cal systxt.cal
	$(BINDIR)/cal -t systxt $<

iutil.obj: iutil.cal systxt.cal
	$(BINDIR)/cal -t systxt $<

sysio.obj: sysio.cal systxt.cal
	$(BINDIR)/cal -t systxt $<

clean:
	rm -f *.obj *.lib *.lst

#---------------------------  End Of File  --------------------------------
