/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

// A collection of reusable macros that pocketlang use. This file doesn't have
// any dependencies, you can just drag and drop this file in your project if
// you want to use these macros.

#ifndef PK_COMMON_H
#define PK_COMMON_H

#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif

#include <stdio.h> //< Only needed here for ASSERT() macro and for release mode
                   //< TODO; macro use this to print a crash report.

#define TOSTRING(x) #x
#define STRINGIFY(x) TOSTRING(x)

// CONCAT_LINE(X) will result evaluvated X<__LINE__>.
#define __CONCAT_LINE_INTERNAL_R(a, b) a ## b
#define __CONCAT_LINE_INTERNAL_F(a, b) __CONCAT_LINE_INTERNAL_R(a, b)
#define CONCAT_LINE(X) __CONCAT_LINE_INTERNAL_F(X, __LINE__)

// The internal assertion macro, this will print error and break regardless of
// the build target (debug or release). Use ASSERT() for debug assertion and
// use __ASSERT() for TODOs.
#define __ASSERT(condition, message)                                 \
  do {                                                               \
    if (!(condition)) {                                              \
      fprintf(stderr, "Assertion failed: %s\n\tat %s() (%s:%i)\n"    \
                      "\tcondition: %s",                             \
        message, __func__, __FILE__, __LINE__, #condition);          \
      DEBUG_BREAK();                                                 \
      abort();                                                       \
    }                                                                \
  } while (false)

#define NO_OP do {} while (false)

#ifdef DEBUG

#ifdef _MSC_VER
  #define DEBUG_BREAK() __debugbreak()
#else
  #define DEBUG_BREAK() __builtin_trap()
#endif

// This will terminate the compilation if the [condition] is false, because of
// char _assertion_failure_<__LINE__>[-1] evaluated.
#define STATIC_ASSERT(condition) \
  static char CONCAT_LINE(_assertion_failure_)[2*!!(condition) - 1]

#define ASSERT(condition, message) __ASSERT(condition, message)

#define ASSERT_INDEX(index, size) \
  ASSERT(index >= 0 && index < size, "Index out of bounds.")

#define UNREACHABLE()                                                \
  do {                                                               \
    fprintf(stderr, "Execution reached an unreachable path\n"        \
      "\tat %s() (%s:%i)\n", __func__, __FILE__, __LINE__);          \
    DEBUG_BREAK();                                                   \
    abort();                                                         \
  } while (false)

#else

#define STATIC_ASSERT(condition) NO_OP

#define DEBUG_BREAK() NO_OP
#define ASSERT(condition, message) NO_OP
#define ASSERT_INDEX(index, size) NO_OP

#if defined(_MSC_VER)
  #define UNREACHABLE() __assume(0)
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
  #define UNREACHABLE() __builtin_unreachable()
#elif defined(__EMSCRIPTEN__) || defined(__clang__)
  #if __has_builtin(__builtin_unreachable)
    #define UNREACHABLE() __builtin_unreachable()
  #else
    #define UNREACHABLE() NO_OP
  #endif
#else
  #define UNREACHABLE() NO_OP
#endif

#endif // DEBUG

#if defined(_MSC_VER)
  #define forceinline __forceinline
#else
  #define forceinline __attribute__((always_inline))
#endif

// To use dynamic variably-sized struct with a tail array add an array at the
// end of the struct with size DYNAMIC_TAIL_ARRAY. This method was a legacy
// standard called "struct hack".
#if defined(_MSC_VER) || __STDC_VERSION__ >= 199901L // stdc >= c99
  #define DYNAMIC_TAIL_ARRAY
#else
  #define DYNAMIC_TAIL_ARRAY 0
#endif

// Using __ASSERT() for make it crash in release binary too.
#define TODO __ASSERT(false, "TODO: It hasn't implemented yet.")
#define OOPS "Oops a bug!! report please."

// Returns the docstring of the function, which is a static const char* defined
// just above the function by the DEF() macro below.
#define DOCSTRING(fn) _pk_doc_##fn

// A macro to declare a function, with docstring, which is defined as
// _pk_doc_<fn> = docstring; That'll used to generate function help text.
// [signature] is the function name and parameter names with type information.
// ex: `io.open(path:String, mode:String) -> io.File`
#define DEF(fn, signature, docstring)                            \
  static const char* DOCSTRING(fn) = signature "\n\n" docstring; \
  static void fn(PKVM* vm)

#endif //PK_COMMON_H
