/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include "modules.h"

// Note: Everything here is for testing the native API, and will have to
//       refactor everything.

// Allocate a new module object of type [Ty].
#define NEW_OBJ(Ty) (Ty*)malloc(sizeof(Ty))

// Dellocate module object, allocated by NEW_OBJ(). Called by the freeObj
// callback.
#define FREE_OBJ(ptr) free(ptr)

void initObj(Obj* obj, ObjType type) {
  obj->type = type;
}

void objGetAttrib(PKVM* vm, void* instance, uint32_t id, PkStringPtr attrib) {
  Obj* obj = (Obj*)instance;
  ASSERT(obj->type == (ObjType)id, OOPS);

  if (obj->type == OBJ_FILE) {
    File* file = (File*)obj;
    if (strcmp(attrib.string, "closed") == 0) {
      pkReturnBool(vm, file->closed);
      return;
    }
  }

  return; // Attribute not found.
}

bool objSetAttrib(PKVM* vm, void* instance, uint32_t id, PkStringPtr attrib) {
  Obj* obj = (Obj*)instance;
  ASSERT(obj->type == (ObjType)id, OOPS);

  if (obj->type == OBJ_FILE) {
    File* file = (File*)obj;
    // Nothing to change.
    (void)file;
  }

  return false;
}

void freeObj(PKVM* vm, void* instance, uint32_t id) {
  Obj* obj = (Obj*)instance;
  ASSERT(obj->type == (ObjType)id, OOPS);

  // If the file isn't closed, close it to flush it's buffer.
  if (obj->type == OBJ_FILE) {
    File* file = (File*)obj;
    if (!file->closed) {
      if (fclose(file->fp) != 0) { /* TODO: error! */ }
      file->closed = true;
    }
  }

  FREE_OBJ(obj);
}

const char* getObjName(uint32_t id) {
  switch ((ObjType)id) {
    case OBJ_FILE: return "File";
  }
  return NULL;
}

/*****************************************************************************/
/* PATH MODULE                                                               */
/*****************************************************************************/

#include "thirdparty/cwalk/cwalk.h"
  #if defined(_WIN32) && (defined(_MSC_VER) || defined(__TINYC__))
  #include "thirdparty/dirent/dirent.h"
#else
  #include <dirent.h>
#endif

#if defined(_WIN32)
  #include <windows.h>
  #include <direct.h>
  #define get_cwd _getcwd
#else
  #include <unistd.h>
  #define get_cwd getcwd
#endif

// The cstring pointer buffer size used in path.join(p1, p2, ...). Tune this
// value as needed.
#define MAX_JOIN_PATHS 8

/*****************************************************************************/
/* PUBLIC FUNCTIONS                                                          */
/*****************************************************************************/

bool pathIsAbsolute(const char* path) {
  return cwk_path_is_absolute(path);
}

void pathGetDirName(const char* path, size_t* length) {
  cwk_path_get_dirname(path, length);
}

size_t pathNormalize(const char* path, char* buff, size_t buff_size) {
  return cwk_path_normalize(path, buff, buff_size);
}

size_t pathJoin(const char* path_a, const char* path_b, char* buffer,
  size_t buff_size) {
  return cwk_path_join(path_a, path_b, buffer, buff_size);
}

/*****************************************************************************/
/* PATH INTERNAL FUNCTIONS                                                   */
/*****************************************************************************/

static inline bool pathIsFileExists(const char* path) {
  FILE* file = fopen(path, "r");
  if (file != NULL) {
    fclose(file);
    return true;
  }
  return false;
}

// Reference: https://stackoverflow.com/a/12510903/10846399
static inline bool pathIsDirectoryExists(const char* path) {
  DIR* dir = opendir(path);
  if (dir) { /* Directory exists. */
    closedir(dir);
    return true;
  } else if (errno == ENOENT) { /* Directory does not exist. */
  } else { /* opendir() failed for some other reason. */
  }

  return false;
}

static inline bool pathIsExists(const char* path) {
  return pathIsFileExists(path) || pathIsDirectoryExists(path);
}

static inline size_t pathAbs(const char* path, char* buff, size_t buff_size) {

  char cwd[FILENAME_MAX];

  if (get_cwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: handle error.
  }

  return cwk_path_get_absolute(cwd, path, buff, buff_size);
}

/*****************************************************************************/
/* PATH MODULES FUNCTIONS                                                    */
/*****************************************************************************/

static void _pathSetStyleUnix(PKVM* vm) {
  bool value;
  if (!pkGetArgBool(vm, 1, &value)) return;
  cwk_path_set_style((value) ? CWK_STYLE_UNIX : CWK_STYLE_WINDOWS);
}

