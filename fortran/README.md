# FORTRAN 77 Compiler
This directory and its subdirectories contain _KFTC_ (Kevin's FORTRAN Compiler), an
implementation of a FORTRAN 77 compiler and its supporting runtime and intrinsic function
libraries for COS 1.17. The original compiler provided by Cray Research, Inc. was lost from
the COS 1.17 image recovered for the
[Cray simulator](https://github.com/andrastantos/cray-sim). _KFTC_ is intended to fill this
gap for hobbyists interested in experiencing FORTRAN programming in the COS 1.17 environment.
The compiler is strongly influenced by, but not identical in functionality to, the original
Cray compiler as documented by
[CFT77 Reference Manual](http://bitsavers.trailing-edge.com/pdf/cray/CFT/SR-0018B_CFT77_Reference_Feb88.pdf).

_KFTC_ is a recursive descent parser. It parses FORTRAN 77 source code and produces CAL (Cray
Assembly Language) source code as output. The [CAL assembler](../README.md#cal) can then be
applied to produce Cray X-MP relocatable object code, and the
[LDR linking loader](../README.md#ldr) can be applied to the object code to resolve external
references and produce an absolute executable that will run on COS. A script named `cft77` is
provided in this directory to facilitate the full _compile-assemble-link_ process.

The `Makefile` in this directory enables the compiler to be built as a cross-compiler or as
a native compiler. To build it as a cross-compiler, simply run `make` without specifying any
arguments, as in:

```
make
```

You may specify the `install` rule to install it as a command on your build system, as in:

```
sudo make install
```

To build the compiler as a native compiler,
[this fork of Amsterdam Compiler Kit (ACK)](https://github.com/kej715/ack)
must be built and installed first. Then, run `make` specifying the `cos` rule, as in:

```
make clean
make cos
```

See [Going Native with the Tools](../README.md#native) for instructions about installing the
native compiler and the libraries on which it depends on a Cray X-MP system running COS 1.17.

FORTRAN 77 features not yet implemented by _KFTC_ include:

- Statement functions
- Implied DO loops
- Direct access files
- EQUIVALENCE
- Alternate return points in SUBROUTINE and RETURN
- EXTERNAL
- ENTRY
- INTRINSIC
- SAVE, but variables are allocated in static memory by default, so always saved in that case
- POINTER, a language extension provided by Cray FORTRAN
- DOUBLE PRECISION is handled the same as REAL
- COMPLEX

In addition, _KFTC_ does not yet implement any vectorization.
