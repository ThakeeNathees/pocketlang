/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <stdio.h>

#ifndef PK_AMALGAMATED
#include "debug.h"
#include "vm.h"
#endif

// FIXME:
// Refactor this. Maybe move to a module, Rgb values are hardcoded ?!
// Should check stderr/stdout etc.
static void _printRed(PKVM* vm, const char* msg) {
  if (vm->config.use_ansi_escape) {
    vm->config.stderr_write(vm, "\033[38;2;220;100;100m");
    vm->config.stderr_write(vm, msg);
    vm->config.stderr_write(vm, "\033[0m");
  } else {
    vm->config.stderr_write(vm, msg);
  }
}

void reportCompileTimeError(PKVM* vm, const char* path, int line,
                            const char* source, const char* at, int length,
                            const char* fmt, va_list args) {

  pkWriteFn writefn = vm->config.stderr_write;
  if (writefn == NULL) return;

  pkByteBuffer buff;
  pkByteBufferInit(&buff);
  {
    // Initial size set to 512. Will grow if needed.
    pkByteBufferReserve(&buff, vm, 512);

    buff.count = 0;
    writefn(vm, path);
    writefn(vm, ":");
    snprintf((char*)buff.data, buff.capacity, "%d", line);
    writefn(vm, (char*)buff.data);
    _printRed(vm, " error: ");

    // Print the error message.
    buff.count = 0;

    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, fmt, args_copy) + 1;
    va_end(args_copy);

    ASSERT(size >= 0, "vnsprintf() failed.");
    pkByteBufferReserve(&buff, vm, size);
    vsnprintf((char*)buff.data, size, fmt, args);
    writefn(vm, (char*)buff.data);
    writefn(vm, "\n");

    // Print the lines. (TODO: Optimize it).

    int start = line - 2; // First line.
    if (start < 1) start = 1;
    int end = start + 5; // Exclisive last line.

    int line_number_width = 5;

    int curr_line = line;
    const char* c = at;

    // Get the first character of the [start] line.
    if (c != source) {
      do {
        c--;
        if (*c == '\n') curr_line--;
        if (c == source) break;
      } while (curr_line >= start);
    }

    curr_line = start;
    if (c != source) {
      ASSERT(*c == '\n', OOPS);
      c++; // Enter the line.
    }

    // Print each lines.
    while (curr_line < end) {

      buff.count = 0;
      snprintf((char*)buff.data, buff.capacity,
               "%*d", line_number_width, curr_line);
      writefn(vm, (char*)buff.data);
      writefn(vm, " | ");

      if (curr_line != line) {
        // Run to the line end.
        const char* line_start = c;
        while (*c != '\0' && *c != '\n') c++;

        buff.count = 0;
        pkByteBufferAddString(&buff, vm, line_start,
                              (uint32_t)(c - line_start));
        pkByteBufferWrite(&buff, vm, '\0');
        writefn(vm, (char*)buff.data);
        writefn(vm, "\n");

      } else {

        const char* line_start = c;

        // Print line till error.
        buff.count = 0;
        pkByteBufferAddString(&buff, vm, line_start,
                              (uint32_t)(at - line_start));
        pkByteBufferWrite(&buff, vm, '\0');
        writefn(vm, (char*)buff.data);

        // Print error token - if the error token is a new line ignore it.
        if (*at != '\n') {
          buff.count = 0;
          pkByteBufferAddString(&buff, vm, at, length);
          pkByteBufferWrite(&buff, vm, '\0');
          _printRed(vm, (char*)buff.data);

          // Run to the line end. Note that tk.length is not reliable and
          // sometimes longer than the actual string which will cause a
          // buffer overflow.
          const char* tail_start = at;
          for (int i = 0; i < length; i++) {
            if (*tail_start == '\0') break;
            tail_start++;
          }

          c = tail_start;
          while (*c != '\0' && *c != '\n') c++;

          // Print rest of the line.
          if (c != tail_start) {
            buff.count = 0;
            pkByteBufferAddString(&buff, vm, tail_start,
                                  (uint32_t)(c - tail_start));
            pkByteBufferWrite(&buff, vm, '\0');
            writefn(vm, (char*)buff.data);
          }
        } else {
          c = at; // Run 'c' to the end of the line.
        }
        writefn(vm, "\n");

        // White space before error token.
        buff.count = 0;
        pkByteBufferFill(&buff, vm, ' ', line_number_width);
        pkByteBufferAddString(&buff, vm, " | ", 3);

        for (const char* c2 = line_start; c2 < at; c2++) {
          char white_space = (*c2 == '\t') ? '\t' : ' ';
          pkByteBufferWrite(&buff, vm, white_space);
        }

        pkByteBufferWrite(&buff, vm, '\0');
        writefn(vm, (char*)buff.data);

        // Error token underline.
        buff.count = 0;
        pkByteBufferFill(&buff, vm, '~', (uint32_t)(length ? length : 1));
        pkByteBufferWrite(&buff, vm, '\0');
        _printRed(vm, (char*)buff.data);
        writefn(vm, "\n");

      }

      if (*c == '\0') break;
      curr_line++; c++;
    }
  }
  pkByteBufferClear(&buff, vm);
}

