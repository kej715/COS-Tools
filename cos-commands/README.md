# COS Commands
This directory contains implementations of various COS commands that were lost from the
COS 1.17 image recovered for the [Cray simulator](https://github.com/andrastantos/cray-sim).
These commands are documented in [SR-0011 O Cray X-MP and Cray-1 Computer Systems COS Version
1 Reference Manual](http://www.bitsavers.org/pdf/cray/COS/SR-0011-O-CRAY_XMP_and_CRAY_1_Computer_Systems-COS_Version_1_Reference_Manual-May_1987.OCR.pdf).

The currently supported commands are:

- __COPYD__ Copy Blocked Dataset.
- __COPYF__ Copy Blocked Files.
- __COPYR__ Copy Blocked Records.
- __NOTE__ Write Text to a Dataset.
- __SKIPD__ Skip Blocked Dataset.
- __SKIPF__ Skip Blocked Files. This implementation deviates from the reference manual in that it does not support skipping backward, i.e., negative values of the __NF__ parameter are not supported.
- __SKIPR__ Skip Blocked Records. This implementation deviates from the reference manual in that it does not support skipping backward, i.e., negative values of the __NR__ parameter are not supported.
