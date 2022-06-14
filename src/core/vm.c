/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <math.h>

#ifndef PK_AMALGAMATED
#include "vm.h"
#include "utils.h"
#include "debug.h"
#endif

PkHandle* vmNewHandle(PKVM* vm, Var value) {
  PkHandle* handle = (PkHandle*)ALLOCATE(vm, PkHandle);
  handle->value = value;
  handle->prev = NULL;
  handle->next = vm->handles;
  if (handle->next != NULL) handle->next->prev = handle;
  vm->handles = handle;
  return handle;
}

void* vmRealloc(PKVM* vm, void* memory, size_t old_size, size_t new_size) {

  // Track the total allocated memory of the VM to trigger the GC.
  // if vmRealloc is called for freeing, the old_size would be 0 since
  // deallocated bytes are traced by garbage collector.
  //
  // If we're running a garbage collection, VM's byte allocated will be
  // re-calculated at vmCollectGarbage() which is equal to the remaining
  // objects after clening the garbage. And clearing a garbage will
  // recursively invoke this function so we shouldn't modify it.
  if (!vm->collecting_garbage) {
    vm->bytes_allocated += new_size - old_size;
  }

  // If we're garbage collecting no new allocation is allowed.
  ASSERT(!vm->collecting_garbage || new_size == 0,
         "No new allocation is allowed while garbage collection is running.");

  if (new_size > 0 && vm->bytes_allocated > vm->next_gc) {
    ASSERT(vm->collecting_garbage == false, OOPS);
    vm->collecting_garbage = true;
    vmCollectGarbage(vm);
    vm->collecting_garbage = false;
  }

  return vm->config.realloc_fn(memory, new_size, vm->config.user_data);
}

void vmPushTempRef(PKVM* vm, Object* obj) {
  ASSERT(obj != NULL, "Cannot reference to NULL.");
  ASSERT(vm->temp_reference_count < MAX_TEMP_REFERENCE,
    "Too many temp references");
  vm->temp_reference[vm->temp_reference_count++] = obj;
}

void vmPopTempRef(PKVM* vm) {
  ASSERT(vm->temp_reference_count > 0,
         "Temporary reference is empty to pop.");
  vm->temp_reference_count--;
}

void vmRegisterModule(PKVM* vm, Module* module, String* key) {
  ASSERT((((module->name != NULL) && IS_STR_EQ(module->name, key)) ||
         IS_STR_EQ(module->path, key)), OOPS);
  // FIXME:
  // Not sure what to do, if a module the the same key already exists. Should
  // I override or assert.
  mapSet(vm, vm->modules, VAR_OBJ(key), VAR_OBJ(module));
}

Module* vmGetModule(PKVM* vm, String* key) {
  Var module = mapGet(vm->modules, VAR_OBJ(key));
  if (IS_UNDEF(module)) return NULL;
  ASSERT(AS_OBJ(module)->type == OBJ_MODULE, OOPS);
  return (Module*)AS_OBJ(module);
}

void vmCollectGarbage(PKVM* vm) {

  // Mark builtin functions.
  for (int i = 0; i < vm->builtins_count; i++) {
    markObject(vm, &vm->builtins_funcs[i]->_super);
  }

  // Mark primitive types' classes.
  for (int i = 0; i < PK_INSTANCE; i++) {
    // It's possible that a garbage collection could be triggered while we're
    // building the primitives and the class could be NULL.
    if (vm->builtin_classes[i] == NULL) continue;

    markObject(vm, &vm->builtin_classes[i]->_super);
  }

  // Mark the modules and search path.
  markObject(vm, &vm->modules->_super);
  markObject(vm, &vm->search_paths->_super);

  // Mark temp references.
  for (int i = 0; i < vm->temp_reference_count; i++) {
    markObject(vm, vm->temp_reference[i]);
  }

  // Mark the handles.
  for (PkHandle* h = vm->handles; h != NULL; h = h->next) {
    markValue(vm, h->value);
  }

  // Garbage collection triggered at the middle of a compilation.
  if (vm->compiler != NULL) {
    compilerMarkObjects(vm, vm->compiler);
  }

  if (vm->fiber != NULL) {
    markObject(vm, &vm->fiber->_super);
  }

  // Reset VM's bytes_allocated value and count it again so that we don't
  // required to know the size of each object that'll be freeing.
  vm->bytes_allocated = 0;

  // Pop the marked objects from the working set and push all of it's
  // referenced objects. This will repeat till no more objects left in the
  // working set.
  popMarkedObjects(vm);

  // Now [vm->bytes_allocated] is equal to the number of bytes allocated for
  // the root objects which are marked above. Since we're garbage collecting
  // freeObject() shouldn't modify vm->bytes_allocated. We ensure this by
  // copying the value to [bytes_allocated] and check after freeing.
#ifdef DEBUG
  size_t bytes_allocated = vm->bytes_allocated;
#endif

  // Now sweep all the un-marked objects in then link list and remove them
  // from the chain.

  // [ptr] is an Object* reference that should be equal to the next
  // non-garbage Object*.
  Object** ptr = &vm->first;
  while (*ptr != NULL) {

    // If the object the pointer points to wasn't marked it's unreachable.
    // Clean it. And update the pointer points to the next object.
    if (!(*ptr)->is_marked) {
      Object* garbage = *ptr;
      *ptr = garbage->next;
      freeObject(vm, garbage);

    } else {
      // Unmark the object for the next garbage collection.
      (*ptr)->is_marked = false;
      ptr = &(*ptr)->next;
    }
  }

#ifdef DEBUG
  ASSERT(bytes_allocated = vm->bytes_allocated, OOPS);
#endif

  // Next GC heap size will be change depends on the byte we've left with now,
  // and the [heap_fill_percent].
  vm->next_gc = vm->bytes_allocated + (
    (vm->bytes_allocated * vm->heap_fill_percent) / 100);
  if (vm->next_gc < vm->min_heap_size) vm->next_gc = vm->min_heap_size;
}

#define _ERR_FAIL(msg)                             \
  do {                                             \
    if (vm->fiber != NULL) VM_SET_ERROR(vm, msg);  \
    return false;                                  \
  } while (false)

bool vmPrepareFiber(PKVM* vm, Fiber* fiber, int argc, Var* argv) {
  ASSERT(fiber->closure->fn->arity >= -1,
         OOPS " (Forget to initialize arity.)");

  if ((fiber->closure->fn->arity != -1) && argc != fiber->closure->fn->arity) {
    char buff[STR_INT_BUFF_SIZE];
    sprintf(buff, "%d", fiber->closure->fn->arity);
    _ERR_FAIL(stringFormat(vm, "Expected exactly $ argument(s) for "
                           "function $.", buff, fiber->closure->fn->name));
  }

  if (fiber->state != FIBER_NEW) {
    switch (fiber->state) {
      case FIBER_NEW: UNREACHABLE();
      case FIBER_RUNNING:
        _ERR_FAIL(newString(vm, "The fiber has already been running."));
      case FIBER_YIELDED:
        _ERR_FAIL(newString(vm, "Cannot run a fiber which is yielded, use "
          "fiber_resume() instead."));
      case FIBER_DONE:
        _ERR_FAIL(newString(vm, "The fiber has done running."));
    }
    UNREACHABLE();
  }

  ASSERT(fiber->stack != NULL && fiber->sp == fiber->stack + 1, OOPS);
  ASSERT(fiber->ret == fiber->stack, OOPS);

  vmEnsureStackSize(vm, fiber, (int) (fiber->sp - fiber->stack) + argc);
  ASSERT((fiber->stack + fiber->stack_size) - fiber->sp >= argc, OOPS);

  // Pass the function arguments.

  // ARG1 is fiber, function arguments are ARG(2), ARG(3), ... ARG(argc).
  // And ret[0] is the return value, parameters starts at ret[1], ...
  for (int i = 0; i < argc; i++) {
    fiber->ret[1 + i] = *(argv + i); // +1: ret[0] is return value.
  }
  fiber->sp += argc; // Parameters.

  // Native functions doesn't own a stack frame so, we're done here.
  if (fiber->closure->fn->is_native) return true;

  // Assert we have the first frame (to push the arguments). And assert we have
  // enough stack space for parameters.
  ASSERT(fiber->frame_count == 1, OOPS);
  ASSERT(fiber->frames[0].rbp == fiber->ret, OOPS);

  // Capture self.
  fiber->frames[0].self = fiber->self;
  fiber->self = VAR_UNDEFINED;

  // On success return true.
  return true;
}

