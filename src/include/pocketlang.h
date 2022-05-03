/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
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


/*****************************************************************************/
/* POCKETLANG TYPEDEFS & CALLBACKS                                           */
/*****************************************************************************/

// PocketLang Virtual Machine. It'll contain the state of the execution, stack,
// heap, and manage memory allocations.
typedef struct PKVM PKVM;

// A handle to the pocketlang variables. It'll hold the reference to the
// variable and ensure that the variable it holds won't be garbage collected
// till it's released with pkReleaseHandle().
typedef struct PkHandle PkHandle;

typedef enum PkVarType PkVarType;
typedef enum PkResult PkResult; //< _2rf_: Remove unexp EOF.
typedef struct PkStringPtr PkStringPtr; //< _2rf_
typedef struct PkConfiguration PkConfiguration; //< _2rf_
typedef struct PkCompileOptions PkCompileOptions; //< _2rf_

// C function pointer which is callable from pocketLang by native module
// functions.
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

// Function callback to write [text] to stdout or stderr.
typedef void (*pkWriteFn) (PKVM* vm, const char* text);

// A function callback to read a line from stdin. The returned string shouldn't
// contain a line ending (\n or \r\n). The returned string **must** be
// allocated with pkAllocString() and the VM will claim the ownership of the
// string.
typedef char* (*pkReadFn) (PKVM* vm);

// _2rf_: Remove this.
//
// A function callback symbol for clean/free the pkStringResult.
typedef void (*pkResultDoneFn) (PKVM* vm, PkStringPtr result);

// _2rf_: Remove the duplicate (migration).
//
// A function callback to resolve the import script name from the [from] path
// to an absolute (or relative to the cwd). This is required to solve same
// script imported with different relative path. Set the string attribute to
// NULL to indicate if it's failed to resolve.
typedef PkStringPtr (*pkResolvePathFn) (PKVM* vm, const char* from,
                                        const char* path);

// A function callback to resolve the import script name from the [from]
// path to an absolute (or relative to the cwd). This is required to solve
// same script imported with different relative path. Return NULL to indicate
// failure to resolve. Otherwise the string **must** be allocated with
// pkAllocString() and the VM will claim the ownership of the string.
typedef char* (*pkResolvePathFn_2rf_) (PKVM* vm, const char* from,
                                       const char* path);

// Load and return the script. Called by the compiler to fetch initial source
// code and source for import statements. Return NULL to indicate failure to
// load. Otherwise the string **must** be allocated with pkAllocString() and
// the VM will claim the ownership of the string.
typedef char* (*pkLoadScriptFn) (PKVM* vm, const char* path);

// A function callback to allocate and return a new instance of the registered
// class. Which will be called when the instance is constructed. The returned/
// data is expected to be alive till the delete callback occurs.
typedef void* (*pkNewInstanceFn) ();

// A function callback to de-allocate the aloocated native instance of the
// registered class.
typedef void (*pkDeleteInstanceFn) (void*);

/*****************************************************************************/
/* POCKETLANG TYPES                                                          */
/*****************************************************************************/

// Type enum of the pocketlang's first class types. Note that Object isn't
// instanciable (as of now) but they're considered first calss.
enum PkVarType {
  PK_OBJECT = 0,

  PK_NULL,
  PK_BOOL,
  PK_NUMBER,
  PK_STRING,
  PK_LIST,
  PK_MAP,
  PK_RANGE,
  PK_MODULE,
  PK_CLOSURE,
  PK_FIBER,
  PK_CLASS,
  PK_INSTANCE,
};

// Result that pocketlang will return after a compilation or running a script
// or a function or evaluating an expression.
enum PkResult {
  PK_RESULT_SUCCESS = 0,    // Successfully finished the execution.

  // Unexpected EOF while compiling the source. This is another compile time
  // error that will ONLY be returned if we're compiling with the REPL mode set
  // in the compile options. We need this specific error to indicate the host
  // application to add another line to the last input. If REPL is not enabled,
  // this will be PK_RESULT_COMPILE_ERROR.
  PK_RESULT_UNEXPECTED_EOF,

  PK_RESULT_COMPILE_ERROR,  // Compilation failed.
  PK_RESULT_RUNTIME_ERROR,  // An error occurred at runtime.
};

// A string pointer wrapper to pass c string between host application and
// pocket VM. With a on_done() callback to clean it when the pocket VM is done
// with the string.
struct PkStringPtr {
  const char* string;     //< The string result.
  pkResultDoneFn on_done; //< Called once vm done with the string.
  void* user_data;        //< User related data.

