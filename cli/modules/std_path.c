/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "modules.h"

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
/* PATH SHARED FUNCTIONS                                                     */
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
/* PATH MODULE FUNCTIONS                                                     */
/*****************************************************************************/

DEF(_pathSetStyleUnix, "") {
  bool value;
  if (!pkValidateSlotBool(vm, 1, &value)) return;
  cwk_path_set_style((value) ? CWK_STYLE_UNIX : CWK_STYLE_WINDOWS);
}

DEF(_pathGetCWD, "") {
  char cwd[FILENAME_MAX];
  if (get_cwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: Handle error.
  }
  pkSetSlotString(vm, 0, cwd);
}

DEF(_pathAbspath, "") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  char abspath[FILENAME_MAX];
  uint32_t len = (uint32_t) pathAbs(path, abspath, sizeof(abspath));
  pkSetSlotStringLength(vm, 0, abspath, len);
}

DEF(_pathRelpath, "") {
  const char* from, * path;
  if (!pkValidateSlotString(vm, 1, &from, NULL)) return;
  if (!pkValidateSlotString(vm, 2, &path, NULL)) return;

  char abs_from[FILENAME_MAX];
  pathAbs(from, abs_from, sizeof(abs_from));

  char abs_path[FILENAME_MAX];
  pathAbs(path, abs_path, sizeof(abs_path));

  char result[FILENAME_MAX];
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

  char result[FILENAME_MAX];
  uint32_t len = (uint32_t) cwk_path_join_multiple(paths, result,
                                                   sizeof(result));
  pkSetSlotStringLength(vm, 0, result, len);
}

DEF(_pathNormalize, "") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  char result[FILENAME_MAX];
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
