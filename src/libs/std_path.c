/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_AMALGAMATED
#include "libs.h"
#endif

// FIXME:
// In windows MinGw support most of the posix libraries. So we only need to
// check if it's MSVC (and tcc in windows) or not for posix headers and
// Refactor the bellow macro includes.

#include "thirdparty/cwalk/cwalk.h" //<< AMALG_INLINE >>
#include "thirdparty/cwalk/cwalk.c" //<< AMALG_INLINE >>

#include <sys/stat.h>

#ifdef _WIN32
  #include <windows.h>
#endif

#if defined(_MSC_VER) || (defined(_WIN32) && defined(__TINYC__))
  #include <direct.h>
  #include <io.h>

  #include "thirdparty/dirent/dirent.h"  //<< AMALG_INLINE >>

  // access() function flag defines for windows.
  // Reference :https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/access-waccess?view=msvc-170#remarks
  #define F_OK 0
  #define W_OK 2
  #define R_OK 4

  #define access _access
  #define getcwd _getcwd

#else
  #include <dirent.h>
  #include <unistd.h>
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

static inline size_t pathAbs(const char* path, char* buff, size_t buffsz);
static inline bool pathIsFile(const char* path);
static inline bool pathIsDir(const char* path);

/*****************************************************************************/
/* PATH SHARED FUNCTIONS                                                     */
/*****************************************************************************/

// check if path + ext exists. If not return 0, otherwise path + ext will
// written to the buffer and return the total length.
//
// [path] and [buff] should be char array with size of FILENAME_MAX. This
// function will write path + ext to the buff. If the path exists it'll return
// true.
static inline size_t checkImportExists(char* path, const char* ext,
                                       char* buff) {
  size_t path_size = strlen(path);
  size_t ext_size = strlen(ext);

  // If the path size is too large we're bailing out.
  if (path_size + ext_size + 1 >= FILENAME_MAX) return 0;

  // Now we're safe to use strcpy.
  strcpy(buff, path);
  strcpy(buff + path_size, ext);

  if (!pathIsFile(buff)) return 0;
  return path_size + ext_size;
}

// Try all import paths by appending pocketlang supported extensions to the
// path (ex: path + ".pk", path + ".dll", path + ".so", path + "/_init.pk", ...
// if such a path exists it'll allocate a pocketlang string and return it.
//
// Note that [path] and [buff] should be char array with size of FILENAME_MAX.
// The buffer will be used as a working memory.
static char* tryImportPaths(PKVM* vm, char* path, char* buff) {
  size_t path_size = 0;

  static const char* EXT[] = {
    // Path can already end with '.pk' or anything when running from
    // pkRunFile() so it's mandatory for the bellow empty string.
    "",

    ".pk",

#ifdef _WIN32
    "\\_init.pk",
#else
    "/_init.pk",
#endif

#ifndef PK_NO_DL
  #if defined(_WIN32)
    ".dll",
    "\\_init.dll",

  #elif defined(__APPLE__)
    ".dylib",
    "/_init.dylib",

  #elif defined(__linux__)
    ".so",
    "/_init.so",
  #endif
#endif
    NULL, // Sentinal to mark the array end.
  };

  for (const char** ext = EXT; *ext != NULL; ext++) {
    if ((path_size = checkImportExists(path, *ext, buff)) != 0) {
      break;
    }
  }

  char* ret = NULL;
  if (path_size != 0) {
    ret = pkRealloc(vm, NULL, path_size + 1);
    memcpy(ret, buff, path_size + 1);
  }
  return ret;
}

// Implementation of pocketlang import path resolving function.
char* pathResolveImport(PKVM* vm, const char* from, const char* path) {

  // Buffers to store intermediate path results.
  char buff1[FILENAME_MAX];
  char buff2[FILENAME_MAX];

  // If the path is absolute, Just normalize and return it. Resolve path will
  // only be absolute when the path is provided from the command line.
  if (cwk_path_is_absolute(path)) {

    // buff1 = normalized path. +1 for null terminator.
    cwk_path_normalize(path, buff1, sizeof(buff1));

    return tryImportPaths(vm, buff1, buff2);
  }

  if (from == NULL) { //< [path] is relative to cwd.

    // buff1 = absolute path of [path].
    pathAbs(path, buff1, sizeof(buff1));

    // buff2 = normalized path. +1 for null terminator.
    cwk_path_normalize(buff1, buff2, sizeof(buff2));

    return tryImportPaths(vm, buff2, buff1);
  }

  // Regardless of the platform both '/' and '\\' will be used by pocketlang
  // to indicate its the path of a directory.
  char last = from[strlen(from) - 1];

  // buff1 = absolute path of [from].
  pathAbs(from, buff1, sizeof(buff1));

  // If the [from] path isn't a directory we use the dirname of the from
  // script.
  if (last != '/' && last != '\\') {
    size_t from_dir_length = 0;
    cwk_path_get_dirname(buff1, &from_dir_length);
    if (from_dir_length == 0) return NULL;
    buff1[from_dir_length] = '\0';
  }

  // buff2 = absolute joined path.
  cwk_path_join(buff1, path, buff2, sizeof(buff2));

  // buff1 = normalized absolute path. +1 for null terminator
  cwk_path_normalize(buff2, buff1, sizeof(buff1));

  return tryImportPaths(vm, buff1, buff2);
}