  // These values are provided by the pocket VM to the host application, you're
  // not expected to set this when provideing string to the pocket VM.
  uint32_t length;  //< Length of the string.
  uint32_t hash;    //< Its 32 bit FNV-1a hash.
};

struct PkConfiguration {

  // The callback used to allocate, reallocate, and free. If the function
  // pointer is NULL it defaults to the VM's realloc(), free() wrappers.
  pkReallocFn realloc_fn;

  pkWriteFn stderr_write;
  pkWriteFn stdout_write;
  pkReadFn stdin_read;

  pkResolvePathFn resolve_path_fn;
  pkLoadScriptFn load_script_fn;

  // If true stderr calls will use ansi color codes.
  bool use_ansi_color;

  // User defined data associated with VM.
  void* user_data;
};

// The options to configure the compilation provided by the command line
// arguments (or other ways the host application provides).
struct PkCompileOptions {

  // Compile debug version of the source.
  bool debug;

  // Set to true if compiling in REPL mode, This will print repr version of
  // each evaluated non-null values.
  bool repl_mode;
};

/*****************************************************************************/
/* POCKETLANG PUBLIC API                                                     */
/*****************************************************************************/

// Create a new PkConfiguration with the default values and return it.
// Override those default configuration to adopt to another hosting
// application.
PK_PUBLIC PkConfiguration pkNewConfiguration();

// Create a new pkCompilerOptions with the default values and return it.
// Override those default configuration to adopt to another hosting
// application.
PK_PUBLIC PkCompileOptions pkNewCompilerOptions();

// Allocate, initialize and returns a new VM.
PK_PUBLIC PKVM* pkNewVM(PkConfiguration* config);

// Clean the VM and dispose all the resources allocated by the VM.
PK_PUBLIC void pkFreeVM(PKVM* vm);

// Update the user data of the vm.
PK_PUBLIC void pkSetUserData(PKVM* vm, void* user_data);

// Returns the associated user data.
PK_PUBLIC void* pkGetUserData(const PKVM* vm);

// Allocate memory with [size] and return it. This function should be called 
// when the host application want to send strings to the PKVM that are claimed
// by the VM once allocated.
PK_PUBLIC char* pkAllocString(PKVM* vm, size_t size);

// Complementary function to pkAllocString. This should not be called if the
// string is returned to the VM. Since PKVM will claim the ownership and
// deallocate the string itself.
PK_PUBLIC void pkDeAllocString(PKVM* vm, char* ptr);

// Release the handle and allow its value to be garbage collected. Always call
// this for every handles before freeing the VM.
PK_PUBLIC void pkReleaseHandle(PKVM* vm, PkHandle* handle);

// Add a new module named [name] to the [vm]. Note that the module shouldn't
// already existed, otherwise an assertion will fail to indicate that.
PK_PUBLIC PkHandle* pkNewModule(PKVM* vm, const char* name);

// Register the module to the PKVM's modules map, once after it can be
// imported in other modules.
PK_PUBLIC void pkRegisterModule(PKVM* vm, PkHandle* module);

// Add a native function to the given module. If [arity] is -1 that means
// the function has variadic parameters and use pkGetArgc() to get the argc.
// Note that the function will be added as a global variable of the module,
// to retrieve the function use pkModuleGetGlobal().
PK_PUBLIC void pkModuleAddFunction(PKVM* vm, PkHandle* module,
                                   const char* name,
                                   pkNativeFn fptr, int arity);

// Returns the main function of the [module]. When a module is compiled all of
// it's statements are wrapped around an implicit main function.
PK_PUBLIC PkHandle* pkModuleGetMainFunction(PKVM* vm, PkHandle* module);

// Create a new class on the [module] with the [name] and return it.
// If the [base_class] is NULL by default it'll set to "Object" class.
PK_PUBLIC PkHandle* pkNewClass(PKVM* vm, const char* name,
                               PkHandle* base_class, PkHandle* module,
                               pkNewInstanceFn new_fn,
                               pkDeleteInstanceFn delete_fn);

// Add a native method to the given class. If the [arity] is -1 that means
// the method has variadic parameters and use pkGetArgc() to get the argc.
PK_PUBLIC void pkClassAddMethod(PKVM* vm, PkHandle* cls,
                                const char* name,
                                pkNativeFn fptr, int arity);

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

