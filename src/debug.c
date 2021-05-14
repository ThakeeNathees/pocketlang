/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include <stdio.h>

#include "core.h"
#include "debug.h"
#include "vm.h"

static const char* op_name[] = {
  #define OPCODE(name, params, stack) #name,
  #include "opcodes.h"
  #undef OPCODE
  NULL,
};


static void _dumpValue(PKVM* vm, Var value, bool recursive) {
  if (IS_NULL(value)) {
    printf("null");
    return;
  }
  if (IS_BOOL(value)) {
    printf((AS_BOOL(value)) ? "true" : "false");
    return;
  }
  if (IS_NUM(value)) {
    printf("%.14g", AS_NUM(value));
    return;
  }
  ASSERT(IS_OBJ(value), OOPS);
  Object* obj = AS_OBJ(value);
  switch (obj->type) {
    case OBJ_STRING:
      printf("\"%s\"", ((String*)obj)->data);
      return;
    case OBJ_LIST:
    {
      List* list = ((List*)obj);
      if (recursive) {
        printf("[...]");
      } else {
        printf("[");
        for (uint32_t i = 0; i < list->elements.count; i++) {
          if (i != 0) printf(", ");
          _dumpValue(vm, list->elements.data[i], true);
        }
        printf("]");
      }
      return;
    }

    case OBJ_MAP:
      TODO;
      return;

    case OBJ_RANGE:
    {
      Range* range = ((Range*)obj);
      printf("%.2g..%.2g", range->from, range->to);
      return;
    }

    case OBJ_SCRIPT:
      printf("[Script:%p]", obj);
      return;
    case OBJ_FUNC:
      printf("[Fn:%p]", obj);
      return;

    case OBJ_FIBER:
      printf("[Fn:%p]", obj);
      return;

    case OBJ_USER:
      printf("[UserObj:%p]", obj);
      return;
  }
}

void dumpValue(PKVM* vm, Var value) {
  _dumpValue(vm, value, false);
}

