#ifndef COSLDR_H
#define COSLDR_H
/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: cosldr.h
**
**  Description:
**      This file defines constants associated with COS loader tables.
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**      http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
**--------------------------------------------------------------------------
*/

/*
 *  Table types
 */
#define LDR_TT_PWT  006  /* Partial Word Table        */
#define LDR_TT_DMT  007  /* Debug Map Table           */
#define LDR_TT_DFT  010  /* Directory (BUILD) Table   */
#define LDR_TT_SMT  011  /* Symbol Table              */
#define LDR_TT_DPT  013  /* Duplication Table         */
#define LDR_TT_XRT  014  /* External Relocation Table */
#define LDR_TT_BRT  015  /* Block Relocation Table    */
#define LDR_TT_TXT  016  /* Text Table                */
#define LDR_TT_PDT  017  /* Program Description Table */

/*----------------------------------------------------------------------------
    PDT - Program Description Table
  
    The PDT is the first loader table of a program binary. It contains information
    needed for relocation, such as entry points, externals, blocks, indices, and
    time stamps. It begins with a header word. The header word is followed by a
    header entry, then block descriptions, then entry point descriptions, then
    external descriptions, then a trailer. Any pf the block, entry point, and 
    external description sections can be empty.

         Field        Byte     Word   Bits  Description 
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_PDT_TT       0  /*    0   0- 3  Table type (O'17)                          */
#define LDR_PDT_WC       0  /*    0   4-27  Word count including header word           */
#define LDR_PDT_XL       3  /*    0  28-41  Number of words - external descriptors     */
#define LDR_PDT_EL       5  /*    0  42-55  Number of words - entry point descriptors  */
#define LDR_PDT_BL       7  /*    0  56-63  Number of words - block name descriptors   */

#define LDR_PDT_HL      14  /*    1  50-63  Word length of header entry                */

#define LDR_PDT_OVL     16  /*    2      2  Overlay flag                               */
#define LDR_PDT_SBC     16  /*    2      3  SBCA flag                                  */
#define LDR_PDT_MTX     16  /*    2      4  Machine type extension fields present      */
#define LDR_PDT_MT      16  /*    2   5- 6  Machine type (obsolete)                    */
#define LDR_PDT_CSQ     16  /*    2      7  Calling sequence flag                      */
#define LDR_PDT_TYP     17  /*    2      8  PDT type - 0=old PDT, 1=new PDT            */
#define LDR_PDT_STK     17  /*    2      9  Stack flag                                 */
#define LDR_PDT_OVT     17  /*    2     10  Overlay type - 0=type 1, 1=type 2          */
#define LDR_PDT_MOD     17  /*    2  12-15  Relocatable overlay module type            */
#define LDR_PDT_SSM     18  /*    2     16  Secure memory flag                         */
#define LDR_PDT_SDR     18  /*    2     17  SDR module flag                            */
#define LDR_PDT_SDM     18  /*    2     18  Special job - implies no echo              */
#define LDR_PDT_XMA     18  /*    2     19  XMA flag                                   */
                            /*                 0 - no extended memory                  */
                            /*                 1 - extended memory (Cray X-MP)         */

#define LDR_PDT_SIS     24  /*    3   0-31  Initial stack size                         */
#define LDR_PDT_SIN     28  /*    3  32-63  Stack increment size                       */

#define LDR_PDT_MIS     32  /*    4   0-31  Initial managed memory size                */
#define LDR_PDT_MIN     36  /*    4  32-63  Managed memory increment size              */

#define LDR_PDT_R$1     40  /*    5   0-63  Reserved for future use                    */

#define LDR_PDT_BCI     48  /*    6   0-63  Block common initialization value          */

#define LDR_PDT_SC0     56  /*    7   0-63  Privilege word                             */
#define LDR_PDT_SC1     64  /*    8   0-63  Privilege word                             */
#define LDR_PDT_SC2     72  /*    9   0-63  Privilege word                             */
#define LDR_PDT_SC3     80  /*   10   0-63  Privilege word                             */

