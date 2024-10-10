/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: catstr.c
**
**  Description:
**      This file contains a function that concatenates one character string
**      onto another.
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

extern unsigned long _getstr(int size);

unsigned long _catstr(unsigned long s1, unsigned long s2) {
    unsigned long res;
    int s1Len;
    int s2Len;
    char *s;
    char *s1p;
    char *s2p;

    s1Len = s1 >> 32;
    s1p = (char *)(s1 & 0xffffffff);
    s2Len = s2 >> 32;
    s2p = (char *)(s2 & 0xffffffff);
    res = _getstr(s1Len + s2Len);
    s = (char *)(res & 0xffffffff);
    while (s1Len-- > 0) *s++ = *s1p++;
    while (s2Len-- > 0) *s++ = *s2p++;

    return res;
}
