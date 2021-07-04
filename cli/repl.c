/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

// The REPL (Read Evaluate Print Loop) implementation.
// https://en.wikipedia.org/wiki/Read-eval-print_loop.

#include "internal.h"

#include <ctype.h> // isspace
#include "utils.h"

// FIXME: use fgetc char by char till reach a new line.
// TODO: Will need refactoring to use a byte buffer.
//
// Read a line from stdin and returns it without the line ending. Accepting
// an optional argument [length] (could be NULL). Note that the returned string
// is heap allocated and should be cleaned by 'free()' function.
const char* read_line(uint32_t* length) {
  const int size = 1024;
  char* mem = (char*)malloc(size);
  if (fgets(mem, size, stdin) == NULL) {
    // TODO: handle error.
  }
  size_t len = strlen(mem);

  // FIXME: handle \r\n, this is temp.
  mem[len - 1] = '\0';
  if (length != NULL) *length = (uint32_t)(len - 1);

  return mem;
}

// Read a line from stdin and returns it without the line ending.
static void readLine(ByteBuffer* buff) {
  do {
    char c = (char)fgetc(stdin);
    if (c == EOF || c == '\n') break;

    byteBufferWrite(buff, (uint8_t)c);
  } while (true);

  byteBufferWrite(buff, '\0');
}

// Returns true if the string is empty, used to check if the input line is
// empty to skip compilation of empty string.
static inline bool is_str_empty(const char* line) {
  ASSERT(line != NULL, OOPS);

  for (const char* c = line; *c != '\0'; c++) {
    if (!isspace(*c)) return false;
  }
  return true;
}

// The main loop of the REPL. Will return the exit code.
int repl(PKVM* vm, const PkCompileOptions* options) {

  // Set repl_mode of the user_data.
  VmUserData* user_data = (VmUserData*)pkGetUserData(vm);
  user_data->repl_mode = true;

  // The main module that'll be used to compile and execute the input source.
  PkHandle* module = pkNewModule(vm, "$(REPL)");

  // A buffer to store lines read from stdin.
  ByteBuffer lines;
  byteBufferInit(&lines);

  // A buffer to store a line read from stdin.
  ByteBuffer line;
  byteBufferInit(&line);

  // Will be set to true if the compilation failed with unexpected EOF to add
  // more lines to the [lines] buffer.
  bool need_more_lines = false;

  bool done = false;
  do {

    // Print the input listening line.
    if (!need_more_lines) {
      printf(">>> ");
    } else {
      printf("... ");
    }

    // Read a line from stdin and add the line to the lines buffer.
    readLine(&line);
    bool is_empty = is_str_empty((const char*)line.data);

    // If the line is empty, we don't have to compile it.
    if (is_empty && !need_more_lines) {
      byteBufferClear(&line);
      ASSERT(lines.count == 0, OOPS);
      continue;
    }

    if (lines.count != 0) byteBufferWrite(&lines, '\n');
    byteBufferAddString(&lines, (const char*)line.data, line.count - 1);
    byteBufferClear(&line);
    byteBufferWrite(&lines, '\0');

    // Compile the buffer to the module.
    PkStringPtr source_ptr = { (const char*)lines.data, NULL, NULL, 0, 0 };
    PkResult result = pkCompileModule(vm, module, source_ptr, options);

    if (result == PK_RESULT_UNEXPECTED_EOF) {
      ASSERT(lines.count > 0 && lines.data[lines.count - 1] == '\0', OOPS);
      lines.count -= 1; // Remove the null byte to append a new string.
      need_more_lines = true;
      continue;
    }

    // We're buffering the lines for unexpected EOF, if we reached here that
    // means it's either successfully compiled or compilation error. Clean the
    // buffer for the next iteration.
    need_more_lines = false;
    byteBufferClear(&lines);

    if (result != PK_RESULT_SUCCESS) continue;

    // Compiled source would be the "main" function of the module. Run it.
    PkHandle* _main = pkGetFunction(vm, module, PK_IMPLICIT_MAIN_NAME);
    PkHandle* fiber = pkNewFiber(vm, _main);
    result = pkRunFiber(vm, fiber, 0, NULL);
    pkReleaseHandle(vm, _main);
    pkReleaseHandle(vm, fiber);

  } while (!done);

  byteBufferClear(&lines);
  pkReleaseHandle(vm, module);

  return 0;
}

