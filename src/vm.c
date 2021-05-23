/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "vm.h"

#include "core.h"
#include "utils.h"

#if DEBUG_DUMP_CALL_STACK
  #include "debug.h" //< Wrap around debug macro.
#endif

#define HAS_ERROR() (vm->fiber->error != NULL)

// Initially allocated call frame capacity. Will grow dynamically.
#define INITIAL_CALL_FRAMES 4

// Minimum size of the stack.
#define MIN_STACK_SIZE 128

// The allocated size the'll trigger the first GC. (~10MB).
#define INITIAL_GC_SIZE (1024 * 1024 * 10)

// The heap size might shrink if the remaining allocated bytes after a GC 
// is less than the one before the last GC. So we need a minimum size.
#define MIN_HEAP_SIZE (1024 * 1024)

// The heap size for the next GC will be calculated as the bytes we have
// allocated so far plus the fill factor of it.
#define HEAP_FILL_PERCENT 75

static void* defaultRealloc(void* memory, size_t new_size, void* user_data) {
  if (new_size == 0) {
    free(memory);
    return NULL;
  }
  return realloc(memory, new_size);
}

void* vmRealloc(PKVM* self, void* memory, size_t old_size, size_t new_size) {

  // TODO: Debug trace allocations here.

  // Track the total allocated memory of the VM to trigger the GC.
  // if vmRealloc is called for freeing, the old_size would be 0 since 
  // deallocated bytes are traced by garbage collector.
  self->bytes_allocated += new_size - old_size;

  if (new_size > 0 && self->bytes_allocated > self->next_gc) {
    vmCollectGarbage(self);
  }

  return self->config.realloc_fn(memory, new_size, self->config.user_data);
}

pkConfiguration pkNewConfiguration() {
  pkConfiguration config;
  config.realloc_fn = defaultRealloc;

  config.error_fn = NULL;
  config.write_fn = NULL;

  config.load_script_fn = NULL;
  config.resolve_path_fn = NULL;
  config.user_data = NULL;

  return config;
}

PKVM* pkNewVM(pkConfiguration* config) {

  pkConfiguration default_config = pkNewConfiguration();

  if (config == NULL) config = &default_config;

  PKVM* vm = (PKVM*)config->realloc_fn(NULL, sizeof(PKVM), config->user_data);
  memset(vm, 0, sizeof(PKVM));

  vm->config = *config;
  vm->gray_list_count = 0;
  vm->gray_list_capacity = MIN_CAPACITY;
  vm->gray_list = (Object**)vm->config.realloc_fn(
    NULL, sizeof(Object*) * vm->gray_list_capacity, NULL);
  vm->next_gc = INITIAL_GC_SIZE;
  vm->min_heap_size = MIN_HEAP_SIZE;
  vm->heap_fill_percent = HEAP_FILL_PERCENT;

  vm->scripts = newMap(vm);
  vm->core_libs = newMap(vm);
  vm->builtins_count = 0;

  initializeCore(vm);
  return vm;
}

void pkFreeVM(PKVM* self) {

  Object* obj = self->first;
  while (obj != NULL) {
    Object* next = obj->next;
    freeObject(self, obj);
    obj = next;
  }

  self->gray_list = (Object**)self->config.realloc_fn(
    self->gray_list, 0, self->config.user_data);

  // Tell the host application that it forget to release all of it's handles
  // before freeing the VM.
  __ASSERT(self->handles == NULL, "Not all handles were released.");

  DEALLOCATE(self, self);
}

PkHandle* pkNewHandle(PKVM* vm, PkVar value) {
  return vmNewHandle(vm, *((Var*)value));
}

PkVar pkGetHandleValue(PkHandle* handle) {
  return (PkVar)&handle->value;
}

void pkReleaseHandle(PKVM* vm, PkHandle* handle) {
  __ASSERT(handle != NULL, "Given handle was NULL.");

  // If the handle is the head of the vm's handle chain set it to the next one.
  if (handle == vm->handles) {
    vm->handles = handle->next;
  }

  // Remove the handle from the chain by connecting the both ends together.
  if (handle->next) handle->next->prev = handle->prev;
  if (handle->prev) handle->prev->next = handle->next;

  // Free the handle.
  DEALLOCATE(vm, handle);
}

/*****************************************************************************/
/* VM INTERNALS                                                              */
/*****************************************************************************/

