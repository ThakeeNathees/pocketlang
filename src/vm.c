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

void* vmRealloc(MSVM* self, void* memory, size_t old_size, size_t new_size) {

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

void msInitConfiguration(MSConfiguration* config) {
  config->realloc_fn = defaultRealloc;

  // TODO: Handle Null functions before calling them.
  config->error_fn = NULL;
  config->write_fn = NULL;

  config->load_script_fn = NULL;
  config->load_script_done_fn = NULL;
  config->user_data = NULL;
}

MSVM* msNewVM(MSConfiguration* config) {
  MSVM* vm = (MSVM*)malloc(sizeof(MSVM));
  vmInit(vm, config);
  return vm;
}

void msFreeVM(MSVM* self) {
  // TODO: Check if vm already freed.

  Object* obj = self->first;
  while (obj != NULL) {
    Object* next = obj->next;
    freeObject(self, obj);
    obj = next;
  }

  self->gray_list = (Object**)self->config.realloc_fn(
    self->gray_list, 0, self->config.user_data);
  self->config.realloc_fn(self, 0, self->config.user_data);
}

void vmInit(MSVM* self, MSConfiguration* config) {
  memset(self, 0, sizeof(MSVM));
  self->config = *config;

  self->gray_list_count = 0;
  self->gray_list_capacity = 8; // TODO: refactor the magic '8' here.
  self->gray_list = (Object**)self->config.realloc_fn(
    NULL, sizeof(Object*) * self->gray_list_capacity, NULL);
  self->next_gc = 1024 * 1024 * 10; // TODO:

  // TODO: no need to initialize if already done by another vm.
  initializeCore(self);
}

void vmPushTempRef(MSVM* self, Object* obj) {
  ASSERT(obj != NULL, "Cannot reference to NULL.");
  ASSERT(self->temp_reference_count < MAX_TEMP_REFERENCE, 
         "Too many temp references");
  self->temp_reference[self->temp_reference_count++] = obj;
}

void vmPopTempRef(MSVM* self) {
  ASSERT(self->temp_reference_count > 0, "Temporary reference is empty to pop.");
  self->temp_reference_count--;
}

void vmCollectGarbage(MSVM* self) {

  // Reset VM's bytes_allocated value and count it again so that we don't
  // required to know the size of each object that'll be freeing.
  self->bytes_allocated = 0;

  // Mark core objects (mostlikely builtin functions).
  markCoreObjects(self);

  // Mark all the 'std' scripts.
  for (int i = 0; i < self->std_count; i++) {
    grayObject(&(self->std_scripts[i]->_super), self);
  }

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

void vmAddStdScript(MSVM* self, Script* script) {
  ASSERT(self->std_count < MAX_SCRIPT_CACHE, OOPS);
  self->std_scripts[self->std_count++] = script;
}

Script* vmGetStdScript(MSVM* self, const char* name) {
  for (int i = 0; i < self->std_count; i++) {
    Script* scr = self->std_scripts[i];
    // +4 to skip "std:".
    if (strcmp(name, scr->name + 4) == 0) {
      return scr;
    }
  }
  return NULL;
}

void* msGetUserData(MSVM* vm) {
  return vm->config.user_data;
}

void msSetUserData(MSVM* vm, void* user_data) {
  vm->config.user_data = user_data;
}

/******************************************************************************
 * RUNTIME                                                                    *
 *****************************************************************************/

static void ensureStackSize(MSVM* vm, int size) {
  if (vm->fiber->stack_size > size) return;
  TODO;
}

static inline void pushCallFrame(MSVM* vm, Function* fn) {
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

void msSetRuntimeError(MSVM* vm, const char* format, ...) {
  vm->fiber->error = newString(vm, "TODO:", 5);
  TODO;
}

void vmReportError(MSVM* vm) {
  ASSERT(HAS_ERROR(), "runtimeError() should be called after an error.");
  ASSERT(false, "TODO: create debug.h");
}

MSInterpretResult msInterpret(MSVM* vm, const char* file) {
  Script* script = compileSource(vm, file);
  if (script == NULL) return RESULT_COMPILE_ERROR;

  vm->script = script;
  return vmRunScript(vm, script);
}

#ifdef DEBUG
#include <stdio.h>

// FIXME: for temp debugging. (implement dump stack frames).
void _debugRuntime(MSVM* vm) {
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

MSInterpretResult vmRunScript(MSVM* vm, Script* _script) {

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
#define CHECK_ERROR()              \
  do {                             \
    if (HAS_ERROR()) {             \
      vmReportError(vm);           \
      return RESULT_RUNTIME_ERROR; \
    }                              \
  } while (false)

// Note: '##__VA_ARGS__' is not portable but most common compilers including
// gcc, msvc, clang, tcc (c99) supports.
#define RUNTIME_ERROR(fmt, ...)                \
  do {                                         \
    msSetRuntimeError(vm, fmt, ##__VA_ARGS__); \
    vmReportError(vm);                         \
    return RESULT_RUNTIME_ERROR;               \
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
      int index = READ_SHORT();
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

    OPCODE(LIST_APPEND):
    {
      Var elem = POP();
      Var list = *(vm->fiber->sp - 1);
      ASSERT(IS_OBJ(list) && AS_OBJ(list)->type == OBJ_LIST, OOPS);
      varBufferWrite(&((List*)AS_OBJ(list))->elements, vm, elem);
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
      int index = READ_SHORT();
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
      int index = READ_SHORT();
      rbp[index + 1] = PEEK();  // +1: rbp[0] is return value.
      DISPATCH();
    }

    OPCODE(PUSH_GLOBAL):
    {
      int index = READ_SHORT();
      ASSERT(index < script->globals.count, OOPS);
      PUSH(script->globals.data[index]);
      DISPATCH();
    }

    OPCODE(STORE_GLOBAL):
    {
      int index = READ_SHORT();
      ASSERT(index < script->globals.count, OOPS);
      script->globals.data[index] = PEEK();
      DISPATCH();
    }

    OPCODE(PUSH_FN):
    {
      int index = READ_SHORT();
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

    OPCODE(CALL):
    {
      int argc = READ_SHORT();
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

    OPCODE(ITER) :
    {
      Var* iter_value = (vm->fiber->sp - 1);
      Var* iterator   = (vm->fiber->sp - 2);
      Var* container  = (vm->fiber->sp - 3);
      int jump_offset = READ_SHORT();
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
      int offset = READ_SHORT();
      ip += offset;
      DISPATCH();
    }

    OPCODE(LOOP):
    {
      int offset = READ_SHORT();
      ip -= offset;
      DISPATCH();
    }


    OPCODE(JUMP_IF):
    {
      Var cond = POP();
      int offset = READ_SHORT();
      if (toBool(cond)) {
        ip += offset;
      }
      DISPATCH();
    }

    OPCODE(JUMP_IF_NOT):
    {
      Var cond = POP();
      int offset = READ_SHORT();
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
        return RESULT_SUCCESS;
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

    OPCODE(GET_ATTRIB_AOP):
    {
      Var on = *(vm->fiber->sp - 1);
      String* name = script->names.data[READ_SHORT()];
      PUSH(varGetAttrib(vm, on, name));
      DISPATCH();
    }

    OPCODE(SET_ATTRIB):
      TODO;

    OPCODE(GET_SUBSCRIPT):
    {
      Var key = POP();
      Var on = POP();
      PUSH(varGetSubscript(vm, on, key));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GET_SUBSCRIPT_AOP):
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
    OPCODE(OR):
    OPCODE(EQEQ):
    OPCODE(NOTEQ):
      TODO;

    OPCODE(LT):
    {
      PUSH(VAR_BOOL(varLesser(vm, POP(), POP())));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(LTEQ):
      TODO;

    OPCODE(GT):
    {
      PUSH(VAR_BOOL(varGreater(vm, POP(), POP())));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GTEQ):
      TODO;

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
    OPCODE(END):
      TODO;
      break;

    default:
      UNREACHABLE();

  }

  return RESULT_SUCCESS;
}
