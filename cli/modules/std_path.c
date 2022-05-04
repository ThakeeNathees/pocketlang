/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "modules.h"

// FIXME:
// In windows MinGw support most of the posix libraries. So we only need to
// check if it's MSVC (and tcc in windows) or not for posix headers and
// Refactor the bellow macro includes.

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

// The maximum path size that pocketlang's default import system supports
// including the null terminator. To be able to support more characters
// override the functions from the host application. Since this is very much
// platform specific we're defining a more general limit.
// See: https://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html
#define MAX_PATH_LEN 4096

// The cstring pointer buffer size used in path.join(p1, p2, ...). Tune this
// value as needed.
#define MAX_JOIN_PATHS 8

static inline size_t pathAbs(const char* path, char* buff, size_t buff_size);
static inline bool pathIsFileExists(const char* path);

/*****************************************************************************/
/* PATH SHARED FUNCTIONS                                                     */
/*****************************************************************************/

// Implementation of pocketlang import path resolving function.
char* pathResolveImport(PKVM* vm, const char* from, const char* path) {

  // Buffers to store intermediate path results.
  char buff1[FILENAME_MAX];
  char buff2[FILENAME_MAX];

  // If the path is absolute, Just normalize and return it.
  if (cwk_path_is_absolute(path)) {
    // buff1 = normalized path. +1 for null terminator.
    size_t size = cwk_path_normalize(path, buff1, sizeof(buff1)) + 1;

    char* ret = pkAllocString(vm, size);
    memcpy(ret, buff1, size);
    return ret;
  }

  if (from == NULL) { //< [path] is relative to cwd.

    // buff1 = absolute path of [path].
    pathAbs(path, buff1, sizeof(buff1));

    // buff2 = normalized path. +1 for null terminator.
    size_t size = cwk_path_normalize(buff1, buff2, sizeof(buff2)) + 1;

    char* ret = pkAllocString(vm, size);
    memcpy(ret, buff2, size);
    return ret;
  }

  // Import statements doesn't support absolute paths.
  ASSERT(cwk_path_is_absolute(from), "From path should be absolute.");

  // From is a path of a script. Try relative to that script.
  {
    size_t from_dir_length = 0;
    cwk_path_get_dirname(from, &from_dir_length);
    if (from_dir_length == 0) return NULL;

    // buff1 = from directory.
    strncpy(buff1, from, sizeof(buff1));
    buff1[from_dir_length] = '\0';

    // buff2 = absolute joined path.
    cwk_path_join(buff1, path, buff2, sizeof(buff2));

    // buff1 = normalized absolute path. +1 for null terminator
    size_t size = cwk_path_normalize(buff2, buff1, sizeof(buff1)) + 1;

    if (pathIsFileExists(buff1)) {
      char* ret = pkAllocString(vm, size);
      memcpy(ret, buff1, size);
      return ret;
    }
  }

  // Cannot resolve the path. Return NULL to indicate failure.
  return NULL;
}

/*****************************************************************************/
/* PATH INTERNAL FUNCTIONS                                                   */
/*****************************************************************************/

// FIXME: refactor (ref: https://stackoverflow.com/a/230068/10846399)
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

  char cwd[MAX_PATH_LEN];

  if (get_cwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: handle error.
  }

  return cwk_path_get_absolute(cwd, path, buff, buff_size);
}

/*****************************************************************************/
/* PATH MODULE FUNCTIONS                                                     */
/*****************************************************************************/

DEF(_pathSetStyleUnix, "") {
  bool value;
  if (!pkValidateSlotBool(vm, 1, &value)) return;
  cwk_path_set_style((value) ? CWK_STYLE_UNIX : CWK_STYLE_WINDOWS);
}

DEF(_pathGetCWD, "") {
  char cwd[MAX_PATH_LEN];
  if (get_cwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: Handle error.
  }
  pkSetSlotString(vm, 0, cwd);
}

DEF(_pathAbspath, "") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  char abspath[MAX_PATH_LEN];
  uint32_t len = (uint32_t) pathAbs(path, abspath, sizeof(abspath));
  pkSetSlotStringLength(vm, 0, abspath, len);
}

DEF(_pathRelpath, "") {
  const char* from, * path;
  if (!pkValidateSlotString(vm, 1, &from, NULL)) return;
  if (!pkValidateSlotString(vm, 2, &path, NULL)) return;

  char abs_from[MAX_PATH_LEN];
  pathAbs(from, abs_from, sizeof(abs_from));

  char abs_path[MAX_PATH_LEN];
  pathAbs(path, abs_path, sizeof(abs_path));

  char result[MAX_PATH_LEN];
  uint32_t len = (uint32_t) cwk_path_get_relative(abs_from, abs_path,
                                                  result, sizeof(result));
  pkSetSlotStringLength(vm, 0, result, len);
}

DEF(_pathJoin, "") {
  const char* paths[MAX_JOIN_PATHS + 1]; // +1 for NULL.
  int argc = pkGetArgc(vm);

  if (argc > MAX_JOIN_PATHS) {
    pkSetRuntimeError(vm, "Cannot join more than " STRINGIFY(MAX_JOIN_PATHS)
                           "paths.");
    return;
  }

  for (int i = 0; i < argc; i++) {
    pkValidateSlotString(vm, i + 1, &paths[i], NULL);
  }
  paths[argc] = NULL;

  char result[MAX_PATH_LEN];
  uint32_t len = (uint32_t) cwk_path_join_multiple(paths, result,
                                                   sizeof(result));
  pkSetSlotStringLength(vm, 0, result, len);
}

DEF(_pathNormalize, "") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  char result[MAX_PATH_LEN];
  uint32_t len = (uint32_t) cwk_path_normalize(path, result, sizeof(result));
  pkSetSlotStringLength(vm, 0, result, len);
}

DEF(_pathBaseName, "") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  const char* base_name;
  size_t length;
  cwk_path_get_basename(path, &base_name, &length);
  pkSetSlotStringLength(vm, 0, base_name, (uint32_t)length);
}

DEF(_pathDirName, "") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  size_t length;
  cwk_path_get_dirname(path, &length);
  pkSetSlotStringLength(vm, 0, path, (uint32_t)length);
}

DEF(_pathIsPathAbs, "") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  pkSetSlotBool(vm, 0, cwk_path_is_absolute(path));
}

DEF(_pathGetExtension, "") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  const char* ext;
  size_t length;
  if (cwk_path_get_extension(path, &ext, &length)) {
    pkSetSlotStringLength(vm, 0, ext, (uint32_t)length);
  } else {
    pkSetSlotStringLength(vm, 0, NULL, 0);
  }
}

DEF(_pathExists, "") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;
  pkSetSlotBool(vm, 0, pathIsExists(path));
}

DEF(_pathIsFile, "") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;
  pkSetSlotBool(vm, 0, pathIsFileExists(path));
}

DEF(_pathIsDir, "") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;
  pkSetSlotBool(vm, 0, pathIsDirectoryExists(path));
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

  pkRegisterModule(vm, path);
  pkReleaseHandle(vm, path);
}