bool vmSwitchFiber(PKVM* vm, Fiber* fiber, Var* value) {
  if (fiber->state != FIBER_YIELDED) {
    switch (fiber->state) {
      case FIBER_NEW:
        _ERR_FAIL(newString(vm, "The fiber hasn't started. call fiber_run() "
          "to start."));
      case FIBER_RUNNING:
        _ERR_FAIL(newString(vm, "The fiber has already been running."));
      case FIBER_YIELDED: UNREACHABLE();
      case FIBER_DONE:
        _ERR_FAIL(newString(vm, "The fiber has done running."));
    }
    UNREACHABLE();
  }

  // Pass the resume argument if it has any.

  // Assert if we have a call frame and the stack size enough for the return
  // value and the resumed value.
  ASSERT(fiber->frame_count != 0, OOPS);
  ASSERT((fiber->stack + fiber->stack_size) - fiber->sp >= 2, OOPS);

  // fb->ret will points to the return value of the 'yield()' call.
  if (value == NULL) *fiber->ret = VAR_NULL;
  else *fiber->ret = *value;

  // Switch fiber.
  fiber->caller = vm->fiber;
  vm->fiber = fiber;

  // On success return true.
  return true;
}

#undef _ERR_FAIL

void vmYieldFiber(PKVM* vm, Var* value) {

  Fiber* caller = vm->fiber->caller;

  // Return the yield value to the caller fiber.
  if (caller != NULL) {
    if (value == NULL) *caller->ret = VAR_NULL;
    else *caller->ret = *value;
  }

  // Can be resumed by another caller fiber.
  vm->fiber->caller = NULL;
  vm->fiber->state = FIBER_YIELDED;
  vm->fiber = caller;
}

PkResult vmCallMethod(PKVM* vm, Var self, Closure* fn,
                      int argc, Var* argv, Var* ret) {
  ASSERT(argc >= 0, "argc cannot be negative.");
  ASSERT(argc == 0 || argv != NULL, "argv was NULL when argc > 0.");

  Fiber* fiber = newFiber(vm, fn);
  fiber->self = self;
  fiber->native = vm->fiber;
  vmPushTempRef(vm, &fiber->_super); // fiber.
  bool success = vmPrepareFiber(vm, fiber, argc, argv);

  if (!success) {
    vmPopTempRef(vm); // fiber.
    return PK_RESULT_RUNTIME_ERROR;
  }

  PkResult result;

  Fiber* last = vm->fiber;
  if (last != NULL) vmPushTempRef(vm, &last->_super); // last.
  {
    if (fiber->closure->fn->is_native) {

      ASSERT(fiber->closure->fn->native != NULL, "Native function was NULL");
      vm->fiber = fiber;
      fiber->closure->fn->native(vm);
      if (VM_HAS_ERROR(vm)) {
        if (last != NULL) last->error = vm->fiber->error;
        result = PK_RESULT_RUNTIME_ERROR;
      } else {
        result = PK_RESULT_SUCCESS;
      }

    } else {
      result = vmRunFiber(vm, fiber);
    }
  }

  if (last != NULL) vmPopTempRef(vm); // last.
  vmPopTempRef(vm); // fiber.

  vm->fiber = last;

  if (ret != NULL) *ret = *fiber->ret;

  return result;
}

PkResult vmCallFunction(PKVM* vm, Closure* fn, int argc, Var* argv, Var* ret) {

  // Calling functions and methods are the same, except for the methods have
  // self defined, and for functions it'll be VAR_UNDEFINED.
  return vmCallMethod(vm, VAR_UNDEFINED, fn, argc, argv, ret);
}

#ifndef PK_NO_DL

// Returns true if the path ends with ".dll" or ".so".
static bool _isPathDL(String* path) {
  const char* dlext[] = {
    ".so",
    ".dll",
    NULL,
  };

  for (const char** ext = dlext; *ext != NULL; ext++) {
    const char* start = path->data + (path->length - strlen(*ext));
    if (!strncmp(start, *ext, strlen(*ext))) return true;
  }

  return false;
}

static Module* _importDL(PKVM* vm, String* resolved, String* name) {
  if (vm->config.import_dl_fn == NULL) {
    VM_SET_ERROR(vm, newString(vm, "Dynamic library importer not provided."));
    return NULL;
  }

  ASSERT(vm->config.load_dl_fn != NULL, OOPS);
  void* handle = vm->config.load_dl_fn(vm, resolved->data);

  if (handle == NULL) {
    VM_SET_ERROR(vm, stringFormat(vm, "Error loading module at \"@\"",
      resolved));
    return NULL;
  }

  // Since the DL library can use stack via slots api, we need to update
  // ret and then restore it back. We're using offset instead of a pointer
  // because the stack might be reallocated if it grows.
  uintptr_t ret_offset = vm->fiber->ret - vm->fiber->stack;
  vm->fiber->ret = vm->fiber->sp;
  PkHandle* pkhandle = vm->config.import_dl_fn(vm, handle);
  vm->fiber->ret = vm->fiber->stack + ret_offset;

  if (pkhandle == NULL) {
    VM_SET_ERROR(vm, stringFormat(vm, "Error loading module at \"@\"",
      resolved));
    return NULL;
  }

  if (!IS_OBJ_TYPE(pkhandle->value, OBJ_MODULE)) {
    VM_SET_ERROR(vm, stringFormat(vm, "Returned handle wasn't a "
                 "module at \"@\"", resolved));
    return NULL;
  }

  Module* module = (Module*) AS_OBJ(pkhandle->value);
  module->name = name;
  module->path = resolved;
  module->handle = handle;
  vmRegisterModule(vm, module, resolved);

  pkReleaseHandle(vm, pkhandle);
  return module;
}

void vmUnloadDlHandle(PKVM* vm, void* handle) {
  if (handle && vm->config.unload_dl_fn)
    vm->config.unload_dl_fn(vm, handle);
}
#endif // PK_NO_DL

/*****************************************************************************/
/* VM INTERNALS                                                              */
/*****************************************************************************/