void vmPushTempRef(PKVM* self, Object* obj) {
  ASSERT(obj != NULL, "Cannot reference to NULL.");
  ASSERT(self->temp_reference_count < MAX_TEMP_REFERENCE, 
         "Too many temp references");
  self->temp_reference[self->temp_reference_count++] = obj;
}

void vmPopTempRef(PKVM* self) {
  ASSERT(self->temp_reference_count > 0, "Temporary reference is empty to pop.");
  self->temp_reference_count--;
}

PkHandle* vmNewHandle(PKVM* self, Var value) {
  PkHandle* handle = (PkHandle*)ALLOCATE(self, PkHandle);
  handle->value = value;
  handle->prev = NULL;
  handle->next = self->handles;
  if (handle->next != NULL) handle->next->prev = handle;
  self->handles = handle;
  return handle;
}

void vmCollectGarbage(PKVM* self) {

  // Reset VM's bytes_allocated value and count it again so that we don't
  // required to know the size of each object that'll be freeing.
  self->bytes_allocated = 0;

  // Mark the core libs and builtin functions.
  grayObject(self, &self->core_libs->_super);
  for (int i = 0; i < self->builtins_count; i++) {
    grayObject(self, &self->builtins[i].fn->_super);
  }

  // Mark the scripts cache.
  grayObject(self, &self->scripts->_super);

  // Mark temp references.
  for (int i = 0; i < self->temp_reference_count; i++) {
    grayObject(self, self->temp_reference[i]);
  }

  // Mark the handles.
  for (PkHandle* handle = self->handles; handle != NULL; handle = handle->next) {
    grayValue(self, handle->value);
  }

  // Garbage collection triggered at the middle of a compilation.
  if (self->compiler != NULL) {
    compilerMarkObjects(self, self->compiler);
  }

  // Garbage collection triggered at the middle of runtime.
  if (self->script != NULL) {
    grayObject(self, &self->script->_super);
  }

  if (self->fiber != NULL) {
    grayObject(self, &self->fiber->_super);
  }
  
  blackenObjects(self);

  // Now sweep all the un-marked objects in then link list and remove them
  // from the chain.

  // [ptr] is an Object* reference that should be equal to the next
  // non-garbage Object*.
  Object** ptr = &self->first;
  while (*ptr != NULL) {

    // If the object the pointer points to wasn't marked it's unreachable.
    // Clean it. And update the pointer points to the next object.
    if (!(*ptr)->is_marked) {
      Object* garbage = *ptr;
      *ptr = garbage->next;
      freeObject(self, garbage);

    } else {
      // Unmark the object for the next garbage collection.
      (*ptr)->is_marked = false;
      ptr = &(*ptr)->next;
    }
  }

  // Next GC heap size will be change depends on the byte we've left with now,
  // and the [heap_fill_percent].
  self->next_gc = self->bytes_allocated + (
                 (self->bytes_allocated * self->heap_fill_percent) / 100);
  if (self->next_gc < self->min_heap_size) self->next_gc = self->min_heap_size;
}

void* pkGetUserData(PKVM* vm) {
  return vm->config.user_data;
}

void pkSetUserData(PKVM* vm, void* user_data) {
  vm->config.user_data = user_data;
}

static Script* getScript(PKVM* vm, String* path) {
  Var scr = mapGet(vm->scripts, VAR_OBJ(&path->_super));
  if (IS_UNDEF(scr)) return NULL;
  ASSERT(AS_OBJ(scr)->type == OBJ_SCRIPT, OOPS);
  return (Script*)AS_OBJ(scr);
}

/******************************************************************************
 * RUNTIME                                                                    *
 *****************************************************************************/

// If failed to resolve it'll return false. Parameter [result] should be points
// to the string which is the path that has to be resolved and once it resolved
// the provided result's string's on_done() will be called and, it's string will 
// be updated with the new resolved path string.
static bool resolveScriptPath(PKVM* vm, pkStringPtr* path_string) {
  if (vm->config.resolve_path_fn == NULL) return true;

  const char* path = path_string->string;
  pkStringPtr resolved;

  Fiber* fiber = vm->fiber;
  if (fiber == NULL || fiber->frame_count <= 0) {
    // fiber == NULL => vm haven't started yet and it's a root script.
    resolved = vm->config.resolve_path_fn(vm, NULL, path);
  } else {
    Function* fn = fiber->frames[fiber->frame_count - 1].fn;
    resolved = vm->config.resolve_path_fn(vm, fn->owner->path->data, path);
  }

  // Done with the last string and update it with the new string.
  if (path_string->on_done != NULL) path_string->on_done(vm, *path_string);
  *path_string = resolved;

  return path_string->string != NULL;
}

