/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <math.h>

#ifndef PK_AMALGAMATED
#include "libs.h"
#endif

#include <stdlib.h>
#include <sys/stat.h>

#if defined(__EMSCRIPTEN__)
  #define _PKOS_WEB_
  #define OS_NAME "web"

#elif defined(_WIN32) || defined(__NT__)
  #define _PKOS_WIN_
  #define OS_NAME "windows"

#elif defined(__APPLE__)
  #define _PKOS_APPLE_
  #define OS_NAME "apple" // TODO: should I use darwin?.

#elif defined(__linux__)
  #define _PKOS_LINUX_
  #define OS_NAME "linux"

#elif defined(__unix__)
  #define _PKOS_UNIX_
  #define OS_NAME "unix"

#elif defined(_POSIX_VERSION)
  #define _PKOS_POSIX_
  #define OS_NAME "posix"

#else
  #define _PKOS_UNKNOWN_
  #define OS_NAME "<?>"
#endif

#if defined(_MSC_VER) || (defined(_WIN32) && defined(__TINYC__))
  #include <windows.h>
  #include <direct.h>
  #include <io.h>

  #define getcwd  _getcwd
  #define chdir   _chdir
  #define mkdir   _mkdir
  #define rmdir   _rmdir
  #define unlink _unlink

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

bool osGetExeFilePath(char* buff, int size) {

#if defined(_PKOS_WIN_)
    int bytes = GetModuleFileNameA(NULL, buff, size);
    ASSERT(bytes > 0, "GetModuleFileName failed.");
    return true;

#elif defined(_PKOS_APPLE_)
  unsigned sz = size;
  _NSGetExecutablePath(buff, &sz);
  return true;

#elif defined(_PKOS_LINUX_)
    char tmp[MAX_PATH_LEN];
    sprintf(tmp, "/proc/%d/exe", getpid());
    int len = readlink(tmp, buff, size);
    buff[len] = '\0';
    return true;

#else
    return false;
#endif
}

// Yes both 'os' and 'path' have getcwd functions.
DEF(_osGetCWD,
  "os.getcwd() -> String\n"
  "Returns the current working directory") {
  char cwd[MAX_PATH_LEN];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: Handle error.
  }
  pkSetSlotString(vm, 0, cwd);
}

DEF(_osChdir,
  "os.chdir(path:String)\n"
  "Change the current working directory") {

  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  if (chdir(path)) REPORT_ERRNO(chdir);
}

DEF(_osMkdir,
  "os.mkdir(path:String)\n"
  "Creates a directory at the path. The path should be valid.") {

  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

#if defined(_PKOS_WIN_)
  if (mkdir(path)) {
#else
  if (mkdir(path, 0x1ff)) { // TODO: mode flags.
#endif

    // If the directory exists (errno == EEXIST) should I skip it silently ?
    REPORT_ERRNO(mkdir);
  }
}

DEF(_osRmdir,
  "os.rmdir(path:String)\n"
  "Removes an empty directory at the path.") {

  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;
  if (rmdir(path)) REPORT_ERRNO(rmdir);
}

DEF(_osUnlink,
  "os.rmdir(path:String)\n"
  "Removes a file at the path.") {

  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;
  if (unlink(path)) REPORT_ERRNO(unlink);
}

DEF(_osModitime,
  "os.moditime(path:String) -> Number\n"
  "Returns the modified timestamp of the file.") {
  const char* path;
  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  double mtime = 0;
  struct stat path_stat;
  if (stat(path, &path_stat) == 0) mtime = (double) path_stat.st_mtime;
  pkSetSlotNumber(vm, 0, mtime);
}

DEF(_osFileSize,
  "os.filesize(path:String) -> Number\n"
  "Returns the file size in bytes.") {
  const char* path;

  if (!pkValidateSlotString(vm, 1, &path, NULL)) return;

  struct stat path_stat;
  if (stat(path, &path_stat) || ((path_stat.st_mode & S_IFMT) != S_IFREG)) {
    pkSetRuntimeErrorFmt(vm, "Path '%s' wasn't a file.", path);
    return;
  }

  pkSetSlotNumber(vm, 0, path_stat.st_size);

}

DEF(_osSystem,
  "os.system(cmd:String) -> Number\n"
  "Execute the command in a subprocess, Returns the exit code of the child "
  "process.") {
  const char* cmd;

  if (!pkValidateSlotString(vm, 1, &cmd, NULL)) return;

  errno = 0;
  int code = system(cmd);
  if (errno != 0) {
    REPORT_ERRNO(system);
    return;
  }

  pkSetSlotNumber(vm, 0, (double) code);
}

DEF(_osGetenv,
  "os.getenv(name:String) -> String\n"
  "Returns the environment variable as String if it exists otherwise it'll "
  "return null.") {

  const char* name;
  if (!pkValidateSlotString(vm, 1, &name, NULL)) return;

  const char* value = getenv(name);
  if (value == NULL) {
    pkSetSlotNull(vm, 0);
    return;
  }

  pkSetSlotString(vm, 0, value);
}

DEF(_osExepath,
  "os.exepath() -> String\n"
  "Returns the path of the pocket interpreter executable.") {

  char buff[MAX_PATH_LEN];
  if (!osGetExeFilePath(buff, MAX_PATH_LEN)) {
    pkSetRuntimeError(vm, "Cannot obtain ececutable path.");
    return;
  }

  pkSetSlotString(vm, 0, buff);
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleOS(PKVM* vm) {

  PkHandle* os = pkNewModule(vm, "os");

  pkReserveSlots(vm, 2);
  pkSetSlotHandle(vm, 0, os);       // slots[0] = os
  pkSetSlotString(vm, 1, OS_NAME);  // slots[1] = "windows"
  pkSetAttribute(vm, 0, "NAME", 1); // os.NAME = "windows"

  pkModuleAddFunction(vm, os, "getcwd", _osGetCWD, 0);
  pkModuleAddFunction(vm, os, "chdir", _osChdir, 1);
  pkModuleAddFunction(vm, os, "mkdir", _osMkdir, 1);
  pkModuleAddFunction(vm, os, "rmdir", _osRmdir, 1);
  pkModuleAddFunction(vm, os, "unlink", _osUnlink, 1);
  pkModuleAddFunction(vm, os, "moditime", _osModitime, 1);
  pkModuleAddFunction(vm, os, "filesize", _osFileSize, 1);
  pkModuleAddFunction(vm, os, "system", _osSystem, 1);
  pkModuleAddFunction(vm, os, "getenv", _osGetenv, 1);
  pkModuleAddFunction(vm, os, "exepath", _osExepath, 0);

  // TODO:
  // - Implement makedirs which recursively mkdir().
  // - Implement copyfile() and copytree()
  // - Implement rmtree() which recursively rmdir() and file.
  //
  //pkModuleAddSource(vm, os, "import path; def makedirs(p) ... end");

  pkRegisterModule(vm, os);
  pkReleaseHandle(vm, os);
}

#undef OS_NAME
#undef MAX_PATH_LEN
