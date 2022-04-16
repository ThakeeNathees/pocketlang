/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "modules.h"

// Note: Everything here is for testing the native API, and will have to
//       refactor everything.

// Allocate a new module object of type [Ty].
#define NEW_OBJ(Ty) (Ty*)malloc(sizeof(Ty))

// Dellocate module object, allocated by NEW_OBJ(). Called by the freeObj
// callback.
#define FREE_OBJ(ptr) free(ptr)

/*****************************************************************************/
/* MODULE FUNCTIONS DECLARATION                                              */
/*****************************************************************************/

void fileGetAttrib(PKVM* vm, File* file, const char* attrib);
bool fileSetAttrib(PKVM* vm, File* file, const char* attrib);
void fileClean(PKVM* vm, File* file);

void registerModuleFile(PKVM* vm);
void registerModulePath(PKVM* vm);

/*****************************************************************************/
/* MODULE PUBLIC FUNCTIONS                                                   */
/*****************************************************************************/

void initObj(Obj* obj, ObjType type) {
  obj->type = type;
}

void objGetAttrib(PKVM* vm, void* instance, uint32_t id, PkStringPtr attrib) {
  Obj* obj = (Obj*)instance;
  ASSERT(obj->type == (ObjType)id, OOPS);

  switch (obj->type) {
    case OBJ_FILE:
      fileGetAttrib(vm, (File*)obj, attrib.string);
      return;
  }
  STATIC_ASSERT(_OBJ_MAX_ == 2);
}

bool objSetAttrib(PKVM* vm, void* instance, uint32_t id, PkStringPtr attrib) {
  Obj* obj = (Obj*)instance;
  ASSERT(obj->type == (ObjType)id, OOPS);

  switch (obj->type) {
    case OBJ_FILE:
      return fileSetAttrib(vm, (File*)obj, attrib.string);
  }
  STATIC_ASSERT(_OBJ_MAX_ == 2);

  return false;
}

void freeObj(PKVM* vm, void* instance, uint32_t id) {
  Obj* obj = (Obj*)instance;
  ASSERT(obj->type == (ObjType)id, OOPS);

  switch (obj->type) {
    case OBJ_FILE:
      fileClean(vm, (File*)obj);
  }
  STATIC_ASSERT(_OBJ_MAX_ == 2);

  FREE_OBJ(obj);
}

const char* getObjName(uint32_t id) {
  switch ((ObjType)id) {
    case OBJ_FILE: return "File";
  }
  STATIC_ASSERT(_OBJ_MAX_ == 2);
  return NULL;
}

/*****************************************************************************/
/* REGISTER MODULES                                                          */
/*****************************************************************************/

void registerModules(PKVM* vm) {
  registerModuleFile(vm);
  registerModulePath(vm);
}
