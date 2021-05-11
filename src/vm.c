/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "vm.h"

#include "core.h"
#include "debug.h"
#include "utils.h"

#define HAS_ERROR() (vm->fiber->error != NULL)

// Initially allocated call frame capacity. Will grow dynamically.
#define INITIAL_CALL_FRAMES 4

// Minimum size of the stack.
#define MIN_STACK_SIZE 128

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
  Function* f = (Function*)memory;
  if (new_size == 0) {
    free(memory);
    return NULL;
  }

  return self->config.realloc_fn(memory, new_size, self->config.user_data);
}

void pkInitConfiguration(pkConfiguration* config) {
  config->realloc_fn = defaultRealloc;

  // TODO: Handle Null functions before calling them.
  config->error_fn = NULL;
  config->write_fn = NULL;

  config->load_script_fn = NULL;
  config->resolve_path_fn = NULL;
  config->user_data = NULL;
}

PKVM* pkNewVM(pkConfiguration* config) {

  // TODO: If the [config] is NULL, initialize a default one.

  pkReallocFn realloc_fn = defaultRealloc;
  void* user_data = NULL;
  if (config != NULL) {
    realloc_fn = config->realloc_fn;
    user_data = config->user_data;
  }
  PKVM* vm = (PKVM*)realloc_fn(NULL, sizeof(PKVM), user_data);
  memset(vm, 0, sizeof(PKVM));

  vm->config = *config;
  vm->gray_list_count = 0;
  vm->gray_list_capacity = MIN_CAPACITY;
  vm->gray_list = (Object**)vm->config.realloc_fn(
    NULL, sizeof(Object*) * vm->gray_list_capacity, NULL);
  vm->next_gc = 1024 * 1024 * 10; // TODO:

  vm->scripts = newMap(vm);

  // TODO: no need to initialize if already done by another vm.
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

  DEALLOCATE(self, self);
}

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

void vmCollectGarbage(PKVM* self) {

  // Reset VM's bytes_allocated value and count it again so that we don't
  // required to know the size of each object that'll be freeing.
  self->bytes_allocated = 0;

  // Mark core objects (mostlikely builtin functions).
  markCoreObjects(self);

  // Mark the scripts cache.
  grayObject(&self->scripts->_super, self);

  // Mark temp references.
  for (int i = 0; i < self->temp_reference_count; i++) {
    grayObject(self->temp_reference[i], self);
  }

  // Garbage collection triggered at the middle of a compilation.
  if (self->compiler != NULL) {
    compilerMarkObjects(self->compiler, self);
  }

  // Garbage collection triggered at the middle of runtime.
  if (self->script != NULL) {
    grayObject(&self->script->_super, self);
  }

  if (self->fiber != NULL) {
    grayObject(&self->fiber->_super, self);
  }
  
  blackenObjects(self);

  TODO; // Sweep.
}

void* pkGetUserData(PKVM* vm) {
  return vm->config.user_data;
}

void pkSetUserData(PKVM* vm, void* user_data) {
  vm->config.user_data = user_data;
}

static Script* getScript(PKVM* vm, String* name) {
  Var scr = mapGet(vm->scripts, VAR_OBJ(&name->_super));
  if (IS_UNDEF(scr)) return NULL;
  ASSERT(AS_OBJ(scr)->type == OBJ_SCRIPT, OOPS);
  return (Script*)AS_OBJ(scr);
}

/******************************************************************************
 * RUNTIME                                                                    *
 *****************************************************************************/

// If failed to resolve it'll return false. Parameter [resolved] will be
// updated with a resolved path.
static bool resolveScriptPath(PKVM* vm, String** resolved) {
  if (vm->config.resolve_path_fn == NULL) return true;

  pkStringResult result;
  const char* path = (*resolved)->data;

  Fiber* fiber = vm->fiber;
  if (fiber == NULL || fiber->frame_count <= 0) {
    // fiber == NULL => vm haven't started yet and it's a root script.
    result = vm->config.resolve_path_fn(vm, NULL, path);
  } else {
    Function* fn = fiber->frames[fiber->frame_count - 1].fn;
    result = vm->config.resolve_path_fn(vm, fn->owner->name->data, path);
  }
  if (!result.success) return false;

  // If the resolved string is the SAME as [path] don't allocate a new string.
  if (result.string != path) *resolved = newString(vm, result.string);
  if (result.on_done != NULL) result.on_done(vm, result);

  return true;
}

