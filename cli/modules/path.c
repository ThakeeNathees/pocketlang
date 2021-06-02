/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */


#include <errno.h>
#include <pocketlang.h>
#include <stdio.h> /* defines FILENAME_MAX */

#include "../thirdparty/cwalk/cwalk.h"
#if defined(_WIN32) && (defined(_MSC_VER) || defined(__TINYC__))
  #include "../thirdparty/dirent/dirent.h"
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

// TODO: No error is handled below. I should check for path with size more than
// FILENAME_MAX.
 

// TODO: this macros should be moved to a general place of in cli.
#define TOSTRING(x) #x
#define STRINGIFY(x) TOSTRING(x)

// The cstring pointer buffer size used in path.join(p1, p2, ...). Tune this
// value as needed.
#define MAX_JOIN_PATHS 8

/*****************************************************************************/
/* PUBLIC FUNCTIONS                                                          */
/*****************************************************************************/

void pathInit() {
  cwk_path_set_style(CWK_STYLE_UNIX);
}

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
/* INTERNAL FUNCTIONS                                                        */
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
  } else if (ENOENT == errno) { /* Directory does not exist. */
  } else { /* opendir() failed for some other reason. */
  }

  return false;
}

static inline bool pathIsExists(const char* path) {
  return pathIsFileExists(path) || pathIsDirectoryExists(path);
}

/*****************************************************************************/
/* MODULE FUNCTIONS                                                          */
/*****************************************************************************/

static void _pathGetCWD(PKVM* vm) {
  char cwd[FILENAME_MAX];
  char* res = get_cwd(cwd, sizeof(cwd)); // Check if res is NULL.
  pkReturnString(vm, cwd);
}

static void _pathAbspath(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path)) return;

  char cwd[FILENAME_MAX];
  char* res = get_cwd(cwd, sizeof(cwd)); // Check if res is NULL.

  char abspath[FILENAME_MAX];
  size_t len = cwk_path_get_absolute(cwd, path, abspath, sizeof(abspath));
  pkReturnStringLength(vm, abspath, len);
}

static void _pathRelpath(PKVM* vm) {
  const char *from, *path;
  if (!pkGetArgString(vm, 1, &from)) return;
  if (!pkGetArgString(vm, 2, &path)) return;

  char result[FILENAME_MAX];
  size_t len = cwk_path_get_relative(from, path, result, sizeof(result));
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
    pkGetArgString(vm, i+1, &paths[i]);
  }
  paths[argc] = NULL;

  char result[FILENAME_MAX];
  size_t len = cwk_path_join_multiple(paths, result, sizeof(result));
  pkReturnStringLength(vm, result, len);
}

static void _pathNormalize(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path)) return;

  char result[FILENAME_MAX];
  size_t len = cwk_path_normalize(path, result, sizeof(result));
  pkReturnStringLength(vm, result, len);
}

static void _pathBaseName(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path)) return;

  const char* base_name;
  size_t length;
  cwk_path_get_basename(path, &base_name, &length);
  pkReturnString(vm, base_name);
}

static void _pathDirName(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path)) return;

  size_t length;
  cwk_path_get_dirname(path, &length);
  pkReturnStringLength(vm, path, length);
}

static void _pathIsPathAbs(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path)) return;

  pkReturnBool(vm, cwk_path_is_absolute(path));
}

static void _pathGetExtension(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path)) return;

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
  if (!pkGetArgString(vm, 1, &path)) return;
  pkReturnBool(vm, pathIsExists(path));
}

static void _pathIsFile(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path)) return;
  pkReturnBool(vm, pathIsFileExists(path));
}

static void _pathIsDir(PKVM* vm) {
  const char* path;
  if (!pkGetArgString(vm, 1, &path)) return;
  pkReturnBool(vm, pathIsDirectoryExists(path));
}

/*****************************************************************************/
/* REGISTER MODULE                                                           */
/*****************************************************************************/

void registerModulePath(PKVM* vm) {
  PkHandle* path = pkNewModule(vm, "path");

  pkModuleAddFunction(vm, path, "getcwd",    _pathGetCWD,    0);
  pkModuleAddFunction(vm, path, "abspath",   _pathAbspath,   1);
  pkModuleAddFunction(vm, path, "relpath",   _pathRelpath,   2);
  pkModuleAddFunction(vm, path, "join",      _pathJoin,     -1);
  pkModuleAddFunction(vm, path, "normalize", _pathNormalize, 1);
  pkModuleAddFunction(vm, path, "basename",  _pathBaseName,  1);
  pkModuleAddFunction(vm, path, "dirname",   _pathDirName,   1);
  pkModuleAddFunction(vm, path, "isabspath", _pathIsPathAbs, 1);
  pkModuleAddFunction(vm, path, "getext",    _pathGetExtension, 1);
  pkModuleAddFunction(vm, path, "exists",    _pathExists,    1);
  pkModuleAddFunction(vm, path, "isfile",    _pathIsFile,    1);
  pkModuleAddFunction(vm, path, "isdir",     _pathIsDir,     1);

  pkReleaseHandle(vm, path);
}