#define LDR_PDT_UD1     88  /*   11   0-63  User definable word 1                      */
#define LDR_PDT_UD2     96  /*   12   0-63  User definable word 2                      */

#define LDR_PDT_NRD    104  /*   13      0  NORED flag                                 */
#define LDR_PDT_HLM    109  /*   13  40-63  HLM for binary                             */

#define LDR_PDT_PAD    112  /*   14   0-31  Pad increment for field length             */
#define LDR_PDT_BC     116  /*   14  32-63  Blank common increment                     */

#define LDR_PDT_MBA    120  /*   15   0-31  Managed memory base address                */
#define LDR_PDT_MAV    124  /*   15  32-63  Managed memory available base address      */

#define LDR_PDT_MEP    128  /*   16   0-31  Managed memory epsilon                     */
#define LDR_PDT_BCP    132  /*   16  32-63  B.%STKBCP value                            */

#define LDR_PDT_CTP    136  /*   17   0-31  B.%STKCTP value                            */
#define LDR_PDT_ATP    140  /*   17  32-63  B.%STKATP value                            */

#define LDR_PDT_MCL    144  /*   18   0-63  Machine characteristics entry length       */

#define LDR_PDT_PMT    152  /*   19   0-63  Primary machine type code                  */

#define LDR_PDT_A32    166  /*   20     50  YMP 32-bit addressing required             */
#define LDR_PDT_COR    166  /*   20     51  Control Operand Range Intrpts (MDI/DRI)    */
#define LDR_PDT_CLS    166  /*   20     52  Cluster registers required                 */
#define LDR_PDT_STR    166  /*   20     53  Status register required                   */
#define LDR_PDT_BDM    166  /*   20     54  Bidirectional memory must be disabled      */
#define LDR_PDT_HPM    166  /*   20     55  Hardware permformance monitor required     */
#define LDR_PDT_AVL    167  /*   20     56  Additional vector logical unit assumed     */
#define LDR_PDT_NVR    167  /*   20     57  No vector recursion permitted              */
#define LDR_PDT_VRR    167  /*   20     58  Vector recursion required                  */
#define LDR_PDT_RVL    167  /*   20     59  Ability to read vector length required     */
#define LDR_PDT_PC     167  /*   20     60  Programmable clock required                */
#define LDR_PDT_CIG    167  /*   20     61  Compress/index gather/scatter required     */
#define LDR_PDT_EMR    167  /*   20     62  Extended memory addressing required        */
#define LDR_PDT_VP     167  /*   20     63  Vector population count unit required      */

/*----------------------------------------------------------------------------
    PDT Block entry

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_PDT_PGM      0  /*    0   0-63  Program name                               */
#define LDR_PDT_AF       8  /*    1      0  Absolute module flag                       */
#define LDR_PDT_FE       8  /*    1      1  Fatal error flag                           */
#define LDR_PDT_BD       8  /*    1      2  Block data                                 */
#define LDR_PDT_AL       8  /*    1      3  Program block align flag                   */
#define LDR_PDT_PCO      8  /*    1      4  Program block 'code only' flag             */
#define LDR_PDT_ORG     10  /*    1  16-39  Origin address                             */
#define LDR_PDT_PRL     13  /*    1  40-63  Program length                             */

/*----------------------------------------------------------------------------
    PDT Common block entry

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_PDT_BKN      0  /*    0   0-63  Block name                                 */
#define LDR_PDT_BKT      8  /*    1   0- 9  Block type:                                */
                            /*                 0 - common                              */
                            /*                 1 - mixed (code/data)                   */
                            /*                 2 - code                                */
                            /*                 3 - data                                */
                            /*                 4 - const                               */
                            /*                 5 - dynamic                             */
                            /*                 6 - task common                         */
#define LDR_PDT_BKP      9  /*    1  10-15  Block placement:                           */
                            /*                 0 - global memory                       */
                            /*                 1 - reserved for Cray 2 local memory    */
                            /*                 2 - extended memory Cray X-MP           */
