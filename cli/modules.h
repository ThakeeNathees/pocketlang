/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include "internal.h"

/*****************************************************************************/
/* MODULE OBJECTS                                                            */
/*****************************************************************************/

// Type enums of cli module objects.
typedef enum {
  OBJ_FILE = 1,
} ObjType;

// The abstract type of the objects.
typedef struct {
  ObjType type;
} Obj;

// File access mode.
typedef enum {

  FMODE_READ       = (1 << 0),
  FMODE_WRITE      = (1 << 1),
  FMODE_APPEND     = (1 << 2),

  _FMODE_EXT       = (1 << 3),
  FMODE_READ_EXT   = (_FMODE_EXT | FMODE_READ),
  FMODE_WRITE_EXT  = (_FMODE_EXT | FMODE_WRITE),
  FMODE_APPEND_EXT = (_FMODE_EXT | FMODE_APPEND),
} FileAccessMode;

// Str  | If already exists | If does not exist |
// -----+-------------------+-------------------|
// 'r'  |  read from start  |   failure to open |
// 'w'  |  destroy contents |   create new      |
// 'a'  |  write to end     |   create new      |
// 'r+' |  read from start  |   error           |
// 'w+' |  destroy contents |   create new      |
// 'a+' |  write to end     |   create new      |

// A wrapper around the FILE* for the File module.
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
// instance.
bool objGetAttrib(PKVM* vm, void* instance, PkStringPtr attrib);

// The free callback of the object, that'll called by pocketlang when a
// pocketlang native instance garbage collected.
void freeObj(PKVM* vm, void* instance);

// The native instance get_name callback used to get the name of a native
// instance from pocketlang. Here the id we're using is the ObjType enum.
const char* getObjName(uint32_t id);

// Registers all the cli modules.
void registerModules(PKVM* vm);

// 'path' moudle public functions used at various cli functions.
bool pathIsAbsolute(const char* path);
void pathGetDirName(const char* path, size_t* length);
size_t pathNormalize(const char* path, char* buff, size_t buff_size);
size_t pathJoin(const char* from, const char* path, char* buffer,
                size_t buff_size);