static void _pathGetCWD(PKVM* vm) {
  char cwd[FILENAME_MAX];
  if (get_cwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: Handle error.
  }
  pkReturnString(vm, cwd);
}

static void _pathAbspath(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path, NULL)) return;

  char abspath[FILENAME_MAX];
  size_t len = pathAbs(path, abspath, sizeof(abspath));
  pkReturnStringLength(vm, abspath, len);
}

static void _pathRelpath(PKVM* vm) {
  const char* from, * path;
  if (!pkGetArgString(vm, 1, &from, NULL)) return;
  if (!pkGetArgString(vm, 2, &path, NULL)) return;

  char abs_from[FILENAME_MAX];
  pathAbs(from, abs_from, sizeof(abs_from));

  char abs_path[FILENAME_MAX];
  pathAbs(path, abs_path, sizeof(abs_path));

  char result[FILENAME_MAX];
  size_t len = cwk_path_get_relative(abs_from, abs_path,
    result, sizeof(result));
  pkReturnStringLength(vm, result, len);
}

static void _pathJoin(PKVM* vm) {
  const char* paths[MAX_JOIN_PATHS + 1]; // +1 for NULL.
  int argc = pkGetArgc(vm);

  if (argc > MAX_JOIN_PATHS) {
    pkSetRuntimeError(vm, "Cannot join more than " STRINGIFY(MAX_JOIN_PATHS)
                           "paths.");
    return;
  }

  for (int i = 0; i < argc; i++) {
    pkGetArgString(vm, i + 1, &paths[i], NULL);
  }
  paths[argc] = NULL;

  char result[FILENAME_MAX];
  size_t len = cwk_path_join_multiple(paths, result, sizeof(result));
  pkReturnStringLength(vm, result, len);
}

static void _pathNormalize(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path, NULL)) return;

  char result[FILENAME_MAX];
  size_t len = cwk_path_normalize(path, result, sizeof(result));
  pkReturnStringLength(vm, result, len);
}

static void _pathBaseName(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path, NULL)) return;

  const char* base_name;
  size_t length;
  cwk_path_get_basename(path, &base_name, &length);
  pkReturnString(vm, base_name);
}

static void _pathDirName(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path, NULL)) return;

  size_t length;
  cwk_path_get_dirname(path, &length);
  pkReturnStringLength(vm, path, length);
}

static void _pathIsPathAbs(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path, NULL)) return;

  pkReturnBool(vm, cwk_path_is_absolute(path));
}

static void _pathGetExtension(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path, NULL)) return;

  const char* ext;
  size_t length;
  if (cwk_path_get_extension(path, &ext, &length)) {
    pkReturnStringLength(vm, ext, length);
  } else {
    pkReturnStringLength(vm, NULL, 0);
  }
}

static void _pathExists(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path, NULL)) return;
  pkReturnBool(vm, pathIsExists(path));
}

static void _pathIsFile(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path, NULL)) return;
  pkReturnBool(vm, pathIsFileExists(path));
}

static void _pathIsDir(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path, NULL)) return;
  pkReturnBool(vm, pathIsDirectoryExists(path));
}

void registerModulePath(PKVM* vm) {
  PkHandle* path = pkNewModule(vm, "path");

  pkModuleAddFunction(vm, path, "setunix",   _pathSetStyleUnix, 1);
  pkModuleAddFunction(vm, path, "getcwd",    _pathGetCWD,       0);
  pkModuleAddFunction(vm, path, "abspath",   _pathAbspath,      1);
  pkModuleAddFunction(vm, path, "relpath",   _pathRelpath,      2);
  pkModuleAddFunction(vm, path, "join",      _pathJoin,        -1);
  pkModuleAddFunction(vm, path, "normalize", _pathNormalize,    1);
  pkModuleAddFunction(vm, path, "basename",  _pathBaseName,     1);
  pkModuleAddFunction(vm, path, "dirname",   _pathDirName,      1);
  pkModuleAddFunction(vm, path, "isabspath", _pathIsPathAbs,    1);
  pkModuleAddFunction(vm, path, "getext",    _pathGetExtension, 1);
  pkModuleAddFunction(vm, path, "exists",    _pathExists,       1);
  pkModuleAddFunction(vm, path, "isfile",    _pathIsFile,       1);
  pkModuleAddFunction(vm, path, "isdir",     _pathIsDir,        1);

  pkReleaseHandle(vm, path);
}

/*****************************************************************************/
/* FILE MODULE                                                               */
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

  // TODO: this is temproary.
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

/*****************************************************************************/
/* REGISTER MODULES                                                          */
/*****************************************************************************/

void registerModules(PKVM* vm) {
  registerModuleFile(vm);
  registerModulePath(vm);
}
