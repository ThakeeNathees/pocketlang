/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef LIBS_H
#define LIBS_H

#ifndef PK_AMALGAMATED
#include <pocketlang.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

/*****************************************************************************/
/* MODULES INTERNAL                                                          */
/*****************************************************************************/

// We're re defining some core internal macros here which should be considered
// as macro re-definition in amalgamated source, so we're skipping it for
// amalgumated build.
#ifndef PK_AMALGAMATED

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
      fprintf(stderr, "Assertion failed: %s\n\tat %s() (%s:%i)\n",   \
        message, __func__, __FILE__, __LINE__);                      \
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

// Using __ASSERT() for make it crash in release binary too.
#define TODO __ASSERT(false, "TODO: It hasn't implemented yet.")
#define OOPS "Oops a bug!! report please."

// Returns the docstring of the function, which is a static const char* defined
// just above the function by the DEF() macro below.
#define DOCSTRING(fn) __doc_##fn

// A macro to declare a function, with docstring, which is defined as
// ___doc_<fn> = docstring; That'll used to generate function help text.
#define DEF(fn, docstring)                      \
  static const char* DOCSTRING(fn) = docstring; \
  static void fn(PKVM* vm)

#endif // PK_AMALGAMATED

/*****************************************************************************/
/* SHARED FUNCTIONS                                                          */
/*****************************************************************************/

// These are "public" module functions that can be shared. Since some modules
// can be used for cli's internals we're defining such functions here and they
// will be imported in the cli.

// The pocketlang's import statement path resolving function. This
// implementation is required by pockelang from it's hosting application
// inorder to use the import statements.
char* pathResolveImport(PKVM * vm, const char* from, const char* path);

// Register all the the libraries to the PKVM.
void registerLibs(PKVM* vm);

// Cleanup registered libraries call this only if the libraries were registered
// with registerLibs() function.
void cleanupLibs(PKVM* vm);

#endif // LIBS_H
