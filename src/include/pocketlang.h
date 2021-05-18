/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef MINISCRIPT_H
#define MINISCRIPT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// The version number macros.
#define PK_VERSION_MAJOR 0
#define PK_VERSION_MINOR 1
#define PK_VERSION_PATCH 0

// String representation of the value.
#define PK_VERSION_STRING "0.1.0"

// miniscript visibility macros. define PK_DLL for using miniscript as a 
// shared library and define PK_COMPILE to export symbols.

#ifdef _MSC_VER
  #define _PK_EXPORT __declspec(dllexport)
  #define _PK_IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
  #define _PK_EXPORT __attribute__((visibility ("default")))
  #define _PK_IMPORT
#else
  #define _PK_EXPORT
  #define _PK_IMPORT
#endif

#ifdef PK_DLL
  #ifdef PK_COMPILE
    #define PK_PUBLIC _PK_EXPORT
  #else
    #define PK_PUBLIC _PK_IMPORT
  #endif
#else
  #define PK_PUBLIC
#endif

// MiniScript Virtual Machine.
// it'll contain the state of the execution, stack, heap, and manage memory
// allocations.
typedef struct PKVM PKVM;

// Nan-Tagging could be disable for debugging/portability purposes only when
// compiling the compiler. Do not change this if using the miniscript library
// for embedding. To disable when compiling the compiler, define
// `VAR_NAN_TAGGING 0`, otherwise it defaults to Nan-Tagging.
#ifndef VAR_NAN_TAGGING
  #define VAR_NAN_TAGGING 1
#endif

#if VAR_NAN_TAGGING
  typedef uint64_t Var;
#else
  typedef struct Var Var;
#endif

// A function that'll be called for all the allocation calls by PKVM.
//
// - To allocate new memory it'll pass NULL to parameter [memory] and the
//   required size to [new_size]. On failure the return value would be NULL.
//
// - When reallocating an existing memory if it's grow in place the return
//   address would be the same as [memory] otherwise a new address.
//
// - To free an allocated memory pass [memory] and 0 to [new_size]. The
//   function will return NULL.
typedef void* (*pkReallocFn)(void* memory, size_t new_size, void* user_data);

// C function pointer which is callable from MiniScript.
typedef void (*pkNativeFn)(PKVM* vm);

typedef enum {

  // Compile time errors (syntax errors, unresolved fn, etc).
  PK_ERROR_COMPILE = 0,

  // Runtime error message.
  PK_ERROR_RUNTIME,

  // One entry of a runtime error stack.
  PK_ERROR_STACKTRACE,
} PKErrorType;

// Error callback function pointer. for runtime error it'll call first with
// PK_ERROR_RUNTIME followed by multiple callbacks with PK_ERROR_STACKTRACE.
typedef void (*pkErrorFn) (PKVM* vm, PKErrorType type,
                           const char* file, int line,
                           const char* message);

// A function callback used by `print()` statement.
typedef void (*pkWriteFn) (PKVM* vm, const char* text);

typedef struct pkStringPtr pkStringPtr;

// A function callback symbol for clean/free the pkStringResult.
typedef void (*pkResultDoneFn) (PKVM* vm, pkStringPtr result);

// A string pointer wrapper to pass cstring around with a on_done() callback
// to clean it when the user of the string done with the string.
struct pkStringPtr {
  const char* string;     //< The string result.
  pkResultDoneFn on_done; //< Called once vm done with the string.
  void* user_data;        //< User related data.
};

// A function callback to resolve the import script name from the [from] path
// to an absolute (or relative to the cwd). This is required to solve same
// script imported with different relative path. Set the string attribute to
// NULL to indicate if it's failed to resolve.
typedef pkStringPtr(*pkResolvePathFn) (PKVM* vm, const char* from,
                                       const char* path);

// Load and return the script. Called by the compiler to fetch initial source
// code and source for import statements. Set the string attribute to NULL
// to indicate if it's failed to load the script.
typedef pkStringPtr(*pkLoadScriptFn) (PKVM* vm, const char* path);

// This function will be called once it done with the loaded script only if
// it's corresponding MSLoadScriptResult is succeeded (ie. is_failed = false).
//typedef void (*pkLoadDoneFn) (PKVM* vm, pkStringResult result);

typedef struct {

  // The callback used to allocate, reallocate, and free. If the function
  // pointer is NULL it defaults to the VM's realloc(), free() wrappers.
  pkReallocFn realloc_fn;

  pkErrorFn error_fn;
  pkWriteFn write_fn;

  pkResolvePathFn resolve_path_fn;
  pkLoadScriptFn load_script_fn;

  // User defined data associated with VM.
  void* user_data;

} pkConfiguration;

// Initialize the configuration and set ALL of it's values to the defaults.
// Call this before setting any particular field of it.
PK_PUBLIC void pkInitConfiguration(pkConfiguration* config);

typedef enum {
  PK_RESULT_SUCCESS = 0,
  PK_RESULT_COMPILE_ERROR,
  PK_RESULT_RUNTIME_ERROR,
} PKInterpretResult;

// Allocate initialize and returns a new VM
PK_PUBLIC PKVM* pkNewVM(pkConfiguration* config);

// Clean the VM and dispose all the resources allocated by the VM.
PK_PUBLIC void pkFreeVM(PKVM* vm);

// Interpret the source and return the result.
PK_PUBLIC PKInterpretResult pkInterpretSource(PKVM* vm,
                                              const char* source,
                                              const char* path);

// Compile and execut file at given path.
PK_PUBLIC PKInterpretResult pkInterpret(PKVM* vm, const char* path);

// Set a runtime error to vm.
PK_PUBLIC void pkSetRuntimeError(PKVM* vm, const char* message);

// Returns the associated user data.
PK_PUBLIC void* pkGetUserData(PKVM* vm);

// Update the user data of the vm.
PK_PUBLIC void pkSetUserData(PKVM* vm, void* user_data);

// Encode types to var.
// TODO: user need to use vmPushTempRoot() for strings.
PK_PUBLIC Var pkVarBool(PKVM* vm, bool value);
PK_PUBLIC Var pkVarNumber(PKVM* vm, double value);
PK_PUBLIC Var pkVarString(PKVM* vm, const char* value);

// Decode var types.
// TODO: const char* should be copied otherwise it'll become dangling pointer.
PK_PUBLIC bool pkAsBool(PKVM* vm, Var value);
PK_PUBLIC double pkAsNumber(PKVM* vm, Var value);
PK_PUBLIC const char* pkAsString(PKVM* vm, Var value);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MINISCRIPT_H
