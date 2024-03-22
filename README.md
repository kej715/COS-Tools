# COS-Tools

This repository provides tools for creating software that will execute on a Cray X-MP
supercomputer running the COS operating system. In particular, these tools can be used
to create programs that will run on Andras Tantos' Cray supercomputer simulator,
[cray-sim](https://github.com/andrastantos/cray-sim), under the COS 1.17 operating system
provided in Andras' repository. Tools provided by this repo currently include:

- __cal__. A cross-assembler supporting [Cray Assembly Language v2](http://www.bitsavers.org/pdf/cray/CAL/SR-2003_CAL_Assembler_Version_2_Feb86.pdf).
- __dasm__. A disassembler able to disassemble executables produced by __ldr__.
- __ldr__. A linking loader producing executables from relocatables produced by the __cal__ cross-assembler and libraries managed by __lib__.
- __lib__. A library manager supporting collections of relocatables produced by the __cal__ cross-assembler.

Use _make_ or _gmake_ without any arguments to build the tools for execution on MacOS or
Linux, as in:

```
make
```

To install the tools, use the `install` target, as in:

```
sudo make install
```

By default, the tools are installed in the directory `/usr/local/bin`. Edit the `Makefile` if
you want to install them elsewhere.

## cal

__cal__ is a cross-assembler for the Cray Assembly Language. It accepts source programs
complying with the Cray Assemble Language as defined in [Cray Assembler Version 2 Reference
Manual](http://www.bitsavers.org/pdf/cray/CAL/SR-2003_CAL_Assembler_Version_2_Feb86.pdf).
It produces relocatable object files compatible with the structure defined by the COS
operating system and accepted by the __ldr__ tool.

The synopsis of the __cal__ command is:

```
cal [-f][-l lfile][-n ident][-o ofile][-T dlist][-t tfile]...[-w][-x] sfile ...
  -f       - enable flexible syntax
  -l lfile - listing file
  -n ident - identity of the source module
  -o ofile - object file
  -s       - disable section stacking
  -T dlist - text file directory list
  -t tfile - external text file
  -w       - exit with error status on warning indications
  -x       - enable implicit external symbols
  sfile - source file(s)
```

The `-f`, `-n`, `-s`, and `-x` parameters are intended mainly for use by the cross-compilers
provided by the [Cray X-MP fork](https://github.com/kej715/ack) of the ACK (Amsterdam
Compiler Kit).

A typical usage of the __cal__ command looks like:

```
cal hello.cal
```

This would cross-assemble a source file named `hello.cal` and produce an object file named
`hello.obj` and a listing file named `hello.lst`. You can use the `-o` and `-l` arguments to
override these default names based upon the source file name. Additionally, if you specify
`-l 0`, no listing file will be produced.

## dasm

__dasm__ is a disassembler. It accepts an absolute executable produced by __ldr__ and
produces a disassembly listing revealing the Cray X-MP machine instructions contained in
the executable. The synopsis of the __dasm__ command is:

```
dasm path [start] [limit]
  path  - COS executable file
  start - parcel address at which to start disassembly (default: 0200a)
  limit - parcel address at which to end disassembly (default: end of executable
```

## ldr

__ldr__ is a linking loader for the COS operating system and Cray X-MP computer system. It
accepts object files produced by the __cal__ cross-assembler and libraries produced by the
__lib__ library manager, and it produces absolute binary executables suitable for execution on
COS. The synopsis of the __ldr__ command is:

```
ldr [-m mfile][-o ofile] sfile...
  -m mfile - load map file
  -o ofile - executable output file
  sfile    - source file(s)
```

A typical usage of the __ldr__ command looks like:

```
ldr -m hello.map -o hello.abs hello.obj syslib.lib
```

This would link a relocatable object module named _hello.obj_ with a library named
_syslib.lib_ and produce an absolute executable named _hello.abs_. It would also produce
a file named _hello.map_ containing a load map providing details about the linking
operation.

## lib

__lib__ is an object library manager for collections of relocatable object modules produced
by __cal__. The synopsis of the __lib__ command is:

```
lib [-l lfile][-o ofile][-r name...] sfile...
  -l lfile - listing file
  -o ofile - output library file
  -r name  - name(s) of modules to omit from output library file
  sfile    - source object and library file(s)
```

A typical usage of the __lib__ command looks like:

```
lib -l - -o math.lib exp.obj log2.obj sqrt.obj
```

If the `-o` option is not specified, no output library file is produced. Thus, to list the
contents of a library, execute the __lib__ command as in:

```
lib -l - math.lib
```

## <a id="running"></a>Running on Cray X-MP

Andras Tantos' Cray supercomputer simulator,
[cray-sim](https://github.com/andrastantos/cray-sim), supports the Cray X-MP with the
COS 1.17 operating system. He has published a delightful narrative of the project in
[The Cray Files](http://www.modularcircuits.com/blog/articles/the-cray-files/), and this
narrative includes documentation about installing and running COS 1.17 on the simulator.

Unfortunately, the copy of COS 1.17 recovered by Andras does not include any programming
language translators, and no other working copies of COS are currently known to exist.
The tools provided by this repository help to resolve the lack of programming tools in the
recovered copy of COS by providing a cross-assembler and linking loader that will produce COS
executables. This repository enables hobbyists to create assembly language programs for the
iconic Cray X-MP supercomputer and run them on the simulator.

A significant challenge, however, is how to move cross-assembled executables onto a simulated
Cray X-MP. One solution is to install [DtCyber](https://github.com/kej715/DtCyber) and
leverage its ability to run _Cray Station_ software. DtCyber is a simulator for
[Control Data Corporation's (CDC)](https://en.wikipedia.org/wiki/Control_Data_Corporation)
_Cyber_ series of supercomputers. Installing the
[NOS 2.8.7](https://github.com/kej715/DtCyber/tree/main/NOS2.8.7#readme)
operating system on _DtCyber_ enables using Cray's _Cray Station_ software to exchange files
and batch jobs between the CDC and Cray supercomputers.

In addition to _Cray Station_, the NOS 2.8.7 operating system has a mature set of other data
communication software including TCP/IP applications (e.g., FTP),
[Kermit](https://en.wikipedia.org/wiki/Kermit_(protocol)),
and [XModem](https://en.wikipedia.org/wiki/XMODEM).
For example, it is possible to use an FTP client to upload a cross-assembled Cray binary to
a CDC machine running NOS 2.8.7 and then use _Cray Station_ to transfer the file to a
Cray X-MP running COS. Likewise, Kermit or XModem can be used to upload files to a CDC
machine running NOS 2.8.7.

Given this, the following log of a user session illustrates how a user could create an
assembly language program, use __cal__ to cross-assemble it, use __ldr__ to produce an
absolute executable, upload the absolute executable to NOS 2.8.7 using
FTP, create a NOS CCL procedure to transfer it to and run it on a Cray X-MP, and view the
results returned.

First, display the source code, a simple assembly language program that displays a trivial
log message:

```
kej@Kevins-Air examples % cat hello.cal
         TITLE     'Hello'
         SUBTITLE  'Simple *Hello World* program'
         IDENT     HELLO
         COMMENT   'HELLO - A simple *Hello World* program'
         ENTRY     HELLO
         START     HELLO

HELLO    S0        O'004          ; F$MSG
         S1        ='Hello world!'Z
         S2        O'17           ; CLASS=1, OR=1, FC=3
         EX
         S0        O'000          ; F$ADV
         EX

         END
```
Cross-assemble it:
```
kej@Kevins-Air examples % cal hello.cal
```
The cross-assembler produces a relocatable object file named _hello.obj_ and a listing file
named _hello.lst_:
```
-rw-r--r--@ 1 kej  staff   427 Mar  9 12:23 hello.cal
-rw-r--r--@ 1 kej  staff  2361 Mar 21 21:12 hello.lst
-rw-r--r--@ 1 kej  staff   456 Mar 21 21:12 hello.obj
```
Use __ldr__ to produce an absolute executable from the relocatable object file:
```
kej@Kevins-Air examples % ldr hello.obj
kej@Kevins-Air examples % ls -l hello.*
-rw-r--r--@ 1 kej  staff   416 Mar 21 21:15 hello.abs
-rw-r--r--@ 1 kej  staff   427 Mar  9 12:23 hello.cal
-rw-r--r--@ 1 kej  staff  2361 Mar 21 21:12 hello.lst
-rw-r--r--@ 1 kej  staff   456 Mar 21 21:12 hello.obj
```
Use FTP to upload it to the NOS 2.8.7 system (_kevins-max.local_) as an indirect access
permanent file named HELLO, in binary mode:
```
kej@Kevins-Air examples % ftp kevins-max.local
Trying [fe80::1889:9f48:a180:d9ce%en0]:21 ...
ftp: Can't connect to `fe80::1889:9f48:a180:d9ce%en0:21': Connection refused
Trying 192.168.1.238:21 ...
Connected to kevins-max.local.
220 SERVICE READY FOR NEW USER.
Name (kevins-max.local:kej): guest
331 USER NAME OKAY, NEED PASSWORD.
Password:
230 USER LOGGED IN, PROCEED.
Remote system type is NOS.
ftp> bin
200 COMMAND OKAY.
ftp> put hello.abs hello/ia
local: hello.abs remote: hello/ia
227 ENTERING PASSIVE MODE (192,168,1,238,30,11)
150 FILE STATUS OKAY; ABOUT TO OPEN DATA CONNECTION.
100% |*****************************************************************************************************************************************|   416      842.84 KiB/s    00:00 ETA
226 CLOSING DATA CONNECTION.
416 bytes sent in 00:00 (1.95 KiB/s)
ftp> quit
221 SERVICE CLOSING CONTROL CONNECTION. LOGGED OUT.
```
Use Telnet to log into the NOS 2.8.7 system:
```
kej@Kevins-Air examples % telnet kevins-max.local
Trying 192.168.1.238...
Connected to kevins-max.local.
Escape character is '^]'.

Connecting to host - please wait ...
Connected

WELCOME TO THE NOS SOFTWARE SYSTEM.
COPYRIGHT CONTROL DATA SYSTEMS INC. 1994.
24/03/21. 21.20.34. TE04P06
NCCMAX - CYBER 875.                     NOS 2.8.7 871/871.
FAMILY:
USER NAME: guest
PASSWORD:

JSN: ABXC, NAMIAF
/
```
Create and save a CCL procedure file containing a procedure named RUN. This procedure will
use the _Cray Station_ interface to submit a batch job to the Cray X-MP supercomputer.
When the job starts running on the Cray X-MP, it uses the _Cray Station_ interface to
retrieve a file containing a COS executable from the NOS 2.8.7 system. After retrieving the
file, it runs it. When the job completes, the _Cray Station_ interface will return the log
and any output produced by the executable to the NOS system, and this will appear as a
file in the user's wait queue.

This procedure file needs only to be created and saved once and can be re-used again and again
later, either to re-run the original executable, or to run other ones.
```
/new,cray
/text
 ENTER TEXT MODE.

.PROC,RUN*I,F=(*F,*N=CRAYBIN).
.IF,FILE(F,AS),LOCAL.
  REWIND,F.
.ELSE,LOCAL.
  GET,F.
.ENDIF,LOCAL.
COPYBF,F,ZZZCBIN.
REPLACE,ZZZCBIN=F.
CSUBMIT,CRAYJOB,TO.
REVERT,NOLIST.
.DATA,CRAYJOB.
JOB,JN=CRAYRUN,T=60.
ACCOUNT,AC=CRAY,APW=XYZZY,UPW=QUASAR.
ECHO,ON=ALL.
OPTION,STAT=ON.
FETCH,DN=F,MF=FE,DF=TR,^
TEXT='USER,GUEST,GUEST.GET,F.CTASK.'.
F.

 EXIT TEXT MODE.
/save,cray
```
Note that _Control-T_ is pressed to exit TEXT mode.

The CATLIST command displays all of the user's permanent files. This confirms that the
cross-compiled executable, FIB, uploaded previously and the procedure file, CRAY, just
created have been saved as indirect access files as expected.
```
/catlist
 CATALOG OF  GUEST            FM/CYBER   24/03/21. 21.23.07.



 INDIRECT ACCESS FILES

  CRAY      FTPPRLG   HELLO     LOGIN     MAILBOX

 DIRECT ACCESS FILES

  GAMFILE   LIBRARY   RELFILE

         5 INDIRECT ACCESS FILES ON DISK.  TOTAL PRUS =        10.
         3 DIRECT ACCESS FILES ON DISK.    TOTAL PRUS =        72.
```
Use the BEGIN command to execute the procedure named RUN in the procedure file named CRAY
and pass the name of the file containing the cross-assembled executable as a parameter:
```
/begin,run,cray,hello
```
Use the STATUS command to monitor progress. First, the batch job destined for the Cray X-MP
may appear in the NOS system's INPUT queue.
```
/status,jsn

 JSN SC CS DS LID STATUS                JSN SC CS DS LID STATUS

 ACAE.B.  .BC.XMP.INPUT QUEUE           ABZR.T.ON.BC.   .EXECUTING
```
Then, it may seem to disappear. While actually running on the Cray X-MP, it may not be
visible to the NOS system.
```
/status,jsn

 JSN SC CS DS LID STATUS                JSN SC CS DS LID STATUS

 ABZR.T.ON.BC.   .EXECUTING
```
However, the _Cray Station_ interface will submit a batch job to the NOS system to
participate in transferring the cross-assembled executable from NOS to COS, so you might catch
the brief execution of that job.
```
/status,jsn

 JSN SC CS DS LID STATUS                JSN SC CS DS LID STATUS

 ABZR.T.ON.BC.   .EXECUTING             ACAF.B.NI.BC.   .EXECUTING
```
When the batch job on the Cray X-MP side has completed, its log and output will be
returned to the NOS 2.8.7 WAIT queue.
```
/status,jsn

 JSN SC CS DS LID STATUS                JSN SC CS DS LID STATUS

 ABZR.T.ON.BC.   .EXECUTING             ACAH.S.  .BC.MAX.WAIT QUEUE
```
Use the QGET command to retrieve the output by JSN from the WAIT queue, and then display it
using the LIST command:
```
/qget,acah
 QGET COMPLETE.
/list,f=acah
1
  20:02:29.8633       0.0007    CSP             CRAY XMP-14 SN 302      LEADING EDGE TECHNOLOGIES       03/21/84
  20:02:29.8662       0.0008    CSP
  20:02:29.8697       0.0010    CSP             CRAY OPERATING SYSTEM           COS 1.17  ASSEMBLY DATE 02/28/89
  20:02:29.8726       0.0011    CSP
  20:02:29.8753       0.0012    CSP
  20:02:29.9016       0.0014    CSP     JOB,JN=CRAYRUN,T=60.
  20:02:30.0044       0.0105    CSP     ACCOUNT,AC=,APW=,UPW=.
  20:02:30.0101       0.0109    CSP
  20:02:30.0126       0.0110    CSP     .......................................................................
  20:02:30.0153       0.0111    CSP
  20:02:30.0493       0.0122    CSP     ECHO,ON=ALL.
  20:02:30.0592       0.0129    CSP     OPTION,STAT=ON.
  20:02:30.0735       0.0162    CSP     FETCH,DN=HELLO,MF=FE,DF=TR,^
  20:02:30.0796       0.0186    CSP     TEXT=.
  20:02:33.1901       0.0189    SCP     ACAF 21.27.42.USER,GUEST,.
  20:02:33.1920       0.0189    SCP     ACAF 21.27.43.CHARGE,   *                  ,
  20:02:33.1939       0.0189    SCP     ACAF 21.27.44. TRO - PF AND TAPE TRANSPARENT OUTPUT.
  20:02:33.1960       0.0189    SCP     ACAF 21.27.44.TRO: DATASET HELLO  ,         70B WORDS,     0.185
  20:02:33.5543       0.0189    SCP     SS004 - DATASET RECEIVED FROM FRONT END
  20:02:35.2084       0.0206    CSP     HELLO.
  20:02:35.2217       0.0209    USER    HELLO WORLD!
  20:02:35.2438       0.0213    CSP     END OF JOB
  20:02:35.2485       0.0216    EXP     SY005 - HELLO         512 WRDS,    1 IOS,      1 REQ,       1 STRS,     .017 SEC
  20:02:35.2507       0.0216    EXP           - 29-1-22A       18 STRS   READ:         1 REQ,       1 STRS,     .019 SEC
  20:02:35.2595       0.0217    CSP
  20:02:35.2617       0.0218    CSP
  20:02:35.2660       0.0221    CSP     CHARGES
  20:02:35.2686       0.0221    CSP     CS032 - DATASET NOT LOCAL:
  20:02:35.2714       0.0222    ABORT   AB025 - USER PROGRAM REQUESTED ABORT
  20:02:35.2735       0.0222    ABORT         - P=00001417B   TASK-ID=0001
  20:02:35.2755       0.0222    ABORT         - BASE=00775000 LIMIT=01153000 CPU-NUMBER=00
  20:02:35.2774       0.0222    ABORT         - JOB STEP ABORTED.
```
