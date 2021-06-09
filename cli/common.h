/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include <pocketlang.h>

#define CLI_NOTICE                                                            \
  "PocketLang " PK_VERSION_STRING " (https://github.com/ThakeeNathees/pocketlang/)\n" \
  "Copyright(c) 2020 - 2021 ThakeeNathees.\n"                                 \
  "Free and open source software under the terms of the MIT license.\n"

// Note that the cli itself is not a part of the pocketlang compiler, instead
// its a host application to run pocketlang from the command line. We're
// embedding the pocketlang VM and we can only use its public APIs, not any
// internals of it, including assertion macros. So we're re-defining those
// macros here (like if it's a new project).

/*****************************************************************************/
/* COMMON MACROS                                                             */
/*****************************************************************************/

// These macros below are copied from pocketlang at "src/common.h". See above
// for not re-using these macros.

// The internal assertion macro, this will print error and break regardless of
// the build target (debug or release). Use ASSERT() for debug assertion and
// use __ASSERT() for TODOs and assetions in public methods (to indicate that
// the host application did something wrong).
#define __ASSERT(condition, message)                               \
  do {                                                             \
    if (!(condition)) {                                            \
      fprintf(stderr, "Assertion failed: %s\n\tat %s() (%s:%i)\n", \
        message, __func__, __FILE__, __LINE__);                    \
      DEBUG_BREAK();                                               \
      abort();                                                     \
    }                                                              \
  } while (false)

#define NO_OP do {} while (false)

#ifdef DEBUG

#ifdef _MSC_VER
  #define DEBUG_BREAK() __debugbreak()
#else
  #define DEBUG_BREAK() __builtin_trap()
#endif

#define ASSERT(condition, message) __ASSERT(condition, message)

#define ASSERT_INDEX(index, size) \
  ASSERT(index >= 0 && index < size, "Index out of bounds.")

#define UNREACHABLE()                                         \
  do {                                                        \
    fprintf(stderr, "Execution reached an unreachable path\n" \
      "\tat %s() (%s:%i)\n", __func__, __FILE__, __LINE__);   \
    abort();                                                  \
  } while (false)

#else

#define DEBUG_BREAK() NO_OP
#define ASSERT(condition, message) NO_OP
#define ASSERT_INDEX(index, size) NO_OP

// Reference : https://github.com/wren-lang/
#if defined( _MSC_VER )
  #define UNREACHABLE() __assume(0)
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
  #define UNREACHABLE() __builtin_unreachable()
#else
  #define UNREACHABLE() NO_OP
#endif

#endif // DEBUG

#if defined( _MSC_VER )
  #define forceinline __forceinline
#else
  #define forceinline __attribute__((always_inline))
#endif

// Using __ASSERT() for make it to crash in release binary too.
#define TODO __ASSERT(false, "TODO: It hasn't been implemented yet.")
#define OOPS "Oops a bug!! report please."

#define TOSTRING(x) #x
#define STRINGIFY(x) TOSTRING(x)

/*****************************************************************************/
/* CLI DEFINES                                                               */
/*****************************************************************************/

 // FIXME: the vm user data of cli.
typedef struct {
  bool repl_mode;
} VmUserData;

#endif // COMMON_H
