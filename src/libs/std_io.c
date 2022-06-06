/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <math.h>

#ifndef PK_AMALGAMATED
#include "libs.h"
#include "../core/value.h"
#endif

DEF(_ioWrite,
  "io.write(stream:Var, bytes:String) -> Null",
  "Warning: the function is subjected to be changed anytime soon.\n"
  "Write [bytes] string to the stream. stream should be any of io.stdin, "
  "io.stdout, io.stderr.") {

  double stream;
  if (!pkValidateSlotNumber(vm, 1, &stream)) return;

  if (stream != 0 && stream != 1 && stream != 2) {
    pkSetRuntimeErrorFmt(vm, "Invalid stream (%g). Only use any of io.stdin, "
                         "io.stdout, io.stderr.", stream);
    return;
  }

  const char* bytes;
  uint32_t length;
  if (!pkValidateSlotString(vm, 2, &bytes, &length)) return;

  switch ((int) stream) {
    case 0:
      pkSetRuntimeError(vm, "Cannot write to stdin.");
      return;

    case 1:
      fwrite(bytes, sizeof(char), length, stdout);
      return;

    case 2:
      fwrite(bytes, sizeof(char), length, stderr);
      return;
  }
}

DEF(_ioFlush,
  "io.flush() -> Null",
  "Warning: the function is subjected to be changed anytime soon.\n"
  "Flush stdout buffer.") {
  fflush(stdout);
}

DEF(_ioGetc,
  "io.getc() -> String",
  "Read a single character from stdin and return it.") {
  char c = (char) fgetc(stdin);
  pkSetSlotStringLength(vm, 0, &c, 1);
}

/*****************************************************************************/
/* FILE CLASS                                                                */
/*****************************************************************************/

 // Str  | If already exists | If does not exist |
 // -----+-------------------+-------------------|
 // 'r'  |  read from start  |   failure to open |
 // 'w'  |  destroy contents |   create new      |
 // 'a'  |  write to end     |   create new      |
 // 'r+' |  read from start  |   error           |
 // 'w+' |  destroy contents |   create new      |
 // 'a+' |  write to end     |   create new      |
typedef enum {
  FMODE_NONE       = 0,

  FMODE_READ       = (1 << 0),
  FMODE_WRITE      = (1 << 1),
  FMODE_APPEND     = (1 << 2),

  _FMODE_EXT       = (1 << 3),
  _FMODE_BIN       = (1 << 4),

  FMODE_READ_EXT   = (_FMODE_EXT | FMODE_READ),
  FMODE_WRITE_EXT  = (_FMODE_EXT | FMODE_WRITE),
  FMODE_APPEND_EXT = (_FMODE_EXT | FMODE_APPEND),

  FMODE_READ_BIN   = (_FMODE_BIN | FMODE_READ),
  FMODE_WRITE_BIN  = (_FMODE_BIN | FMODE_WRITE),
  FMODE_APPEND_BIN = (_FMODE_BIN | FMODE_APPEND),

  FMODE_READ_BIN_EXT   = (_FMODE_BIN | FMODE_READ_EXT),
  FMODE_WRITE_BIN_EXT  = (_FMODE_BIN | FMODE_WRITE_EXT),
  FMODE_APPEND_BIN_EXT = (_FMODE_BIN | FMODE_APPEND_EXT),

} FileAccessMode;

typedef struct {
  FILE* fp;            // C file poinnter.
  FileAccessMode mode; // Access mode of the file.
  bool closed;         // True if the file isn't closed yet.
} File;

void* _fileNew(PKVM* vm) {
  File* file = pkRealloc(vm, NULL, sizeof(File));
  ASSERT(file != NULL, "pkRealloc failed.");
  file->closed = true;
  file->mode = FMODE_NONE;
  file->fp = NULL;
  return file;
}

void _fileDelete(PKVM* vm, void* ptr) {
  File* file = (File*)ptr;
  if (!file->closed) {
    ASSERT(file->fp != NULL, OOPS);
    if (fclose(file->fp) != 0) { /* TODO: error! */ }
    file->closed = true;
    file->fp = NULL;

  } else {
    ASSERT(file->fp == NULL, OOPS);
  }

  pkRealloc(vm, file, 0);
}

/*****************************************************************************/
/* FILE MODULE FUNCTIONS                                                     */
/*****************************************************************************/

