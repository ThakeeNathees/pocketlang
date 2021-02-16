/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef MINISCRIPT_H
#define MINISCRIPT_H

#include <stdint.h>
#include <stdbool.h>

// The version number macros.
#define MS_VERSION_MAJOR 0
#define MS_VERSION_MINOR 1
#define MS_VERSION_PATCH 0

// String representation of the value.
#define MS_VERSION_STRING "0.1.0"

// MiniScript Virtual Machine.
// it'll contain the state of the execution, stack, heap, and manage memory
// allocations.
typedef struct MSVM MSVM;

// C function pointer which is callable from MiniScript.
typedef void (*MiniScriptNativeFn)(MSVM* vm);

typedef enum {

  // Compile time errors (syntax errors, unresolved fn, etc).
  MS_ERROR_COMPILE,

  // Runtime error message.
  MS_ERROR_RUNTIME,

  // One entry of a runtime error stack.
  MS_ERROR_STACKTRACE,
} MSErrorType;

// Error callback function pointer. for runtime error it'll call first with
// MS_ERROR_RUNTIME followed by multiple callbacks with MS_ERROR_STACKTRACE.
typedef void (*MiniScriptErrorFn) (MSVM* vm, MSErrorType type,
                                   const char* file, int line,
                                   const char* message);

// A function callback used by `print()` statement.
typedef void (*MiniScriptWriteFn) (MSVM* vm, const char* text);

// Result of the MiniScriptLoadScriptFn function.
typedef struct {
  bool is_failed;
  const char* source;
  void* user_data;
} MSLoadScriptResult;

// Load and return the script. Called by the compiler to fetch initial source
// code and source for import statements.
typedef MSLoadScriptResult (*MiniScriptLoadScriptFn)(MSVM* vm,
                                                     const char* path);

// This function will be called once it done with the loaded script.
// [user_data] would be be the one returned in MiniScriptLoadScriptFn
// which is useful to free the source memory if needed.
typedef void (*MiniScriptLoadScriptDoneFn) (MSVM* vm, const char* path,
                                            void* user_data);

typedef struct {

  MiniScriptErrorFn error_fn;
  MiniScriptWriteFn write_fn;

  MiniScriptLoadScriptFn load_script_fn;
  MiniScriptLoadScriptDoneFn load_script_done_fn;

  // User defined data associated with VM.
  void* user_data;

} MSConfiguration;

typedef enum {
  RESULT_SUCCESS = 0,
  RESULT_COMPILE_ERROR,
  RESULT_RUNTIME_ERROR,
} MSInterpretResult;

// Allocate initialize and returns a new VM
MSVM* msNewVM(MSConfiguration* config);

// Compile and execut file at given path.
MSInterpretResult msInterpret(MSVM* vm, const char* file);

// Set a runtime error to vm.
void msSetRuntimeError(MSVM* vm, const char* format, ...);

#endif // MINISCRIPT_H
