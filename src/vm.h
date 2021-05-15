/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef VM_H
#define VM_H

#include "common.h"
#include "compiler.h"
#include "var.h"

// The maximum number of temporary object reference to protect them from being
// garbage collected.
#define MAX_TEMP_REFERENCE 16

// The capacity of the builtin function array in the VM.
#define BUILTIN_FN_CAPACITY 50

typedef enum {
  #define OPCODE(name, _, __) OP_##name,
  #include "opcodes.h"
  #undef OPCODE
} Opcode;

// Builtin functions are stored in an array in the VM (unlike script functions
// they're member of function buffer of the script) and this struct is a single
// entry of the array.
typedef struct {
  const char* name; //< Name of the function.
  int length;       //< Length of the name.
  Function* fn;     //< Native function pointer.
} BuiltinFn;

struct PKVM {

  // The first object in the link list of all heap allocated objects.
  Object* first;

  // The number of bytes allocated by the vm and not (yet) garbage collected.
  size_t bytes_allocated;

  // The number of bytes that'll trigger the next GC.
  size_t next_gc;

  // In the tri coloring scheme gray is the working list. We recursively pop
  // from the list color it balck and add it's referenced objects to gray_list.
  Object** gray_list;
  int gray_list_count;
  int gray_list_capacity;

  // A stack of temporary object references to ensure that the object
  // doesn't garbage collected.
  Object* temp_reference[MAX_TEMP_REFERENCE];
  int temp_reference_count;

  // VM's configurations.
  pkConfiguration config;

  // Current compiler reference to mark it's heap allocated objects. Note that
  // The compiler isn't heap allocated.
  Compiler* compiler;

  // A cache of the compiled scripts with their path as key and the Scrpit
  // object as the value.
  Map* scripts;

  // A map of core libraries with their name as the key and the Script object
  // as the value.
  Map* core_libs;

  // Array of all builtin functions.
  BuiltinFn builtins[BUILTIN_FN_CAPACITY];
  int builtins_count;

  // Execution variables ////////////////////////////////////////////////////

  // The root script of the runtime and it's one of the VM's reference root.
  // VM is responsible to manage the memory (TODO: implement handlers).
  Script* script;

  // Current fiber.
  Fiber* fiber;
};

// A realloc wrapper which handles memory allocations of the VM.
// - To allocate new memory pass NULL to parameter [memory] and 0 to
//   parameter [old_size] on failure it'll return NULL.
// - To free an already allocated memory pass 0 to parameter [old_size]
//   and it'll returns NULL.
// - The [old_size] parameter is required to keep track of the VM's
//    allocations to trigger the garbage collections.
// If deallocating (free) using vmRealloc the old_size should be 0 as it's not
// going to track deallocated bytes, instead use garbage collector to do it.
void* vmRealloc(PKVM* self, void* memory, size_t old_size, size_t new_size);

// Push the object to temporary references stack.
void vmPushTempRef(PKVM* self, Object* obj);

// Pop the top most object from temporary reference stack.
void vmPopTempRef(PKVM* self);

// Trigger garbage collection manually.
void vmCollectGarbage(PKVM* self);

// Runs the script and return result.
PKInterpretResult vmRunScript(PKVM* vm, Script* script);

#endif // VM_H
