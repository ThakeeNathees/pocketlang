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
  "io.write(stream:var, bytes:String) -> null\n"
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

    // TODO: If the string contain null bytes it won't print anything after
    // that, not sure if that needs to be fixed.
    case 1:
      fprintf(stdout, "%s", bytes);
      return;

    case 2:
      fprintf(stderr, "%s", bytes);
      return;
  }
}

DEF(_ioFlush,
  "io.flush() -> null\n"
  "Warning: the function is subjected to be changed anytime soon.\n"
  "Flush stdout buffer.\n") {
  fflush(stdout);
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
  FMODE_WRITE_BIN  = (_FMODE_BIN | FMODE_READ),
  FMODE_APPEND_BIN = (_FMODE_BIN | FMODE_READ),

  FMODE_READ_BIN_EXT   = (_FMODE_BIN | FMODE_READ_EXT),
  FMODE_WRITE_BIN_EXT  = (_FMODE_BIN | FMODE_WRITE_EXT),
  FMODE_APPEND_BIN_EXT = (_FMODE_BIN | FMODE_APPEND_EXT),

} FileAccessMode;

typedef struct {
  FILE* fp;            // C file poinnter.
  FileAccessMode mode; // Access mode of the file.
  bool closed;         // True if the file isn't closed yet.
} File;

void* _newFile(PKVM* vm) {
  File* file = pkRealloc(vm, NULL, sizeof(File));
  ASSERT(file != NULL, "pkRealloc failed.");
  file->closed = true;
  file->mode = FMODE_NONE;
  file->fp = NULL;
  return file;
}

void _deleteFile(PKVM* vm, void* ptr) {
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

DEF(_fileOpen, "") {

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

DEF(_fileRead, "") {

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
    pkSetRuntimeErrorFmt(vm, "An error occured at C.fread() - '%s'.",
                              strerror(errno));
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
DEF(_fileGetLine, "") {
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
        pkSetRuntimeErrorFmt(vm, "Error while reading line - '%s'.",
                             strerror(errno));
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

DEF(_fileWrite, "") {

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
    pkSetRuntimeErrorFmt(vm, "An error occureed at C.fwrite() - '%s'.",
                         strerror(errno));
    return;
  }
}

DEF(_fileClose, "") {

  File* file = (File*) pkGetSelf(vm);

  if (file->closed) {
    ASSERT(file->fp == NULL, OOPS);
    pkSetRuntimeError(vm, "File already closed.");
    return;
  }

  if (fclose(file->fp) != 0) {
    pkSetRuntimeError(vm, "fclose() failed!.");
  }

  file->fp = NULL;
  file->closed = true;
}

DEF(_fileSeek,
  "io.File.seek(offset:int, whence:int) -> null\n"
  "") {

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
    pkSetRuntimeErrorFmt(vm, "An error occureed at C.fseek() - '%s'.",
                         strerror(errno));
    return;
  }
}

DEF(_fileTell, "") {
  File* file = (File*) pkGetSelf(vm);

  if (file->closed) {
    pkSetRuntimeError(vm, "Cannot tell from a closed file.");
    return;
  }

  // C.ftell() doesn't "throw" any error right?
  pkSetSlotNumber(vm, 0, (double) ftell(file->fp));
}

void registerModuleIO(PKVM* vm) {

  PkHandle* io = pkNewModule(vm, "io");

  pkReserveSlots(vm, 2);
  pkSetSlotHandle(vm, 0, io);         // slot[0]        = io
  pkSetSlotNumber(vm, 1, 0);          // slot[1]        = 0
  pkSetAttribute(vm,  0, "stdin", 1); // slot[0].stdin  = slot[1]
  pkSetSlotNumber(vm, 1, 1);          // slot[1]        = 1
  pkSetAttribute(vm, 0, "stdout", 1); // slot[0].stdout = slot[1]
  pkSetSlotNumber(vm, 1, 2);          // slot[1]        = 2
  pkSetAttribute(vm, 0, "stderr", 1); // slot[0].stderr = slot[1]

  pkModuleAddFunction(vm, io, "write", _ioWrite, 2);
  pkModuleAddFunction(vm, io, "flush", _ioFlush, 0);

  PkHandle* cls_file = pkNewClass(vm, "File", NULL, io, _newFile, _deleteFile);
  pkClassAddMethod(vm, cls_file, "open",     _fileOpen,    -1);
  pkClassAddMethod(vm, cls_file, "read",     _fileRead,    -1);
  pkClassAddMethod(vm, cls_file, "write",    _fileWrite,    1);
  pkClassAddMethod(vm, cls_file, "getline",  _fileGetLine,  0);
  pkClassAddMethod(vm, cls_file, "close",    _fileClose,    0);
  pkClassAddMethod(vm, cls_file, "seek",     _fileSeek,    -1);
  pkClassAddMethod(vm, cls_file, "tell",     _fileTell,     0);
  pkReleaseHandle(vm, cls_file);

  pkRegisterModule(vm, io);
  pkReleaseHandle(vm, io);
}
