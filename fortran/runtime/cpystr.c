/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: cpystr.c
**
**  Description:
**      This file contains a function that copies one character string
**      to another.
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

void _cpystr(unsigned long to, unsigned long from) {
    int fromLen;
    char *fp;
    int toLen;
    char *tp;

    fromLen = from >> 32;
    fp = (char *)(from & 0xffffffff);
    toLen = to >> 32;
    tp = (char *)(to & 0xffffffff);
    while (fromLen > 0 && toLen > 0) {
        *tp++ = *fp++;
        fromLen -= 1;
        toLen -= 1;
    }
    while (toLen > 0) {
        *tp++ = ' ';
        toLen -= 1;
    }
}