/*****************************************************************************/
/* PATH INTERNAL FUNCTIONS                                                   */
/*****************************************************************************/

static inline bool pathIsFile(const char* path) {
  struct stat path_stat;
  if (stat(path, &path_stat)) return false; // Error: might be path not exists.
  return (path_stat.st_mode & S_IFMT) == S_IFREG;
}

static inline bool pathIsDir(const char* path) {
  struct stat path_stat;
  if (stat(path, &path_stat)) return false; // Error: might be path not exists.
  return (path_stat.st_mode & S_IFMT) == S_IFDIR;
}

static inline time_t pathMtime(const char* path) {
  struct stat path_stat;
  if (stat(path, &path_stat)) return 0; // Error: might be path not exists.
  return path_stat.st_mtime;
}

static inline bool pathIsExists(const char* path) {
  return access(path, F_OK) == 0;
}

static inline size_t pathAbs(const char* path, char* buff, size_t buffsz) {

  char cwd[MAX_PATH_LEN];

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: handle error.
  }

  return cwk_path_get_absolute(cwd, path, buff, buffsz);
}

/*****************************************************************************/
/* PATH MODULE FUNCTIONS                                                     */
/*****************************************************************************/

DEF(_pathGetCWD,
  "path.getcwd() -> String",
  "Returns the current working directory.") {
  char cwd[MAX_PATH_LEN];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: Handle error.
  }
  pkSetSlotString(vm, 0, cwd);
}

DEF(_pathAbspath,
  "path.abspath(path:String) -> String",
  "Returns the absolute path of the [path].") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  char abspath[MAX_PATH_LEN];
  uint32_t len = (uint32_t) pathAbs(path, abspath, sizeof(abspath));
  pkSetSlotStringLength(vm, 0, abspath, len);
}

DEF(_pathRelpath,
  "path.relpath(path:String, from:String) -> String",
  "Returns the relative path of the [path] argument from the [from] "
  "directory.") {
  const char* path, * from;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;
  if (!pkValidateSlotString(vm, 2, &from, NULL)) return;

  char abs_path[MAX_PATH_LEN];
  pathAbs(path, abs_path, sizeof(abs_path));

  char abs_from[MAX_PATH_LEN];
  pathAbs(from, abs_from, sizeof(abs_from));

  char result[MAX_PATH_LEN];
  uint32_t len = (uint32_t) cwk_path_get_relative(abs_from, abs_path,
                                                  result, sizeof(result));
  pkSetSlotStringLength(vm, 0, result, len);
}

DEF(_pathJoin,
  "path.join(...) -> String",
  "Joins path with path seperator and return it. The maximum count of paths "
  "which can be joined for a call is " TOSTRING(MAX_JOIN_PATHS) ".") {
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

DEF(_pathNormpath,
  "path.normpath(path:String) -> String",
  "Returns the normalized path of the [path].") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  char result[MAX_PATH_LEN];
  uint32_t len = (uint32_t) cwk_path_normalize(path, result, sizeof(result));
  pkSetSlotStringLength(vm, 0, result, len);
}

DEF(_pathBaseName,
  "path.basename(path:String) -> String",
  "Returns the final component for the path") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  const char* base_name;
  size_t length;
  cwk_path_get_basename(path, &base_name, &length);
  pkSetSlotStringLength(vm, 0, base_name, (uint32_t)length);
}

DEF(_pathDirName,
  "path.dirname(path:String) -> String",
  "Returns the directory of the path.") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  size_t length;
  cwk_path_get_dirname(path, &length);
  pkSetSlotStringLength(vm, 0, path, (uint32_t)length);
}

