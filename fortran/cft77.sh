#!/bin/sh
#
#  Cross-compile a FORTRAN 77 program. This script assumes that the
#  tools defined in the COS-Tools repository have been built and
#  installed and also that the Amsterdam Compiler Kit has been built
#  and installed.
#
#  Arguments:
#    $1 : name of FORTRAN source file, assumed to have extension ".f"
#
#  Produces:
#    $1.s   : assembly language source file
#    $1.o   : Cray X-MP object file
#    $1.map : load map
#    $1.abs : Cray X-MP absoluate executable
#
export ACKROOT=/usr/local/share/ack
kftc -l $1.flst -o $1.s -s $1.f
cal -x -o $1.o -i $1.s
ldr -o $1.abs -m $1.map $1.o runtime/libio.lib runtime/librt.lib intrinsic/libintf.lib $ACKROOT/cos/libem.a $ACKROOT/cos/libsys.a $ACKROOT/cos/libc.a
