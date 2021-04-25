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

// A function that'll be called for all the allocation calls by MSVM.
//
// - To allocate new memory it'll pass NULL to parameter [memory] and the
//   required size to [new_size]. On failure the return value would be NULL.
//
// - When reallocating an existing memory if it's grow in place the return
//   address would be the same as [memory] otherwise a new address.
//
// - To free an allocated memory pass [memory] and 0 to [new_size]. The
//   function will return NULL.
typedef void* (*MiniScriptReallocFn)(void* memory, size_t new_size, void* user_data);

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

  // The callback used to allocate, reallocate, and free. If the function
  // pointer is NULL it defaults to the VM's realloc(), free() wrappers.
  MiniScriptReallocFn realloc_fn;

  MiniScriptErrorFn error_fn;
  MiniScriptWriteFn write_fn;

  MiniScriptLoadScriptFn load_script_fn;
  MiniScriptLoadScriptDoneFn load_script_done_fn;

  // User defined data associated with VM.
  void* user_data;

} MSConfiguration;

// Initialize the configuration and set ALL of it's values to the defaults.
// Call this before setting any particular field of it.
void MSInitConfiguration(MSConfiguration* config);

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

// Returns the associated user data.
void* msGetUserData(MSVM* vm);

// Update the user data of the vm.
void msSetUserData(MSVM* vm, void* user_data);

// Encode types to var.
// TODO: user need to use vmPushTempRoot() for strings.
Var msVarBool(MSVM* vm, bool value);
Var msVarNumber(MSVM* vm, double value);
Var msVarString(MSVM* vm, const char* value);

// Decode var types.
// TODO: const char* should be copied otherwise it'll become dangling pointer.
bool msAsBool(MSVM* vm, Var value);
double msAsNumber(MSVM* vm, Var value);
const char* msAsString(MSVM* vm, Var value);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MINISCRIPT_H