#define LDR_PDT_UD3     11  /*    1  30-34  Reserved for customer use                  */
#define LDR_PDT_BKD     12  /*    1     35  Dynamic common flag (not used by COS)      */
#define LDR_PDT_UD4     12  /*    1  36-38  Reserved for customer use                  */
#define LDR_PDT_ALC     12  /*    1     39  Common block ALIGN flag                    */
#define LDR_PDT_BKL     13  /*    1  40-63  Block length                               */

/*----------------------------------------------------------------------------
    PDT Entry point entry

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_PDT_EPN      0  /*    0   0-63  Entry point name                           */
#define LDR_PDT_EPE     14  /*    1     55  Primary entry type                         */
#define LDR_PDT_EBI     15  /*    1  56-62  Block index                                */
#define LDR_PDT_EPQ     15  /*    1     63  Relocation mode                            */
#define LDR_PDT_EPV     16  /*    2   0-63  Entry point relocation value               */

/*----------------------------------------------------------------------------
    PDT External entry

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_PDT_EXN      0  /*    0   0-63  External name                              */

/*----------------------------------------------------------------------------
    PDT trailer entry

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_PDT_TDA      0  /*    0   0-63  Date of PDT generation                     */
#define LDR_PDT_TTI      8  /*    1   0-63  Time of PDT generation                     */
#define LDR_PDT_TOS     16  /*    2   0-63  Operating system version                   */
#define LDR_PDT_TOD     24  /*    3   0-63  Operating system assembly date             */
#define LDR_PDT_TXX     32  /*    4   0-63  Reserved                                   */
#define LDR_PDT_TNM     40  /*    5   0-63  Name of generating product                 */
#define LDR_PDT_TVR     48  /*    6   0-63  Version of generating product              */
#define LDR_PDT_TCM     88  /*   11   0-63  Comments                                   */

/*----------------------------------------------------------------------------
    BRT - Block Relocation Table

    The BRT contains information that enables the loader to relocate relative
    addresses within a program. Any number of BRT entries can appear after the
    heading. In the standard BRT format, there are two BRT entries per word. In
    the extended BRT format, there is one BRT entry per word.

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_BRT_TT       0  /*    0   0- 3  Table type                                 */
#define LDR_BRT_WC       0  /*    0   4-27  Word count including header                */
#define LDR_BRT_X        3  /*    0     28  Format bit:                                */
                            /*                0 - Standard BRT format                  */
                            /*                1 - Extended BRT format                  */
#define LDR_BRT_BI       4  /*    0  32-38  Block index of destination                 */

/*----------------------------------------------------------------------------
    Standard BRT format (2 entries per word)

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_BRT_EBI1     0  /*    0   0- 6  Block index                                */
#define LDR_BRT_EQ1      0  /*    0      7  Relocation mode                            */
                            /*              0 = Word                                   */
                            /*              1 = Parcel                                 */
#define LDR_BRT_EWA1     1  /*    0   8-31  Parcel address to be modified              */
#define LDR_BRT_EBI2     4  /*    0  32-38  Block index                                */
#define LDR_BRT_EQ2      4  /*    0     39  Relocation mode                            */
#define LDR_BRT_EWA3     5  /*    0  40-63  Parcel address to be modified              */

/*----------------------------------------------------------------------------
    Extended BRT format (1 entry per word)

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_BRT_XBI      2  /*    0  19-25  Block index of destination                 */
#define LDR_BRT_XW       3  /*    0  26-31  Width of field to be modified              */
#define LDR_BRT_XQ       4  /*    0     32  Relocation mode:                           */
                            /*              0 = Word                                   */
                            /*              1 = Quarter word                           */
#define LDR_BRT_XN       4  /*    0     33  Negative bit:                              */
                            /*              0 = Value passed is positive               */
                            /*              1 = Value passed is negative               */
#define LDR_BRT_XBA      4  /*    0  34-63  Bit address                                */

