#ifndef BASICTYPES_H
#define BASICTYPES_H
/*--------------------------------------------------------------------------
**
**  Copyright 2021 Kevin E. Jordan
**
**  Name: basetypes.h
**
**  Description:
**      This file contains base types defining numeric types
**      of various lengths.
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
**  Basic data types
*/
#if defined(_WIN32)
    typedef signed char  i8;
    typedef signed short i16;
    typedef signed long  i32;
    typedef signed __int64 i64;
    typedef unsigned char  u8;
    typedef unsigned short u16;
    typedef unsigned long  u32;
    typedef unsigned __int64 u64;
#elif defined (__GNUC__) || defined(__SunOS)
    #if defined(__amd64) || defined(__amd64__) || defined(__alpha__) || defined(__powerpc64__) || defined(__ppc64__) \
        || defined(__APPLE__) || (defined(__sparc64__) && defined(__arch64__))
        /*
        **  64-bit systems
        */
        typedef signed char i8;
        typedef signed short i16;
        typedef signed int i32;
        typedef signed long int i64;
        typedef unsigned char u8;
        typedef unsigned short u16;
        typedef unsigned int u32;
        typedef unsigned long int u64;
    #elif defined(__i386) || defined(__i386__) || defined(__powerpc__) || defined(__ppc__) \
        || defined(__sparc__) || defined(__hppa__) || defined(__arm__)
        /*
        **  32-bit systems
        */
        typedef signed char i8;
        typedef signed short i16;
        typedef signed int i32;
        typedef signed long long int i64;
        typedef unsigned char u8;
        typedef unsigned short u16;
        typedef unsigned int u32;
        typedef unsigned long long int u64;
    #else
        #error "Unable to determine sizes of basic data types"
    #endif
#elif defined(__ACK) && defined(__crayxmp)
    #include <sys/types.h>
    typedef signed char i8;
    typedef signed short i16;
    typedef signed int i32;
    typedef signed long i64;
    typedef unsigned int u32;
#else
    #error "Unable to determine sizes of basic data types"
#endif

typedef double f64;

#if (!defined(__cplusplus) && !defined(bool))
    typedef int bool;
#endif

#if defined(__APPLE__)
    #include <stdbool.h>
#endif

#endif
