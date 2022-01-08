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

CFLAGS  = -O3 -std=gnu99 $(INCL) $(EXTRACFLAGS)
#CFLAGS  = -O0 -g -std=gnu99 $(INCL) $(EXTRACFLAGS)

HDRS = basetypes.h    \
       const.h        \
       cosdataset.h   \
       cosldr.h       \
       proto.h        \
       types.h

OBJS = cosdataset.o   \
       error.o        \
       global.o       \
       inst.o         \
       io.o           \
       list.o         \
       main.o         \
       object.o       \
       parse.o        \
       services.o     \
       trees.o

cal: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $+

all: clean cal

clean:
	rm -f *.o

%.o : %.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

#---------------------------  End Of File  --------------------------------