DEF(_pathIsPathAbs,
  "path.isabspath(path:String) -> Bool",
  "Returns true if the path is absolute otherwise false.") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  pkSetSlotBool(vm, 0, cwk_path_is_absolute(path));
}

DEF(_pathGetExtension,
  "path.getext(path:String) -> String",
  "Returns the file extension of the path.") {
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

DEF(_pathExists,
  "path.exists(path:String) -> String",
  "Returns true if the file exists.") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;
  pkSetSlotBool(vm, 0, pathIsExists(path));
}

DEF(_pathIsFile,
  "path.isfile(path:String) -> Bool",
  "Returns true if the path is a file.") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;
  pkSetSlotBool(vm, 0, pathIsFile(path));
}

DEF(_pathIsDir,
  "path.isdir(path:String) -> Bool",
  "Returns true if the path is a directory.") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;
  pkSetSlotBool(vm, 0, pathIsDir(path));
}

DEF(_pathListDir,
  "path.listdir(path:String='.') -> List",
  "Returns all the entries in the directory at the [path].") {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 0, 1)) return;

  const char* path = ".";
  if (argc == 1) if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  if (!pathIsExists(path)) {
    pkSetRuntimeErrorFmt(vm, "Path '%s' does not exists.", path);
    return;
  }

  // We create a new list at slot[0] and use slot[1] as our working memory
  // overriding our parameter.
  pkNewList(vm, 0);

  DIR* dirstream = opendir(path);
  if (dirstream) {
    struct dirent* dir;
    while ((dir = readdir(dirstream)) != NULL) {
      if (!strcmp(dir->d_name, ".")) continue;
      if (!strcmp(dir->d_name, "..")) continue;

      pkSetSlotString(vm, 1, dir->d_name);
      if (!pkListInsert(vm, 0, -1, 1)) return;
    }
    closedir(dirstream);
  }
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

// Add the executables path and exe_path + 'libs/' as a search path for
// the PKVM.
void _registerSearchPaths(PKVM* vm) {
  enum cwk_path_style ps = cwk_path_get_style();

  char cwd[MAX_PATH_LEN];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    size_t len = strlen(cwd);
    if (len < MAX_PATH_LEN - 1) {
      cwd[len] = (ps == CWK_STYLE_WINDOWS) ? '\\' : '/';
      cwd[++len] = '\0';
      pkAddSearchPath(vm, cwd);
    }
  }

  char buff[MAX_PATH_LEN];
  if (!osGetExeFilePath(buff, MAX_PATH_LEN)) return;
  size_t length;
  cwk_path_get_dirname(buff, &length);
  if (length == 0) return;

  // Add path separator. Otherwise pkAddSearchPath will fail an assertion.
  char last = buff[length - 1];
  if (last != '/' && last != '\\') {
    last = (ps == CWK_STYLE_WINDOWS) ? '\\' : '/';
    buff[length++] = last;
  }

  // FIXME: the bellow code is hard coded.
  ASSERT(length + strlen("libs/") < MAX_PATH_LEN, OOPS);
  strcpy(buff + length, (ps == CWK_STYLE_WINDOWS) ? "libs\\" : "libs/");

  pkAddSearchPath(vm, buff);
}

void registerModulePath(PKVM* vm) {

  _registerSearchPaths(vm);

  PkHandle* path = pkNewModule(vm, "path");

  REGISTER_FN(path, "getcwd",    _pathGetCWD,       0);
  REGISTER_FN(path, "abspath",   _pathAbspath,      1);
  REGISTER_FN(path, "relpath",   _pathRelpath,      2);
  REGISTER_FN(path, "join",      _pathJoin,        -1);
  REGISTER_FN(path, "normpath",  _pathNormpath,     1);
  REGISTER_FN(path, "basename",  _pathBaseName,     1);
  REGISTER_FN(path, "dirname",   _pathDirName,      1);
  REGISTER_FN(path, "isabspath", _pathIsPathAbs,    1);
  REGISTER_FN(path, "getext",    _pathGetExtension, 1);
  REGISTER_FN(path, "exists",    _pathExists,       1);
  REGISTER_FN(path, "isfile",    _pathIsFile,       1);
  REGISTER_FN(path, "isdir",     _pathIsDir,        1);
  REGISTER_FN(path, "listdir",   _pathListDir,     -1);

  pkRegisterModule(vm, path);
  pkReleaseHandle(vm, path);
}

#undef MAX_PATH_LEN
#undef MAX_JOIN_PATHS