/*----------------------------------------------------------------------------
    DF - Directory File

    COS has a utility named BUILD. BUILD is a utility program for creating and
    maintaining library datasets. BUILD generates a directory file consisting of
    a 1-word header followed by a variable-length entry for each program in the
    library.

    Any of the three sets of names (block, entry, or external) can be null. Entry
    names correspond to names of main programs and subroutines and to names of
    any labeled common blocks that are initialized by DATA statements in FORTRAN
    programs.

    Directory header word

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_DF_TT        0  /*    0   0- 3  Table type                                 */
#define LDR_DF_WC        2  /*    0  16-39  Directory's word count                     */
#define LDR_DF_ID        5  /*    0  40-63  'D01' in ASCII                             */

/*----------------------------------------------------------------------------
    Directory table (BUILD) entry

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_BD_TYPE      0  /*    0   0- 3  Entry type:                                */
                            /*                  0 = Absolute binary                    */
                            /*                  1 = Relocatable binary                 */
                            /*                  2 = Control statement processor        */
                            /*                  3 = Data or undefined                  */
#define LDR_BD_EWC       0  /*    0   4-24  Word count of entry, maximum 66,048        */
#define LDR_BD_XL        3  /*    0  25-39  Number of external names                   */
#define LDR_BD_EL        5  /*    0  40-54  Number of entry names                      */
#define LDR_BD_BL        6  /*    0  55-63  Number of block names                      */
#define LDR_BD_FN        8  /*    1   0-63  8-character name of program module         */
#define LDR_BD_STAT     16  /*    2   0- 3  Entry status                               */
#define LDR_BD_LM       16  /*    2      4  Load module flag (LDR set and used)        */
#define LDR_BD_FWC      16  /*    2   5-30  Program module's maximum word count        */
#define LDR_BD_FWA      19  /*    2  31-63  Program module's location                  */
#define LDR_BD_ENT      24  /*  3-n   0-63  Directory entry blocks, entry names,       */
                            /*              externals                                  */

/*----------------------------------------------------------------------------
    DMT - Debug Map Table

    The DMT is a binary version of the load map that the loader produces.
    Subsequent products or job steps, such as a debugger, use DMT.

    Debug Map Table Header

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_DM_TT        0  /*    0   0- 3  Table type (7)                             */
#define LDR_DM_WC        0  /*    0   4-27  Table word count                           */
#define LDR_DM_XFER      5  /*    0  40-63  Transfer address (0 if not used)           */
#define LDR_DM_OVF       8  /*    1      0  Overlay flag; set if overlays exist        */
#define LDR_DM_ONAM     16  /*    2   0-63  Overlay name in ASCII                      */

/*----------------------------------------------------------------------------
    Debug Map Table entry

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_DM_CBF       0  /*    0      0  Common block flag                          */
#define LDR_DM_BNAM      0  /*    0   1-63  Block name in ASCII                        */
#define LDR_DM_TSCM      8  /*    1      0  Task common flag                           */
#define LDR_DM_BFIL      8  /*    1   1- 9  Number of words of fill                    */
#define LDR_DM_BDI       9  /*    1  10-15  Dataset index                              */
#define LDR_DM_BLEN     10  /*    1  16-39  Length of the block in words               */
#define LDR_DM_BFWA     13  /*    1  40-63  First word address of the block            */

/*----------------------------------------------------------------------------
    DPT - Duplication Table

    The DPT duplicates a word a given number of times. It provides a compact
    form for expressing a large number of words at load time without requiring
    a correspondingly large number of words in the relocatable dataset.

    Duplication Table Header

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_DPT_TT       0  /*    0   0- 3  Table type (O'13)                          */
#define LDR_DPT_WC       0  /*    0   4-27  Word count including header                */

/*----------------------------------------------------------------------------
    Duplication Table entry

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_DPT_INC      0  /*    0   0- 7  Increment between stores of source word    */
#define LDR_DPT_NDP      1  /*    0   8-31  Number of times word at SWA is duplicated  */
#define LDR_DPT_BI       4  /*    0  32-38  Block index                                */
#define LDR_DPT_SWA      5  /*    0  40-63  Source word address                        */

