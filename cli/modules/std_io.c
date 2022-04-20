/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "modules.h"

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
  FMODE_READ_EXT   = (_FMODE_EXT | FMODE_READ),
  FMODE_WRITE_EXT  = (_FMODE_EXT | FMODE_WRITE),
  FMODE_APPEND_EXT = (_FMODE_EXT | FMODE_APPEND),
} FileAccessMode;

typedef struct {
  FILE* fp;            // C file poinnter.
  FileAccessMode mode; // Access mode of the file.
  bool closed;         // True if the file isn't closed yet.
} File;

void* _newFile() {
  File* file = NEW_OBJ(File);
  file->closed = true;
  file->mode = FMODE_NONE;
  file->fp = NULL;
  return file;
}

void _deleteFile(void* ptr) {
  File* file = (File*)ptr;
  if (!file->closed) {
    if (fclose(file->fp) != 0) { /* TODO: error! */ }
    file->closed = true;
  }
  FREE_OBJ(file);
}

/*****************************************************************************/
/* FILE MODULE FUNCTIONS                                                     */
/*****************************************************************************/

static void _fileOpen(PKVM* vm) {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 1, 2)) return;

  const char* path;
  if (!pkGetArgString(vm, 1, &path, NULL)) return;

  const char* mode_str = "r";
  FileAccessMode mode = FMODE_READ;

  if (argc == 2) {
    if (!pkGetArgString(vm, 2, &mode_str, NULL)) return;

    // Check if the mode string is valid, and update the mode value.
    do {
      if (strcmp(mode_str, "r")  == 0) { mode = FMODE_READ;       break; }
      if (strcmp(mode_str, "w")  == 0) { mode = FMODE_WRITE;      break; }
      if (strcmp(mode_str, "a")  == 0) { mode = FMODE_APPEND;     break; }
      if (strcmp(mode_str, "r+") == 0) { mode = FMODE_READ_EXT;   break; }
      if (strcmp(mode_str, "w+") == 0) { mode = FMODE_WRITE_EXT;  break; }
      if (strcmp(mode_str, "a+") == 0) { mode = FMODE_APPEND_EXT; break; }

      // TODO: (fmt, ...) va_arg for runtime error public api.
      // If we reached here, that means it's an invalid mode string.
      pkSetRuntimeError(vm, "Invalid mode string.");
      return;
    } while (false);
  }

  // This TODO is just a blockade from running the bellow code, complete the
  // native interface and test before removing it.
  TODO;

  FILE* fp = fopen(path, mode_str);

  if (fp != NULL) {
    File* self = (File*)pkGetSelf(vm);
    self->fp = fp;
    self->mode = mode;
    self->closed = false;

  } else {
    pkSetRuntimeError(vm, "Error opening the file.");
  }
}

static void _fileRead(PKVM* vm) {
  // This TODO is just a blockade from running the bellow code, complete the
  // native interface and test before removing it.
  TODO;

  File* file = (File*)pkGetSelf(vm);

  if (file->closed) {
    pkSetRuntimeError(vm, "Cannot read from a closed file.");
    return;
  }

  if ((file->mode != FMODE_READ) && ((_FMODE_EXT & file->mode) == 0)) {
    pkSetRuntimeError(vm, "File is not readable.");
    return;
  }

  // TODO: this is temporary.
  char buff[2048];
  fread((void*)buff, sizeof(char), sizeof(buff), file->fp);
  pkReturnString(vm, (const char*)buff);
}

static void _fileWrite(PKVM* vm) {
  // This TODO is just a blockade from running the bellow code, complete the
  // native interface and test before removing it.
  TODO;

  File* file = (File*)pkGetSelf(vm);
  const char* text; uint32_t length;
  if (!pkGetArgString(vm, 1, &text, &length)) return;

  if (file->closed) {
    pkSetRuntimeError(vm, "Cannot write to a closed file.");
    return;
  }

  if ((file->mode != FMODE_WRITE) && ((_FMODE_EXT & file->mode) == 0)) {
    pkSetRuntimeError(vm, "File is not writable.");
    return;
  }

  fwrite(text, sizeof(char), (size_t)length, file->fp);
}

static void _fileClose(PKVM* vm) {
  // This TODO is just a blockade from running the bellow code, complete the
  // native interface and test before removing it.
  TODO;

  File* file = (File*)pkGetSelf(vm);

  if (file->closed) {
    pkSetRuntimeError(vm, "File already closed.");
    return;
  }

  if (fclose(file->fp) != 0) {
    pkSetRuntimeError(vm, "fclose() failed!\n"                     \
                      "  at " __FILE__ ":" STRINGIFY(__LINE__) ".");
  }
  file->closed = true;
}

void registerModuleIO(PKVM* vm) {
  PkHandle* io = pkNewModule(vm, "io");

  PkHandle* cls_file = pkNewClass(vm, "File", NULL, io, _newFile, _deleteFile);
  pkClassAddMethod(vm, cls_file, "open",  _fileOpen, -1);
  pkClassAddMethod(vm, cls_file, "read",  _fileRead,  0);
  pkClassAddMethod(vm, cls_file, "write", _fileWrite, 1);
  pkClassAddMethod(vm, cls_file, "close", _fileClose, 0);
  pkReleaseHandle(vm, cls_file);

  pkRegisterModule(vm, io);
  pkReleaseHandle(vm, io);
}