// Import and return Script object as Var. If the script is imported and
// compiled here it'll set [is_new_script] to true oterwise (using the cached
// script) set to false.
static Var importScript(PKVM* vm, String* path_name) {

  // Check in the core libs.
  Script* scr = getCoreLib(vm, path_name);
  if (scr != NULL) return VAR_OBJ(&scr->_super);

  // Check in the scripts cache.
  Var entry = mapGet(vm->scripts, VAR_OBJ(&path_name->_super));
  if (!IS_UNDEF(entry)) {
    ASSERT(AS_OBJ(entry)->type == OBJ_SCRIPT, OOPS);
    return entry;
  }

  // Imported scripts were resolved at compile time.
  UNREACHABLE();

  return VAR_NULL;
}

static void ensureStackSize(PKVM* vm, int size) {
  Fiber* fiber = vm->fiber;

  if (fiber->stack_size > size) return;
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

  /*
                                         '        '
                 '        '              '        '
                 '        '              |        | <new_rsp
        old_rsp> |        |              |        |
                 |        |       .----> | value  | <new_ptr
                 |        |       |      |        |
        old_ptr> | value  | ------'      |________| <new_rbp
                 |        | ^            new stack
        old_rbp> |________| | height
                 old stack

                new_ptr = new_rbp      + height
                        = fiber->stack + ( old_ptr  - old_rbp )  */
#define MAP_PTR(old_ptr) (fiber->stack + ((old_ptr) - old_rbp))

  // Update the stack top pointer.
  fiber->sp = MAP_PTR(fiber->sp);

  // Update the stack base pointer of the call frames.
  for (int i = 0; i < fiber->frame_capacity; i++) {
    CallFrame* frame = fiber->frames + i;
    frame->rbp = MAP_PTR(frame->rbp);
  }

}

static inline void pushCallFrame(PKVM* vm, Function* fn) {
  ASSERT(!fn->is_native, "Native function shouldn't use call frames.");

  // Grow the stack frame if needed.
  if (vm->fiber->frame_count + 1 > vm->fiber->frame_capacity) {
    int new_capacity = vm->fiber->frame_capacity * 2;
    vm->fiber->frames = (CallFrame*)vmRealloc(vm, vm->fiber->frames,
                           sizeof(CallFrame) * vm->fiber->frame_capacity,
                           sizeof(CallFrame) * new_capacity);
    vm->fiber->frame_capacity = new_capacity;
  }

  // Grow the stack if needed.
  int stack_size = (int)(vm->fiber->sp - vm->fiber->stack);
  int needed = stack_size + fn->fn->stack_size;
  // TODO: set stack overflow maybe?.
  ensureStackSize(vm, needed);

  CallFrame* frame = &vm->fiber->frames[vm->fiber->frame_count++];
  frame->rbp = vm->fiber->ret;
  frame->fn = fn;
  frame->ip = fn->fn->opcodes.data;
}

void pkSetRuntimeError(PKVM* vm, const char* message) {
  __ASSERT(vm->fiber != NULL, "This function can only be called at runtime.");
  vm->fiber->error = newString(vm, message);
}

void vmReportError(PKVM* vm) {
  ASSERT(HAS_ERROR(), "runtimeError() should be called after an error.");
  // TODO: pass the error to the caller of the fiber.

  // Print the Error message and stack trace.
  if (vm->config.error_fn == NULL) return;
  Fiber* fiber = vm->fiber;
  vm->config.error_fn(vm, PK_ERROR_RUNTIME, NULL, -1, fiber->error->data);
  for (int i = fiber->frame_count - 1; i >= 0; i--) {
    CallFrame* frame = &fiber->frames[i];
    Function* fn = frame->fn;
    ASSERT(!fn->is_native, OOPS);
    int line = fn->fn->oplines.data[frame->ip - fn->fn->opcodes.data - 1];
    vm->config.error_fn(vm, PK_ERROR_STACKTRACE, fn->owner->path->data, line,
                        fn->name);
  }
}

