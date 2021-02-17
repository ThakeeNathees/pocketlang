/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "vm.h"

#include "core.h"
#include "debug.h"
#include "utils.h"

#define HAS_ERROR() (vm->error != NULL)

// Initially allocated call frame capacity. Will grow dynamically.
#define INITIAL_CALL_FRAMES 4

// Minimum size of the stack.
#define MIN_STACK_SIZE 128

void* vmRealloc(MSVM* self, void* memory, size_t old_size, size_t new_size) {

  // Track the total allocated memory of the VM to trigger the GC.
  // if vmRealloc is called for freeing the old_size would be 0 since 
  // deallocated bytes are traced by garbage collector.
  self->bytes_allocated += new_size - old_size;

  // TODO: If vm->bytes_allocated > some_value -> GC();

  if (new_size == 0) {
    free(memory);
    return NULL;
  }

  return realloc(memory, new_size);
}

void vmInit(MSVM* self, MSConfiguration* config) {
  memset(self, 0, sizeof(MSVM));
  self->config = *config;

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

#ifdef DEBUG
#include <stdio.h>
// TODO: A function for quick debug. REMOVE.
void _printStackTop(MSVM* vm) {
  if (vm->sp != vm->stack) {
    Var v = *(vm->sp - 1);
    printf("%s\n", toString(vm, v, false)->data);
  }
}
#endif

static void ensureStackSize(MSVM* vm, int size) {
  if (vm->stack_size > size) return;
  TODO;
}

static inline void pushCallFrame(MSVM* vm, Function* fn) {
  ASSERT(!fn->is_native, "Native function shouldn't use call frames.");

  // Grow the stack frame if needed.
  if (vm->frame_count + 1 > vm->frame_capacity) {
    int new_capacity = vm->frame_capacity * 2;
    vm->frames = (CallFrame*)vmRealloc(vm, vm->frames,
                           sizeof(CallFrame) * vm->frame_capacity,
                           sizeof(CallFrame) * new_capacity);
    vm->frame_capacity = new_capacity;
  }

  // Grow the stack if needed.
  int stack_size = (int)(vm->sp - vm->stack);
  int needed = stack_size + fn->fn->stack_size;
  ensureStackSize(vm, needed);

  CallFrame* frame = &vm->frames[vm->frame_count++];
  frame->rbp = vm->rbp + 1; // vm->rbp is the return value.
  frame->fn = fn;
  frame->ip = fn->fn->opcodes.data;
}

void msSetRuntimeError(MSVM* vm, const char* format, ...) {
  vm->error = newString(vm, "TODO:", 5);
  TODO;
}

void vmReportError(MSVM* vm) {
  ASSERT(HAS_ERROR(), "runtimeError() should be called after an error.");
  ASSERT(false, "TODO: create debug.h");
}

MSVM* msNewVM(MSConfiguration* config) {
  MSVM* vm = (MSVM*)malloc(sizeof(MSVM));
  vmInit(vm, config);
  return vm;
}

MSInterpretResult msInterpret(MSVM* vm, const char* file) {
  Script* script = compileSource(vm, file);
  if (script == NULL) return RESULT_COMPILE_ERROR;

  // TODO: Check if scripts size is enough.
  vm->scripts[vm->script_count++] = script;
  return vmRunScript(vm, script);
}

MSInterpretResult vmRunScript(MSVM* vm, Script* _script) {

  register uint8_t* ip;      //< Current instruction pointer.
  register Var* rbp;         //< Stack base pointer register.
  register CallFrame* frame; //< Current call frame.
  register Script* script;   //< Currently executing script.

#define PUSH(value) (*vm->sp++ = (value))
#define POP()       (*(--vm->sp))
#define DROP()      (--vm->sp)
#define PEEK()      (*(vm->sp - 1))
#define READ_BYTE() (*ip++)
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
#define LOAD_FRAME()                         \
  do {                                       \
    frame = &vm->frames[vm->frame_count-1];  \
    ip = frame->ip;                          \
    rbp = frame->rbp;                        \
    script = frame->fn->owner;               \
  } while (false)

#ifdef OPCODE
  #error "OPCODE" should not be deifined here.
#endif

#define DEBUG_INSTRUCTION() //_printStackTop(vm)

#define SWITCH(code)                 \
  L_vm_main_loop:                    \
  DEBUG_INSTRUCTION();               \
  switch (code = (Opcode)READ_BYTE())
#define OPCODE(code) case OP_##code
#define DISPATCH()   goto L_vm_main_loop

  // Allocate stack.
  int stack_size = utilPowerOf2Ceil(_script->body->fn->stack_size + 1);
  if (stack_size < MIN_STACK_SIZE) stack_size = MIN_STACK_SIZE;
  vm->stack_size = stack_size;
  vm->stack = ALLOCATE_ARRAY(vm, Var, vm->stack_size);
  vm->sp = vm->stack;
  vm->rbp = vm->stack;
  PUSH(VAR_NULL); // Return value of the script body.

  // Allocate call frames.
  vm->frame_capacity = INITIAL_CALL_FRAMES;
  vm->frames = ALLOCATE_ARRAY(vm, CallFrame, vm->frame_capacity);
  vm->frame_count = 1;

  // Initialize VM's first frame.
  vm->frames[0].ip = _script->body->fn->opcodes.data;
  vm->frames[0].fn = _script->body;
  vm->frames[0].rbp = vm->rbp + 1; // +1 to skip script's null return value.

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
      Var list = *(vm->sp - 1);
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
      PUSH(rbp[index]);
      DISPATCH();
    }
    OPCODE(PUSH_LOCAL_N):
    {
      int index = READ_SHORT();
      PUSH(rbp[index]);
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
      rbp[index] = PEEK();
      DISPATCH();
    }
    OPCODE(STORE_LOCAL_N):
    {
      int index = READ_SHORT();
      rbp[index] = PEEK();
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
      Var* callable = vm->sp - argc - 1;

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
        vm->rbp = callable;

        if (fn->is_native) {
          fn->native(vm);
          // Pop function arguments except for the return value.
          vm->sp = vm->rbp + 1;
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
      Var* iter_value = (vm->sp - 1);
      Var* iterator   = (vm->sp - 2);
      Var* container  = (vm->sp - 3);
      int jump_offset = READ_SHORT();
      if (!varIterate(vm, *container, iterator, iter_value)) {
        DROP(); //< Iter value.
        DROP(); //< Iterator.
        DROP(); //< Container.
        ip += jump_offset;
      }
      DISPATCH();
    }

    OPCODE(JUMP): {
      int offset = READ_SHORT();
      ip += offset;
      DISPATCH();
    }

    OPCODE(LOOP): {
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
      vm->frame_count--;

      // If no more call frames. We're done.
      if (vm->frame_count == 0) {
        vm->sp = vm->stack;
        PUSH(ret);
        return RESULT_SUCCESS;
      }

      // Set the return value.
      *(frame->rbp - 1) = ret;

      // Pop the locals and update stack pointer.
      vm->sp = frame->rbp;

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
      Var on = *(vm->sp - 1);
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
      Var key = *(vm->sp - 1);
      Var on = *(vm->sp - 2);
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
