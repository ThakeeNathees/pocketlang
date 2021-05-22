/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef PK_COMMON_H
#define PK_COMMON_H

#include "include/pocketlang.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Set this to dump compiled opcodes of each functions.
#define DEBUG_DUMP_COMPILED_CODE 0

// Set this to dump stack frame before executing the next instruction.
#define DEBUG_DUMP_CALL_STACK 0

#include <stdio.h> //< Only needed for ASSERT() macro and for release mode
                   //< TODO; macro use this to print a crash report.

// The internal assertion macro, this will print error and break regardless of
// the build target (debug or release). Use ASSERT() for debug assertion and
// use __ASSERT() for TODOs and assetion's in public methods (to indicate that
// the host application did something wrong).
#define __ASSERT(condition, message)                                 \
  do {                                                               \
    if (!(condition)) {                                              \
      fprintf(stderr, "Assertion failed: %s\n\tat %s() (%s:%i)\n",   \
        message, __func__, __FILE__, __LINE__);                      \
      DEBUG_BREAK();                                                 \
      abort();                                                       \
    }                                                                \
  } while (false)

#ifdef DEBUG

#ifdef _MSC_VER
  #define DEBUG_BREAK() __debugbreak()
#else
  #define DEBUG_BREAK() __builtin_trap()
#endif

#define ASSERT(condition, message) __ASSERT(condition, message)

#define ASSERT_INDEX(index, size) \
  ASSERT(index >= 0 && index < size, "Index out of bounds.")

#define UNREACHABLE()                                                \
  do {                                                               \
    fprintf(stderr, "Execution reached an unreachable path\n"        \
      "\tat %s() (%s:%i)\n", __func__, __FILE__, __LINE__);          \
    abort();                                                         \
  } while (false)

#else

#define DEBUG_BREAK()
#define ASSERT(condition, message) do { } while (false)
#define ASSERT_INDEX(index, size) do {} while (false)

// Reference : https://github.com/wren-lang/
#if defined( _MSC_VER )
  #define UNREACHABLE() __assume(0)
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
  #define UNREACHABLE() __builtin_unreachable()
#else
  #define UNREACHABLE()
#endif

#endif // DEBUG

// Using __ASSERT() for make it crash in release binary too.
#define TODO __ASSERT(false, "TODO: It hasn't implemented yet.")
#define OOPS "Oops a bug!! report plese."

#define STRINGIFY(x) TOSTRING(x)
#define TOSTRING(x) #x
#define M_CONC(a, b) M_CONC_(a, b)
#define M_CONC_(a, b) a##b

// The factor by which a buffer will grow when it's capacity reached.
#define GROW_FACTOR 2

// The initial capacity of a buffer.
#define MIN_CAPACITY 8

// Allocate object of [type] using the vmRealloc function.
#define ALLOCATE(vm, type) \
    ((type*)vmRealloc(vm, NULL, 0, sizeof(type)))

// Allocate object of [type] which has a dynamic tail array of type [tail_type]
// with [count] entries.
#define ALLOCATE_DYNAMIC(vm, type, count, tail_type) \
    ((type*)vmRealloc(vm, NULL, 0, sizeof(type) + sizeof(tail_type) * (count)))

// Allocate [count] ammount of object of [type] array.
#define ALLOCATE_ARRAY(vm, type, count) \
    ((type*)vmRealloc(vm, NULL, 0, sizeof(type) * (count)))

// Deallocate a pointer allocated by vmRealloc before.
#define DEALLOCATE(vm, pointer) \
    vmRealloc(vm, pointer, 0, 0)

// Unique number to identify for various cases.
typedef uint32_t ID;

#if VAR_NAN_TAGGING
  typedef uint64_t Var;
#else
  typedef struct Var Var;
#endif

typedef struct Object Object;
typedef struct String String;
typedef struct List List;
typedef struct Map Map;
typedef struct Range Range;
typedef struct Script Script;
typedef struct Function Function;
typedef struct Fiber Fiber;

#endif //PK_COMMON_H