static Module* _importScript(PKVM* vm, String* resolved, String* name) {
  char* source = vm->config.load_script_fn(vm, resolved->data);
  if (source == NULL) {
    VM_SET_ERROR(vm, stringFormat(vm, "Error loading module at \"@\"",
      resolved));
    return NULL;
  }

  // Make a new module, compile and cache it.
  Module* module = newModule(vm);
  module->path = resolved;
  module->name = name;

  vmPushTempRef(vm, &module->_super); // module.
  {
    initializeModule(vm, module, false);
    PkResult result = compile(vm, module, source, NULL);
    pkRealloc(vm, source, 0);
    if (result == PK_RESULT_SUCCESS) {
      vmRegisterModule(vm, module, resolved);
    } else {
      VM_SET_ERROR(vm, stringFormat(vm, "Error compiling module at \"@\"",
                   resolved));
      module = NULL; //< set to null to indicate error.
    }
  }
  vmPopTempRef(vm); // module.

  return module;
}

// Import and return the Module object with the [path] string. If the path
// starts with with './' or '../' we'll only try relative imports, otherwise
// we'll search native modules first and then at relative path.
Var vmImportModule(PKVM* vm, String* from, String* path) {
  ASSERT((path != NULL) && (path->length > 0), OOPS);

  bool is_relative = path->data[0] == '.';

  // If not relative check the [path] in the modules cache with the name
  // (before resolving the path).
  if (!is_relative) {
    // If not relative path we first search in modules cache. It'll find the
    // native module or the already imported cache of the script.
    Var entry = mapGet(vm->modules, VAR_OBJ(path));
    if (!IS_UNDEF(entry)) {
      ASSERT(AS_OBJ(entry)->type == OBJ_MODULE, OOPS);
      return entry; // We're done.
    }
  }
  char* _resolved = NULL;

  const char* from_path = (from) ? from->data : NULL;
  uint32_t search_path_idx = 0;

  do {
    // If we reached here. It's not a native module (ie. module's absolute path
    // is required to import and cache).
    _resolved = vm->config.resolve_path_fn(vm, from_path, path->data);
    if (_resolved) break;

    if (search_path_idx >= vm->search_paths->elements.count) break;

    Var sp = vm->search_paths->elements.data[search_path_idx++];
    ASSERT(IS_OBJ_TYPE(sp, OBJ_STRING), OOPS);
    from_path = ((String*) AS_OBJ(sp))->data;

  } while (true);

  if (_resolved == NULL) { // Can't resolve a relative module.
    pkRealloc(vm, _resolved, 0);
    VM_SET_ERROR(vm, stringFormat(vm, "Cannot import module '@'", path));
    return VAR_NULL;
  }

  String* resolved = newString(vm, _resolved);
  pkRealloc(vm, _resolved, 0);

  // If the script already imported and cached, return it.
  Var entry = mapGet(vm->modules, VAR_OBJ(resolved));
  if (!IS_UNDEF(entry)) {
    ASSERT(AS_OBJ(entry)->type == OBJ_MODULE, OOPS);
    return entry; // We're done.
  }

  // The script not exists in the VM, make sure we have the script loading
  // api function.

  #ifndef PK_NO_DL
  bool isdl = _isPathDL(resolved);
  if (isdl && vm->config.load_dl_fn == NULL
      || vm->config.load_script_fn == NULL) {
  #else
  if (vm->config.load_script_fn == NULL) {
  #endif

    VM_SET_ERROR(vm, newString(vm, "Cannot import. The hosting application "
                               "haven't registered the module loading API"));
    return VAR_NULL;
  }

  Module* module = NULL;

  vmPushTempRef(vm, &resolved->_super); // resolved.
  {
    // FIXME:
    // stringReplace() function expect 2 strings old, and new to replace but
    // we cannot afford to allocate new strings for single char, so we're
    // replaceing and rehashing the string here. I should add some update
    // string function that update a string after it's data was modified.
    //
    // The path of the module contain '/' which was replacement of '.' in the
    // import syntax, this is done so that path resolving can be done easily.
    // However it needs to be '.' for the name of the module.
    String* _name = newStringLength(vm, path->data, path->length);
    for (char* c = _name->data; c < _name->data + _name->length; c++) {
      if (*c == '/') *c = '.';
    }
    _name->hash = utilHashString(_name->data);
    vmPushTempRef(vm, &_name->_super); // _name.

    #ifndef PK_NO_DL
    if (isdl) module = _importDL(vm, resolved, _name);
    else /* ... */
    #endif
    module = _importScript(vm, resolved, _name);

    vmPopTempRef(vm); // _name.
  }
  vmPopTempRef(vm); // resolved.

  if (module == NULL) {
    ASSERT(VM_HAS_ERROR(vm), OOPS);
    return VAR_NULL;
  }

  return VAR_OBJ(module);
}

void vmEnsureStackSize(PKVM* vm, Fiber* fiber, int size) {

  if (size >= (MAX_STACK_SIZE / sizeof(Var))) {
    VM_SET_ERROR(vm, newString(vm, "Maximum stack limit reached."));
    return;
  }

  if (fiber->stack_size >= size) return;

  int new_size = utilPowerOf2Ceil(size);

  Var* old_rbp = fiber->stack; //< Old stack base pointer.
  fiber->stack = (Var*)vmRealloc(vm, fiber->stack,
                                 sizeof(Var) * fiber->stack_size,
                                 sizeof(Var) * new_size);
  fiber->stack_size = new_size;

  // If the old stack base pointer is the same as the current, that means the
  // stack hasn't been moved by the reallocation. In that case we're done.
  if (old_rbp == fiber->stack) return;

  // If we reached here that means the stack is moved by the reallocation and
  // we have to update all the pointers that pointing to the old stack slots.

  //
  //                                     '        '
  //             '        '              '        '
  //             '        '              |        | <new_rsp
  //    old_rsp> |        |              |        |
  //             |        |       .----> | value  | <new_ptr
  //             |        |       |      |        |
  //    old_ptr> | value  | ------'      |________| <new_rbp
  //             |        | ^            new stack
  //    old_rbp> |________| | height
  //             old stack
  //
  //            new_ptr = new_rbp      + height
  //                    = fiber->stack + ( old_ptr  - old_rbp )
#define MAP_PTR(old_ptr) (fiber->stack + ((old_ptr) - old_rbp))

  // Update the stack top pointer and the return pointer.
  fiber->sp = MAP_PTR(fiber->sp);
  fiber->ret = MAP_PTR(fiber->ret);

  // Update the stack base pointer of the call frames.
  for (int i = 0; i < fiber->frame_count; i++) {
    CallFrame* frame = fiber->frames + i;
    frame->rbp = MAP_PTR(frame->rbp);
  }
}

// The return address for the next call frame (rbp) has to be set to the
// fiber's ret (fiber->ret == next rbp).
static inline void pushCallFrame(PKVM* vm, const Closure* closure) {
  ASSERT(!closure->fn->is_native, OOPS);
  ASSERT(vm->fiber->ret != NULL, OOPS);

  // Grow the stack frame if needed.
  if (vm->fiber->frame_count + 1 > vm->fiber->frame_capacity) {

    // Native functions doesn't allocate a frame initially.
    int new_capacity = vm->fiber->frame_capacity << 1;
    if (new_capacity == 0) new_capacity = 1;

    vm->fiber->frames = (CallFrame*)vmRealloc(vm, vm->fiber->frames,
                           sizeof(CallFrame) * vm->fiber->frame_capacity,
                           sizeof(CallFrame) * new_capacity);
    vm->fiber->frame_capacity = new_capacity;
  }

  // Grow the stack if needed.
  int current_stack_slots = (int)(vm->fiber->sp - vm->fiber->stack) + 1;
  int needed = closure->fn->fn->stack_size + current_stack_slots;
  vmEnsureStackSize(vm, vm->fiber, needed);

  CallFrame* frame = vm->fiber->frames + vm->fiber->frame_count++;
  frame->rbp = vm->fiber->ret;
  frame->closure = closure;
  frame->ip = closure->fn->fn->opcodes.data;

  // Capture self.
  frame->self = vm->fiber->self;
  vm->fiber->self = VAR_UNDEFINED;
}

static inline void reuseCallFrame(PKVM* vm, const Closure* closure) {

  ASSERT(!closure->fn->is_native, OOPS);
  ASSERT(closure->fn->arity >= 0, OOPS);
  ASSERT(vm->fiber->frame_count > 0, OOPS);

  Fiber* fb = vm->fiber;

  CallFrame* frame = fb->frames + fb->frame_count - 1;
  frame->closure = closure;
  frame->ip = closure->fn->fn->opcodes.data;

  // Capture self.
  frame->self = vm->fiber->self;
  vm->fiber->self = VAR_UNDEFINED;

  ASSERT(*frame->rbp == VAR_NULL, OOPS);

  // Move all the argument(s) to the base of the current frame.
  Var* arg = fb->sp - closure->fn->arity;
  Var* target = frame->rbp + 1;
  for (; arg < fb->sp; arg++, target++) {
    *target = *arg;
  }

  // At this point target points to the stack pointer of the next call.
  fb->sp = target;

  // Grow the stack if needed (least probably).
  int needed = (closure->fn->fn->stack_size +
                (int)(vm->fiber->sp - vm->fiber->stack));
  vmEnsureStackSize(vm, vm->fiber, needed);
}

// Capture the [local] into an upvalue and return it. If the upvalue already
// exists on the fiber, it'll return it.
static Upvalue* captureUpvalue(PKVM* vm, Fiber* fiber, Var* local) {

  // If the fiber doesn't have any upvalues yet, create new one and add it.
  if (fiber->open_upvalues == NULL) {
    Upvalue* upvalue = newUpvalue(vm, local);
    fiber->open_upvalues = upvalue;
    return upvalue;
  }

  // In the bellow diagram 'u0' is the head of the open upvalues of the fiber.
  // We'll walk through the upvalues to see if any of it's value is similar
  // to the [local] we want to capture.
  //
  // This can be optimized with binary search since the upvalues are sorted
  // but it's not a frequent task neither the number of upvalues would be very
  // few and the local mostly located at the stack top.
  //
  // 1. If say 'l3' is what we want to capture, that local already has an
  //    upavlue 'u1' return it.
  // 2. If say 'l4' is what we want to capture, It doesn't have an upvalue yet.
  //    Create a new upvalue and insert to the link list (ie. u1.next = u3,
  //    u3.next = u2) and return it.
  //
  //           |      |
  //           |  l1  | <-- u0 (u1.value = l3)
  //           |  l2  |     |
  //           |  l3  | <-- u1 (u1.value = l3)
  //           |  l4  |     |
  //           |  l5  | <-- u2 (u2.value = l5)
  //           '------'     |
  //            stack       NULL

  // Edge case: if the local is located higher than all the open upvalues, we
  // cannot walk the chain, it's going to be the new head of the open upvalues.
  if (fiber->open_upvalues->ptr < local) {
    Upvalue* head = newUpvalue(vm, local);
    head->next = fiber->open_upvalues;
    fiber->open_upvalues = head;
    return head;
  }

  // Now we walk the chain of open upvalues and if we find an upvalue for the
  // local return it, otherwise insert it in the chain.
  Upvalue* last = NULL;
  Upvalue* current = fiber->open_upvalues;

  while (current->ptr > local) {
    last = current;
    current = current->next;

    // If the current is NULL, we've walked all the way to the end of the open
    // upvalues, and there isn't one upvalue for the local.
    if (current == NULL) {
      last->next = newUpvalue(vm, local);
      return last->next;
    }
  }

  // If [current] is the upvalue that captured [local] then return it.
  if (current->ptr == local) return current;

  ASSERT(last != NULL, OOPS);

  // If we've reached here, the upvalue isn't found, create a new one and
  // insert it to the chain.
  Upvalue* upvalue = newUpvalue(vm, local);
  last->next = upvalue;
  upvalue->next = current;
  return upvalue;
}

// Close all the upvalues for the locals including [top] and higher in the
// stack.
static void closeUpvalues(Fiber* fiber, Var* top) {

  while (fiber->open_upvalues != NULL && fiber->open_upvalues->ptr >= top) {
    Upvalue* upvalue = fiber->open_upvalues;
    upvalue->closed = *upvalue->ptr;
    upvalue->ptr = &upvalue->closed;

    fiber->open_upvalues = upvalue->next;
  }
}

static void vmReportError(PKVM* vm) {
  ASSERT(VM_HAS_ERROR(vm), "runtimeError() should be called after an error.");

  // TODO: pass the error to the caller of the fiber.

  if (vm->config.stderr_write == NULL) return;
  reportRuntimeError(vm, vm->fiber);
}

/******************************************************************************
 * RUNTIME                                                                    *
 *****************************************************************************/

PkResult vmRunFiber(PKVM* vm, Fiber* fiber_) {

  // Set the fiber as the VM's current fiber (another root object) to prevent
  // it from garbage collection and get the reference from native functions.
  // If this is being called when running another fiber, that'll be garbage
  // collected, if protected with vmPushTempRef() by the caller otherwise.
  vm->fiber = fiber_;

  ASSERT(fiber_->state == FIBER_NEW || fiber_->state == FIBER_YIELDED, OOPS);
  fiber_->state = FIBER_RUNNING;

  // The instruction pointer.
  register const uint8_t* ip;

  register Var* rbp;         //< Stack base pointer register.
  register Var* self;        //< Points to the self in the current call frame.
  register CallFrame* frame; //< Current call frame.
  register Module* module;   //< Currently executing module.
  register Fiber* fiber = fiber_;

#if DEBUG
  #define PUSH(value)                                                       \
  do {                                                                      \
    ASSERT(fiber->sp < (fiber->stack + ((intptr_t) fiber->stack_size - 1)), \
           OOPS);                                                           \
    (*fiber->sp++ = (value));                                               \
  } while (false)
#else
  #define PUSH(value)  (*fiber->sp++ = (value))
#endif

#define POP()        (*(--fiber->sp))
#define DROP()       (--fiber->sp)
#define PEEK(off)    (*(fiber->sp + (off)))
#define READ_BYTE()  (*ip++)
#define READ_SHORT() (ip+=2, (uint16_t)((ip[-2] << 8) | ip[-1]))

// Switch back to the caller of the current fiber, will be called when we're
// done with the fiber or aborting it for runtime errors.
#define FIBER_SWITCH_BACK()                                         \
  do {                                                              \
    Fiber* caller = fiber->caller;                                  \
    ASSERT(caller == NULL || caller->state == FIBER_RUNNING, OOPS); \
    fiber->state = FIBER_DONE;                                      \
    fiber->caller = NULL;                                           \
    fiber = caller;                                                 \
    vm->fiber = fiber;                                              \
  } while (false)

// Check if any runtime error exists and if so returns RESULT_RUNTIME_ERROR.
#define CHECK_ERROR()                 \
  do {                                \
    if (VM_HAS_ERROR(vm)) {           \
      UPDATE_FRAME();                 \
      vmReportError(vm);              \
      FIBER_SWITCH_BACK();            \
      return PK_RESULT_RUNTIME_ERROR; \
    }                                 \
  } while (false)

// [err_msg] must be of type String.
#define RUNTIME_ERROR(err_msg)       \
  do {                               \
    VM_SET_ERROR(vm, err_msg);       \
    UPDATE_FRAME();                  \
    vmReportError(vm);               \
    FIBER_SWITCH_BACK();             \
    return PK_RESULT_RUNTIME_ERROR;  \
  } while (false)

// Load the last call frame to vm's execution variables to resume/run the
// function.
#define LOAD_FRAME()                               \
  do {                                             \
    frame = &fiber->frames[fiber->frame_count-1];  \
    ip = frame->ip;                                \
    rbp = frame->rbp;                              \
    self = &frame->self;                           \
    module = frame->closure->fn->owner;            \
  } while (false)

// Update the frame's execution variables before pushing another call frame.
#define UPDATE_FRAME() frame->ip = ip

#ifdef OPCODE
  #error "OPCODE" should not be deifined here.
#endif

#define SWITCH() Opcode instruction; switch (instruction = (Opcode)READ_BYTE())
#define OPCODE(code) case OP_##code
#define DISPATCH()   goto L_vm_main_loop

  // Load the fiber's top call frame to the vm's execution variables.
  LOAD_FRAME();

L_vm_main_loop:
  // This NO_OP is required since Labels can only be followed by statements
  // and, declarations are not statements, If the macro DUMP_STACK isn't
  // defined, the next line become a declaration (Opcode instruction;).
  NO_OP;

#define _DUMP_STACK()           \
  do {                          \
    system("cls"); /* FIXME: */ \
    dumpGlobalValues(vm);       \
    dumpStackFrame(vm);         \
    DEBUG_BREAK();              \
  } while (false)

#if DUMP_STACK && defined(DEBUG)
  _DUMP_STACK();
#endif
#undef _DUMP_STACK

  SWITCH() {

    OPCODE(PUSH_CONSTANT):
    {
      uint16_t index = READ_SHORT();
      ASSERT_INDEX(index, module->constants.count);
      PUSH(module->constants.data[index]);
      DISPATCH();
    }

    OPCODE(PUSH_NULL):
      PUSH(VAR_NULL);
      DISPATCH();

    OPCODE(PUSH_0):
      PUSH(VAR_NUM(0));
      DISPATCH();

    OPCODE(PUSH_TRUE):
      PUSH(VAR_TRUE);
      DISPATCH();

    OPCODE(PUSH_FALSE):
      PUSH(VAR_FALSE);
      DISPATCH();

    OPCODE(SWAP):
    {
      Var tmp = *(fiber->sp - 1);
      *(fiber->sp - 1) = *(fiber->sp - 2);
      *(fiber->sp - 2) = tmp;
      DISPATCH();
    }

    OPCODE(DUP):
    {
      PUSH(*(fiber->sp - 1));
      DISPATCH();
    }

    OPCODE(PUSH_LIST):
    {
      List* list = newList(vm, (uint32_t)READ_SHORT());
      PUSH(VAR_OBJ(list));
      DISPATCH();
    }

    OPCODE(PUSH_MAP):
    {
      Map* map = newMap(vm);
      PUSH(VAR_OBJ(map));
      DISPATCH();
    }

    OPCODE(PUSH_SELF):
    {
      PUSH(*self);
      DISPATCH();
    }

    OPCODE(LIST_APPEND):
    {
      Var elem = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var list = PEEK(-2);
      ASSERT(IS_OBJ_TYPE(list, OBJ_LIST), OOPS);
      pkVarBufferWrite(&((List*)AS_OBJ(list))->elements, vm, elem);
      DROP(); // elem
      DISPATCH();
    }

    OPCODE(MAP_INSERT):
    {
      Var value = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var key = PEEK(-2);   // Don't pop yet, we need the reference for gc.
      Var on = PEEK(-3);

      ASSERT(IS_OBJ_TYPE(on, OBJ_MAP), OOPS);

      if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
        RUNTIME_ERROR(stringFormat(vm, "$ type is not hashable.",
                      varTypeName(key)));
      }
      mapSet(vm, (Map*)AS_OBJ(on), key, value);

      DROP(); // value
      DROP(); // key

      DISPATCH();
    }

    OPCODE(PUSH_LOCAL_0):
    OPCODE(PUSH_LOCAL_1):
    OPCODE(PUSH_LOCAL_2):
    OPCODE(PUSH_LOCAL_3):
    OPCODE(PUSH_LOCAL_4):
    OPCODE(PUSH_LOCAL_5):
    OPCODE(PUSH_LOCAL_6):
    OPCODE(PUSH_LOCAL_7):
    OPCODE(PUSH_LOCAL_8):
    {
      int index = (int)(instruction - OP_PUSH_LOCAL_0);
      PUSH(rbp[index + 1]); // +1: rbp[0] is return value.
      DISPATCH();
    }
    OPCODE(PUSH_LOCAL_N):
    {
      uint8_t index = READ_BYTE();
      PUSH(rbp[index + 1]);  // +1: rbp[0] is return value.
      DISPATCH();
    }

    OPCODE(STORE_LOCAL_0):
    OPCODE(STORE_LOCAL_1):
    OPCODE(STORE_LOCAL_2):
    OPCODE(STORE_LOCAL_3):
    OPCODE(STORE_LOCAL_4):
    OPCODE(STORE_LOCAL_5):
    OPCODE(STORE_LOCAL_6):
    OPCODE(STORE_LOCAL_7):
    OPCODE(STORE_LOCAL_8):
    {
      int index = (int)(instruction - OP_STORE_LOCAL_0);
      rbp[index + 1] = PEEK(-1);  // +1: rbp[0] is return value.
      DISPATCH();
    }
    OPCODE(STORE_LOCAL_N):
    {
      uint8_t index = READ_BYTE();
      rbp[index + 1] = PEEK(-1);  // +1: rbp[0] is return value.
      DISPATCH();
    }

    OPCODE(PUSH_GLOBAL):
    {
      uint8_t index = READ_BYTE();
      ASSERT_INDEX(index, module->globals.count);
      PUSH(module->globals.data[index]);
      DISPATCH();
    }

    OPCODE(STORE_GLOBAL):
    {
      uint8_t index = READ_BYTE();
      ASSERT_INDEX(index, module->globals.count);
      module->globals.data[index] = PEEK(-1);
      DISPATCH();
    }

    OPCODE(PUSH_BUILTIN_FN):
    {
      uint8_t index = READ_BYTE();
      ASSERT_INDEX(index, vm->builtins_count);
      Closure* closure = vm->builtins_funcs[index];
      PUSH(VAR_OBJ(closure));
      DISPATCH();
    }

    OPCODE(PUSH_BUILTIN_TY):
    {
      uint8_t index = READ_BYTE();
      ASSERT_INDEX(index, PK_INSTANCE);
      Class* cls = vm->builtin_classes[index];
      PUSH(VAR_OBJ(cls));
      DISPATCH();
    }

    OPCODE(PUSH_UPVALUE):
    {
      uint8_t index = READ_BYTE();
      PUSH(*(frame->closure->upvalues[index]->ptr));
      DISPATCH();
    }

    OPCODE(STORE_UPVALUE):
    {
      uint8_t index = READ_BYTE();
      *(frame->closure->upvalues[index]->ptr) = PEEK(-1);
      DISPATCH();
    }

    OPCODE(PUSH_CLOSURE):
    {
      uint16_t index = READ_SHORT();
      ASSERT_INDEX(index, module->constants.count);
      ASSERT(IS_OBJ_TYPE(module->constants.data[index], OBJ_FUNC), OOPS);
      Function* fn = (Function*)AS_OBJ(module->constants.data[index]);

      Closure* closure = newClosure(vm, fn);
      vmPushTempRef(vm, &closure->_super); // closure.

      // Capture the vaupes.
      for (int i = 0; i < fn->upvalue_count; i++) {
        uint8_t is_immediate = READ_BYTE();
        uint8_t idx = READ_BYTE();

        if (is_immediate) {
          // rbp[0] is the return value, rbp + 1 is the first local and so on.
          closure->upvalues[i] = captureUpvalue(vm, fiber, (rbp + 1 + idx));
        } else {
          // The upvalue is already captured by the current function, reuse it.
          closure->upvalues[i] = frame->closure->upvalues[idx];
        }
      }

      PUSH(VAR_OBJ(closure));
      vmPopTempRef(vm); // closure.

      DISPATCH();
    }

    OPCODE(CREATE_CLASS):
    {
      Var cls = POP();
      if (!IS_OBJ_TYPE(cls, OBJ_CLASS)) {
        RUNTIME_ERROR(newString(vm, "Cannot inherit a non class object."));
      }

      Class* base = (Class*)AS_OBJ(cls);

      // All Builtin type class except for Object are "final" ie. cannot be
      // inherited from.
      if (base->class_of != PK_INSTANCE && base->class_of != PK_OBJECT) {
        RUNTIME_ERROR(stringFormat(vm, "$ type cannot be inherited.",
                      getPkVarTypeName(base->class_of)));
      }

      uint16_t index = READ_SHORT();
      ASSERT_INDEX(index, module->constants.count);
      ASSERT(IS_OBJ_TYPE(module->constants.data[index], OBJ_CLASS), OOPS);

      Class* drived = (Class*)AS_OBJ(module->constants.data[index]);
      drived->super_class = base;

      PUSH(VAR_OBJ(drived));
      DISPATCH();
    }

    OPCODE(BIND_METHOD):
    {
      ASSERT(IS_OBJ_TYPE(PEEK(-1), OBJ_CLOSURE), OOPS);
      ASSERT(IS_OBJ_TYPE(PEEK(-2), OBJ_CLASS), OOPS);

      Closure* method = (Closure*)AS_OBJ(PEEK(-1));
      Class* cls = (Class*)AS_OBJ(PEEK(-2));

      if (strcmp(method->fn->name, CTOR_NAME) == 0) {
        cls->ctor = method;
      }

      pkClosureBufferWrite(&cls->methods, vm, method);

      DROP();
      DISPATCH();
    }

    OPCODE(CLOSE_UPVALUE):
    {
      closeUpvalues(fiber, fiber->sp - 1);
      DROP();
      DISPATCH();
    }

    OPCODE(POP):
      DROP();
      DISPATCH();

    OPCODE(IMPORT):
    {
      uint16_t index = READ_SHORT();
      String* name = moduleGetStringAt(module, (int)index);
      ASSERT(name != NULL, OOPS);

      Var _imported = vmImportModule(vm, module->path, name);
      CHECK_ERROR();
      ASSERT(IS_OBJ_TYPE(_imported, OBJ_MODULE), OOPS);

      PUSH(_imported);

      Module* imported = (Module*)AS_OBJ(_imported);
      if (!imported->initialized) {
        imported->initialized = true;

        ASSERT(imported->body != NULL, OOPS);

        UPDATE_FRAME(); //< Update the current frame's ip.

        // Note that we're setting the main function's return address to the
        // module itself (for every other function we'll push a null at the rbp
        // before calling them and it'll be returned without modified if the
        // function doesn't returned anything). Also We can't return from the
        // body of the module, so the main function will return what's at the
        // rbp without modifying it. So at the end of the main function the
        // stack top would be the module itself.
        fiber->ret = fiber->sp - 1;
        pushCallFrame(vm, imported->body);

        LOAD_FRAME();  //< Load the top frame to vm's execution variables.
        CHECK_ERROR(); //< Stack overflow.
      }

      DISPATCH();
    }

    {
      uint8_t argc;
      Var callable;
      const Closure* closure;

      uint16_t index; //< To get the method name.
      String* name; //< The method name.

    OPCODE(SUPER_CALL):
        argc = READ_BYTE();
        fiber->ret = (fiber->sp - argc - 1);
        fiber->self = *fiber->ret; //< Self for the next call.
        index = READ_SHORT();
        name = moduleGetStringAt(module, (int)index);
        Closure* super_method = getSuperMethod(vm, fiber->self, name);
        CHECK_ERROR(); // Will return if super_method is NULL.
        callable = VAR_OBJ(super_method);
      goto L_do_call;

    OPCODE(METHOD_CALL):
      argc = READ_BYTE();
      fiber->ret = (fiber->sp - argc - 1);
      fiber->self = *fiber->ret; //< Self for the next call.

      index = READ_SHORT();
      name = moduleGetStringAt(module, (int)index);
      callable = getMethod(vm, fiber->self, name, NULL);
      CHECK_ERROR();
      goto L_do_call;

    OPCODE(CALL):
    OPCODE(TAIL_CALL):
      argc = READ_BYTE();
      fiber->ret = fiber->sp - argc - 1;
      callable = *fiber->ret;

L_do_call:
      // Raw functions cannot be on the stack, since they're not first class
      // citizens.
      ASSERT(!IS_OBJ_TYPE(callable, OBJ_FUNC), OOPS);

      *(fiber->ret) = VAR_NULL; //< Set the return value to null.

      if (IS_OBJ_TYPE(callable, OBJ_CLOSURE)) {
        closure = (const Closure*)AS_OBJ(callable);

      } else if (IS_OBJ_TYPE(callable, OBJ_METHOD_BIND)) {
        const MethodBind* mb = (const MethodBind*) AS_OBJ(callable);
        if (IS_UNDEF(mb->instance)) {
          RUNTIME_ERROR(newString(vm, "Cannot call an unbound method."));
          CHECK_ERROR();
        }
        fiber->self = mb->instance;
        closure = mb->method;

      } else if (IS_OBJ_TYPE(callable, OBJ_CLASS)) {
        Class* cls = (Class*)AS_OBJ(callable);

        // Allocate / create a new self before calling constructor on it.
        fiber->self = preConstructSelf(vm, cls);
        CHECK_ERROR();

        // Note:
        // For pocketlang instance the constructor will update self and return
        // the instance (which might not be necessary since we're setting it
        // here).
        *fiber->ret = fiber->self;

        closure = (const Closure*)(cls)->ctor;
        while (closure == NULL) {
          cls = cls->super_class;
          if (cls == NULL) break;
          closure = cls->ctor;
        }

        // No constructor is defined on the class. Just return self.
        if (closure == NULL) {
          if (argc != 0) {
            String* msg = stringFormat(vm, "Expected exactly 0 argument(s) "
                                       "for constructor $.", cls->name->data);
            RUNTIME_ERROR(msg);
          }

          fiber->self = VAR_UNDEFINED;
          DISPATCH();
        }

      } else {
        RUNTIME_ERROR(stringFormat(vm, "$ '$'.", "Expected a callable to "
                      "call, instead got",
                      varTypeName(callable)));
      }

      // If we reached here it's a valid callable.
      ASSERT(closure != NULL, OOPS);

      // -1 argument means multiple number of args.
      if (closure->fn->arity != -1 && closure->fn->arity != argc) {
        char buff[STR_INT_BUFF_SIZE]; sprintf(buff, "%d", closure->fn->arity);
        String* msg = stringFormat(vm, "Expected exactly $ argument(s) "
                                  "for function $", buff, closure->fn->name);
        RUNTIME_ERROR(msg);
      }

      if (closure->fn->is_native) {

        if (closure->fn->native == NULL) {
          RUNTIME_ERROR(stringFormat(vm,
            "Native function pointer of $ was NULL.", closure->fn->name));
        }

        // Update the current frame's ip.
        UPDATE_FRAME();

        closure->fn->native(vm); //< Call the native function.

        // Calling yield() will change vm->fiber to it's caller fiber, which
        // would be null if we're not running the function with a fiber.
        if (vm->fiber == NULL) return PK_RESULT_SUCCESS;

        // Pop function arguments except for the return value.
        // Note that calling fiber_new() and yield() would change the
        // vm->fiber so we're using fiber.
        fiber->sp = fiber->ret + 1;

        // If the fiber has changed, Load the top frame to vm's execution
        // variables.
        if (vm->fiber != fiber) {
          fiber = vm->fiber;
          LOAD_FRAME();
        }

        CHECK_ERROR();

      } else {

        if (instruction == OP_TAIL_CALL) {
          reuseCallFrame(vm, closure);
          LOAD_FRAME();  //< Re-load the frame to vm's execution variables.

        } else {
          ASSERT((instruction == OP_CALL) ||
                 (instruction == OP_METHOD_CALL) ||
                 (instruction == OP_SUPER_CALL), OOPS);

          UPDATE_FRAME(); //< Update the current frame's ip.
          pushCallFrame(vm, closure);
          LOAD_FRAME();  //< Load the top frame to vm's execution variables.
          CHECK_ERROR(); //< Stack overflow.
        }
      }

      DISPATCH();
    }

    OPCODE(ITER_TEST):
    {
      Var seq = PEEK(-3);

      // Primitive types are not iterable.
      if (!IS_OBJ(seq)) {
        if (IS_NULL(seq)) {
          RUNTIME_ERROR(newString(vm, "Null is not iterable."));
        } else if (IS_BOOL(seq)) {
          RUNTIME_ERROR(newString(vm, "Boolenan is not iterable."));
        } else if (IS_NUM(seq)) {
          RUNTIME_ERROR(newString(vm, "Number is not iterable."));
        } else {
          UNREACHABLE();
        }
      }

      DISPATCH();
    }

    // TODO: move this to a function in pk_core.c.
    OPCODE(ITER):
    {
      Var* value    = (fiber->sp - 1);
      Var* iterator = (fiber->sp - 2);
      Var seq       = PEEK(-3);
      uint16_t jump_offset = READ_SHORT();

    #define JUMP_ITER_EXIT() \
      do {                   \
        ip += jump_offset;   \
        DISPATCH();          \
      } while (false)

      ASSERT(IS_NUM(*iterator), OOPS);
      double it = AS_NUM(*iterator); //< Nth iteration.
      ASSERT(AS_NUM(*iterator) == (int32_t)trunc(it), OOPS);

      Object* obj = AS_OBJ(seq);
      switch (obj->type) {

        case OBJ_STRING: {
          uint32_t iter = (int32_t)trunc(it);

          // TODO: // Need to consider utf8.
          String* str = ((String*)obj);
          if (iter >= str->length) JUMP_ITER_EXIT();

          //TODO: vm's char (and reusable) strings.
          *value = VAR_OBJ(newStringLength(vm, str->data + iter, 1));
          *iterator = VAR_NUM((double)iter + 1);

        } DISPATCH();

        case OBJ_LIST: {
          uint32_t iter = (int32_t)trunc(it);
          pkVarBuffer* elems = &((List*)obj)->elements;
          if (iter >= elems->count) JUMP_ITER_EXIT();
          *value = elems->data[iter];
          *iterator = VAR_NUM((double)iter + 1);

        } DISPATCH();

        case OBJ_MAP: {
          uint32_t iter = (int32_t)trunc(it);

          Map* map = (Map*)obj;
          if (map->entries == NULL) JUMP_ITER_EXIT();
          MapEntry* e = map->entries + iter;
          for (; iter < map->capacity; iter++, e++) {
            if (!IS_UNDEF(e->key)) break;
          }
          if (iter >= map->capacity) JUMP_ITER_EXIT();

          *value = map->entries[iter].key;
          *iterator = VAR_NUM((double)iter + 1);

        } DISPATCH();

        case OBJ_RANGE: {
          double from = ((Range*)obj)->from;
          double to = ((Range*)obj)->to;
          if (from == to) JUMP_ITER_EXIT();

          double current;
          if (from <= to) { //< Straight range.
            current = from + it;
          } else {          //< Reversed range.
            current = from - it;
          }
          if (current == to) JUMP_ITER_EXIT();
          *value = VAR_NUM(current);
          *iterator = VAR_NUM(it + 1);

        } DISPATCH();

        case OBJ_MODULE:
        case OBJ_FUNC:
        case OBJ_CLOSURE:
        case OBJ_METHOD_BIND:
        case OBJ_UPVALUE:
        case OBJ_FIBER:
        case OBJ_CLASS:
        case OBJ_INST:
          TODO; break;
        default:
          UNREACHABLE();
      }

      DISPATCH();
    }

    OPCODE(JUMP):
    {
      uint16_t offset = READ_SHORT();
      ip += offset;
      DISPATCH();
    }

    OPCODE(LOOP):
    {
      uint16_t offset = READ_SHORT();
      ip -= offset;
      DISPATCH();
    }

    OPCODE(JUMP_IF):
    {
      Var cond = POP();
      uint16_t offset = READ_SHORT();
      if (toBool(cond)) {
        ip += offset;
      }
      DISPATCH();
    }

    OPCODE(JUMP_IF_NOT):
    {
      Var cond = POP();
      uint16_t offset = READ_SHORT();
      if (!toBool(cond)) {
        ip += offset;
      }
      DISPATCH();
    }

    OPCODE(OR):
    {
      Var cond = PEEK(-1);
      uint16_t offset = READ_SHORT();
      if (toBool(cond)) {
        ip += offset;
      } else {
        DROP();
      }
      DISPATCH();
    }

    OPCODE(AND):
    {
      Var cond = PEEK(-1);
      uint16_t offset = READ_SHORT();
      if (!toBool(cond)) {
        ip += offset;
      } else {
        DROP();
      }
      DISPATCH();
    }

    OPCODE(RETURN):
    {

      // Close all the locals of the current frame.
      closeUpvalues(fiber, rbp + 1);

      // Set the return value.
      Var ret_value = POP();

      // Pop the last frame, and if no more call frames, we're done with the
      // current fiber.
      if (--fiber->frame_count == 0) {
        // TODO: if we're evaluating an expression we need to set it's
        // value on the stack.
        //fiber->sp = fiber->stack; ??

        if (fiber->caller == NULL) {
          *fiber->ret = ret_value;
          return PK_RESULT_SUCCESS;

        } else {
          FIBER_SWITCH_BACK();
          *fiber->ret = ret_value;
        }

      } else {
        *rbp = ret_value;
        // Pop the params (locals should have popped at this point) and update
        // stack pointer.
        fiber->sp = rbp + 1; // +1: rbp is returned value.
      }

      LOAD_FRAME();
      DISPATCH();
    }

    OPCODE(GET_ATTRIB):
    {
      Var on = PEEK(-1); // Don't pop yet, we need the reference for gc.
      String* name = moduleGetStringAt(module, READ_SHORT());
      ASSERT(name != NULL, OOPS);
      Var value = varGetAttrib(vm, on, name);
      DROP(); // on
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GET_ATTRIB_KEEP):
    {
      Var on = PEEK(-1);
      String* name = moduleGetStringAt(module, READ_SHORT());
      ASSERT(name != NULL, OOPS);
      PUSH(varGetAttrib(vm, on, name));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(SET_ATTRIB):
    {
      Var value = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var on = PEEK(-2);    // Don't pop yet, we need the reference for gc.
      String* name = moduleGetStringAt(module, READ_SHORT());
      ASSERT(name != NULL, OOPS);
      varSetAttrib(vm, on, name, value);

      DROP(); // value
      DROP(); // on
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GET_SUBSCRIPT):
    {
      Var key = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var on = PEEK(-2);  // Don't pop yet, we need the reference for gc.
      Var value = varGetSubscript(vm, on, key);
      DROP(); // key
      DROP(); // on
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GET_SUBSCRIPT_KEEP):
    {
      Var key = PEEK(-1);
      Var on = PEEK(-2);
      PUSH(varGetSubscript(vm, on, key));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(SET_SUBSCRIPT):
    {
      Var value = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var key = PEEK(-2);   // Don't pop yet, we need the reference for gc.
      Var on = PEEK(-3);    // Don't pop yet, we need the reference for gc.
      varsetSubscript(vm, on, key, value);
      DROP(); // value
      DROP(); // key
      DROP(); // on
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(POSITIVE):
    {
      // Don't pop yet, we need the reference for gc.
      Var self_ = PEEK(-1);
      Var result = varPositive(vm, self_);
      DROP(); // self
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(NEGATIVE):
    {
      // Don't pop yet, we need the reference for gc.
      Var v = PEEK(-1);
      Var result = varNegative(vm, v);
      DROP(); // self
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(NOT):
    {
      // Don't pop yet, we need the reference for gc.
      Var v = PEEK(-1);
      Var result = varNot(vm, v);
      DROP(); // self
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_NOT):
    {
      // Don't pop yet, we need the reference for gc.
      Var v = PEEK(-1);
      Var result = varBitNot(vm, v);
      DROP(); // self
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    // Do not ever use PUSH(binaryOp(vm, POP(), POP()));
    // Function parameters are not evaluated in a defined order in C.

    OPCODE(ADD):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE(); ASSERT(inplace <= 1, OOPS);
      Var result = varAdd(vm, l, r, inplace);
      DROP(); DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(SUBTRACT):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE(); ASSERT(inplace <= 1, OOPS);
      Var result = varSubtract(vm, l, r, inplace);
      DROP(); DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(MULTIPLY):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE(); ASSERT(inplace <= 1, OOPS);
      Var result = varMultiply(vm, l, r, inplace);
      DROP(); DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(DIVIDE):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE(); ASSERT(inplace <= 1, OOPS);
      Var result = varDivide(vm, l, r, inplace);
      DROP(); DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(EXPONENT):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE(); ASSERT(inplace <= 1, OOPS);
      Var result = varExponent(vm, l, r, inplace);
      DROP(); DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(MOD):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE(); ASSERT(inplace <= 1, OOPS);
      Var result = varModulo(vm, l, r, inplace);
      DROP(); DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_AND) :
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE(); ASSERT(inplace <= 1, OOPS);
      Var result = varBitAnd(vm, l, r, inplace);
      DROP(); DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_OR):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE(); ASSERT(inplace <= 1, OOPS);
      Var result = varBitOr(vm, l, r, inplace);
      DROP(); DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_XOR):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE(); ASSERT(inplace <= 1, OOPS);
      Var result = varBitXor(vm, l, r, inplace);
      DROP(); DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_LSHIFT):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE(); ASSERT(inplace <= 1, OOPS);
      Var result = varBitLshift(vm, l, r, inplace);
      DROP(); DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_RSHIFT):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE(); ASSERT(inplace <= 1, OOPS);
      Var result = varBitRshift(vm, l, r, inplace);
      DROP(); DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(EQEQ):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      Var result = varEqals(vm, l, r);
      DROP(); DROP(); // r, l
      PUSH(result);
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(NOTEQ):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      Var result = varEqals(vm, l, r);
      DROP(); DROP(); // r, l
      PUSH(VAR_BOOL(!toBool(result)));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(LT):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      Var result = varLesser(vm, l, r);
      DROP(); DROP(); // r, l
      PUSH(result);
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(LTEQ):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);

      Var result = varLesser(vm, l, r);
      CHECK_ERROR();
      bool lteq = toBool(result);

      if (!lteq) result = varEqals(vm, l, r);
      CHECK_ERROR();

      DROP(); DROP(); // r, l
      PUSH(result);
      DISPATCH();
    }

    OPCODE(GT):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      Var result = varGreater(vm, l, r);
      DROP(); DROP(); // r, l
      PUSH(result);
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GTEQ):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      Var result = varGreater(vm, l, r);
      CHECK_ERROR();
      bool gteq = toBool(result);

      if (!gteq) result = varEqals(vm, l, r);
      CHECK_ERROR();

      DROP(); DROP(); // r, l
      PUSH(result);
      DISPATCH();
    }

    OPCODE(RANGE):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      Var result = varOpRange(vm, l, r);
      DROP(); DROP(); // r, l
      PUSH(result);
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(IN):
    {
      // Don't pop yet, we need the reference for gc.
      Var container = PEEK(-1), elem = PEEK(-2);
      bool contains = varContains(vm, elem, container);
      DROP(); DROP(); // container, elem
      PUSH(VAR_BOOL(contains));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(IS):
    {
      // Don't pop yet, we need the reference for gc.
      Var type = PEEK(-1), inst = PEEK(-2);
      bool is = varIsType(vm, inst, type);
      DROP(); DROP(); // container, elem
      PUSH(VAR_BOOL(is));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(REPL_PRINT):
    {
      if (vm->config.stdout_write != NULL) {
        Var tmp = PEEK(-1);
        if (!IS_NULL(tmp)) {
          vm->config.stdout_write(vm, varToString(vm, tmp, true)->data);
          vm->config.stdout_write(vm, "\n");
        }
      }
      DISPATCH();
    }

    OPCODE(END):
      UNREACHABLE();
      break;

    default:
      UNREACHABLE();

  }

  UNREACHABLE();
  return PK_RESULT_RUNTIME_ERROR;
}
