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

/*****************************************************************************/
/* MODULE OBJECTS                                                            */
/*****************************************************************************/

// Str  | If already exists | If does not exist |
// -----+-------------------+-------------------|
// 'r'  |  read from start  |   failure to open |
// 'w'  |  destroy contents |   create new      |
// 'a'  |  write to end     |   create new      |
// 'r+' |  read from start  |   error           |
// 'w+' |  destroy contents |   create new      |
// 'a+' |  write to end     |   create new      |
typedef enum {
  FMODE_READ = (1 << 0),
  FMODE_WRITE = (1 << 1),
  FMODE_APPEND = (1 << 2),
  _FMODE_EXT = (1 << 3),
  FMODE_READ_EXT = (_FMODE_EXT | FMODE_READ),
  FMODE_WRITE_EXT = (_FMODE_EXT | FMODE_WRITE),
  FMODE_APPEND_EXT = (_FMODE_EXT | FMODE_APPEND),
} FileAccessMode;

typedef enum {
  OBJ_FILE = 1,

  _OBJ_MAX_
} ObjType;

typedef struct {
  ObjType type;
} Obj;

typedef struct {
  Obj _super;
  FILE* fp;            // C file poinnter.
  FileAccessMode mode; // Access mode of the file.
  bool closed;         // True if the file isn't closed yet.
} File;

/*****************************************************************************/
/* MODULE PUBLIC FUNCTIONS                                                   */
/*****************************************************************************/

// Initialize the native module object with it's default values.
void initObj(Obj* obj, ObjType type);

// A function callback called by pocket VM to get attribute of a native
// instance. The value of the attributes will be returned with pkReturn...()
// functions and if the attribute doesn't exists on the instance we're
// shouldn't return anything, PKVM will know it and set error (or use some
// common attributes like "as_string", "as_repr", etc).
void objGetAttrib(PKVM* vm, void* instance, uint32_t id, PkStringPtr attrib);

// A function callback called by pocket VM to set attribute of a native
// instance.
bool objSetAttrib(PKVM* vm, void* instance, uint32_t id, PkStringPtr attrib);

// The free callback of the object, that'll called by pocketlang when a
// pocketlang native instance garbage collected.
void freeObj(PKVM* vm, void* instance, uint32_t id);

// The native instance get_name callback used to get the name of a native
// instance from pocketlang. Here the id we're using is the ObjType enum.
const char* getObjName(uint32_t id);

// Registers all the cli modules.
void registerModules(PKVM* vm);

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
