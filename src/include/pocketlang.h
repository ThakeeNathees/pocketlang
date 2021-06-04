/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef POCKETLANG_H
#define POCKETLANG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*****************************************************************************/
/* POCKETLANG DEFINES                                                        */
/*****************************************************************************/

// The version number macros.
// Major Version - Increment when changes break compatibility.
// Minor Version - Increment when new functionality added to public api.
// Patch Version - Increment when bug fixed or minor changes between releases.
#define PK_VERSION_MAJOR 0
#define PK_VERSION_MINOR 1
#define PK_VERSION_PATCH 0

// String representation of the version.
#define PK_VERSION_STRING "0.1.0"

// Pocketlang visibility macros. define PK_DLL for using pocketlang as a 
// shared library and define PK_COMPILE to export symbols when compiling the
// pocketlang it self as a shared library.

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

// A convinent macro to define documentation of funcions. Use it to document 
// your native functions.
// 
// PK_DOC(foo,
//   "The function will print 'foo' on the console.") {
//   printf("foo\n");
// }
// 
#define PK_DOC(func, doc) \
  /* TODO: static char __pkdoc__##func[] = doc;*/ void func(PKVM* vm)

/*****************************************************************************/
/* POCKETLANG TYPES                                                          */
/*****************************************************************************/

// PocketLang Virtual Machine. It'll contain the state of the execution, stack,
// heap, and manage memory allocations.
typedef struct PKVM PKVM;

// A handle to the pocketlang variables. It'll hold the reference to the
// variable and ensure that the variable it holds won't be garbage collected
// till it released with pkReleaseHandle().
typedef struct PkHandle PkHandle;

// A temproary pointer to the pocketlang variable. This pointer is aquired
// from the pocketlang's current stack frame and the pointer will become
// dangling once after the stack frame is popped.
typedef void* PkVar;

// Type enum of the pocketlang varaibles, this can be used to get the type
// from a Var* in the method pkGetVarType().
typedef enum {
  PK_NULL,
  PK_BOOL,
  PK_NUMBER,
  PK_STRING,
  PK_LIST,
  PK_MAP,
  PK_RANGE,
  PK_SCRIPT,
  PK_FUNCTION,
  PK_FIBER,
} PkVarType;

typedef struct pkStringPtr pkStringPtr;
typedef struct pkConfiguration pkConfiguration;

// Type of the error message that pocketlang will provide with the pkErrorFn
// callback.
typedef enum {
  PK_ERROR_COMPILE = 0, // Compile time errors.
  PK_ERROR_RUNTIME,     // Runtime error message.
  PK_ERROR_STACKTRACE,  // One entry of a runtime error stack.
} PkErrorType;

// Result that pocketlang will return after running a script or a function
// or evaluvating an expression.
typedef enum {
  PK_RESULT_SUCCESS = 0,    // Successfully finished the execution.
  PK_RESULT_COMPILE_ERROR,  // Compilation failed.
  PK_RESULT_RUNTIME_ERROR,  // An error occured at runtime.
} PkInterpretResult;

/*****************************************************************************/
/* POCKETLANG FUNCTION POINTERS & CALLBACKS                                  */
/*****************************************************************************/

// C function pointer which is callable from PocketLang.
typedef void (*pkNativeFn)(PKVM* vm);

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

// Error callback function pointer. for runtime error it'll call first with
// PK_ERROR_RUNTIME followed by multiple callbacks with PK_ERROR_STACKTRACE.
typedef void (*pkErrorFn) (PKVM* vm, PkErrorType type,
                           const char* file, int line,
                           const char* message);

// A function callback used by `print()` statement.
typedef void (*pkWriteFn) (PKVM* vm, const char* text);

// A function callback symbol for clean/free the pkStringResult.
typedef void (*pkResultDoneFn) (PKVM* vm, pkStringPtr result);

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

/*****************************************************************************/
/* POCKETLANG PUBLIC API                                                     */
/*****************************************************************************/

// Create a new pkConfiguraition with the default values and return it.
// Override those default configuration to adopt to another hosting
// application.
PK_PUBLIC pkConfiguration pkNewConfiguration();

// Allocate initialize and returns a new VM
PK_PUBLIC PKVM* pkNewVM(pkConfiguration* config);

// Clean the VM and dispose all the resources allocated by the VM.
PK_PUBLIC void pkFreeVM(PKVM* vm);

// Update the user data of the vm.
PK_PUBLIC void pkSetUserData(PKVM* vm, void* user_data);

// Returns the associated user data.
PK_PUBLIC void* pkGetUserData(const PKVM* vm);

// Create a new handle for the [value]. This is usefull to keep the [value]
// alive once it aquired from the stack. Do not use the [value] once creating
// a new handle for it instead get the value from the handle by using
// pkGetHandleValue() function.
PK_PUBLIC PkHandle* pkNewHandle(PKVM* vm, PkVar value);

// Return the PkVar pointer in the handle, the returned pointer will be valid
// till the handle is released.
PK_PUBLIC PkVar pkGetHandleValue(const PkHandle* handle);