// This function is responsible to call on_done function if it's done with the 
// provided string pointers.
static PkInterpretResult interpretSource(PKVM* vm, pkStringPtr source,
                                         pkStringPtr path) {
  String* path_name = newString(vm, path.string);
  if (path.on_done) path.on_done(vm, path);
  vmPushTempRef(vm, &path_name->_super); // path_name.

  // Load a new script to the vm's scripts cache.
  Script* scr = getScript(vm, path_name);
  if (scr == NULL) {
    scr = newScript(vm, path_name);
    vmPushTempRef(vm, &scr->_super); // scr.
    mapSet(vm, vm->scripts, VAR_OBJ(&path_name->_super),
      VAR_OBJ(&scr->_super));
    vmPopTempRef(vm); // scr.
  }
  vmPopTempRef(vm); // path_name.

  // Compile the source.
  bool success = compile(vm, scr, source.string);
  if (source.on_done) source.on_done(vm, source);

  if (!success) return PK_RESULT_COMPILE_ERROR;
  vm->script = scr;

  return vmRunScript(vm, scr);
}

PkInterpretResult pkInterpretSource(PKVM* vm, const char* source,
                                    const char* path) {
  // Call the internal interpretSource implementation.
  pkStringPtr source_ptr = { source, NULL, NULL };
  pkStringPtr path_ptr = { path, NULL, NULL };
  return interpretSource(vm, source_ptr, path_ptr);
}

PkInterpretResult pkInterpret(PKVM* vm, const char* path) {

  pkStringPtr resolved;
  resolved.string = path;
  resolved.on_done = NULL;

  // The provided path should already be resolved.
  //if (!resolveScriptPath(vm, &resolved)) {
  //  if (vm->config.error_fn != NULL) {
  //    vm->config.error_fn(vm, PK_ERROR_COMPILE, NULL, -1,
  //      stringFormat(vm, "Failed to resolve path '$'.", path)->data);
  //  }
  //  return PK_RESULT_COMPILE_ERROR;
  //}

  // Load the script source.
  pkStringPtr source = vm->config.load_script_fn(vm, resolved.string);
  if (source.string == NULL) {
    if (vm->config.error_fn != NULL) {
      vm->config.error_fn(vm, PK_ERROR_COMPILE, NULL, -1,
        stringFormat(vm, "Failed to load script '$'.", resolved.string)->data);
    }

    return PK_RESULT_COMPILE_ERROR;
  }

  return interpretSource(vm, source, resolved);
}

