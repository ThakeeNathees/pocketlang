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

void registerModuleFile(PKVM* vm);
void registerModulePath(PKVM* vm);

// Registers all the cli modules.
#define REGISTER_ALL_MODULES(vm) \
  do {                           \
    registerModuleFile(vm);      \
    registerModulePath(vm);      \
  } while (false)

/*****************************************************************************/
/* MODULES INTERNAL                                                          */
/*****************************************************************************/

 // Allocate a new module object of type [Ty].
#define NEW_OBJ(Ty) (Ty*)malloc(sizeof(Ty))

// Dellocate module object, allocated by NEW_OBJ(). Called by the freeObj
// callback.
#define FREE_OBJ(ptr) free(ptr)

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