static void _reportStackFrame(PKVM* vm, CallFrame* frame) {
  pkWriteFn writefn = vm->config.stderr_write;
  const Function* fn = frame->closure->fn;
  ASSERT(!fn->is_native, OOPS);

  // After fetching the instruction the ip will be inceased so we're
  // reducing it by 1. But stack overflows are occure before executing
  // any instruction of that function, so the instruction_index possibly
  // be -1 (set it to zero in that case).
  int instruction_index = (int) (frame->ip - fn->fn->opcodes.data) - 1;
  if (instruction_index == -1) instruction_index++;

  int line = fn->fn->oplines.data[instruction_index];

  if (fn->owner->path == NULL) {

    writefn(vm, "  [at:");
    char buff[STR_INT_BUFF_SIZE];
    sprintf(buff, "%2d", line);
    writefn(vm, buff);
    writefn(vm, "] ");
    writefn(vm, fn->name);
    writefn(vm, "()\n");

  } else {
    writefn(vm, "  ");
    writefn(vm, fn->name);
    writefn(vm, "() [");
    writefn(vm, fn->owner->path->data);
    writefn(vm, ":");
    char buff[STR_INT_BUFF_SIZE];
    sprintf(buff, "%d", line);
    writefn(vm, buff);
    writefn(vm, "]\n");
  }
}

void reportRuntimeError(PKVM* vm, Fiber* fiber) {

  pkWriteFn writefn = vm->config.stderr_write;
  if (writefn == NULL) return;

  // Error message.
  _printRed(vm, "Error: ");
  writefn(vm, fiber->error->data);
  writefn(vm, "\n");

  // If the stack frames are greater than 2 * max_dump_frames + 1,
  // we're only print the first [max_dump_frames] and last [max_dump_frames]
  // lines.
  int max_dump_frames = 10;

  if (fiber->frame_count > 2 * max_dump_frames) {
    for (int i =  0; i < max_dump_frames; i++) {
      CallFrame* frame = &fiber->frames[fiber->frame_count - 1 - i];
      _reportStackFrame(vm, frame);
    }

    int skipped_count = fiber->frame_count - max_dump_frames * 2;
    writefn(vm, "  ...  skipping ");
    char buff[STR_INT_BUFF_SIZE];
    sprintf(buff, "%d", skipped_count);
    writefn(vm, buff);
    writefn(vm, " stack frames\n");

    for (int i = max_dump_frames; i >= 0; i--) {
      CallFrame* frame = &fiber->frames[i];
      _reportStackFrame(vm, frame);
    }

  } else {
    for (int i = fiber->frame_count - 1; i >= 0; i--) {
      CallFrame* frame = &fiber->frames[i];
      _reportStackFrame(vm, frame);
    }
  }

}

// Opcode names array.
static const char* op_names[] = {
  #define OPCODE(name, params, stack) #name,
  #include "opcodes.h"  //<< AMALG_INLINE >>
  #undef OPCODE
};

static void dumpValue(PKVM* vm, Var value) {
  if (!vm->config.stdout_write) return;
  String* repr = toRepr(vm, value);
  vm->config.stdout_write(vm, repr->data);
  // String repr will be garbage collected - No need to clean.
}

