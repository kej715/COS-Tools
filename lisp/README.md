
# LISPF4 - InterLisp Interpreter

LISPF4 is an InterLisp interpreter written by Dr. Mats Nordstrom from Uppsala, Sweden in the
early 1980's. It was written originally in FORTRAN IV. The variant found here derives directly
from Blake McBride's [LISPF4 repo in GitHub](https://github.com/blakemcbride/LISPF4).
Specifically, Blake's repo retains a copy of the
[original LISPF4 distribution file](https://github.com/blakemcbride/LISPF4/blob/master/lispf4.orig) which he acquired from its original author, Dr. Mats Nordstrom.

The LISPF4 variant in this repo differs very little from the original. Small changes have been
made to enable it to be compiled by
[kFTC](https://github.com/kej715/COS-Tools/tree/main/fortran)
and run on COS 1.17. As in Blake's repo, this repo retains a copy of the original distribution
file containing source code and documentation, and it also retains a copy of the original
license file with copyright information.

After building and installing _kFTC_, LISPF4 can be built by running the default rule of
the Makefile in this directory,
[lisp](https://github.com/kej715/COS-Tools/tree/main/lisp). The Makefile will produce a file
named **lispf4.abs**. This is an executable binary of the LISPF4 interpreter that can be
transferred to COS 1.17 on the Cray X-MP simulator and run there. The file named **LISPSYS**
is a text file that must also be transferred to COS 1.17 and made available to the interpreter
as a local dataset. It provides definitions for the interpreter's system atoms. The file
named **LISPINI** can be provided as input to the interpreter to define a collection of
utility functions and create a _rollout_ file that captures the interpreter's initial state.
The ROLLIN function can then be used in subsequent startups to restore this state efficiently
(including the utility function definitions).

For additional information about using LISPF4 and InterLisp, see the following documents:

- [LISPF4 Users Guide](https://github.com/kej715/COS-Tools/blob/main/lisp/UsersGuide.txt) from original software distribution
- [INTERLISP REFERENCE MANUAL](https://bitsavers.org/pdf/xerox/interlisp/Interlisp-Oct_1978.pdf) on BitSavers