void dumpInstructions(PKVM* vm, Function* func) {  


  uint32_t i = 0;
  uint8_t* opcodes = func->fn->opcodes.data;
  uint32_t* lines = func->fn->oplines.data;
  uint32_t line = 1, last_line = 0;

  printf("Instruction Dump of function '%s'\n", func->name);
#define READ_BYTE() (opcodes[i++])
#define READ_SHORT() (i += 2, opcodes[i - 2] << 8 | opcodes[i-1])

#define NO_ARGS() printf("\n")
#define SHORT_ARG() printf("%5d\n", READ_SHORT())
#define INDENTATION "  "

  while (i < func->fn->opcodes.count) {
    ASSERT_INDEX(i, func->fn->opcodes.count);

    // Print the line number.
    line = lines[i];
    if (line != last_line) {
      printf(INDENTATION "%4d:", line);
      last_line = line;
    } else {
      printf(INDENTATION "     ");
    }

    printf(INDENTATION "%4d  %-16s", i, op_name[opcodes[i]]);

    Opcode op = (Opcode)func->fn->opcodes.data[i++];
    switch (op) {
      case OP_CONSTANT:
      {
        int index = READ_SHORT();
        printf("%5d ", index);
        ASSERT_INDEX((uint32_t)index, func->owner->literals.count);
        Var value = func->owner->literals.data[index];
        dumpValue(vm, value);
        printf("\n");
        break;
      }

      case OP_PUSH_NULL:
      case OP_PUSH_SELF:
      case OP_PUSH_TRUE:
      case OP_PUSH_FALSE:
        NO_ARGS();
        break;

      case OP_PUSH_LIST:   SHORT_ARG(); break;
      case OP_PUSH_MAP:    NO_ARGS();   break;
      case OP_LIST_APPEND: NO_ARGS();   break;
      case OP_MAP_INSERT:  NO_ARGS();   break;


      case OP_PUSH_LOCAL_0:
      case OP_PUSH_LOCAL_1:
      case OP_PUSH_LOCAL_2:
      case OP_PUSH_LOCAL_3:
      case OP_PUSH_LOCAL_4:
      case OP_PUSH_LOCAL_5:
      case OP_PUSH_LOCAL_6:
      case OP_PUSH_LOCAL_7:
      case OP_PUSH_LOCAL_8:
        NO_ARGS();
        break;

      case OP_PUSH_LOCAL_N:
        SHORT_ARG();
        break;

      case OP_STORE_LOCAL_0:
      case OP_STORE_LOCAL_1:
      case OP_STORE_LOCAL_2:
      case OP_STORE_LOCAL_3:
      case OP_STORE_LOCAL_4:
      case OP_STORE_LOCAL_5:
      case OP_STORE_LOCAL_6:
      case OP_STORE_LOCAL_7:
      case OP_STORE_LOCAL_8:
        NO_ARGS();
        break;

      case OP_STORE_LOCAL_N:
        SHORT_ARG();
        break;

      case OP_PUSH_GLOBAL:
      case OP_STORE_GLOBAL:
      case OP_PUSH_FN:
        SHORT_ARG();
        break;

      case OP_PUSH_BUILTIN_FN:
      {
        int index = READ_SHORT();
        printf("%5d [Fn:%s]\n", index, getBuiltinFunctionName(index));
        break;
      }
        

      case OP_POP:    NO_ARGS(); break;
      case OP_IMPORT: NO_ARGS(); break;

      case OP_CALL:
        printf("%5d (argc)\n", READ_SHORT());
        break;

      case OP_ITER:
      case OP_JUMP:
      case OP_JUMP_IF:
      case OP_JUMP_IF_NOT:
      {
        int offset = READ_SHORT();
        printf("%5d (ip:%d)\n", offset, i + offset);
        break;
      }

      case OP_LOOP:
      {
        int offset = READ_SHORT();
        printf("%5d (ip:%d)\n", -offset, i - offset);
        break;
      }

      case OP_RETURN: NO_ARGS(); break;

      case OP_GET_ATTRIB:
      case OP_GET_ATTRIB_KEEP:
      case OP_SET_ATTRIB:
        SHORT_ARG();
        break;

      case OP_GET_SUBSCRIPT:
      case OP_GET_SUBSCRIPT_KEEP:
      case OP_SET_SUBSCRIPT:
        NO_ARGS();
        break;

      case OP_NEGATIVE:
      case OP_NOT:
      case OP_BIT_NOT:
      case OP_ADD:
      case OP_SUBTRACT:
      case OP_MULTIPLY:
      case OP_DIVIDE:
      case OP_MOD:
      case OP_BIT_AND:
      case OP_BIT_OR:
      case OP_BIT_XOR:
      case OP_BIT_LSHIFT:
      case OP_BIT_RSHIFT:
      case OP_AND:
      case OP_OR:
      case OP_EQEQ:
      case OP_NOTEQ:
      case OP_LT:
      case OP_LTEQ:
      case OP_GT:
      case OP_GTEQ:
      case OP_RANGE:
      case OP_IN:
      case OP_END:
        NO_ARGS();
        break;

      default:
        UNREACHABLE();
        break;
    }
  }
}

void reportStackTrace(PKVM* vm) {
  if (vm->config.error_fn == NULL) return;

  Fiber* fiber = vm->fiber;

  vm->config.error_fn(vm, PK_ERROR_RUNTIME, NULL, -1, fiber->error->data);

  for (int i = fiber->frame_count - 1; i >= 0; i--) {
    CallFrame* frame = &fiber->frames[i];
    Function* fn = frame->fn;
    ASSERT(!fn->is_native, OOPS);
    int line = fn->fn->oplines.data[frame->ip - fn->fn->opcodes.data - 1];
    vm->config.error_fn(vm, PK_ERROR_STACKTRACE, fn->owner->name->data, line, fn->name);
  }
}

