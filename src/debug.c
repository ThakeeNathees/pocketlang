/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "debug.h"

#include <stdio.h>
#include "core.h"
#include "vm.h"

// To limit maximum elements to be dumpin in a map or a list.
#define MAX_DUMP_ELEMENTS 30

// Opcode names array.
static const char* op_name[] = {
  #define OPCODE(name, params, stack) #name,
  #include "opcodes.h"
  #undef OPCODE
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

          // Terminate the dump if it's too long.
          if (i >= MAX_DUMP_ELEMENTS) {
            printf("...");
            break;
          }
        }
        printf("]");
      }
      return;
    }

    case OBJ_MAP:
    {
      Map* map = (Map*)obj;
      if (recursive) {
        printf("{...}");
      } else {
        printf("{");
        bool first = true;
        for (uint32_t i = 0; i < map->capacity; i++) {
          if (IS_UNDEF(map->entries[i].key)) continue;
          if (!first) { printf(", "); first = false; }

          _dumpValue(vm, map->entries[i].key, true);
          printf(":");
          _dumpValue(vm, map->entries[i].value, true);

          // Terminate the dump if it's too long.
          if (i >= MAX_DUMP_ELEMENTS) {
            printf("...");
            break;
          }

        }
        printf("}");
      }
      return;
    }

    case OBJ_RANGE:
    {
      Range* range = ((Range*)obj);
      printf("%.2g..%.2g", range->from, range->to);
      return;
    }

    case OBJ_SCRIPT:
    {
      Script* scr = (Script*)obj;
      if (scr->moudle != NULL) {
        printf("[Script:%s]", scr->moudle->data);
      } else {
        printf("[Script:\"%s\"]", scr->path->data);
      }
      return;
    }

    case OBJ_FUNC:
      printf("[Fn:%s]", ((Function*)obj)->name);
      return;

    case OBJ_FIBER:
    {
      const Fiber* fb = (const Fiber*)obj;
      printf("[Fiber:%s]", fb->func->name);
      return;
    }

    case OBJ_USER:
      printf("[UserObj:0X%p]", obj);
      return;
  }
}

void dumpValue(PKVM* vm, Var value) {
  _dumpValue(vm, value, false);
}

void dumpFunctionCode(PKVM* vm, Function* func) {

  uint32_t i = 0;
  uint8_t* opcodes = func->fn->opcodes.data;
  uint32_t* lines = func->fn->oplines.data;
  uint32_t line = 1, last_line = 0;

  printf("Instruction Dump of function '%s' (%s)\n", func->name,
         func->owner->path->data);
#define READ_BYTE() (opcodes[i++])
#define READ_SHORT() (i += 2, opcodes[i - 2] << 8 | opcodes[i-1])

#define NO_ARGS() printf("\n")
#define BYTE_ARG() printf("%5d\n", READ_BYTE())
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
      case OP_PUSH_CONSTANT:
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
      case OP_PUSH_0:
      case OP_PUSH_TRUE:
      case OP_PUSH_FALSE:
      case OP_SWAP:
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
        READ_BYTE();
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
        READ_BYTE();
        break;

      case OP_PUSH_GLOBAL:
      case OP_STORE_GLOBAL:
      {
        int index = READ_BYTE();
        int name_index = func->owner->global_names.data[index];
        String* name = func->owner->names.data[name_index];
        printf("%5d '%s'\n", index, name->data);
        break;
      }

      case OP_PUSH_FN:
      {
        int fn_index = READ_BYTE();
        int name_index = func->owner->function_names.data[fn_index];
        String* name = func->owner->names.data[name_index];
        printf("%5d [Fn:%s]\n", fn_index, name->data);
        break;
      }

      case OP_PUSH_BUILTIN_FN:
      {
        int index = READ_BYTE();
        printf("%5d [Fn:%s]\n", index, getBuiltinFunctionName(vm, index));
        break;
      }

      case OP_POP:    NO_ARGS(); break;
      case OP_IMPORT:
      {
        int index = READ_SHORT();
        String* name = func->owner->names.data[index];
        printf("%5d '%s'\n", index, name->data);
        break;
      }

      case OP_CALL:
        printf("%5d (argc)\n", READ_BYTE());
        break;

      case OP_ITER_TEST: NO_ARGS(); break;

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
      {
        int index = READ_SHORT();
        String* name = func->owner->names.data[index];
        printf("%5d '%s'\n", index, name->data);
      } break;

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

void dumpGlobalValues(PKVM* vm) {
  Fiber* fiber = vm->fiber;
  int frame_ind = fiber->frame_count - 1;
  ASSERT(frame_ind >= 0, OOPS);
  CallFrame* frame = &fiber->frames[frame_ind];
  Script* scr = frame->fn->owner;

  for (uint32_t i = 0; i < scr->global_names.count; i++) {
    String* name = scr->names.data[scr->global_names.data[i]];
    Var value = scr->globals.data[i];
    printf("%10s = ", name->data);
    dumpValue(vm, value);
    printf("\n");
  }
}

void dumpStackFrame(PKVM* vm) {
  Fiber* fiber = vm->fiber;
  int frame_ind = fiber->frame_count - 1;
  ASSERT(frame_ind >= 0, OOPS);
  CallFrame* frame = &fiber->frames[frame_ind];
  Var* sp = fiber->sp - 1;

  printf("Frame[%d]\n", frame_ind);
  for (; sp >= frame->rbp; sp--) {
    printf("       ");
    dumpValue(vm, *sp);
    printf("\n");
  }
}