/*----------------------------------------------------------------------------
    PWT - Partial Word Table

    The PWT contains data from the program to be loaded. The data is specified
    as a starting bit of a word and a number of bits. The loading can cross
    boundaries.

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_PWT_TT       0  /*    0   0- 3  Table type (6)                             */
#define LDR_PWT_WC       0  /*    0   4-27  Table word count                           */
#define LDR_PWT_BI       4  /*    0  32-38  Block index                                */
#define LDR_PWT_Q        4  /*    0     39  Relocation mode (always 0)                 */
#define LDR_PWT_LA       5  /*    0  40-63  Relative laod address in block BI          */
#define LDR_PWT_BL      11  /*    1  28-57  Number of bits to be loaded                */
#define LDR_PWT_BO      15  /*    1  58-63  Bit offset; leftmost bit of field to load  */
#define LDR_PWT_W       16  /*  2-n   0-63  Text words to be loaded                    */

/*----------------------------------------------------------------------------
    SMT - Symbol Table

    A relocatable file can contain symbol table information for each program
    unit in a compilation. The information is a sequence of tables of type 11.
    The sequence always includes a subroutine table and can include one or more
    common block tables. The Subroutine Table contains information about the
    subroutine block, the common block(s) referenced by the subroutine, and the
    local symbols.

    Subroutine Table Header

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_SMT_TT       0  /*    0   0- 3  Table type (O'11)                          */
#define LDR_SMT_WC       0  /*    0   4-27  Table word count                           */
#define LDR_SMT_BI       4  /*    0  32-38  Block index                                */
#define LDR_SMT_CML      4  /*    0  39-46  Length in words of table header            */
                            /*                 Always 3 for a common block             */
#define LDR_SMT_DBF      8  /*    1      0  Dynamic block flag:                        */
                            /*                0  Static                                */
                            /*                1  Dynamic                               */
#define LDR_SMT_SYL      8  /*    1   1-16  Symbol block length (words)                */
#define LDR_SMT_DIL      9  /*    1  17-31  Dimension block length                     */
#define LDR_SMT_PL      12  /*    1  32-39  Prologue length (parcel)                   */
#define LDR_SMT_PEA     13  /*    1  40-63  Primary entry address (parcel)             */
#define LDR_SMT_BS      13  /*    1  40-63  Block size (words) of named common block   */
#define LDR_SMT_SN      24  /*    2   0-63  Subroutine name                            */
#define LDR_SMT_NAM     32  /*  3-n   0-63  Name(s) of common block(s)                 */
#define LDR_SMT_CNM     32  /*    3   0-63  Name of common block                       */

/*----------------------------------------------------------------------------
    Symbol Descriptor Format

    Words CML through CML+SYL-1 contain descriptors of local symbolx. Each
    descriptor may be 3 through 6 words long, depending on the symbol length.
    In addition, a dimensioned variable symbol points to a group of words in
    the dimension block for its dimension information.

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_SMT_SL       0  /*    0   0- 1  Symbol name length-1 in words              */
#define LDR_SMT_ST       0  /*    0   2- 6  Symbol type:                               */
                            /*                0  Unknown                               */
                            /*                1  Program (external)                    */
                            /*                2  Entry point                           */
                            /*                3  Label                                 */
                            /*                4  Integer                               */
                            /*                5  Real                                  */
                            /*                6  Complex                               */
                            /*                7  Logical                               */
                            /*                8  Character                             */
                            /*                9  Bit (Boolean)                         */
                            /*               10  File                                  */
                            /*               11  Pointer                               */
                            /*               12  DP Integer                            */
                            /*               13  DP Real                               */
                            /*               14  DP Complex                            */
                            /*               15  Structure                             */
                            /*               16  Address                               */
#define LDR_SMT_CL       0  /*    0   7-10  Symbol class:                              */
                            /*                0  Constant                              */
                            /*                1  Register                              */
                            /*                2  Normal                                */
                            /*                3  Stack                                 */
                            /*                4  Based pointer                         */
                            /*                5  Based descriptor                      */
#define LDR_SMT_DA       1  /*    0     11  Dummy argument (parameter) flag            */
#define LDR_SMT_AM       1  /*    0     12  Argument mode:                             */
                            /*                0  Address                               */
                            /*                1  Value                                 */