DEF(_fileOpen,
  "io.File.open(path:String, mode:String) -> Null",
  "Opens a file at the [path] with the [mode]. Path should be either "
  "absolute or relative to the current working directory. and [mode] can be"
  "'r', 'w', 'a' in combination with 'b' (binary) and/or '+' (extended).\n"
  "```\n"
  " mode | If already exists | If does not exist |\n"
  " -----+-------------------+-------------------|\n"
  " 'r'  |  read from start  |   failure to open |\n"
  " 'w'  |  destroy contents |   create new      |\n"
  " 'a'  |  write to end     |   create new      |\n"
  " 'r+' |  read from start  |   error           |\n"
  " 'w+' |  destroy contents |   create new      |\n"
  " 'a+' |  write to end     |   create new      |\n"
  "```") {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 1, 2)) return;

  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  const char* mode_str = "r";
  FileAccessMode mode = FMODE_READ;

  if (argc == 2) {
    if (!pkValidateSlotString(vm, 2, &mode_str, NULL)) return;

    // TODO: I should properly parser this.
    do {
      if (strcmp(mode_str, "r")  == 0) { mode = FMODE_READ;       break; }
      if (strcmp(mode_str, "w")  == 0) { mode = FMODE_WRITE;      break; }
      if (strcmp(mode_str, "a")  == 0) { mode = FMODE_APPEND;     break; }

      if (strcmp(mode_str, "r+") == 0) { mode = FMODE_READ_EXT;   break; }
      if (strcmp(mode_str, "w+") == 0) { mode = FMODE_WRITE_EXT;  break; }
      if (strcmp(mode_str, "a+") == 0) { mode = FMODE_APPEND_EXT; break; }

      if (strcmp(mode_str, "rb")  == 0) { mode = FMODE_READ_BIN;       break; }
      if (strcmp(mode_str, "wb")  == 0) { mode = FMODE_WRITE_BIN;      break; }
      if (strcmp(mode_str, "ab")  == 0) { mode = FMODE_APPEND_BIN;     break; }

      if (strcmp(mode_str, "rb+") == 0) { mode = FMODE_READ_BIN_EXT;   break; }
      if (strcmp(mode_str, "wb+") == 0) { mode = FMODE_WRITE_BIN_EXT;  break; }
      if (strcmp(mode_str, "ab+") == 0) { mode = FMODE_APPEND_BIN_EXT; break; }

      // TODO: (fmt, ...) va_arg for runtime error public api.
      // If we reached here, that means it's an invalid mode string.
      pkSetRuntimeError(vm, "Invalid mode string.");
      return;
    } while (false);
  }

  FILE* fp = fopen(path, mode_str);

  if (fp != NULL) {
    File* self = (File*) pkGetSelf(vm);
    self->fp = fp;
    self->mode = mode;
    self->closed = false;

  } else {
    pkSetRuntimeError(vm, "Error opening the file.");
  }
}

DEF(_fileRead,
  "io.File.read(count:Number) -> String",
  "Reads [count] number of bytes from the file and return it as String."
  "If the count is -1 it'll read till the end of file and return it.") {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 0, 1)) return;

  // If count == -1, that means read all bytes.
  long count = -1;

  if (argc == 1) {
    // Not using pkValidateInteger since it's supports 32 bit int range.
    // but we need a long.
    double count_;
    if (!pkValidateSlotNumber(vm, 1, &count_)) return;
    if (floor(count_) != count_) {
      pkSetRuntimeError(vm, "Expected an integer.");
      return;
    }
    if (count_ < 0 && count_ != -1) {
      pkSetRuntimeError(vm, "Read bytes count should be either > 0 or == -1.");
      return;
    }
    count = (long) count_;
  }

  File* file = (File*) pkGetSelf(vm);

  if (file->closed) {
    pkSetRuntimeError(vm, "Cannot read from a closed file.");
    return;
  }

  if (!(file->mode & FMODE_READ) && !(_FMODE_EXT & file->mode)) {
    pkSetRuntimeError(vm, "File is not readable.");
    return;
  }

  if (count == -1) {
    // Get the source length. In windows the ftell will includes the cariage
    // return when using ftell with fseek. But that's not an issue since
    // we'll be allocating more memory than needed for fread().
    long current = ftell(file->fp);
    fseek(file->fp, 0, SEEK_END);
    count = ftell(file->fp);
    fseek(file->fp, current, SEEK_SET);
  }

  // Allocate string + 1 for the NULL terminator.
  char* buff = pkRealloc(vm, NULL, (size_t) count + 1);
  ASSERT(buff != NULL, "pkRealloc failed.");

  clearerr(file->fp);
  size_t read = fread(buff, sizeof(char), count, file->fp);

  if (ferror(file->fp)) {
    REPORT_ERRNO(fread);
    goto L_done;
  }

  bool is_read_failed = read > count;
  if (!is_read_failed) buff[read] = '\0';

  // If EOF is already reached it won't read anymore bytes.
  if (read == 0) {
    pkSetSlotStringLength(vm, 0, "", 0);
    goto L_done;
  }

  if (is_read_failed) {
    pkSetRuntimeError(vm, "C.fread() failed.");
    goto L_done;
  }

  // TODO: maybe I should check if the [read] length is larger for uint32_t.
  pkSetSlotStringLength(vm, 0, buff, (uint32_t) read);
  goto L_done;

