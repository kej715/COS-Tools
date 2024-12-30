# FORTRAN 77 Compiler
This directory and its subdirectories contain _kFTC_ (Kevin's FORTRAN Compiler), an
implementation of a FORTRAN 77 compiler and its supporting runtime and intrinsic function
libraries for COS 1.17. The original compiler provided by Cray Research, Inc. was lost from
the COS 1.17 image recovered for the
[Cray simulator](https://github.com/andrastantos/cray-sim). _kFTC_ is intended to fill this
gap for hobbyists interested in experiencing FORTRAN programming in the COS 1.17 environment.
The compiler is strongly influenced by, but not identical in functionality to, the original
Cray compiler as documented by
[CFT77 Reference Manual](http://bitsavers.trailing-edge.com/pdf/cray/CFT/SR-0018B_CFT77_Reference_Feb88.pdf).

_kFTC_ is a single pass, recursive descent parser. It parses FORTRAN 77 source code and produces CAL (Cray
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

## Intrinsic Functions
_kFTC_ implements all ANSI-standard FORTRAN 77 intrinsic functions. The following
intrinsic functions defined by Cray as extensions are supported too, as documented by
[CFT77 Reference Manual](http://bitsavers.trailing-edge.com/pdf/cray/CFT/SR-0018B_CFT77_Reference_Feb88.pdf).

- **CLOCK** return current time of day as ASCII-encoded INTEGER value
- **DATE** return current date as ASCII-encoded INTEGER value
- **IRTC** return the current value of the real-time clock as an INTEGER value
- **JDATE** return current Julian date as ASCII-encoded INTEGER value
- **LOC** return location (address) of argument as a POINTER value
- **MASK** generate bit masks (e.g., for use in BOOLEAN expressions)
- **RTC** return the low 46 bits of the current value of the real-time clock as a REAL value
- **SHIFT** circular left shift
- **SHIFTL** end-off left shift
- **SHIFTR** end-off right shift

Additionally, logical operators such as .AND., .OR., and .NOT. apply to INTEGER values, as
documented for BOOLEAN values by [CFT77 Reference Manual](http://bitsavers.trailing-edge.com/pdf/cray/CFT/SR-0018B_CFT77_Reference_Feb88.pdf).

### Explicit Vector Functions
_kFTC_ does not implement implicit vectorization yet, but its intrinsic function library
provides **explicit** vector functions that make use of Cray X-MP vector registers and vector
instructions. These are defined as follows:

| Function Defn | Description | Result  | Arguments |
|---------------|-------------|---------|-----------|
| CALL **VLEN**(_len_) | Set vector length. This is not actually a function. It is a subroutine that should be called using a CALL statement. It sets the Cray X-MP vector length register. |  | INTEGER
| _Vi_ = **VLOAD**(_Vi_,_span_,_ary_) | Load vector register _Vi_ from array _ary_ with _span_ elements between each element loaded. _ary_ is handled as a 1-dimensional object, and _span_ facilitates mapping FORTRAN's column-major ordering onto it. | INTEGER | _Vi_ INTEGER, _span_ INTEGER, _ary_ INTEGER or REAL |
| _Vi_ = **VSADD**(_Vi_,_expr_,_Vk_) | Add elements of vector register _Vk_ to scalar value _expr_, and store results in vector register _Vi_. | INTEGER | _Vi_ INTEGER, _expr_ INTEGER or REAL, _Vk_ INTEGER) |
| _Vi_ = **VSDIV**(_Vi_,_expr_,_Vk_) | Divide scalar value _expr_ by elements of vector register _Vk_, and store results in vector register _Vi_. | INTEGER | _Vi_ INTEGER, _expr_ REAL, _Vk_ INTEGER) |
| _Vi_ = **VSMUL**(_Vi_,_expr_,_Vk_) | Multiply scalar value _expr_ by elements of vector register _Vk_, and store results in vector register _Vi_. | INTEGER | _Vi_ INTEGER, _expr_ REAL, _Vk_ INTEGER) |
| _Vi_ = **VSSUB**(_Vi_,_expr_,_Vk_) | Subtract elements of vector register _Vk_ from scalar value _expr_, and store results in vector register _Vi_. | INTEGER | _Vi_ INTEGER, _expr_ INTEGER or REAL, _Vk_ INTEGER) |
| _Vi_ = **VSTORE**(_Vi_,_span_,_ary_) | Store vector register _Vi_ into array _ary_ with _span_ elements between each element stored. _ary_ is handled as a 1-dimensional object, and _span_ facilitates mapping FORTRAN's column-major ordering onto it. | INTEGER | _Vi_ INTEGER, _span_ INTEGER, _ary_ INTEGER or REAL |
| _Vi_ = **VVADDI**(_Vi_,_Vj_,_Vk_) | Add elements of vector register _Vk_ to elements of vector register _Vj_ using **integer** addition, and store results in vector register _Vi_. | INTEGER | _Vi_ INTEGER, _Vj_ INTEGER, _Vk_ INTEGER) |
| _Vi_ = **VVADDR**(_Vi_,_Vj_,_Vk_) | Add elements of vector register _Vk_ to elements of vector register _Vj_ using **floating point** addition, and store results in vector register _Vi_. | INTEGER | _Vi_ INTEGER, _Vj_ INTEGER, _Vk_ INTEGER) |
| _Vi_ = **VVDIVR**(_Vi_,_Vj_,_Vk_) | Divide elements of vector register _Vj_ by elements of vector register _Vk_ using **floating point** division, and store results in vector register _Vi_. | INTEGER | _Vi_ INTEGER, _Vj_ INTEGER, _Vk_ INTEGER) |
| _Vi_ = **VVMULR**(_Vi_,_Vj_,_Vk_) | Multiply elements of vector register _Vj_ by elements of vector register _Vk_ using **floating point** multiplication, and store results in vector register _Vi_. | INTEGER | _Vi_ INTEGER, _Vj_ INTEGER, _Vk_ INTEGER) |
| _Vi_ = **VVSUBI**(_Vi_,_Vj_,_Vk_) | Subtract elements of vector register _Vk_ from elements of vector register _Vj_ using **integer** subtraction, and store results in vector register _Vi_. | INTEGER | _Vi_ INTEGER, _Vj_ INTEGER, _Vk_ INTEGER) |
| _Vi_ = **VVSUBR**(_Vi_,_Vj_,_Vk_) | Subtract elements of vector register _Vk_ from elements of vector register _Vj_ using **floating point** subtraction, and store results in vector register _Vi_. | INTEGER | _Vi_ INTEGER, _Vj_ INTEGER, _Vk_ INTEGER) |

