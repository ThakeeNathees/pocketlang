/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "modules.h"

/*****************************************************************************/
/* FILE OBJECT OPERATORS                                                     */
/*****************************************************************************/

void fileGetAttrib(PKVM* vm, File* file, const char* attrib) {
  if (strcmp(attrib, "closed") == 0) {
    pkReturnBool(vm, file->closed);
    return;
  }
}

bool fileSetAttrib(PKVM* vm, File* file, const char* attrib) {
  return false;
}

void fileClean(PKVM* vm, File* file) {
  if (!file->closed) {
    if (fclose(file->fp) != 0) { /* TODO: error! */ }
    file->closed = true;
  }
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

  FILE* fp = fopen(path, mode_str);

  if (fp != NULL) {
    File* file = NEW_OBJ(File);
    initObj(&file->_super, OBJ_FILE);
    file->fp = fp;
    file->mode = mode;
    file->closed = false;

    pkReturnInstNative(vm, (void*)file, OBJ_FILE);

  } else {
    pkReturnNull(vm);
  }
}

static void _fileRead(PKVM* vm) {
  File* file;
  if (!pkGetArgInst(vm, 1, OBJ_FILE, (void**)&file)) return;

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
  File* file;
  const char* text; uint32_t length;
  if (!pkGetArgInst(vm, 1, OBJ_FILE, (void**)&file)) return;
  if (!pkGetArgString(vm, 2, &text, &length)) return;

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
  File* file;
  if (!pkGetArgInst(vm, 1, OBJ_FILE, (void**)&file)) return;

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

void registerModuleFile(PKVM* vm) {
  PkHandle* file = pkNewModule(vm, "File");

  pkModuleAddFunction(vm, file, "open",  _fileOpen, -1);
  pkModuleAddFunction(vm, file, "read",  _fileRead,  1);
  pkModuleAddFunction(vm, file, "write", _fileWrite, 2);
  pkModuleAddFunction(vm, file, "close", _fileClose, 1);

  pkReleaseHandle(vm, file);
}
