/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef MODULES_H
#define MODULES_H

#include <pocketlang.h>

#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

void registerModuleIO(PKVM* vm);
void registerModulePath(PKVM* vm);
void registerModuleMath(PKVM* vm);

// Registers all the cli modules.
#define REGISTER_ALL_MODULES(vm) \
  do {                           \
    registerModuleIO(vm);        \
    registerModulePath(vm);      \
    registerModuleMath(vm);      \
  } while (false)

/*****************************************************************************/
/* MODULES INTERNAL                                                          */
/*****************************************************************************/

// Allocate a new module object of type [Ty].
#define NEW_OBJ(Ty) (Ty*)malloc(sizeof(Ty))

// Dellocate module object, allocated by NEW_OBJ(). Called by the freeObj
// callback.
#define FREE_OBJ(ptr) free(ptr)

// Returns the docstring of the function, which is a static const char* defined
// just above the function by the DEF() macro below.
#define DOCSTRING(fn) __doc_##fn

// A macro to declare a function, with docstring, which is defined as
// ___doc_<fn> = docstring; That'll used to generate function help text.
#define DEF(fn, docstring)                      \
  static const char* DOCSTRING(fn) = docstring; \
  static void fn(PKVM* vm)

/*****************************************************************************/
/* SHARED FUNCTIONS                                                          */
/*****************************************************************************/

// These are "public" module functions that can be shared. Since some modules
// can be used for cli's internals we're defining such functions here and they
// will be imported in the cli.

bool pathIsAbsolute(const char* path);

void pathGetDirName(const char* path, size_t* length);

size_t pathNormalize(const char* path, char* buff, size_t buff_size);

size_t pathJoin(const char* from, const char* path, char* buffer,
                size_t buff_size);

#endif // MODULES_H