L_done:
  pkRealloc(vm, buff, 0);
  return;

}

// Note that fgetline is not standard in older version of C. so we're defining
// something similler.
DEF(_fileGetLine,
  "io.File.getline() -> String",
  "Reads a line from the file and return it as string. This function can only "
  "be used for files that are opened with text mode.") {
  File* file = (File*) pkGetSelf(vm);

  if (file->closed) {
    pkSetRuntimeError(vm, "Cannot read from a closed file.");
    return;
  }

  if (!(file->mode & FMODE_READ) && !(_FMODE_EXT & file->mode)) {
    pkSetRuntimeError(vm, "File is not readable.");
    return;
  }

  if (file->mode & _FMODE_BIN) {
    pkSetRuntimeError(vm, "Cannot getline binary files.");
    return;
  }

  // FIXME:
  // I'm not sure this is an efficient method to read a line using fgetc
  // and a dynamic buffer. Consider fgets() or maybe find '\n' in file
  // and use pkRealloc() with size using ftell() or something.

  pkByteBuffer buff;
  pkByteBufferInit(&buff);
  char c;
  do {
    c = (char) fgetc(file->fp);

    // End of file or error.
    if (c == EOF) {
      if (ferror(file->fp)) {
        REPORT_ERRNO(fgetc);
        goto L_done;
      }
      break; // EOF is reached.
    }

    pkByteBufferWrite(&buff, vm, (uint8_t) c);
    if (c == '\n') break;

  } while (true);

  // A null byte '\0' will be added by pocketlang.
  pkSetSlotStringLength(vm, 0, buff.data, buff.count);

L_done:
  pkByteBufferClear(&buff, vm);
  return;
}

DEF(_fileWrite,
  "io.File.write(data:String) -> Null",
  "Write the [data] to the file. Since pocketlang string support any valid"
  "byte value in it's string, binary data can also be written with strings.") {

  File* file = (File*) pkGetSelf(vm);
  const char* text; uint32_t length;
  if (!pkValidateSlotString(vm, 1, &text, &length)) return;

  if (file->closed) {
    pkSetRuntimeError(vm, "Cannot write to a closed file.");
    return;
  }

  if (!(file->mode & FMODE_WRITE)
    && !(file->mode & FMODE_APPEND)
    && !(file->mode & _FMODE_EXT)) {
    pkSetRuntimeError(vm, "File is not writable.");
    return;
  }

  clearerr(file->fp);
  fwrite(text, sizeof(char), (size_t) length, file->fp);
  if (ferror(file->fp)) {
    REPORT_ERRNO(fwrite);
    return;
  }
}

DEF(_fileClose,
  "io.File.close()",
  "Closes the opend file.") {

  File* file = (File*) pkGetSelf(vm);

  if (file->closed) {
    ASSERT(file->fp == NULL, OOPS);
    pkSetRuntimeError(vm, "File already closed.");
    return;
  }

  if (fclose(file->fp) != 0) {
    REPORT_ERRNO(fclose);
    return;
  }

  file->fp = NULL;
  file->closed = true;
}

DEF(_fileSeek,
  "io.File.seek(offset:Number, whence:Number) -> Null",
  "Move the file read/write offset. where [offset] is the offset from "
  "[whence] which should be any of the bellow three.\n"
  "  0: Begining of the file.\n"
  "  1: Current position.\n"
  "  2: End of the file.") {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 1, 2)) return;

  int32_t offset = 0, whence = 0;
  if (!pkValidateSlotInteger(vm, 1, &offset)) return;

  if (argc == 2) {
    if (!pkValidateSlotInteger(vm, 2, &whence)) return;
    if (whence < 0 || whence > 2) {
      pkSetRuntimeErrorFmt(vm, "Invalid whence value (%i).", whence);
      return;
    }
  }

  File* file = (File*) pkGetSelf(vm);

  if (file->closed) {
    pkSetRuntimeError(vm, "Cannot seek from a closed file.");
    return;
  }

  if (fseek(file->fp, offset, whence) != 0) {
    REPORT_ERRNO(fseek);
    return;
  }
}

