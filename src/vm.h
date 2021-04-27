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
#define MAX_TEMP_REFERENCE 8

// The maximum number of script cache can vm hold at once.
#define MAX_SCRIPT_CACHE 128

typedef enum {
  #define OPCODE(name, _, __) OP_##name,
  #include "opcodes.h"
  #undef OPCODE
} Opcode;

struct MSVM {

  // The first object in the link list of all heap allocated objects.
  Object* first;

  // The number of bytes allocated by the vm and not (yet) garbage collected.
  size_t bytes_allocated;

  // The number of bytes that'll trigger the next GC.
  size_t next_gc;

  // In the tri coloring scheme gray is the working list. We recursively pop
  // from the list color it balck and add it's referenced objects to gray_list.
  Object** marked_list;
  int marked_list_count;
  int marked_list_capacity;

  // A stack of temporary object references to ensure that the object
  // doesn't garbage collected.
  Object* temp_reference[MAX_TEMP_REFERENCE];
  int temp_reference_count;

  // VM's configurations.
  MSConfiguration config;

  // Current compiler reference to mark it's heap allocated objects. Note that
  // The compiler isn't heap allocated.
  Compiler* compiler;

  // Std scripts array. (TODO: assert "std" scripts doesn't have global vars).
  Script* std_scripts[MAX_SCRIPT_CACHE];
  int std_count;

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
void* vmRealloc(MSVM* self, void* memory, size_t old_size, size_t new_size);

// Initialize the vm and update the configuration. If config is NULL it'll use
// the default configuration.
void vmInit(MSVM* self, MSConfiguration* config);

// Push the object to temporary references stack.
void vmPushTempRef(MSVM* self, Object* obj);

// Pop the top most object from temporary reference stack.
void vmPopTempRef(MSVM* self);

// Trigger garbage collection manually.
void vmCollectGarbage(MSVM* self);

// Add a std script to vm when initializing core.
void vmAddStdScript(MSVM* self, Script* script);

// Returns the std script with the name [name]. Note that the name shouldn't
// be start with "std:" but the actual name of the script. If not found
// returns NULL.
Script* vmGetStdScript(MSVM* self, const char* name);

// Runs the script and return result.
MSInterpretResult vmRunScript(MSVM* vm, Script* script);

#endif // VM_H
