/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
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

// Name of the implicit function for a module. When a module is parsed all of
// it's statements are wrapped around an implicit function with this name.
#define PK_IMPLICIT_MAIN_NAME "$(SourceBody)"

/*****************************************************************************/
/* POCKETLANG TYPES                                                          */
/*****************************************************************************/

// PocketLang Virtual Machine. It'll contain the state of the execution, stack,
// heap, and manage memory allocations.
typedef struct PKVM PKVM;

// A handle to the pocketlang variables. It'll hold the reference to the
// variable and ensure that the variable it holds won't be garbage collected
// till it's released with pkReleaseHandle().
typedef struct PkHandle PkHandle;

// A temproary pointer to the pocketlang variable. This pointer is acquired
// from the pocketlang's current stack frame and the pointer will become
// dangling once after the stack frame is popped.
typedef void* PkVar;

// Type enum of the pocketlang variables, this can be used to get the type
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
  PK_CLASS,
  PK_INST,
} PkVarType;

typedef struct PkStringPtr PkStringPtr;
typedef struct PkConfiguration PkConfiguration;
typedef struct PkCompileOptions PkCompileOptions;

// Type of the error message that pocketlang will provide with the pkErrorFn
// callback.
typedef enum {
  PK_ERROR_COMPILE = 0, // Compile time errors.
  PK_ERROR_RUNTIME,     // Runtime error message.
  PK_ERROR_STACKTRACE,  // One entry of a runtime error stack.
} PkErrorType;

// Result that pocketlang will return after a compilation or running a script
// or a function or evaluating an expression.
typedef enum {
  PK_RESULT_SUCCESS = 0,    // Successfully finished the execution.

  // This will be set to `true` if we're running REPL mode and reached an EOF
  // unexpectedly, 

  // Unexpected EOF while compiling the source. This is another compile time
  // error that will ONLY be returned if we're compiling with the REPL mode set
  // in the compile options. We need this specific error to indicate the host
  // application to add another line to the last input. If REPL is not enabled,
  // this will be PK_RESULT_COMPILE_ERROR.
  PK_RESULT_UNEXPECTED_EOF,

  PK_RESULT_COMPILE_ERROR,  // Compilation failed.
  PK_RESULT_RUNTIME_ERROR,  // An error occurred at runtime.
} PkResult;

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

// Error callback function pointer. For runtime error it'll call first with
// PK_ERROR_RUNTIME followed by multiple callbacks with PK_ERROR_STACKTRACE.
// The error messages should be written to stderr.
typedef void (*pkErrorFn) (PKVM* vm, PkErrorType type,
                           const char* file, int line,
                           const char* message);

// A function callback to write [text] to stdout.
typedef void (*pkWriteFn) (PKVM* vm, const char* text);

// A function callback to read a line from stdin. The returned string shouldn't
// contain a line ending (\n or \r\n).
typedef PkStringPtr (*pkReadFn) (PKVM* vm);

// A function callback, that'll be called when a native instance (wrapper) is
// freed by by the garbage collector, to indicate that pocketlang is done with
// the native instance.
typedef void (*pkFreeInstFn) (PKVM* vm, void* instance);

// A function callback to get the name of the native instance from pocketlang,
// using it's [id]. The returned string won't be copied by pocketlang so it's
// expected to be alived since the instance is alive and recomended to return
// a C literal string.
typedef const char* (*pkInstNameFn) (uint32_t id);

// A function callback symbol for clean/free the pkStringResult.
typedef void (*pkResultDoneFn) (PKVM* vm, PkStringPtr result);

// A function callback to resolve the import script name from the [from] path
// to an absolute (or relative to the cwd). This is required to solve same
// script imported with different relative path. Set the string attribute to
// NULL to indicate if it's failed to resolve.
typedef PkStringPtr (*pkResolvePathFn) (PKVM* vm, const char* from,
                                        const char* path);

// Load and return the script. Called by the compiler to fetch initial source
// code and source for import statements. Set the string attribute to NULL
// to indicate if it's failed to load the script.
typedef PkStringPtr (*pkLoadScriptFn) (PKVM* vm, const char* path);

/*****************************************************************************/
/* POCKETLANG PUBLIC API                                                     */
/*****************************************************************************/

// Create a new PkConfiguration with the default values and return it.
// Override those default configuration to adopt to another hosting
// application.
PK_PUBLIC PkConfiguration pkNewConfiguration(void);

// Create a new pkCompilerOptions with the default values and return it.
// Override those default configuration to adopt to another hosting
// application.
PK_PUBLIC PkCompileOptions pkNewCompilerOptions(void);