// Run a function with [argc] arguments and their values are stored at the VM's
// slots starting at index [argv_slot] till the next [argc] consequent slots.
// If [argc] is 0 [argv_slot] value will be ignored.
// To store the return value set [ret_slot] to a valid slot index, if it's
// negative the return value will be ignored.
PK_PUBLIC PkResult pkRunFunction(PKVM* vm, PkHandle* fn,
                                 int argc, int argv_slot, int ret_slot);

/*****************************************************************************/
/* NATIVE / RUNTIME FUNCTION API                                             */
/*****************************************************************************/

// Set a runtime error to VM.
PK_PUBLIC void pkSetRuntimeError(PKVM* vm, const char* message);

// TODO: Set a runtime error to VM, with the formated string.
//PK_PUBLIC void pkSetRuntimeErrorFmt(PKVM* vm, const char* fmt, ...);

// Returns native [self] of the current method as a void*.
PK_PUBLIC void* pkGetSelf(const PKVM* vm);

// Return the current functions argument count. This is needed for functions
// registered with -1 argument count (which means variadic arguments).
PK_PUBLIC int pkGetArgc(const PKVM* vm);

// Check if the argc is in the range of (min <= argc <= max), if it's not, a
// runtime error will be set and return false, otherwise return true. Assuming
// that min <= max, and pocketlang won't validate this in release binary.
PK_PUBLIC bool pkCheckArgcRange(PKVM* vm, int argc, int min, int max);

// SLOTS DOCS 

// Helper function to check if the argument at the [arg] slot is Boolean and
// if not set a runtime error.
PK_PUBLIC bool pkValidateSlotBool(PKVM* vm, int arg, bool* value);

// Helper function to check if the argument at the [arg] slot is Number and
// if not set a runtime error.
PK_PUBLIC bool pkValidateSlotNumber(PKVM* vm, int arg, double* value);

// Helper function to check if the argument at the [arg] slot is String and
// if not set a runtime error.
PK_PUBLIC bool pkValidateSlotString(PKVM* vm, int arg,
                                    const char** value, uint32_t* length);

// Make sure the fiber has [count] number of slots to work with (including the
// arguments).
PK_PUBLIC void pkReserveSlots(PKVM* vm, int count);

// Returns the available number of slots to work with. It has at least the
// number argument the function is registered plus one for return value.
PK_PUBLIC int pkGetSlotsCount(PKVM* vm);

// Returns the type of the variable at the [index] slot.
PK_PUBLIC PkVarType pkGetSlotType(PKVM* vm, int index);

// Returns boolean value at the [index] slot. If the value at the [index]
// is not a boolean it'll be casted (only for booleans).
PK_PUBLIC bool pkGetSlotBool(PKVM* vm, int index);

// Returns number value at the [index] slot. If the value at the [index]
// is not a boolean, an assertion will fail.
PK_PUBLIC double pkGetSlotNumber(PKVM* vm, int index);

// Returns the string at the [index] slot. The returned pointer is only valid
// inside the native function that called this. Afterwards it may garbage
// collected and become demangled. If the [length] is not NULL the length of
// the string will be written.
PK_PUBLIC const char* pkGetSlotString(PKVM* vm, int index, uint32_t* length);

// Capture the variable at the [index] slot and return its handle. As long as
// the handle is not released with `pkReleaseHandle()` the variable won't be
// garbage collected.
PK_PUBLIC PkHandle* pkGetSlotHandle(PKVM* vm, int index);

// Set the [index] slot value as pocketlang null.
PK_PUBLIC void pkSetSlotNull(PKVM* vm, int index);

// Set the [index] slot boolean value as the given [value].
PK_PUBLIC void pkSetSlotBool(PKVM* vm, int index, bool value);

// Set the [index] slot numeric value as the given [value].
PK_PUBLIC void pkSetSlotNumber(PKVM* vm, int index, double value);

// Create a new String copying the [value] and set it to [index] slot.
PK_PUBLIC void pkSetSlotString(PKVM* vm, int index, const char* value);

// Create a new String copying the [value] and set it to [index] slot. Unlike
// the above function it'll copy only the spicified length.
PK_PUBLIC void pkSetSlotStringLength(PKVM* vm, int index,
                                     const char* value, uint32_t length);

// Set the [index] slot's value as the given [handle]. The function won't
// reclaim the ownership of the handle and you can still use it till
// it's released by yourself.
PK_PUBLIC void PkSetSlotHandle(PKVM* vm, int index, PkHandle* handle);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // POCKETLANG_H
