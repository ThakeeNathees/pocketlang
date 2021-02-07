/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef MS_COMMON_H
#define MS_COMMON_H

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// miniscript visibility macros. define MS_DLL for using miniscript as a 
// shared library and define MS_COMPILE to export symbols.

#ifdef _MSC_VER
  #define _MS_EXPORT __declspec(dllexport)
  #define MS_IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
  #define _MS_EXPORT __attribute__((visibility ("default")))
  #define _MS_IMPORT
#else
  #define _MS_EXPORT
  #define _MS_IMPORT
#endif

#ifdef MS_DLL
  #ifdef MS_COMPILE
    #define MS_PUBLIC _MS_EXPORT
  #else
    #define MS_PUBLIC _MS_IMPORT
  #endif
#else
  #define MS_PUBLIC
#endif

// Unique number to identify for various cases.
typedef uint32_t ID;

// Nan-Tagging could be disable for debugging/portability purposes.
// To disable define `VAR_NAN_TAGGING 0`, otherwise it defaults to Nan-Tagging.
#ifndef VAR_NAN_TAGGING
  #define VAR_NAN_TAGGING 1
#endif

#if VAR_NAN_TAGGING
typedef uint64_t Var;
#else
typedef struct Var Var;
#endif

typedef struct Object Object;
typedef struct String String;
typedef struct Array Array;
typedef struct Range Range;

typedef struct Script Script;
//typedef struct Class Class;
typedef struct Function Function;

#ifdef DEBUG

#include <stdio.h>

#define ASSERT(condition, message)                                       \
    do {                                                                 \
        if (!(condition)) {                                              \
            fprintf(stderr, "Assertion failed: %s\n\tat %s() (%s:%i)\n", \
                message, __func__, __FILE__, __LINE__);                  \
            abort();                                                     \
        }                                                                \
    } while (false)

#define UNREACHABLE()                                                    \
    do {                                                                 \
        fprintf(stderr, "Execution reached an unreachable path\n"        \
            "\tat %s() (%s:%i)\n", __FILE__, __LINE__, __func__);        \
        abort();                                                         \
    } while (false)

#else

#define ASSERT(condition, message) do { } while (false)

// Reference : https://github.com/wren-lang/
#if defined( _MSC_VER )
  #define UNREACHABLE() __assume(0)
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
  #define UNREACHABLE() __builtin_unreachable()
#else
  #define UNREACHABLE()
#endif

#endif // DEBUG

// Allocate object of [type] using the vmRealloc function.
#define ALLOCATE(vm, type) \
    ((type*)vmRealloc(vm, NULL, 0, sizeof(type)))

// Allocate object of [type] which has a dynamic tail array of type [tail_type]
// with [count] entries.
#define ALLOCATE_DYNAMIC(vm, type, count, tail_type) \
    ((type*)vmRealloc(vm, NULL, 0, sizeof(type) + sizeof(tail_type) * (count)))

#endif //MS_COMMON_H
