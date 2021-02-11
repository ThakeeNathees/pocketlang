/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef VM_H
#define VM_H

#include "miniscript.h"

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

typedef struct {
  uint8_t* ip;  //< Pointer to the next instruction byte code.
  Function* fn; //< Function of the frame.
  Var* rbp;     //< Stack base pointer. (%rbp)
} CallFrame;

struct MSVM {

  // The first object in the link list of all heap allocated objects.
  Object* first;

  size_t bytes_allocated;

  // A stack of temporary object references to ensure that the object
  // doesn't garbage collected.
  Object* temp_reference[MAX_TEMP_REFERENCE];
  int temp_reference_count;

  // VM's configurations.
  MSConfiguration config;

  // Current compiler reference to mark it's heap allocated objects.
  Compiler* compiler;

  // Execution variables ////////////////////////////////////////////////////

  // Compiled script cache.
  Script* scripts[MAX_SCRIPT_CACHE];

  // Number of script cache.
  int script_count;

  // The stack of the execution holding locals and temps. A heap  allocated
  // Will and grow as needed.
  Var* stack;

  // The stack pointer (%rsp) pointing to the stack top.
  Var* sp;

  // The stack base pointer of the current frame. It'll be updated before
  // calling a native function.
  Var* rbp;

  // Size of the allocated stack.
  int stack_size;

  // Heap allocated array of call frames will grow as needed.
  CallFrame* frames;

  // Capacity of the frames array.
  int frame_capacity;

  // Number of frame entry in frames.
  int frame_count;

  // Runtime error initially NULL, heap allocated.
  String* error;
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

// Runs the script and return result.
MSInterpretResult vmRunScript(MSVM* vm, Script* script);

#endif // VM_H
