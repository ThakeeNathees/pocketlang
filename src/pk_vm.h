/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_VM_H
#define PK_VM_H

#include "pk_compiler.h"
#include "pk_internal.h"
#include "pk_value.h"

// The maximum number of temporary object reference to protect them from being
// garbage collected.
#define MAX_TEMP_REFERENCE 16

// The capacity of the builtin function array in the VM.
#define BUILTIN_FN_CAPACITY 50

// Initially allocated call frame capacity. Will grow dynamically.
#define INITIAL_CALL_FRAMES 4

// The minimum size of the stack that will be initialized for a fiber before
// running one.
#define MIN_STACK_SIZE 128

// The allocated size that will trigger the first GC. (~10MB).
#define INITIAL_GC_SIZE (1024 * 1024 * 10)

// The heap size might shrink if the remaining allocated bytes after a GC
// is less than the one before the last GC. So we need a minimum size.
#define MIN_HEAP_SIZE (1024 * 1024)

// The heap size for the next GC will be calculated as the bytes we have
// allocated so far plus the fill factor of it.
#define HEAP_FILL_PERCENT 75

// Evaluated to "true" if a runtime error set on the current fiber.
#define VM_HAS_ERROR(vm) (vm->fiber->error != NULL)

// Set the error message [err] to the [vm]'s current fiber.
#define VM_SET_ERROR(vm, err)        \
  do {                               \
    ASSERT(!VM_HAS_ERROR(vm), OOPS); \
    (vm->fiber->error = err);        \
  } while (false)

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
  // from the list color it black and add it's referenced objects to gray_list.

  // Working set is the is the list of objects that were marked reachable from
  // VM's root (ex: stack values, temp references, handles, vm's running fiber,
  // current compiler etc). But yet tobe perform a reachability analysis of the
  // objects it reference to.
  Object** working_set;
  int working_set_count;
  int working_set_capacity;

  // A stack of temporary object references to ensure that the object
  // doesn't garbage collected.
  Object* temp_reference[MAX_TEMP_REFERENCE];
  int temp_reference_count;

  // Pointer to the first handle in the doubly linked list of handles. Handles
  // are wrapper around Var that lives on the host application. This linked
  // list will keep them alive till the host uses the variable.
  PkHandle* handles;

  // VM's configurations.
  PkConfiguration config;

  // Current compiler reference to mark it's heap allocated objects. Note that
  // The compiler isn't heap allocated. It'll be a link list of all the
  // compiler we have so far. A new compiler will be created and appended when
  // a new module is being imported and compiled at compiletime.
  Compiler* compiler;

  // A map of all the modules which are compiled or natively registered.
  // The key of the modules will be:
  // 1. Native modules  : name of the module.
  // 2. Compiled script :
  //      - module name if one defined with the module keyword
  //      - otherwise path of the module.
  Map* modules;

  // Array of all builtin functions.
  Closure* builtins[BUILTIN_FN_CAPACITY];
  int builtins_count;

  // An array of all the primitive types' class except for OBJ_INST. Since the
  // type of the objects are enums starting from 0 we can directly get the
  // class by using their enum (ex: primitives[OBJ_LIST]).
  Class* primitives[(int)OBJ_INST];

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
void* vmRealloc(PKVM* vm, void* memory, size_t old_size, size_t new_size);

// Create and return a new handle for the [value].
PkHandle* vmNewHandle(PKVM* vm, Var value);

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
//   First we preform a tree traversal from all the vm's root objects. such as
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
void vmCollectGarbage(PKVM* vm);

// Push the object to temporary references stack. This reference will prevent
// the object from garbage collection.
void vmPushTempRef(PKVM* vm, Object* obj);

// Pop the top most object from temporary reference stack.
void vmPopTempRef(PKVM* vm);

// Regsiter a module to the VM's modules map, the key could be either it's
// name (for core module) or it's path (if it's a compiled script). If the
// key doesn't match either of it's name or path an assertion will fail.
void vmRegisterModule(PKVM* vm, Module* module, String* key);

// Returns the module, where the [key] could be either it's name or path that
// was used to register the module. If it doesn't exists, returns NULL.
Module* vmGetModule(PKVM* vm, String* key);

// ((Context switching - start))
// Prepare a new fiber for execution with the given arguments. That can be used
// different fiber_run apis. Return true on success, otherwise it'll set the
// error to the vm's current fiber (if it has any).
bool vmPrepareFiber(PKVM* vm, Fiber* fiber, int argc, Var** argv);

// ((Context switching - resume))
// Switch the running fiber of the vm from the current fiber to the provided
// [fiber]. with an optional [value] (could be set to NULL). used in different
// fiber_resume apis. Return true on success, otherwise it'll set the error to
// the vm's current fiber (if it has any).
bool vmSwitchFiber(PKVM* vm, Fiber* fiber, Var* value);

// Yield from the current fiber. If the [value] isn't NULL it'll set it as the
// yield value.
void vmYieldFiber(PKVM* vm, Var* value);

#endif // PK_VM_H
