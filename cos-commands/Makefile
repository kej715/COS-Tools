#--------------------------------------------------------------------------
#
#  Copyright 2021 Kevin E. Jordan
#
#  Name: Makefile
#
#  Description:
#      This is a makefile for building the COS commands.
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
COSDIR = ../cos-interface

all:	copyd.abs copyf.abs copyr.abs note.abs skipd.abs skipf.abs skipr.abs

copyd.abs: copyd.obj
	$(BINDIR)/ldr -o $@ $(COSDIR)/libcos.lib $<

copyd.obj: copyd.cal
	$(BINDIR)/cal -t $(COSDIR)/systxt $<

copyf.abs: copyf.obj
	$(BINDIR)/ldr -o $@ $(COSDIR)/libcos.lib $<

copyf.obj: copyf.cal
	$(BINDIR)/cal -t $(COSDIR)/systxt $<

copyr.abs: copyr.obj
	$(BINDIR)/ldr -o $@ $(COSDIR)/libcos.lib $<

copyr.obj: copyr.cal
	$(BINDIR)/cal -t $(COSDIR)/systxt $<

note.abs: note.obj
	$(BINDIR)/ldr -o $@ $(COSDIR)/libcos.lib $<

note.obj: note.cal
	$(BINDIR)/cal -t $(COSDIR)/systxt $<

skipd.abs: skipd.obj
	$(BINDIR)/ldr -o $@ $(COSDIR)/libcos.lib $<

skipd.obj: skipd.cal
	$(BINDIR)/cal -t $(COSDIR)/systxt $<

skipf.abs: skipf.obj
	$(BINDIR)/ldr -o $@ $(COSDIR)/libcos.lib $<

skipf.obj: skipf.cal
	$(BINDIR)/cal -t $(COSDIR)/systxt $<

skipr.abs: skipr.obj
	$(BINDIR)/ldr -o $@ $(COSDIR)/libcos.lib $<

skipr.obj: skipr.cal
	$(BINDIR)/cal -t $(COSDIR)/systxt $<

clean:
	rm -f *.obj *.abs *.lst

#---------------------------  End Of File  --------------------------------