// Release the handle and allow it's value to be garbage collected. Always call
// this for every handles before freeing the VM.
PK_PUBLIC void pkReleaseHandle(PKVM* vm, PkHandle* handle);

// Add a new module named [name] to the [vm]. Note that the module shouldn't
// already existed, otherwise an assertion will fail to indicate that.
PK_PUBLIC PkHandle* pkNewModule(PKVM* vm, const char* name);

// Add a native function to the given script. If [arity] is -1 that means
// The function has variadic parameters and use pkGetArgc() to get the argc.
PK_PUBLIC void pkModuleAddFunction(PKVM* vm, PkHandle* module,
                                   const char* name,
                                   pkNativeFn fptr, int arity);

// Interpret the source and return the result.
PK_PUBLIC PkInterpretResult pkInterpretSource(PKVM* vm,
                                              const char* source,
                                              const char* path);

// Compile and execut file at given path.
PK_PUBLIC PkInterpretResult pkInterpret(PKVM* vm, const char* path);

/*****************************************************************************/
/* POCKETLANG PUBLIC TYPE DEFINES                                            */
/*****************************************************************************/

// A string pointer wrapper to pass cstring from host application to pocketlang
// vm, with a on_done() callback to clean it when the pocketlang vm done with
// the string.
struct pkStringPtr {
  const char* string;     //< The string result.
  pkResultDoneFn on_done; //< Called once vm done with the string.
  void* user_data;        //< User related data.
};

struct pkConfiguration {

  // The callback used to allocate, reallocate, and free. If the function
  // pointer is NULL it defaults to the VM's realloc(), free() wrappers.
  pkReallocFn realloc_fn;

  pkErrorFn error_fn;
  pkWriteFn write_fn;

  pkResolvePathFn resolve_path_fn;
  pkLoadScriptFn load_script_fn;

  // User defined data associated with VM.
  void* user_data;
};

/*****************************************************************************/
/* NATIVE FUNCTION API                                                       */
/*****************************************************************************/

// Set a runtime error to vm.
PK_PUBLIC void pkSetRuntimeError(PKVM* vm, const char* message);

// Return the type of the [value] this will help to get the type of the
// variable that was extracted from pkGetArg() earlier.
PK_PUBLIC PkVarType pkGetValueType(const PkVar value);

// Return the current functions argument count. This is needed for functions
// registered with -1 argument count (which means variadic arguments).
PK_PUBLIC int pkGetArgc(const PKVM* vm);

// Return the [arg] th argument as a PkVar. This pointer will only be
// valid till the current function ends, because it points to the var at the
// stack and it'll popped when the current call frame ended. Use handlers to
// keep the var alive even after that.
PK_PUBLIC PkVar pkGetArg(const PKVM* vm, int arg);

// The below functions are used to extract the function arguments from the
// stack as a type. They will first validate the argument's type and set a
// runtime error if it's not and return false. Otherwise it'll set the [value]
// with the extracted value. Note that the arguments are 1 based (to get the 
// first argument use 1 not 0).

PK_PUBLIC bool pkGetArgBool(PKVM* vm, int arg, bool* vlaue);
PK_PUBLIC bool pkGetArgNumber(PKVM* vm, int arg, double* value);
PK_PUBLIC bool pkGetArgString(PKVM* vm, int arg, const char** value);
PK_PUBLIC bool pkGetArgValue(PKVM* vm, int arg, PkVarType type, PkVar* value);

// The below functions are used to set the return value of the current native
// function's. Don't use it outside a registered native function.

PK_PUBLIC void pkReturnNull(PKVM* vm);
PK_PUBLIC void pkReturnBool(PKVM* vm, bool value);
PK_PUBLIC void pkReturnNumber(PKVM* vm, double value);
PK_PUBLIC void pkReturnString(PKVM* vm, const char* value);
PK_PUBLIC void pkReturnStringLength(PKVM* vm, const char* value, size_t len);
PK_PUBLIC void pkReturnValue(PKVM* vm, PkVar value);

/*****************************************************************************/
/* POCKETLANG TYPE FUNCTIONS                                                 */
/*****************************************************************************/

// The below functions will allocate a new object and return's it's value
// wrapped around a handler.

PK_PUBLIC PkHandle* pkNewString(PKVM* vm, const char* value);
PK_PUBLIC PkHandle* pkNewStringLength(PKVM* vm, const char* value, size_t len);
PK_PUBLIC PkHandle* pkNewList(PKVM* vm);
PK_PUBLIC PkHandle* pkNewMap(PKVM* vm);

PK_PUBLIC const char* pkStringGetData(const PkVar value);


//      TODO:
// The below functions will push the primitive values on the stack and return
// it's pointer as a PkVar it's usefull to convert your primitive values as
// pocketlang variables.
//PK_PUBLIC PkVar pkPushNull(PKVM* vm);
//PK_PUBLIC PkVar pkPushBool(PKVM* vm, bool value);
//PK_PUBLIC PkVar pkPushNumber(PKVM* vm, double value);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // POCKETLANG_H
