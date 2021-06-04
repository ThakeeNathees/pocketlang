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

// A doubly link list of vars that have reference in the host application.
// Handles are wrapper around Var that lives on the host application.
struct PkHandle {
  Var value;

  PkHandle* prev;
  PkHandle* next;
};

// PocketLang Virtual Machine. It'll contain the state of the execution, stack,
// heap, and manage memory allocations.
struct PKVM {

  // The first object in the link list of all heap allocated objects.
  Object* first;

  // The number of bytes allocated by the vm and not (yet) garbage collected.
  size_t bytes_allocated;

  // The number of bytes that'll trigger the next GC.
  size_t next_gc;

  // Minimum size the heap could get.
  size_t min_heap_size;

  // The heap size for the next GC will be calculated as the bytes we have
  // allocated so far plus the fill factor of it.
  int heap_fill_percent;

  // In the tri coloring scheme gray is the working list. We recursively pop
  // from the list color it balck and add it's referenced objects to gray_list.
  Object** gray_list;
  int gray_list_count;
  int gray_list_capacity;

  // A stack of temporary object references to ensure that the object
  // doesn't garbage collected.
  Object* temp_reference[MAX_TEMP_REFERENCE];
  int temp_reference_count;

  // Pointer to the first handle in the doubly linked list of handles. Handles
  // are wrapper around Var that lives on the host application. This linked
  // list will keep them alive till the host uses the variable.
  PkHandle* handles;

  // VM's configurations.
  pkConfiguration config;

  // Current compiler reference to mark it's heap allocated objects. Note that
  // The compiler isn't heap allocated. It'll be a link list of all the
  // compiler we have so far. A new compiler will be created and appended when
  // a new scirpt is being imported and compiled at compiletime.
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

  // The root script of the runtime and it's one of the VM's reference root.
  // VM is responsible to manage the memory.
  Script* script;

  // Current fiber.
  Fiber* fiber;
};

// A realloc() function wrapper which handles memory allocations of the VM.
// - To allocate new memory pass NULL to parameter [memory] and 0 to
//   parameter [old_size] on failure it'll return NULL.
// - To free an already allocated memory pass 0 to parameter [old_size]
//   and it'll returns NULL.
// - The [old_size] parameter is required to keep track of the VM's
//    allocations to trigger the garbage collections.
// If deallocating (free) using vmRealloc the old_size should be 0 as it's not
// going to track deallocated bytes, instead use garbage collector to do it.
void* vmRealloc(PKVM* self, void* memory, size_t old_size, size_t new_size);

// Create and return a new handle for the [value].
PkHandle* vmNewHandle(PKVM* self, Var value);

// Trigger garbage collection. This is an implementation of mark and sweep
// garbage collection (https://en.wikipedia.org/wiki/Tracing_garbage_collection).
// 
// 1. MARKING PHASE
// 
//       |          |
//       |  [obj0] -+---> [obj2] -> [obj6]    .------- Garbage --------.
//       |  [obj3]  |       |                 |                        |
//       |  [obj8]  |       '-----> [obj1]    |   [obj7] ---> [obj5]   |
//       '----------'                         |       [obj4]           |
//        working set                         '------------------------'
// 
//   First we preform a tree traversel from all the vm's root objects. such as
//   stack values, temp references, handles, vm's running fiber, current
//   compiler (if it has any) etc. Mark them (ie. is_marked = true) and add
//   them to the working set (the gray_list). Pop the top object from the
//   working set add all of it's referenced objects to the working set and mark
//   it black (try-color marking) We'll keep doing this till the working set
//   become empty, at this point any object which isn't marked is a garbage.
// 
//   Every single heap allocated objects will be in the VM's link list. Those
//   objects which are reachable have marked (ie. is_marked = true) once the
//   marking phase is completed.
//    .----------------. 
//    |  VM            |
//    | Object* first -+--------> [obj8] -> [obj7] -> [obj6] ... [obj0] -> NULL
//    '----------------' marked =  true      false     true       true
//
// 2. SWEEPING PHASE
// 
//    .----------------.                .-------------.
//    |  VM            |                |             V
//    | Object* first -+--------> [obj8]    [obj7]    [obj6] ... [obj0] -> NULL
//    '----------------' marked =  true      false     true       true
//                                       '--free()--'
//   
//   Once the marking phase is done, we iterate through the objects and remove
//   the objects which are not marked from the linked list and deallocate them.
//
void vmCollectGarbage(PKVM* self);

// Push the object to temporary references stack. This reference will prevent
// the object from garbage collection.
void vmPushTempRef(PKVM* self, Object* obj);

// Pop the top most object from temporary reference stack.
void vmPopTempRef(PKVM* self);

// Runs the script and return result.
PkInterpretResult vmRunScript(PKVM* vm, Script* script);

#endif // VM_H