// Allocate, initialize and returns a new VM
PK_PUBLIC PKVM* pkNewVM(PkConfiguration* config);

// Clean the VM and dispose all the resources allocated by the VM.
PK_PUBLIC void pkFreeVM(PKVM* vm);

// Update the user data of the vm.
PK_PUBLIC void pkSetUserData(PKVM* vm, void* user_data);

// Returns the associated user data.
PK_PUBLIC void* pkGetUserData(const PKVM* vm);

// Create a new handle for the [value]. This is useful to keep the [value]
// alive once it acquired from the stack. Do not use the [value] once
// creating a new handle for it instead get the value from the handle by
// using pkGetHandleValue() function.
PK_PUBLIC PkHandle* pkNewHandle(PKVM* vm, PkVar value);

// Return the PkVar pointer in the handle, the returned pointer will be valid
// till the handle is released.
PK_PUBLIC PkVar pkGetHandleValue(const PkHandle* handle);

// Release the handle and allow its value to be garbage collected. Always call
// this for every handles before freeing the VM.
PK_PUBLIC void pkReleaseHandle(PKVM* vm, PkHandle* handle);

// Add a global [value] to the given module.
PK_PUBLIC void pkModuleAddGlobal(PKVM* vm, PkHandle* module,
                                 const char* name, PkHandle* value);

// Add a native function to the given module. If [arity] is -1 that means
// The function has variadic parameters and use pkGetArgc() to get the argc.
PK_PUBLIC void pkModuleAddFunction(PKVM* vm, PkHandle* module,
                                   const char* name,
                                   pkNativeFn fptr, int arity);

// Returns the function from the [module] as a handle, if not found it'll
// return NULL.
PK_PUBLIC PkHandle* pkGetFunction(PKVM* vm, PkHandle* module,
                                  const char* name);

// Compile the [module] with the provided [source]. Set the compiler options
// with the the [options] argument or set to NULL for default options.
PK_PUBLIC PkResult pkCompileModule(PKVM* vm, PkHandle* module,
                                   PkStringPtr source,
                                   const PkCompileOptions* options);

// Interpret the source and return the result. Once it's done with the source
// and path 'on_done' will be called to clean the string if it's not NULL.
// Set the compiler options with the the [options] argument or it can be set to
// NULL for default options.
PK_PUBLIC PkResult pkInterpretSource(PKVM* vm,
                                     PkStringPtr source,
                                     PkStringPtr path,
                                     const PkCompileOptions* options);

// Runs the fiber's function with the provided arguments (param [arc] is the
// argument count and [argv] are the values). It'll returns it's run status
// result (success or failure) if you need the yielded or returned value use
// the pkFiberGetReturnValue() function, and use pkFiberIsDone() function to
// check if the fiber can be resumed with pkFiberResume() function.
PK_PUBLIC PkResult pkRunFiber(PKVM* vm, PkHandle* fiber,
                              int argc, PkHandle** argv);

// Resume a yielded fiber with an optional [value]. (could be set to NULL)
// It'll returns it's run status result (success or failure) if you need the
// yielded or returned value use the pkFiberGetReturnValue() function.
PK_PUBLIC PkResult pkResumeFiber(PKVM* vm, PkHandle* fiber, PkVar value);

/*****************************************************************************/
/* POCKETLANG PUBLIC TYPE DEFINES                                            */
/*****************************************************************************/

// A string pointer wrapper to pass cstring from host application to pocketlang
// vm, with a on_done() callback to clean it when the pocketlang vm is done with
// the string.
struct PkStringPtr {
  const char* string;     //< The string result.
  pkResultDoneFn on_done; //< Called once vm done with the string.
  void* user_data;        //< User related data.
};

struct PkConfiguration {

  // The callback used to allocate, reallocate, and free. If the function
  // pointer is NULL it defaults to the VM's realloc(), free() wrappers.
  pkReallocFn realloc_fn;

  pkErrorFn error_fn;
  pkWriteFn write_fn;
  pkReadFn read_fn;

  pkFreeInstFn free_inst_fn;
  pkInstNameFn inst_name_fn;

  pkResolvePathFn resolve_path_fn;
  pkLoadScriptFn load_script_fn;

  // User defined data associated with VM.
  void* user_data;
};

// The options to configure the compilation provided by the command line
// arguments (or other ways the host application provides).
struct PkCompileOptions {

  // Compile debug version of the source.
  bool debug;