PkInterpretResult vmRunScript(PKVM* vm, Script* _script) {

  // Reference to the instruction pointer in the call frame.
  register uint8_t** ip;
#define IP (*ip) // Convinent macro to the instruction pointer.

  register Var* rbp;         //< Stack base pointer register.
  register CallFrame* frame; //< Current call frame.
  register Script* script;   //< Currently executing script.

  // TODO: implement fiber from script body.

  vm->fiber = newFiber(vm);
  vm->fiber->func = _script->body;

  // Allocate stack.
  int stack_size = utilPowerOf2Ceil(vm->fiber->func->fn->stack_size + 1);
  if (stack_size < MIN_STACK_SIZE) stack_size = MIN_STACK_SIZE;
  vm->fiber->stack_size = stack_size;
  vm->fiber->stack = ALLOCATE_ARRAY(vm, Var, vm->fiber->stack_size);
  vm->fiber->sp = vm->fiber->stack;
  vm->fiber->ret = vm->fiber->stack;

  // Allocate call frames.
  vm->fiber->frame_capacity = INITIAL_CALL_FRAMES;
  vm->fiber->frames = ALLOCATE_ARRAY(vm, CallFrame, vm->fiber->frame_capacity);
  vm->fiber->frame_count = 1;

  // Initialize VM's first frame.
  vm->fiber->frames[0].ip = _script->body->fn->opcodes.data;
  vm->fiber->frames[0].fn = _script->body;
  vm->fiber->frames[0].rbp = vm->fiber->stack;

#define PUSH(value)  (*vm->fiber->sp++ = (value))
#define POP()        (*(--vm->fiber->sp))
#define DROP()       (--vm->fiber->sp)
#define PEEK(off)    (*(vm->fiber->sp + (off)))
#define READ_BYTE()  (*IP++)
#define READ_SHORT() (IP+=2, (uint16_t)((IP[-2] << 8) | IP[-1]))

// Check if any runtime error exists and if so returns RESULT_RUNTIME_ERROR.
#define CHECK_ERROR()                 \
  do {                                \
    if (HAS_ERROR()) {                \
      vmReportError(vm);              \
      return PK_RESULT_RUNTIME_ERROR; \
    }                                 \
  } while (false)

// [err_msg] must be of type String.
#define RUNTIME_ERROR(err_msg)                 \
  do {                                         \
    vm->fiber->error = err_msg;                \
    vmReportError(vm);                         \
    return PK_RESULT_RUNTIME_ERROR;            \
  } while (false)

// Load the last call frame to vm's execution variables to resume/run the
// function.
#define LOAD_FRAME()                                       \
  do {                                                     \
    frame = &vm->fiber->frames[vm->fiber->frame_count-1];  \
    ip = &(frame->ip);                                     \
    rbp = frame->rbp;                                      \
    script = frame->fn->owner;                             \
  } while (false)

#ifdef OPCODE
  #error "OPCODE" should not be deifined here.
#endif

#if  DEBUG_DUMP_CALL_STACK
  #define DEBUG_CALL_STACK()        \
    do {                            \
      system("cls");                \
      dumpGlobalValues(vm);         \
      dumpStackFrame(vm);           \
    } while (false)
#else
  #define DEBUG_CALL_STACK() ((void*)0)
#endif

#define SWITCH(code)                 \
  L_vm_main_loop:                    \
  DEBUG_CALL_STACK();                \
  switch (code = (Opcode)READ_BYTE())

#define OPCODE(code) case OP_##code
#define DISPATCH()   goto L_vm_main_loop

  PUSH(VAR_NULL); // Return value of the script body.
  LOAD_FRAME();

  Opcode instruction;
  SWITCH(instruction) {

    OPCODE(PUSH_CONSTANT):
    {
      uint16_t index = READ_SHORT();
      ASSERT_INDEX(index, script->literals.count);
      PUSH(script->literals.data[index]);
      DISPATCH();
    }

    OPCODE(PUSH_NULL):
      PUSH(VAR_NULL);
      DISPATCH();

    OPCODE(PUSH_TRUE):
      PUSH(VAR_TRUE);
      DISPATCH();

    OPCODE(PUSH_FALSE):
      PUSH(VAR_FALSE);
      DISPATCH();

    OPCODE(SWAP):
    {
      Var top1 = *(vm->fiber->sp - 1);
      Var top2 = *(vm->fiber->sp - 2);

      *(vm->fiber->sp - 1) = top2;
      *(vm->fiber->sp - 2) = top1;
      DISPATCH();
    }

    OPCODE(PUSH_LIST):
    {
      List* list = newList(vm, (uint32_t)READ_SHORT());
      PUSH(VAR_OBJ(&list->_super));
      DISPATCH();
    }

    OPCODE(PUSH_MAP):
    {
      Map* map = newMap(vm);
      PUSH(VAR_OBJ(&map->_super));
      DISPATCH();
    }

    OPCODE(LIST_APPEND):
    {
      Var elem = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var list = PEEK(-2);
      ASSERT(IS_OBJ(list) && AS_OBJ(list)->type == OBJ_LIST, OOPS);
      varBufferWrite(&((List*)AS_OBJ(list))->elements, vm, elem);
      POP(); // elem
      DISPATCH();
    }

    OPCODE(MAP_INSERT):
    {
      Var value = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var key = PEEK(-2);   // Don't pop yet, we need the reference for gc. 
      Var on = PEEK(-3);

      ASSERT(IS_OBJ(on) && AS_OBJ(on)->type == OBJ_MAP, OOPS);

      if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
        RUNTIME_ERROR(stringFormat(vm, "$ type is not hashable.", varTypeName(key)));
      } else {
        mapSet(vm, (Map*)AS_OBJ(on), key, value);
      }
      varsetSubscript(vm, on, key, value);

      POP(); // value
      POP(); // key

      CHECK_ERROR();
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
      uint16_t index = READ_SHORT();
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
      uint16_t index = READ_SHORT();
      rbp[index + 1] = PEEK(-1);  // +1: rbp[0] is return value.
      DISPATCH();
    }

    OPCODE(PUSH_GLOBAL):
    {
      uint16_t index = READ_SHORT();
      ASSERT(index < script->globals.count, OOPS);
      PUSH(script->globals.data[index]);
      DISPATCH();
    }

    OPCODE(STORE_GLOBAL):
    {
      uint16_t index = READ_SHORT();
      ASSERT(index < script->globals.count, OOPS);
      script->globals.data[index] = PEEK(-1);
      DISPATCH();
    }

    OPCODE(PUSH_FN):
    {
      uint16_t index = READ_SHORT();
      ASSERT(index < script->functions.count, OOPS);
      Function* fn = script->functions.data[index];
      PUSH(VAR_OBJ(&fn->_super));
      DISPATCH();
    }

    OPCODE(PUSH_BUILTIN_FN):
    {
      Function* fn = getBuiltinFunction(vm, READ_SHORT());
      PUSH(VAR_OBJ(&fn->_super));
      DISPATCH();
    }

    OPCODE(POP):
      DROP();
      DISPATCH();

    OPCODE(IMPORT):
    {
      String* name = script->names.data[READ_SHORT()];
      Var script = importScript(vm, name);
      // TODO: initialize global variables.
      PUSH(script);
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(CALL):
    {
      uint16_t argc = READ_SHORT();
      Var* callable = vm->fiber->sp - argc - 1;

      if (IS_OBJ(*callable) && AS_OBJ(*callable)->type == OBJ_FUNC) {

        Function* fn = (Function*)AS_OBJ(*callable);

        // -1 argument means multiple number of args.
        if (fn->arity != -1 && fn->arity != argc) {
          String* arg_str = toString(vm, VAR_NUM(fn->arity), false);
          vmPushTempRef(vm, &arg_str->_super);
          String* msg = stringFormat(vm, "Expected excatly @ argument(s).",
                                     arg_str);
          vmPopTempRef(vm); // arg_str.
          RUNTIME_ERROR(msg);
        }

        // Now the function will never needed in the stack and it'll
        // initialized with VAR_NULL as return value.
        *callable = VAR_NULL;

        // Next call frame starts here. (including return value).
        vm->fiber->ret = callable;

        if (fn->is_native) {
          if (fn->native == NULL) {
            RUNTIME_ERROR(stringFormat(vm,
              "Native function pointer of $ was NULL.", fn->name));
          }
          fn->native(vm);
          // Pop function arguments except for the return value.
          vm->fiber->sp = vm->fiber->ret + 1;
          CHECK_ERROR();

        } else {
          pushCallFrame(vm, fn);
          LOAD_FRAME(); //< Load the top frame to vm's execution variables.
        }

      } else {
        RUNTIME_ERROR(newString(vm, "Expected a function in call."));
      }
      DISPATCH();
    }

    OPCODE(ITER):
    {
      Var* iter_value = (vm->fiber->sp - 1);
      Var* iterator   = (vm->fiber->sp - 2);
      Var* container  = (vm->fiber->sp - 3);
      uint16_t jump_offset = READ_SHORT();
      
      bool iterated = varIterate(vm, *container, iterator, iter_value);
      CHECK_ERROR();
      if (!iterated) {
        IP += jump_offset;
      }
      DISPATCH();
    }

    OPCODE(JUMP):
    {
      uint16_t offset = READ_SHORT();
      IP += offset;
      DISPATCH();
    }

    OPCODE(LOOP):
    {
      uint16_t offset = READ_SHORT();
      IP -= offset;
      DISPATCH();
    }

    OPCODE(JUMP_IF):
    {
      Var cond = POP();
      uint16_t offset = READ_SHORT();
      if (toBool(cond)) {
        IP += offset;
      }
      DISPATCH();
    }

    OPCODE(JUMP_IF_NOT):
    {
      Var cond = POP();
      uint16_t offset = READ_SHORT();
      if (!toBool(cond)) {
        IP += offset;
      }
      DISPATCH();
    }

    OPCODE(RETURN):
    {
      // TODO: handle caller fiber.

      Var ret = POP();

      // Pop the last frame.
      vm->fiber->frame_count--;

      // If no more call frames. We're done.
      if (vm->fiber->frame_count == 0) {
        vm->fiber->sp = vm->fiber->stack;
        PUSH(ret);
        return PK_RESULT_SUCCESS;
      }

      // Set the return value.
      *(frame->rbp) = ret;

      // Pop the locals and update stack pointer.
      vm->fiber->sp = frame->rbp + 1; // +1: rbp is returned value.

      LOAD_FRAME();
      DISPATCH();
    }

    OPCODE(GET_ATTRIB):
    {
      Var on = PEEK(-1); // Don't pop yet, we need the reference for gc. 
      String* name = script->names.data[READ_SHORT()];
      Var value = varGetAttrib(vm, on, name);
      POP(); // on
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GET_ATTRIB_KEEP):
    {
      Var on = PEEK(-1);
      String* name = script->names.data[READ_SHORT()];
      PUSH(varGetAttrib(vm, on, name));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(SET_ATTRIB):
    {
      Var value = PEEK(-1); // Don't pop yet, we need the reference for gc. 
      Var on = PEEK(-2);    // Don't pop yet, we need the reference for gc. 
      String* name = script->names.data[READ_SHORT()];
      varSetAttrib(vm, on, name, value);

      POP(); // value
      POP(); // on
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GET_SUBSCRIPT):
    {
      Var key = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var on = PEEK(-2);  // Don't pop yet, we need the reference for gc.
      Var value = varGetSubscript(vm, on, key);
      POP(); // key
      POP(); // on
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
      POP(); // value
      POP(); // key
      POP(); // on
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(NEGATIVE):
    {
      Var num = POP();
      if (!IS_NUM(num)) {
        RUNTIME_ERROR(newString(vm, "Cannot negate a non numeric value."));
      }
      PUSH(VAR_NUM(-AS_NUM(num)));
      DISPATCH();
    }

    OPCODE(NOT):
    {
      Var val = POP();
      PUSH(VAR_BOOL(!toBool(val)));
      DISPATCH();
    }

    OPCODE(BIT_NOT):
      TODO;

    // Do not ever use PUSH(binaryOp(vm, POP(), POP()));
    // Function parameters are not evaluated in a defined order in C.

    OPCODE(ADD):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      Var value = varAdd(vm, l, r);
      POP(); POP(); // r, l
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(SUBTRACT):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      Var value = varSubtract(vm, l, r);
      POP(); POP(); // r, l
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(MULTIPLY):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      Var value = varMultiply(vm, l, r);
      POP(); POP(); // r, l
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(DIVIDE):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      Var value = varMultiply(vm, l, r);
      POP(); POP(); // r, l
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(MOD):
    {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      Var value = varModulo(vm, l, r);
      POP(); POP(); // r, l
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_AND):
    OPCODE(BIT_OR):
    OPCODE(BIT_XOR):
    OPCODE(BIT_LSHIFT):
    OPCODE(BIT_RSHIFT):

    OPCODE(EQEQ) :
    {
      Var r = POP(), l = POP();
      PUSH(VAR_BOOL(isValuesEqual(l, r)));
      DISPATCH();
    }

    OPCODE(NOTEQ):
    {
      Var r = POP(), l = POP();
      PUSH(VAR_BOOL(!isValuesEqual(l, r)));
      DISPATCH();
    }

    OPCODE(LT):
    {
      Var r = POP(), l = POP();
      PUSH(VAR_BOOL(varLesser(l, r)));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(LTEQ):
    {
      Var r = POP(), l = POP();
      bool lteq = varLesser(l, r);
      CHECK_ERROR();

      if (!lteq) {
        lteq = isValuesEqual(l, r);
        CHECK_ERROR();
      }

      PUSH(VAR_BOOL(lteq));
      DISPATCH();
    }

    OPCODE(GT):
    {
      Var r = POP(), l = POP();
      PUSH(VAR_BOOL(varGreater(l, r)));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GTEQ):
    {
      Var r = POP(), l = POP();
      bool gteq = varGreater(l, r);
      CHECK_ERROR();

      if (!gteq) {
        gteq = isValuesEqual(l, r);
        CHECK_ERROR();
      }

      PUSH(VAR_BOOL(gteq));
      DISPATCH();
    }

    OPCODE(RANGE):
    {
      Var to = PEEK(-1);   // Don't pop yet, we need the reference for gc.
      Var from = PEEK(-2); // Don't pop yet, we need the reference for gc.
      if (!IS_NUM(from) || !IS_NUM(to)) {
        RUNTIME_ERROR(newString(vm, "Range arguments must be number."));
      }
      POP(); // to
      POP(); // from
      PUSH(VAR_OBJ(newRange(vm, AS_NUM(from), AS_NUM(to))));
      DISPATCH();
    }

    OPCODE(IN):
      // TODO: Implement bool varContaines(vm, on, value);
      TODO;

    OPCODE(END):
      TODO;
      break;

    default:
      UNREACHABLE();

  }

  return PK_RESULT_SUCCESS;
}
