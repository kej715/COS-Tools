/*--------------------------------------------------------------------------
**
**  Copyright 2024 Kevin E. Jordan
**
**  Name: cmpstr.c
**
**  Description:
**      This file contains a function that compares one character string
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

int _cmpstr(unsigned long s1, unsigned long s2) {
    int res;
    int s1Len;
    int s2Len;
    char *s1p;
    char *s2p;

    s1Len = s1 >> 32;
    s1p = (char *)(s1 & 0xffffffff);
    s2Len = s2 >> 32;
    s2p = (char *)(s2 & 0xffffffff);
    res = 0;
    while (res == 0 && s1Len > 0 && s2Len > 0) {
        res = *s1p++ - *s2p++;
        s1Len -= 1;
        s2Len -= 1;
    }
    if (res == 0) {
        if (s1Len == 0)
            res = -1;
        else if (s2Len == 0)
            res = 1;
    }
    return res;
}