// Import and return Script object as Var. If the script is imported and
// compiled here it'll set [is_new_script] to true oterwise (using the cached
// script) set to false.
static Var importScript(PKVM* vm, String* name, bool is_core,
                        bool* is_new_script) {
  if (is_core) {
    ASSERT(is_new_script == NULL, OOPS);
    Script* core_lib = getCoreLib(name);
    if (core_lib != NULL) {
      return VAR_OBJ(&core_lib->_super);
    }
    vm->fiber->error = stringFormat(vm, "Failed to import core library @",
                                    name);
    return VAR_NULL;
  }

  // Relative path import.

  ASSERT(is_new_script != NULL, OOPS);
  if (!resolveScriptPath(vm, &name)) {
    vm->fiber->error = stringFormat(vm, "Failed to resolve script @ from @",
                                    name, vm->fiber->func->owner->name);
    return VAR_NULL;
  }

  vmPushTempRef(vm, &name->_super);

  // Check if the script is already cached (then use it).
  Var scr = mapGet(vm->scripts, VAR_OBJ(&name->_super));
  if (!IS_UNDEF(scr)) {
    ASSERT(AS_OBJ(scr)->type == OBJ_SCRIPT, OOPS);
    *is_new_script = false;
    return scr;
  }
  *is_new_script = true;

  pkStringResult result = { false, NULL, NULL };
  if (vm->config.load_script_fn != NULL)
    result = vm->config.load_script_fn(vm, name->data);

  if (!result.success) {
    vmPopTempRef(vm); // name

    String* err_msg = stringFormat(vm, "Cannot import script '@'", name);
    vm->fiber->error = err_msg; //< Set the error msg.

    return VAR_NULL;
  }

  Script* script = newScript(vm, name);
  vmPushTempRef(vm, &script->_super);
  mapSet(vm->scripts, vm, VAR_OBJ(&name->_super), VAR_OBJ(&script->_super));
  vmPopTempRef(vm);

  vmPopTempRef(vm); // name

  bool success = compile(vm, script, result.string);
  if (result.on_done) result.on_done(vm, result);

  if (!success) {
    vm->fiber->error = stringFormat(vm, "Compilation of script '@' failed.",
                                    name);
    return VAR_NULL;
  }

  return VAR_OBJ(&script->_super);
}

static void ensureStackSize(PKVM* vm, int size) {
  if (vm->fiber->stack_size > size) return;
  TODO;
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
  ensureStackSize(vm, needed);

  CallFrame* frame = &vm->fiber->frames[vm->fiber->frame_count++];
  frame->rbp = vm->fiber->ret;
  frame->fn = fn;
  frame->ip = fn->fn->opcodes.data;
}

void pkSetRuntimeError(PKVM* vm, const char* format, ...) {
  vm->fiber->error = newString(vm, "TODO:");
  TODO; // Construct String and set to vm->fiber->error.
}

void vmReportError(PKVM* vm) {
  ASSERT(HAS_ERROR(), "runtimeError() should be called after an error.");
  TODO; // TODO: create debug.h
}

PKInterpretResult pkInterpret(PKVM* vm, const char* file) {

  // Resolve file path.
  String* name = newString(vm, file);
  vmPushTempRef(vm, &name->_super);

  if (!resolveScriptPath(vm, &name)) {
    vm->config.error_fn(vm, PK_ERROR_COMPILE, NULL, -1,
      stringFormat(vm, "Failed to resolve path '$'.", file)->data);
    return PK_RESULT_COMPILE_ERROR;
  }

  // Load the script source.
  pkStringResult res = vm->config.load_script_fn(vm, name->data);
  if (!res.success) {
    vm->config.error_fn(vm, PK_ERROR_COMPILE, NULL, -1,
      stringFormat(vm, "Failed to load script '@'.", name)->data);
    return PK_RESULT_COMPILE_ERROR;
  }

  // Load a new script to the vm's scripts cache.
  Script* scr = getScript(vm, name);
  if (scr == NULL) {
    scr = newScript(vm, name);
    vmPushTempRef(vm, &scr->_super);
    mapSet(vm->scripts, vm, VAR_OBJ(&name->_super), VAR_OBJ(&scr->_super));
    vmPopTempRef(vm);
  }
  vmPopTempRef(vm); // name

  // Compile the source.
  bool success = compile(vm, scr, res.string);
  if (res.on_done) res.on_done(vm, res);

  if (!success) return PK_RESULT_COMPILE_ERROR;
  vm->script = scr;

  return vmRunScript(vm, scr);
}

#ifdef DEBUG
#include <stdio.h>