  // TODO: don't use FILE* pointer or any of <stdio.h> functions here.
  //       instead add a stream option to vm.config.write_fn callback.
  // 
  // Dump the compiled opcodes to the given [dump_stream] FILE* could be stdio,
  // stderr, or a file pointer.
  //bool dump_opcodes;
  //FILE* dump_stream;

  // Set to true if compiling in REPL mode, This will print repr version of
  // each evaluated non-null values. Note that if [repl_mode] is true the
  // [expression] should also be true otherwise it's incompatible, (will fail
  // an assertion).
  bool repl_mode;

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
// stack and it'll popped when the current call frame ended. Use handles to
// keep the var alive even after that.
PK_PUBLIC PkVar pkGetArg(const PKVM* vm, int arg);

// The functions below are used to extract the function arguments from the
// stack as a type. They will first validate the argument's type and set a
// runtime error if it's not and return false. Otherwise it'll set the [value]
// with the extracted value. Note that the arguments are 1 based (to get the 
// first argument use 1 not 0).

PK_PUBLIC bool pkGetArgBool(PKVM* vm, int arg, bool* value);
PK_PUBLIC bool pkGetArgNumber(PKVM* vm, int arg, double* value);
PK_PUBLIC bool pkGetArgString(PKVM* vm, int arg,
                              const char** value, uint32_t* length);
PK_PUBLIC bool pkGetArgInst(PKVM* vm, int arg, uint32_t id, void** value);
PK_PUBLIC bool pkGetArgValue(PKVM* vm, int arg, PkVarType type, PkVar* value);

// The functions follow are used to set the return value of the current native
// function's. Don't use it outside a registered native function.

PK_PUBLIC void pkReturnNull(PKVM* vm);
PK_PUBLIC void pkReturnBool(PKVM* vm, bool value);
PK_PUBLIC void pkReturnNumber(PKVM* vm, double value);
PK_PUBLIC void pkReturnString(PKVM* vm, const char* value);
PK_PUBLIC void pkReturnStringLength(PKVM* vm, const char* value, size_t len);
PK_PUBLIC void pkReturnValue(PKVM* vm, PkVar value);
PK_PUBLIC void pkReturnHandle(PKVM* vm, PkHandle* handle);

PK_PUBLIC void pkReturnInstNative(PKVM* vm, void* data, uint32_t id);

// Returns the cstring pointer of the given string. Make sure if the [value] is
// a string before calling this function, otherwise it'll fail an assertion.
PK_PUBLIC const char* pkStringGetData(const PkVar value);

// Returns the return value or if it's yielded, the yielded value of the fiber
// as PkVar, this value lives on stack and will die (popped) once the fiber
// resumed use handle to keep it alive.
PK_PUBLIC PkVar pkFiberGetReturnValue(const PkHandle* fiber);

// Returns true if the fiber is finished it's execution and cannot be resumed
// anymore.
PK_PUBLIC bool pkFiberIsDone(const PkHandle* fiber);

/*****************************************************************************/
/* POCKETLANG TYPE FUNCTIONS                                                 */
/*****************************************************************************/

// The functions below will allocate a new object and return's it's value
// wrapped around a handler.

PK_PUBLIC PkHandle* pkNewString(PKVM* vm, const char* value);
PK_PUBLIC PkHandle* pkNewStringLength(PKVM* vm, const char* value, size_t len);
PK_PUBLIC PkHandle* pkNewList(PKVM* vm);
PK_PUBLIC PkHandle* pkNewMap(PKVM* vm);

// Add a new module named [name] to the [vm]. Note that the module shouldn't
// already existed, otherwise an assertion will fail to indicate that.
PK_PUBLIC PkHandle* pkNewModule(PKVM* vm, const char* name);

// Create and return a new fiber around the function [fn].
PK_PUBLIC PkHandle* pkNewFiber(PKVM* vm, PkHandle* fn);

// Create and return a native instance around the [data]. The [id] is the
// unique id of the instance, this would be used to check if two instances are
// equal and used to get the name of the instance using NativeTypeNameFn
// callback.
PK_PUBLIC PkHandle* pkNewInstNative(PKVM* vm, void* data, uint32_t id);

// TODO: The functions below will push the primitive values on the stack
// and return it's pointer as a PkVar. It's useful to convert your primitive
// values as pocketlang variables.
//PK_PUBLIC PkVar pkPushNull(PKVM* vm);
//PK_PUBLIC PkVar pkPushBool(PKVM* vm, bool value);
//PK_PUBLIC PkVar pkPushNumber(PKVM* vm, double value);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // POCKETLANG_H
