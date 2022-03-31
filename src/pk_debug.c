/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include "pk_debug.h"

#include <stdio.h>
#include "pk_core.h"
#include "pk_var.h"
#include "pk_vm.h"

// Opcode names array.
static const char* op_names[] = {
  #define OPCODE(name, params, stack) #name,
  #include "pk_opcodes.h"
  #undef OPCODE
};

// Instead of writing foo("a long string", strlen("a long string") or
// foo("a long string may be changed", 28) using _STR_AND_LEN would be cleaner.
// And the strlen(literal cstring) will always optimized.
#define STR_AND_LEN(str) str, (uint32_t)strlen(str)

void dumpValue(PKVM* vm, Var value, pkByteBuffer* buff) {
  String* repr = toRepr(vm, value);
  vmPushTempRef(vm, &repr->_super);
  pkByteBufferAddString(buff, vm, repr->data, repr->length);
  vmPopTempRef(vm);
}

void dumpFunctionCode(PKVM* vm, Function* func, pkByteBuffer* buff) {

#define INDENTATION "  "
#define ADD_CHAR(vm, buff, c) pkByteBufferWrite(buff, vm, c)
#define INT_WIDTH 5
#define ADD_INTEGER(vm, buff, value, width)                          \
  do {                                                               \
    char sbuff[STR_INT_BUFF_SIZE];                                   \
    int length;                                                      \
    if ((width) > 0) length = sprintf(sbuff, "%*d", (width), value); \
    else length = sprintf(sbuff, "%d", value);                       \
    pkByteBufferAddString(buff, vm, sbuff, (uint32_t)length);        \
  } while(false)

  uint32_t i = 0;
  uint8_t* opcodes = func->fn->opcodes.data;
  uint32_t* lines = func->fn->oplines.data;
  uint32_t line = 1, last_line = 0;

  // This will print: Instruction Dump of function 'fn' "path.pk"\n
  pkByteBufferAddString(buff, vm,
                        STR_AND_LEN("Instruction Dump of function '"));
  pkByteBufferAddString(buff, vm, STR_AND_LEN(func->name));
  pkByteBufferAddString(buff, vm, STR_AND_LEN("' \""));
  pkByteBufferAddString(buff, vm, STR_AND_LEN(func->owner->path->data));
  pkByteBufferAddString(buff, vm, STR_AND_LEN("\"\n"));

#define READ_BYTE() (opcodes[i++])
#define READ_SHORT() (i += 2, opcodes[i - 2] << 8 | opcodes[i-1])

#define NO_ARGS() ADD_CHAR(vm, buff, '\n')
#define BYTE_ARG()                                 \
  do {                                             \
    ADD_INTEGER(vm, buff, READ_BYTE(), INT_WIDTH); \
    ADD_CHAR(vm, buff, '\n');                      \
  } while (false)

#define SHORT_ARG()                                 \
  do {                                              \
    ADD_INTEGER(vm, buff, READ_SHORT(), INT_WIDTH); \
    ADD_CHAR(vm, buff, '\n');                       \
  } while (false)

  while (i < func->fn->opcodes.count) {
    ASSERT_INDEX(i, func->fn->opcodes.count);

    // Prints the line number.
    line = lines[i];
    if (line != last_line) {
      last_line = line;
      pkByteBufferAddString(buff, vm, STR_AND_LEN(INDENTATION));
      ADD_INTEGER(vm, buff, line, INT_WIDTH - 1);
      ADD_CHAR(vm, buff, ':');

    } else {
      pkByteBufferAddString(buff, vm, STR_AND_LEN(INDENTATION "     "));
    }

    // Prints: INDENTATION "%4d  %-16s"

    pkByteBufferAddString(buff, vm, STR_AND_LEN(INDENTATION));
    ADD_INTEGER(vm, buff, i, INT_WIDTH - 1);
    pkByteBufferAddString(buff, vm, STR_AND_LEN("  "));

    const char* op_name = op_names[opcodes[i]];
    uint32_t op_length = (uint32_t)strlen(op_name);
    pkByteBufferAddString(buff, vm, op_name, op_length);
    for (uint32_t j = 0; j < 16 - op_length; j++) { // Padding.
      ADD_CHAR(vm, buff, ' ');
    }

    Opcode op = (Opcode)func->fn->opcodes.data[i++];
    switch (op) {
      case OP_PUSH_CONSTANT:
      {
        int index = READ_SHORT();
        ASSERT_INDEX((uint32_t)index, func->owner->literals.count);
        Var value = func->owner->literals.data[index];

        // Prints: %5d [val]\n
        ADD_INTEGER(vm, buff, index, INT_WIDTH);
        ADD_CHAR(vm, buff, ' ');
        dumpValue(vm, value, buff);
        ADD_CHAR(vm, buff, '\n');
        break;
      }

      case OP_PUSH_NULL:
      case OP_PUSH_0:
      case OP_PUSH_TRUE:
      case OP_PUSH_FALSE:
      case OP_SWAP:
        NO_ARGS();
        break;

      case OP_PUSH_LIST:     SHORT_ARG(); break;
      case OP_PUSH_INSTANCE:
      {
        int ty_index = READ_BYTE();
        ASSERT_INDEX((uint32_t)ty_index, func->owner->classes.count);
        uint32_t name_ind = func->owner->classes.data[ty_index]->name;
        ASSERT_INDEX(name_ind, func->owner->names.count);
        String* ty_name = func->owner->names.data[name_ind];

        // Prints: %5d [Ty:%s]\n
        ADD_INTEGER(vm, buff, ty_index, INT_WIDTH);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(" [Ty:"));
        pkByteBufferAddString(buff, vm, ty_name->data, ty_name->length);
        pkByteBufferAddString(buff, vm, STR_AND_LEN("]\n"));
        break;
      }
      case OP_PUSH_MAP:      NO_ARGS();   break;
      case OP_LIST_APPEND:   NO_ARGS();   break;
      case OP_MAP_INSERT:    NO_ARGS();   break;
      case OP_INST_APPEND:   NO_ARGS();   break;

      case OP_PUSH_LOCAL_0:
      case OP_PUSH_LOCAL_1:
      case OP_PUSH_LOCAL_2:
      case OP_PUSH_LOCAL_3:
      case OP_PUSH_LOCAL_4:
      case OP_PUSH_LOCAL_5:
      case OP_PUSH_LOCAL_6:
      case OP_PUSH_LOCAL_7:
      case OP_PUSH_LOCAL_8:
      case OP_PUSH_LOCAL_N:
      {

        int arg;
        if (op == OP_PUSH_LOCAL_N) {
          arg = READ_BYTE();
          ADD_INTEGER(vm, buff, arg, INT_WIDTH);

        } else {
          arg = (int)(op - OP_PUSH_LOCAL_0);
          for (int j = 0; j < INT_WIDTH; j++) ADD_CHAR(vm, buff, ' ');
        }

        if (arg < func->arity) {
          // Prints: (arg:%d)\n
          pkByteBufferAddString(buff, vm, STR_AND_LEN(" (param:"));
          ADD_INTEGER(vm, buff, arg, 1);
          pkByteBufferAddString(buff, vm, STR_AND_LEN(")\n"));

        } else {
          ADD_CHAR(vm, buff, '\n');
        }

      } break;

      case OP_STORE_LOCAL_0:
      case OP_STORE_LOCAL_1:
      case OP_STORE_LOCAL_2:
      case OP_STORE_LOCAL_3:
      case OP_STORE_LOCAL_4:
      case OP_STORE_LOCAL_5:
      case OP_STORE_LOCAL_6:
      case OP_STORE_LOCAL_7:
      case OP_STORE_LOCAL_8:
      case OP_STORE_LOCAL_N:
      {
        int arg;
        if (op == OP_STORE_LOCAL_N) {
          arg = READ_BYTE();
          ADD_INTEGER(vm, buff, arg, INT_WIDTH);

        } else {
          arg = (int)(op - OP_STORE_LOCAL_0);
          for (int j = 0; j < INT_WIDTH; j++) ADD_CHAR(vm, buff, ' ');
        }

        if (arg < func->arity) {
          // Prints: (arg:%d)\n
          pkByteBufferAddString(buff, vm, STR_AND_LEN(" (param:"));
          ADD_INTEGER(vm, buff, arg, 1);
          pkByteBufferAddString(buff, vm, STR_AND_LEN(")\n"));

        } else {
          ADD_CHAR(vm, buff, '\n');
        }

      } break;

      case OP_PUSH_GLOBAL:
      case OP_STORE_GLOBAL:
      {
        int index = READ_BYTE();
        int name_index = func->owner->global_names.data[index];
        String* name = func->owner->names.data[name_index];

        // Prints: %5d '%s'\n
        ADD_INTEGER(vm, buff, index, INT_WIDTH);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(" '"));
        pkByteBufferAddString(buff, vm, name->data, name->length);
        pkByteBufferAddString(buff, vm, STR_AND_LEN("'\n"));
        break;
      }

      case OP_PUSH_FN:
      {
        int fn_index = READ_BYTE();
        const char* name = func->owner->functions.data[fn_index]->name;

        // Prints: %5d [Fn:%s]\n
        ADD_INTEGER(vm, buff, fn_index, INT_WIDTH);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(" [Fn:"));
        pkByteBufferAddString(buff, vm, STR_AND_LEN(name));
        pkByteBufferAddString(buff, vm, STR_AND_LEN("]\n"));
        break;
      }

      case OP_PUSH_TYPE:
      {
        int ty_index = READ_BYTE();
        ASSERT_INDEX((uint32_t)ty_index, func->owner->classes.count);
        uint32_t name_ind = func->owner->classes.data[ty_index]->name;
        ASSERT_INDEX(name_ind, func->owner->names.count);
        String* ty_name = func->owner->names.data[name_ind];

        // Prints: %5d [Ty:%s]\n
        ADD_INTEGER(vm, buff, ty_index, INT_WIDTH);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(" [Ty:"));
        pkByteBufferAddString(buff, vm, ty_name->data, ty_name->length);
        pkByteBufferAddString(buff, vm, STR_AND_LEN("]\n"));

        break;
      }

      case OP_PUSH_BUILTIN_FN:
      {
        int index = READ_BYTE();
        const char* name = getBuiltinFunctionName(vm, index);

        // Prints: %5d [Fn:%s]\n
        ADD_INTEGER(vm, buff, index, INT_WIDTH);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(" [Fn:"));
        pkByteBufferAddString(buff, vm, STR_AND_LEN(name));
        pkByteBufferAddString(buff, vm, STR_AND_LEN("]\n"));
        break;
      }

      case OP_POP:    NO_ARGS(); break;
      case OP_IMPORT:
      {
        int index = READ_SHORT();
        String* name = func->owner->names.data[index];

        // Prints: %5d '%s'\n
        ADD_INTEGER(vm, buff, index, INT_WIDTH);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(" '"));
        pkByteBufferAddString(buff, vm, name->data, name->length);
        pkByteBufferAddString(buff, vm, STR_AND_LEN("'\n"));
        break;
      }

      case OP_CALL:
        // Prints: %5d (argc)\n
        ADD_INTEGER(vm, buff, READ_BYTE(), INT_WIDTH);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(" (argc)\n"));
        break;

      case OP_TAIL_CALL:
        // Prints: %5d (argc)\n
        ADD_INTEGER(vm, buff, READ_BYTE(), INT_WIDTH);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(" (argc)\n"));
        break;

      case OP_ITER_TEST: NO_ARGS(); break;

      case OP_ITER:
      case OP_JUMP:
      case OP_JUMP_IF:
      case OP_JUMP_IF_NOT:
      {
        int offset = READ_SHORT();

        // Prints: %5d (ip:%d)\n
        ADD_INTEGER(vm, buff, offset, INT_WIDTH);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(" (ip:"));
        ADD_INTEGER(vm, buff, i + offset, 0);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(")\n"));
        break;
      }

      case OP_LOOP:
      {
        int offset = READ_SHORT();

        // Prints: %5d (ip:%d)\n
        ADD_INTEGER(vm, buff, -offset, INT_WIDTH);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(" (ip:"));
        ADD_INTEGER(vm, buff, i - offset, 0);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(")\n"));
        break;
      }

      case OP_RETURN: NO_ARGS(); break;

      case OP_GET_ATTRIB:
      case OP_GET_ATTRIB_KEEP:
      case OP_SET_ATTRIB:
      {
        int index = READ_SHORT();
        String* name = func->owner->names.data[index];

        // Prints: %5d '%s'\n
        ADD_INTEGER(vm, buff, index, INT_WIDTH);
        pkByteBufferAddString(buff, vm, STR_AND_LEN(" '"));
        pkByteBufferAddString(buff, vm, name->data, name->length);
        pkByteBufferAddString(buff, vm, STR_AND_LEN("'\n"));
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
      case OP_RANGE_IN:
      case OP_RANGE_EX:
      case OP_IN:
      case OP_REPL_PRINT:
      case OP_END:
        NO_ARGS();
        break;

      default:
        UNREACHABLE();
        break;
    }
  }

  ADD_CHAR(vm, buff, '\0');

