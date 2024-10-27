#ifndef LDRCONST_H
#define LDRCONST_H
/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: ldrconst.h
**
**  Description:
**      This file defines constants used by the loader.
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

#define EXTERN_TABLE_INCREMENT   100
#define FALSE                    0
#define IMAGE_INCREMENT          4096
#define MAX_FILE_PATH_LENGTH     256
#define MAX_LIBRARIES            64
#define MAX_SOURCE_FILES         128
#define RELOC_TABLE_INCREMENT    200
#define TRUE                     1

#define isSet(word, bitnum) (((word) >> ((63-(bitnum)))) & 1)

#endif
