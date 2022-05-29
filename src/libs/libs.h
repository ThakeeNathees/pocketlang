/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef LIBS_H
#define LIBS_H

#ifndef PK_AMALGAMATED
#include <pocketlang.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

// FIXME:
// Since this are part of the "standard" pocketlang libraries, we can include
// pocketlang internals here using the relative path, however it'll make these
// libraries less "portable" in a sence that these files cannot be just drag
// and dropped into another embedded application where is cannot find the
// relative include.
//
#ifndef PK_AMALGAMATED
#include "../core/common.h"
#endif // PK_AMALGAMATED

#include <errno.h>

#define REPORT_ERRNO(fn) \
  pkSetRuntimeErrorFmt(vm, "C." #fn " errno:%i - %s.", errno, strerror(errno))

/*****************************************************************************/
/* SHARED FUNCTIONS                                                          */
/*****************************************************************************/

// These are "public" module functions that can be shared. Since some modules
// can be used for cli's internals we're defining such functions here and they
// will be imported in the cli.

// The pocketlang's import statement path resolving function. This
// implementation is required by pockelang from it's hosting application
// inorder to use the import statements.
char* pathResolveImport(PKVM * vm, const char* from, const char* path);

// Register all the the libraries to the PKVM.
void registerLibs(PKVM* vm);

// Cleanup registered libraries call this only if the libraries were registered
// with registerLibs() function.
void cleanupLibs(PKVM* vm);

#endif // LIBS_H