// Undefin everything defined for this function.
#undef INDENTATION
#undef ADD_CHAR
#undef STR_AND_LEN
#undef READ_BYTE
#undef READ_SHORT
#undef BYTE_ARG
#undef SHORT_ARG

}

//void dumpValue(PKVM* vm, Var value) {
//  pkByteBuffer buff;
//  pkByteBufferInit(&buff);
//  _dumpValueInternal(vm, value, &buff);
//  pkByteBufferWrite(&buff, vm, '\0');
//  printf("%s", (const char*)buff.data);
//}
//
//void dumpFunctionCode(PKVM* vm, Function* func) {
//  pkByteBuffer buff;
//  pkByteBufferInit(&buff);
//  _dumpFunctionCodeInternal(vm, func, &buff);
//  pkByteBufferWrite(&buff, vm, '\0');
//  printf("%s", (const char*)buff.data);
//}

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

    // Dump value. TODO: refactor.
    pkByteBuffer buff;
    pkByteBufferInit(&buff);
    dumpValue(vm, value, &buff);
    pkByteBufferWrite(&buff, vm, '\0');
    printf("%s", (const char*)buff.data);
    pkByteBufferClear(&buff, vm);

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

    // Dump value. TODO: refactor.
    pkByteBuffer buff;
    pkByteBufferInit(&buff);
    dumpValue(vm, *sp, &buff);
    pkByteBufferWrite(&buff, vm, '\0');
    printf("%s", (const char*)buff.data);
    pkByteBufferClear(&buff, vm);

    printf("\n");
  }
}

#undef _STR_AND_LEN
