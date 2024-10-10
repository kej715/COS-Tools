#ifndef CALCONST_H
#define CALCONST_H
/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: calconst.h
**
**  Description:
**      This file defines constants used by the assembler.
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

#define ARG_STACK_SIZE           100
#define BASE_STACK_SIZE          100
#define BLOCK_STACK_SIZE         100
#define COLUMN_LIMIT             72
#define EDIT_CONTROL_STACK_SIZE  100
#define EXTERN_TABLE_INCREMENT   100
#define FALSE                    0
#define IMAGE_INCREMENT          4096
#define LIST_CONTROL_STACK_SIZE  100
#define MACRO_STACK_SIZE         100
#define MASK6                    077
#define MASK9                    0777
#define MASK22                   017777777
#define MASK24                   077777777
#define MAX_ERROR_INDICATIONS    7
#define MAX_FILE_PATH_LENGTH     256
#define MAX_LOCAL_SYMBOLS        10
#define MAX_NAME_LENGTH          8
#define MAX_SOURCE_LINE_LENGTH   90
#define MAX_TITLE_LENGTH         64
#define OP_STACK_SIZE            100
#define QUALIFIER_STACK_SIZE     100
#define RELOC_TABLE_INCREMENT    200
#define SOURCE_FORMAT_STACK_SIZE 100
#define TRUE                     1

#endif
