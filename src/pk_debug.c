/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "pk_debug.h"

#include <stdio.h>
#include "pk_core.h"
#include "pk_value.h"
#include "pk_vm.h"

// Opcode names array.
static const char* op_names[] = {
  #define OPCODE(name, params, stack) #name,
  #include "pk_opcodes.h"
  #undef OPCODE
};

static void dumpValue(PKVM* vm, Var value) {
  if (!vm->config.write_fn) return;
  String* repr = toRepr(vm, value);
  vm->config.write_fn(vm, repr->data);
  // String repr will be garbage collected - No need to clean.
}

void dumpFunctionCode(PKVM* vm, Function* func) {

  if (!vm->config.write_fn) return;

#define _INDENTATION "  "
#define _INT_WIDTH 5 // Width of the integer string to print.

#define PRINT(str) vm->config.write_fn(vm, str)
#define NEWLINE() PRINT("\n")

#define _PRINT_INT(value, width)                                     \
  do {                                                               \
    char sbuff[STR_INT_BUFF_SIZE];                                   \
    int length;                                                      \
    if ((width) > 0) length = sprintf(sbuff, "%*d", (width), value); \
    else length = sprintf(sbuff, "%d", value);                       \
    sbuff[length] = '\0';                                            \
    PRINT(sbuff);                                                    \
  } while(false)
#define PRINT_INT(value) _PRINT_INT(value, _INT_WIDTH)

#define READ_BYTE() (opcodes[i++])
#define READ_SHORT() (i += 2, opcodes[i - 2] << 8 | opcodes[i-1])

#define NO_ARGS() NEWLINE()

#define BYTE_ARG()          \
  do {                      \
    PRINT_INT(READ_BYTE()); \
    NEWLINE();              \
  } while (false)

#define SHORT_ARG()          \
  do {                       \
    PRINT_INT(READ_SHORT()); \
    NEWLINE();               \
  } while (false)

  uint32_t i = 0;
  uint8_t* opcodes = func->fn->opcodes.data;
  uint32_t* lines = func->fn->oplines.data;
  uint32_t line = 1, last_line = 0;

  // This will print: Instruction Dump of function 'fn' "path.pk"\n
  PRINT("Instruction Dump of function ");
  PRINT(func->name);
  PRINT(" ");
  PRINT(func->owner->path->data);
  NEWLINE();

  while (i < func->fn->opcodes.count) {
    ASSERT_INDEX(i, func->fn->opcodes.count);

    // Prints the line number.
    line = lines[i];
    if (line != last_line) {
      last_line = line;
      PRINT(_INDENTATION);
      _PRINT_INT(line, _INT_WIDTH - 1);
      PRINT(":");

    } else {
      PRINT(_INDENTATION "     ");
    }

    // Prints: INDENTATION "%4d  %-16s"
    PRINT(_INDENTATION);
    _PRINT_INT(i, _INT_WIDTH - 1);
    PRINT(_INDENTATION);

    const char* op_name = op_names[opcodes[i]];
    uint32_t op_length = (uint32_t)strlen(op_name);
    PRINT(op_name);
    for (uint32_t j = 0; j < 16 - op_length; j++) { // Padding.
      PRINT(" ");
    }

    Opcode op = (Opcode)func->fn->opcodes.data[i++];
    switch (op) {
      case OP_PUSH_CONSTANT:
      {
        int index = READ_SHORT();
        ASSERT_INDEX((uint32_t)index, func->owner->constants.count);
        Var value = func->owner->constants.data[index];

        // Prints: %5d [val]\n
        PRINT_INT(index);
        PRINT(" ");
        dumpValue(vm, value);
        NEWLINE();
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
        int cls_index = READ_SHORT();
        ASSERT_INDEX((uint32_t)cls_index, func->owner->constants.count);
        Var constant = func->owner->constants.data[cls_index];
        ASSERT(IS_OBJ_TYPE(constant, OBJ_CLASS), OOPS);
        uint32_t name_ind = ((Class*)(AS_OBJ(constant)))->name;
        ASSERT_INDEX(name_ind, func->owner->names.count);
        String* cls_name = func->owner->names.data[name_ind];

        // Prints: %5d [Class:%s]\n
        PRINT_INT(cls_index);
        PRINT(" [Class:");
        PRINT(cls_name->data);
        PRINT("]\n");
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
          PRINT_INT(arg);

        } else {
          arg = (int)(op - OP_PUSH_LOCAL_0);
          for (int j = 0; j < _INT_WIDTH; j++) PRINT(" ");
        }

        // Prints: (arg:%d)\n
        if (arg < func->arity) {
          PRINT(" (param:");
          _PRINT_INT(arg, 1);
          PRINT(")\n");
        } else {
          NEWLINE();
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
          PRINT_INT(arg);

        } else {
          arg = (int)(op - OP_STORE_LOCAL_0);
          for (int j = 0; j < _INT_WIDTH; j++) PRINT(" ");
        }

        if (arg < func->arity) {
          // Prints: (arg:%d)\n
          PRINT(" (param:");
          _PRINT_INT(arg, 1);
          PRINT(")\n");
        } else {
          NEWLINE();
        }

      } break;

      case OP_PUSH_GLOBAL:
      case OP_STORE_GLOBAL:
      {
        int index = READ_BYTE();
        int name_index = func->owner->global_names.data[index];
        String* name = func->owner->names.data[name_index];
        // Prints: %5d '%s'\n
        PRINT_INT(index);
        PRINT(" '");
        PRINT(name->data);
        PRINT("'\n");
        break;
      }

      case OP_PUSH_BUILTIN_FN:
      {
        int index = READ_BYTE();
        ASSERT_INDEX(index, vm->builtins_count);
        const char* name = vm->builtins[index]->fn->name;
        // Prints: %5d [Fn:%s]\n
        PRINT_INT(index);
        PRINT(" [Fn:");
        PRINT(name);
        PRINT("]\n");
        break;
      }

      case OP_PUSH_UPVALUE:
      case OP_STORE_UPVALUE:
      {
        int index = READ_BYTE();
        PRINT_INT(index);
        NEWLINE();
        break;
      }

      case OP_PUSH_CLOSURE:
      {
        int index = READ_SHORT();
        ASSERT_INDEX((uint32_t)index, func->owner->constants.count);
        Var value = func->owner->constants.data[index];
        ASSERT(IS_OBJ_TYPE(value, OBJ_FUNC), OOPS);

        // Prints: %5d [val]\n
        PRINT_INT(index);
        PRINT(" ");
        dumpValue(vm, value);
        NEWLINE();
        break;
      }

      case OP_CLOSE_UPVALUE:
      case OP_POP:
        NO_ARGS();
        break;

      case OP_IMPORT:
      {
        int index = READ_SHORT();
        String* name = func->owner->names.data[index];
        // Prints: %5d '%s'\n
        PRINT_INT(index);
        PRINT(" '");
        PRINT(name->data);
        PRINT("'\n");
        break;
      }

      case OP_CALL:
        // Prints: %5d (argc)\n
        PRINT_INT(READ_BYTE());
        PRINT(" (argc)\n");
        break;

      case OP_TAIL_CALL:
        // Prints: %5d (argc)\n
        PRINT_INT(READ_BYTE());
        PRINT(" (argc)\n");
        break;

      case OP_ITER_TEST: NO_ARGS(); break;

      case OP_ITER:
      case OP_JUMP:
      case OP_JUMP_IF:
      case OP_JUMP_IF_NOT:
      case OP_OR:
      case OP_AND:
      {
        int offset = READ_SHORT();
        // Prints: %5d (ip:%d)\n
        PRINT_INT(offset);
        PRINT(" (ip:");
        _PRINT_INT(i + offset, 0);
        PRINT(")\n");
        break;
      }

      case OP_LOOP:
      {
        int offset = READ_SHORT();
        // Prints: %5d (ip:%d)\n
        PRINT_INT(-offset);
        PRINT(" (ip:");
        _PRINT_INT(i - offset, 0);
        PRINT(")\n");
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
        PRINT_INT(index);
        PRINT(" '");
        PRINT(name->data);
        PRINT("'\n");
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
      case OP_REPL_PRINT:
      case OP_END:
        NO_ARGS();
        break;

      default:
        UNREACHABLE();
        break;
    }
  }

  NEWLINE();

// Undefin everything defined for this function.
#undef PRINT
#undef NEWLINE
#undef _INT_WIDTH
#undef _INDENTATION
#undef READ_BYTE
#undef READ_SHORT
#undef BYTE_ARG
#undef SHORT_ARG
}

void dumpGlobalValues(PKVM* vm) {
  Fiber* fiber = vm->fiber;
  int frame_ind = fiber->frame_count - 1;
  ASSERT(frame_ind >= 0, OOPS);
  CallFrame* frame = &fiber->frames[frame_ind];
  Module* module = frame->closure->fn->owner;

  for (uint32_t i = 0; i < module->global_names.count; i++) {
    String* name = module->names.data[module->global_names.data[i]];
    Var value = module->globals.data[i];
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

    // Dump value. TODO: refactor.
    dumpValue(vm, *sp);
    printf("\n");
  }
}