DEF(_fileTell,
  "io.File.tell() -> Number",
  "Returns the read/write position of the file.") {
  File* file = (File*) pkGetSelf(vm);

  if (file->closed) {
    pkSetRuntimeError(vm, "Cannot tell from a closed file.");
    return;
  }

  // C.ftell() doesn't "throw" any error right?
  pkSetSlotNumber(vm, 0, (double) ftell(file->fp));
}

// TODO: The docstring is copyied from io.File.open() this violates DRY.
DEF(_open,
  "open(path:String, mode:String) -> Null",
  "Opens a file at the [path] with the [mode]. Path should be either "
  "absolute or relative to the current working directory. and [mode] can be"
  "'r', 'w', 'a' in combination with 'b' (binary) and/or '+' (extended).\n"
  "```\n"
  " mode | If already exists | If does not exist |\n"
  " -----+-------------------+-------------------|\n"
  " 'r'  |  read from start  |   failure to open |\n"
  " 'w'  |  destroy contents |   create new      |\n"
  " 'a'  |  write to end     |   create new      |\n"
  " 'r+' |  read from start  |   error           |\n"
  " 'w+' |  destroy contents |   create new      |\n"
  " 'a+' |  write to end     |   create new      |\n"
  "```") {
  pkReserveSlots(vm, 3);

  // slots[1] = path
  // slots[2] = mode
  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 1, 2)) return;
  if (argc == 1) pkSetSlotString(vm, 2, "r");

  if (!pkImportModule(vm, "io", 0)) return;           // slots[0] = io
  if (!pkGetAttribute(vm, 0, "File", 0)) return;      // slots[0] = File
  if (!pkNewInstance(vm, 0, 0, 0, 0)) return;         // slots[0] = File()
  if (!pkCallMethod(vm, 0, "open", 2, 1, -1)) return; // slots[0] = opened file
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleIO(PKVM* vm) {

  PkHandle* io = pkNewModule(vm, "io");

  pkRegisterBuiltinFn(vm, "open", _open, -1, DOCSTRING(_open));

  pkReserveSlots(vm, 2);
  pkSetSlotHandle(vm, 0, io);         // slot[0]        = io
  pkSetSlotNumber(vm, 1, 0);          // slot[1]        = 0
  pkSetAttribute(vm,  0, "stdin", 1); // slot[0].stdin  = slot[1]
  pkSetSlotNumber(vm, 1, 1);          // slot[1]        = 1
  pkSetAttribute(vm, 0, "stdout", 1); // slot[0].stdout = slot[1]
  pkSetSlotNumber(vm, 1, 2);          // slot[1]        = 2
  pkSetAttribute(vm, 0, "stderr", 1); // slot[0].stderr = slot[1]

  REGISTER_FN(io, "write", _ioWrite, 2);
  REGISTER_FN(io, "flush", _ioFlush, 0);
  REGISTER_FN(io, "getc",  _ioGetc, 0);

  PkHandle* cls_file = pkNewClass(vm, "File", NULL, io,
                                  _fileNew, _fileDelete,
                                  "A simple file type.");

  ADD_METHOD(cls_file, "open",     _fileOpen,    -1);
  ADD_METHOD(cls_file, "read",     _fileRead,    -1);
  ADD_METHOD(cls_file, "write",    _fileWrite,    1);
  ADD_METHOD(cls_file, "getline",  _fileGetLine,  0);
  ADD_METHOD(cls_file, "close",    _fileClose,    0);
  ADD_METHOD(cls_file, "seek",     _fileSeek,    -1);
  ADD_METHOD(cls_file, "tell",     _fileTell,     0);
  pkReleaseHandle(vm, cls_file);

  // A convinent function to read file by io.readfile(path).
  pkModuleAddSource(vm, io,
    "def readfile(filepath)\n"
    "  \"Reads a file and return it's content as string\""
    "  fp = File()\n"
    "  fp.open(filepath, 'r')\n"
    "  text = fp.read()\n"
    "  fp.close()\n"
    "  return text\n"
    "end\n");

  pkRegisterModule(vm, io);
  pkReleaseHandle(vm, io);
}
