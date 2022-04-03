/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_INTERNAL
#define PK_INTERNAL

#include "include/pocketlang.h"

#include "pk_common.h"

// Commonly used c standard headers across the sources. Don't include any
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

/*****************************************************************************/
/* INTERNAL CONFIGURATIONS                                                   */
/*****************************************************************************/

// Set this to dump compiled opcodes of each functions.
#define DEBUG_DUMP_COMPILED_CODE 0

// Set this to dump stack frame before executing the next instruction.
#define DEBUG_DUMP_CALL_STACK 1

// Nan-Tagging could be disable for debugging/portability purposes. See "var.h"
// header for more information on Nan-tagging.
#define VAR_NAN_TAGGING 1

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
#define DEALLOCATE(vm, pointer) \
    vmRealloc(vm, pointer, 0, 0)

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

#endif // PK_INTERNAL