void dumpFunctionCode(PKVM* vm, Function* func) {

  if (!vm->config.stdout_write) return;

#define _INDENTATION "  "
#define _INT_WIDTH 5 // Width of the integer string to print.

#define PRINT(str) vm->config.stdout_write(vm, str)
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

  // Either path or name should be valid to a module.
  ASSERT(func->owner->path != NULL || func->owner->name != NULL, OOPS);
  const char* path = (func->owner->path)
                       ? func->owner->path->data
                       : func->owner->name->data;

  // This will print: Instruction Dump of function 'fn' "path.pk"\n
  PRINT("Instruction Dump of function ");
  PRINT(func->name);
  PRINT(" ");
  PRINT(path);
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
      case OP_DUP:
        NO_ARGS();
        break;

      case OP_PUSH_LIST:
        SHORT_ARG();
        break;

      case OP_PUSH_MAP:
      case OP_PUSH_SELF:
      case OP_LIST_APPEND:
      case OP_MAP_INSERT:
        NO_ARGS();
        break;

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
        ASSERT_INDEX(index, (int)func->owner->global_names.count);
        int name_index = func->owner->global_names.data[index];
        ASSERT_INDEX(name_index, (int)func->owner->constants.count);

        Var name = func->owner->constants.data[name_index];
        ASSERT(IS_OBJ_TYPE(name, OBJ_STRING), OOPS);

        // Prints: %5d '%s'\n
        PRINT_INT(index);
        PRINT(" '");
        PRINT(((String*)AS_OBJ(name))->data);
        PRINT("'\n");
        break;
      }

      case OP_PUSH_BUILTIN_FN:
      {
        int index = READ_BYTE();
        ASSERT_INDEX(index, vm->builtins_count);
        const char* name = vm->builtins_funcs[index]->fn->name;
        // Prints: %5d [Fn:%s]\n
        PRINT_INT(index);
        PRINT(" [Fn:");
        PRINT(name);
        PRINT("]\n");
        break;
      }

      case OP_PUSH_BUILTIN_TY:
      {
        int index = READ_BYTE();
        ASSERT_INDEX(index, PK_INSTANCE);
        const char* name = vm->builtin_classes[index]->name->data;
        // Prints: %5d [Fn:%s]\n
        PRINT_INT(index);
        PRINT(" [Class:");
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

      case OP_CREATE_CLASS:
      {
        int index = READ_SHORT();
        ASSERT_INDEX((uint32_t)index, func->owner->constants.count);
        Var value = func->owner->constants.data[index];
        ASSERT(IS_OBJ_TYPE(value, OBJ_CLASS), OOPS);

        // Prints: %5d [val]\n
        PRINT_INT(index);
        PRINT(" ");
        dumpValue(vm, value);
        NEWLINE();
        break;
      }

      case OP_BIND_METHOD:
      case OP_CLOSE_UPVALUE:
      case OP_POP:
        NO_ARGS();
        break;

      case OP_IMPORT:
      {
        int index = READ_SHORT();
        String* name = moduleGetStringAt(func->owner, index);
        ASSERT(name != NULL, OOPS);
        // Prints: %5d '%s'\n
        PRINT_INT(index);
        PRINT(" '");
        PRINT(name->data);
        PRINT("'\n");
        break;
      }

      case OP_SUPER_CALL:
      case OP_METHOD_CALL:
      {
        int argc = READ_BYTE();
        int index = READ_SHORT();
        String* name = moduleGetStringAt(func->owner, index);
        ASSERT(name != NULL, OOPS);

        // Prints: %5d (argc) %d '%s'\n
        PRINT_INT(argc);
        PRINT(" (argc) ");

        _PRINT_INT(index, 0);
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
        String* name = moduleGetStringAt(func->owner, index);
        ASSERT(name != NULL, OOPS);

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

      case OP_POSITIVE:
      case OP_NEGATIVE:
      case OP_NOT:
      case OP_BIT_NOT:

      case OP_ADD:
      case OP_SUBTRACT:
      case OP_MULTIPLY:
      case OP_DIVIDE:
      case OP_EXPONENT:
      case OP_MOD:
      case OP_BIT_AND:
      case OP_BIT_OR:
      case OP_BIT_XOR:
      case OP_BIT_LSHIFT:
      case OP_BIT_RSHIFT:
      {
        uint8_t inplace = READ_BYTE();
        if (inplace == 1) {
          PRINT("(inplace)\n");
        } else {
          PRINT("\n");
          ASSERT(inplace == 0, "inplace should be either 0 or 1");
        }
        break;
      }

      case OP_EQEQ:
      case OP_NOTEQ:
      case OP_LT:
      case OP_LTEQ:
      case OP_GT:
      case OP_GTEQ:
      case OP_RANGE:
      case OP_IN:
      case OP_IS:
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
    String* name = moduleGetStringAt(module, module->global_names.data[i]);
    ASSERT(name != NULL, OOPS);
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

