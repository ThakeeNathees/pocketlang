/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

// This file will include all source files from the modules, modules/thirdparty
// sub directories here into this single file. That'll make it easy to compile
// with the command `gcc cli/*.c ...` instead of having to add every single
// sub directory to the list of source directory.

/*****************************************************************************/
/* STD MODULE SOURCES                                                        */
/*****************************************************************************/

#include "modules/modules.c"

#include "modules/std_file.c"
#include "modules/std_path.c"

/*****************************************************************************/
/* THIRDPARTY SOURCES                                                        */
/*****************************************************************************/

// Library : cwalk
// License : MIT
// Source  : https://github.com/likle/cwalk/
// Doc     : https://likle.github.io/cwalk/
// About   : Path library for C/C++. Cross-Platform for Windows, MacOS and
//           Linux. Supports UNIX and Windows path styles on those platforms.
#include "modules/thirdparty/cwalk/cwalk.c"

// Library : argparse
// License : MIT
// Source  : https://github.com/cofyc/argparse/
// About   : Command-line arguments parsing library.
#include "thirdparty/argparse/argparse.c"

// Library : dlfcn-win32
// License : MIT
// Source  : https://github.com/dlfcn-win32/dlfcn-win32/
// About   :  An implementation of dlfcn for Windows.
#ifdef _WIN32
// FIXME:
// This library redefine the malloc family macro, which cause a compile
// time warning.
//
//  #include "modules/thirdparty/dlfcn-win32/dlfcn.c"
#endif
