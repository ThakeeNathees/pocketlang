/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_INTERNAL
#define PK_INTERNAL

#ifndef PK_AMALGAMATED
#include <pocketlang.h>
#include "common.h"
#endif

// Commonly used C standard headers across the sources. Don't include any
// headers that are specific to a single source here, instead include them in
// their source files explicitly (can not be implicitly included by another
// header). And don't include any C standard headers in any of the pocketlang
// headers.
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// __STDC_LIMIT_MACROS and __STDC_CONSTANT_MACROS are a workaround to
// allow C++ programs to use stdint.h macros specified in the C99
// standard that aren't in the C++ standard.
#define __STDC_LIMIT_MACROS
#include <stdint.h>

#if defined(__GNUC__)
  #pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
  #pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(__clang__)
  #pragma clang diagnostic ignored "-Wint-to-pointer-cast"
  #pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
  #pragma warning(disable:26812)
#endif

/*****************************************************************************/
/* INTERNAL CONFIGURATIONS                                                   */
/*****************************************************************************/

// Set this to dump compiled opcodes of each functions.
#define DUMP_BYTECODE 0

// Dump the stack values and the globals.
#define DUMP_STACK 0

// Nan-Tagging could be disable for debugging/portability purposes. See "var.h"
// header for more information on Nan-tagging.
#define VAR_NAN_TAGGING 1

// The maximum size of the pocketlang stack. This value is arbitrary. currently
// it's 800 KB. Change this to any value upto 2147483647 (signed integer max)
// if you want.
#define MAX_STACK_SIZE 1024 * 800

// The maximum number of argument a pocketlang function supported to call. This
// value is arbitrary and feel free to change it. (Just used this limit for an
// internal buffer to store values before calling a new fiber).
#define MAX_ARGC 32

// The factor by which a buffer will grow when it's capacity reached.
#define GROW_FACTOR 2

// The initial minimum capacity of a buffer to allocate.
#define MIN_CAPACITY 8

// The size of the error message buffer, used ar vsnprintf (since c99) buffer.
#define ERROR_MESSAGE_SIZE 512

/*****************************************************************************/
/* ALLOCATION MACROS                                                         */
/*****************************************************************************/

// Allocate object of [type] using the vmRealloc function.
#define ALLOCATE(vm, type) \
  ((type*)vmRealloc(vm, NULL, 0, sizeof(type)))

// Allocate object of [type] which has a dynamic tail array of type [tail_type]
// with [count] entries.
#define ALLOCATE_DYNAMIC(vm, type, count, tail_type) \
  ((type*)vmRealloc(vm, NULL, 0, sizeof(type) + sizeof(tail_type) * (count)))

// Allocate [count] amount of object of [type] array.
#define ALLOCATE_ARRAY(vm, type, count) \
  ((type*)vmRealloc(vm, NULL, 0, sizeof(type) * (count)))

// Deallocate a pointer allocated by vmRealloc before.
#define DEALLOCATE(vm, pointer, type) \
  vmRealloc(vm, pointer, sizeof(type), 0)

// Deallocate object of [type] which has a dynamic tail array of type
// [tail_type] with [count] entries.
#define DEALLOCATE_DYNAMIC(vm, pointer, type, count, tail_type) \
  ((type*)vmRealloc(vm, pointer,                                \
    sizeof(type) + sizeof(tail_type) * (count), 0))

// Deallocate [count] amount of object of [type] array.
#define DEALLOCATE_ARRAY(vm, pointer, type, count) \
  ((type*)vmRealloc(vm, pointer, sizeof(type) * (count), 0))

/*****************************************************************************/
/* REUSABLE INTERNAL MACROS                                                  */
/*****************************************************************************/

// Here we're switching the FNV-1a hash value of the name (cstring). Which is
// an efficient way than having multiple if (attrib == "name"). From O(n) * k
// to O(1) where n is the length of the string and k is the number of string
// comparison.
//
// ex:
//     switch (attrib->hash) { // str = "length"
//       case CHECK_HASH("length", 0x83d03615) : { return string->length; }
//     }
//
// In C++11 this can be achieved (in a better way) with user defined literals
// and constexpr. (Reference from my previous compiler written in C++).
// https://github.com/ThakeeNathees/carbon/
//
// However there is a python script that's matching the CHECK_HASH() macro
// calls and validate if the string and the hash values are matching.
// TODO: port it to the CI/CD process at github actions.
//
#define CHECK_HASH(name, hash) hash

// The formated string to convert double to string. It'll be with the minimum
// length string representation of either a regular float or a scientific
// notation (at most 15 decimal points).
// Reference: https://www.cplusplus.com/reference/cstdio/printf/
#define DOUBLE_FMT "%.16g"

// Double number to string buffer size, used in sprintf with DOUBLE_FMT.
//  A largest number : "-1.234567890123456e+308"
// +  1 fot sign '+' or '-'
// + 16 fot significant digits
// +  1 for decimal point '.'
// +  1 for exponent char 'e'
// +  1 for sign of exponent
// +  3 for the exponent digits
// +  1 for null byte '\0'
#define STR_DBL_BUFF_SIZE 24

// Integer number to string buffer size, used in sprintf with format "%d".
// The minimum 32 bit integer = -2147483648
// +  1 for sign '-'
// + 10 for digits
// +  1 for null byte '\0'
#define STR_INT_BUFF_SIZE 12

// Integer number (double) to hex string buffer size.
// The maximum value an unsigned 64 bit integer can get is
// 0xffffffffffffffff which is 16 characters.
// + 16 for hex digits
// +  1 for sign '-'
// +  2 for '0x' prefix
// +  1 for null byte '\0'
#define STR_HEX_BUFF_SIZE 20

// Integer number (double) to bin string buffer size.
// The maximum value an unsigned 64 bit integer can get is 0b11111... 64 1s.
// + 64 for bin digits
// +  1 for sign '-'
// +  2 for '0b' prefix
// +  1 for null byte '\0'
#define STR_BIN_BUFF_SIZE 68

#endif // PK_INTERNAL