// FIXME: for temp debugging. (implement dump stack frames).
void _debugRuntime(PKVM* vm) {
  return;
  system("cls");
  Fiber* fiber = vm->fiber;

  for (int i = fiber->frame_count - 1; i >= 0; i--) {
    CallFrame frame = fiber->frames[i];

    Var* top = fiber->sp - 1;
    if (i != fiber->frame_count - 1) {
      top = fiber->frames[i + 1].rbp - 1;
    }

    for (; top >= frame.rbp; top--) {
      printf("[*]: ");
      dumpValue(vm, *top); printf("\n");
    }

    printf("----------------\n");
  }
}

#endif

PKInterpretResult vmRunScript(PKVM* vm, Script* _script) {

  register uint8_t* ip;      //< Current instruction pointer.
  register Var* rbp;         //< Stack base pointer register.
  register CallFrame* frame; //< Current call frame.
  register Script* script;   //< Currently executing script.

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
#define PEEK()       (*(vm->fiber->sp - 1))
#define READ_BYTE()  (*ip++)
#define READ_SHORT() (ip+=2, (uint16_t)((ip[-2] << 8) | ip[-1]))

// Check if any runtime error exists and if so returns RESULT_RUNTIME_ERROR.
#define CHECK_ERROR()                 \
  do {                                \
    if (HAS_ERROR()) {                \
      vmReportError(vm);              \
      return PK_RESULT_RUNTIME_ERROR; \
    }                                 \
  } while (false)

// Note: '##__VA_ARGS__' is not portable but most common compilers including
// gcc, msvc, clang, tcc (c99) supports.
#define RUNTIME_ERROR(fmt, ...)                \
  do {                                         \
    pkSetRuntimeError(vm, fmt, ##__VA_ARGS__); \
    vmReportError(vm);                         \
    return PK_RESULT_RUNTIME_ERROR;            \
  } while (false)

// Store the current frame to vm's call frame before pushing a new frame.
// Frames rbp will set once it created and will never change.
#define STORE_FRAME() frame->ip = ip

// Update the call frame and ip once vm's call frame pushed or popped.
// fuction call, return or done running imported script.
#define LOAD_FRAME()                                       \
  do {                                                     \
    frame = &vm->fiber->frames[vm->fiber->frame_count-1];  \
    ip = frame->ip;                                        \
    rbp = frame->rbp;                                      \
    script = frame->fn->owner;                             \
  } while (false)

#ifdef OPCODE
  #error "OPCODE" should not be deifined here.
#endif

#if DEBUG
  #define DEBUG_INSTRUCTION() _debugRuntime(vm)
#else
  #define DEBUG_INSTRUCTION() do { } while (false)
#endif


#define SWITCH(code)                 \
  L_vm_main_loop:                    \
  DEBUG_INSTRUCTION();               \
  switch (code = (Opcode)READ_BYTE())
#define OPCODE(code) case OP_##code
#define DISPATCH()   goto L_vm_main_loop

  PUSH(VAR_NULL); // Return value of the script body.
  LOAD_FRAME();

  Opcode instruction;
  SWITCH(instruction) {

    OPCODE(CONSTANT):
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
      Var elem = POP();
      Var list = PEEK();
      ASSERT(IS_OBJ(list) && AS_OBJ(list)->type == OBJ_LIST, OOPS);
      varBufferWrite(&((List*)AS_OBJ(list))->elements, vm, elem);
      DISPATCH();
    }

    OPCODE(MAP_INSERT):
    {
      Var value = POP();
      Var key = POP();
      Var on = PEEK();

      varsetSubscript(vm, on, key, value);
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
      rbp[index + 1] = PEEK();  // +1: rbp[0] is return value.
      DISPATCH();
    }
    OPCODE(STORE_LOCAL_N):
    {
      uint16_t index = READ_SHORT();
      rbp[index + 1] = PEEK();  // +1: rbp[0] is return value.
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
      script->globals.data[index] = PEEK();
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
      Function* fn = getBuiltinFunction(READ_SHORT());
      PUSH(VAR_OBJ(&fn->_super));
      DISPATCH();
    }

    OPCODE(POP):
      DROP();
      DISPATCH();

    OPCODE(IMPORT) :
    {
      String* name = script->names.data[READ_SHORT()];
      PUSH(importScript(vm, name, true, NULL));
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
          RUNTIME_ERROR("Expected excatly %d argument(s).", fn->arity);
        }

        // Now the function will never needed in the stack and it'll
        // initialized with VAR_NULL as return value.
        *callable = VAR_NULL;

        // Next call frame starts here. (including return value).
        vm->fiber->ret = callable;

        if (fn->is_native) {
          if (fn->native == NULL) {
            RUNTIME_ERROR("Native function pointer of %s was NULL.", fn->name);
          }
          fn->native(vm);
          // Pop function arguments except for the return value.
          vm->fiber->sp = vm->fiber->ret + 1;
          CHECK_ERROR();

        } else {
          STORE_FRAME();
          pushCallFrame(vm, fn);
          LOAD_FRAME();
        }

      } else {
        RUNTIME_ERROR("Expected a function in call.");
      }
      DISPATCH();
    }

    OPCODE(ITER):
    {
      Var* iter_value = (vm->fiber->sp - 1);
      Var* iterator   = (vm->fiber->sp - 2);
      Var* container  = (vm->fiber->sp - 3);
      uint16_t jump_offset = READ_SHORT();
      if (!varIterate(vm, *container, iterator, iter_value)) {
        DROP(); //< Iter value.
        DROP(); //< Iterator.
        DROP(); //< Container.
        ip += jump_offset;
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

    OPCODE(RETURN):
    {
      Var ret = POP();
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
      Var on = POP();
      String* name = script->names.data[READ_SHORT()];
      PUSH(varGetAttrib(vm, on, name));
      DISPATCH();
    }

    OPCODE(GET_ATTRIB_KEEP):
    {
      Var on = PEEK();
      String* name = script->names.data[READ_SHORT()];
      PUSH(varGetAttrib(vm, on, name));
      DISPATCH();
    }

    OPCODE(SET_ATTRIB):
    {
      Var value = POP();
      Var on = POP();
      String* name = script->names.data[READ_SHORT()];
      varSetAttrib(vm, on, name, value);
      PUSH(value);
      DISPATCH();
    }

    OPCODE(GET_SUBSCRIPT):
    {
      Var key = POP();
      Var on = POP();
      PUSH(varGetSubscript(vm, on, key));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GET_SUBSCRIPT_KEEP):
    {
      Var key = *(vm->fiber->sp - 1);
      Var on = *(vm->fiber->sp - 2);
      PUSH(varGetSubscript(vm, on, key));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(SET_SUBSCRIPT):
    {
      Var value = POP();
      Var key = POP();
      Var on = POP();

      varsetSubscript(vm, on, key, value);
      CHECK_ERROR();
      PUSH(value);

      DISPATCH();
    }

    OPCODE(NEGATIVE):
    {
      Var num = POP();
      if (!IS_NUM(num)) {
        RUNTIME_ERROR("Cannot negate a non numeric value.");
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

    OPCODE(ADD):
      PUSH(varAdd(vm, POP(), POP()));
      CHECK_ERROR();
      DISPATCH();

    OPCODE(SUBTRACT):
      PUSH(varSubtract(vm, POP(), POP()));
      CHECK_ERROR();
      DISPATCH();

    OPCODE(MULTIPLY):
      PUSH(varMultiply(vm, POP(), POP()));
      CHECK_ERROR();
      DISPATCH();

    OPCODE(DIVIDE):
      PUSH(varDivide(vm, POP(), POP()));
      CHECK_ERROR();
      DISPATCH();

    OPCODE(MOD):
    OPCODE(BIT_AND):
    OPCODE(BIT_OR):
    OPCODE(BIT_XOR):
    OPCODE(BIT_LSHIFT):
    OPCODE(BIT_RSHIFT):
    OPCODE(AND):
      TODO;

    OPCODE(OR):
    {
      TODO;
      // Python like or operator.
      //Var v1 = POP(), v2 = POP();
      //if (toBool(v1)) {
      //  PUSH(v1);
      //} else {
      //  PUSH(v2);
      //}
      DISPATCH();
    }

    OPCODE(EQEQ):
      PUSH(VAR_BOOL(isValuesEqual(POP(), POP())));
      DISPATCH();

    OPCODE(NOTEQ):
      PUSH(VAR_BOOL(!isValuesEqual(POP(), POP())));
      DISPATCH();

    OPCODE(LT):
    {
      PUSH(VAR_BOOL(varLesser(vm, POP(), POP())));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(LTEQ):
    {
      Var l = POP(), r = POP();
      bool lteq = varLesser(vm, l, r);
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
      PUSH(VAR_BOOL(varGreater(vm, POP(), POP())));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GTEQ):
    {
      Var l = POP(), r = POP();
      bool gteq = varGreater(vm, l, r);
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
      Var to = POP();
      Var from = POP();
      if (!IS_NUM(from) || !IS_NUM(to)) {
        RUNTIME_ERROR("Range arguments must be number.");
      }
      PUSH(VAR_OBJ(newRange(vm, AS_NUM(from), AS_NUM(to))));
      DISPATCH();
    }

    OPCODE(IN):
      // TODO: Implement bool varContaines(vm, on, value);

    OPCODE(END):
      TODO;
      break;

    default:
      UNREACHABLE();

  }

  return PK_RESULT_SUCCESS;
}