Notice that the functions are composable. For example, a single expression can load, operate
upon, and store registers, as in:
```
      PROGRAM EXAMPLE
      INTEGER V1, V2, V3
      PARAMETER (V1=1, V2=2, V3=3)
      REAL A1(64), A2(64), A3(64)
      
      DO 100 I = 1, 64
        A1(I) = I
        A2(I) = 3.14159
 100  CONTINUE
 
      CALL VLEN(64)
      
      I = VSTORE(
     &      VVMULR(V3,                 ! vector-vector multiply, result to register V3
     &             VLOAD(V1, 1, A1),   ! load register V1 from array A1
     &             VLOAD(V2, 1, A2)),  ! load register V2 from array A2
     &      1, A3)                     ! store register V3 in array A3

      PRINT '(8(1X,8F8.3,/))', (A3(I), I = 1, 64)
      
      END
```
This program sets the vector length to 64 (the full length of a Cray X-MP vector register),
then it loads two vector registers, V1 and V2, from arrays A1 and A2, respectively,
and it uses vector multiplication to produce a result in vector register V3, which it then
stores in array A3. The program's output will look like:
```
    3.141   6.283   9.424  12.566  15.707  18.849  21.991  25.132
   28.274  31.415  34.557  37.699  40.840  43.982  47.123  50.265
   53.407  56.548  59.690  62.831  65.973  69.114  72.256  75.398
   78.539  81.681  84.822  87.964  91.106  94.247  97.389 100.530
  103.672 106.814 109.955 113.097 116.238 119.380 122.522 125.663
  128.805 131.946 135.088 138.229 141.371 144.513 147.654 150.796
  153.937 157.079 160.221 163.362 166.504 169.645 172.787 175.929
  179.070 182.212 185.353 188.495 191.636 194.778 197.920 201.061
```
As mentioned in the descriptions of **VLOAD** and **VSTORE**, FORTRAN maps arrays onto storage
using column-major ordering. For example, given the following definition of an array:
```
      REAL MATRIX(10, 20)
```
MATRIX has 10 rows, and each row contains 20 elements. Due to FORTRAN's column-major ordering,
the elements in each row of MATRIX are stored 10 elements apart. The elements in each column
are stored sequentially. The following code would load the first **column** of MATRIX into vector
register V3:
```
     CALL VLEN(10)
     I = VLOAD(3, 1, MATRIX)
```
The following code would load the second **row** of MATRIX into vector register V5:
```
     CALL VLEN(20)
     I = VLOAD(5, 10, MATRIX(2,1))
```

### Additional Intrinsic Functions
The following additional intrinsic functions are provided by _kFTC_:

| Function Defn | Description | Result  | Arguments |
|---------------|-------------|---------|-----------|
| _i_ = **CPUTIME**() | Returns the current amount of CPU time consumed by the COS job. | INTEGER | none |


## Features not yet implemented

FORTRAN 77 features not yet implemented by _kFTC_ include:

- Implied DO loops in DATA statements (implied DO in I/O statements *is* supported)
- Direct access files
- Alternate return points in SUBROUTINE and RETURN
- EXTERNAL
- ENTRY
- INTRINSIC
- DOUBLE PRECISION is handled the same as REAL
- COMPLEX

In addition, _kFTC_ does not yet implement any implicit vectorization.