#define LDR_SMT_EQ       1  /*    0     13  Equivalence flag                           */
#define LDR_SMT_PSI      1  /*    0  14-29  Parent symbol index                        */
#define LDR_SMT_ND       3  /*    0  30-32  Number of dimensions                       */
#define LDR_SMT_ASM      4  /*    0     33  Array storage mode:                        */
                            /*                0  By column                             */
                            /*                1  By row                                */
#define LDR_SMT_DBI      4  /*    0  34-48  Dimension block index                      */
#define LDR_SMT_EL       6  /*    0  49-63  Element length (in bits)                   */
#define LDR_CL_DEP       8  /*    1   0-63  Symbol class-dependent information:        */
#define LDR_SMT_SNM     16  /*    2      0  Symbol name; 1-4 words                     */

/*----------------------------------------------------------------------------
    Word S2 for class 0 (constant value)

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_SMT_SVL      8  /*    1   0-63                                             */

/*----------------------------------------------------------------------------
    Word S2 for class 1 (register)

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_SMT_RT      12  /*    1  34-37  Register type:                             */
                            /*                1 A                                      */
                            /*                2 B                                      */
                            /*                3 S                                      */
                            /*                4 T                                      */
                            /*                5 V                                      */
                            /*                6 Special                                */
#define LDR_SMT_RN      12  /*    1  38-47  Register number of subtype                 */

/*----------------------------------------------------------------------------
    Word S2 for classes 2-5

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_SMT_BI2      8  /*    1   3- 9  Block index                                */
#define LDR_SMT_SSL      9  /*    1  10-33  Symbol storage length (words occupied)     */
#define LDR_SMT_BO      12  /*    1  34-63  Bit offset                                 */

/*----------------------------------------------------------------------------
    The dimension descriptor portion fo the Subroutine or Common Block Table
    contains a dimension descriptor for each dimensioned variable symbol. Each
    descriptor consists of an n-word entry, where n is the dimension of the 
    variable.

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_SMT_LDE      1  /*    0     10  Lower dimension expression                 */
#define LDR_SMT_LDI      1  /*    0     11  Lower dimension indirect                   */
#define LDR_SMT_LD       2  /*    0  12-35  Lower dimension                            */
#define LDR_SMT_UDE      4  /*    0     38  Upper dimension expression                 */
#define LDR_SMT_UDI      4  /*    0     39  Upper dimension indirect                   */
#define LDR_SMT_UD       5  /*    0  40-63  Upper dimension                            */

/*----------------------------------------------------------------------------
    TXT - Text Table

    The TXT contains the code or data of the program to be loaded.

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_TXT_TT       0  /*    0   0- 3  Table type (O'16)                          */
#define LDR_TXT_WC       0  /*    0   4-27  Table word count                           */
#define LDR_TXT_BI       4  /*    0  32-38  Block index                                */
#define LDR_TXT_Q        4  /*    0     39  Relocation mode (always 0)                 */
#define LDR_TXT_LA       5  /*    0  40-63  Relative load address in block BI          */
#define LDR_TXT_W        8  /*  1-n   0-63  Text words to be loaded                    */

/*----------------------------------------------------------------------------
    XRT - Text Table

    The XRT contains information that enables the loader to relocate external
    references.

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_XRT_TT       0  /*    0   0- 3  Table type (O'14)                          */
#define LDR_XRT_WC       0  /*    0   4-27  Table word count                           */

/*----------------------------------------------------------------------------
    XRT entry

         Field        Byte     Word   Bits  Description
         ----------   ----     ----  -----  ------------------------------------------ */
#define LDR_XRT_BI       0  /*    0   6-12  Block index                                */
#define LDR_XRT_Q        1  /*    0     13  Q flag:                                    */
                            /*                0  Word address                          */
                            /*                1  Parcel address                        */
#define LDR_XRT_XI       1  /*    0  14-27  External index                             */
#define LDR_XRT_L        3  /*    0  28-33  Length of bits of the relocation field     */
#define LDR_XRT_BA       4  /*    0  34-63  Bit address within (BI) of rightmost bit   */
                            /*              to be modified                             */

#endif
