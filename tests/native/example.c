
#include <pocketlang.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// The script we're using to test the native Vector type.
static const char* code = "                           \n\
  import Vector # The native module.                  \n\
  print('Module        =', Vector)                    \n\
                                                      \n\
  vec1 = Vector.new(1, 2) # Calling native method.    \n\
  print('vec1          =', 'Vector.new(1, 2)')        \n\
  print()                                             \n\
                                                      \n\
  # Using the native getter.                          \n\
  print('vec1.x        =', vec1.x)                    \n\
  print('vec1.y        =', vec1.y)                    \n\
  print('vec1.length   =', vec1.length)               \n\
  print()                                             \n\
                                                      \n\
  # Using the native setter.                          \n\
  vec1.x = 3; vec1.y = 4;                             \n\
  print('vec1.x        =', vec1.x)                    \n\
  print('vec1.y        =', vec1.y)                    \n\
  print('vec1.length   =', vec1.length)               \n\
  print()                                             \n\
                                                      \n\
  vec2 = Vector.new(5, 6)                             \n\
  vec3 = Vector.add(vec1, vec2)                       \n\
  print('vec3          =', 'Vector.add(vec1, vec2)')  \n\
  print('vec3.x        =', vec3.x)                    \n\
  print('vec3.y        =', vec3.y)                    \n\
                                                      \n\
";

/*****************************************************************************/
/* NATIVE TYPE DEFINES & CALLBACKS                                           */
/*****************************************************************************/

// An enum value of native object, used as unique of the type in pocketlang.
typedef enum {
  OBJ_VECTOR = 0,
} ObjType;

typedef struct {
  ObjType type;
} Obj;

typedef struct {
  Obj base;    // "Inherits" objects.
  double x, y; // Vector variables.
} Vector;

// Get name callback, will called from pocketlang to get the type name from
// the ID (the enum value).
const char* getObjName(uint32_t id) {
  switch ((ObjType)id) {
    case OBJ_VECTOR: return "Vector";
  }
  return NULL; // Unreachable.
}

// Instance getter callback to get a value from the native instance.
// The hash value and the length of the string are provided with the
// argument [attrib].
void objGetAttrib(PKVM* vm, void* instance, PkStringPtr attrib) {
  
  Obj* obj = (Obj*)instance;
  
  switch (obj->type) {
    case OBJ_VECTOR: {
      Vector* vector = ((Vector*)obj);
      if (strcmp(attrib.string, "x") == 0) {
        pkReturnNumber(vm, vector->x);
        return;
        
      } else if (strcmp(attrib.string, "y") == 0) {
        pkReturnNumber(vm, vector->y);
        return;

      } else if (strcmp(attrib.string, "length") == 0) {
        double length = sqrt(pow(vector->x, 2) + pow(vector->y, 2));
        pkReturnNumber(vm, length);
        return;

      }
    } break;
  }
  
  // If we reached here that means the attribute doesn't exists.
  // Since we haven't used pkReturn...() function, pocket VM already
  // know that the attribute doesn't exists. just return.
  return;
}

// Instance setter callback to set the value to the native instance.
// The hash value and the length of the string are provided with the
// argument [attrib].
bool objSetAttrib(PKVM* vm, void* instance, PkStringPtr attrib) {
  
  Obj* obj = (Obj*)instance;
  
  switch (obj->type) {
    case OBJ_VECTOR: {
      if (strcmp(attrib.string, "x") == 0) {
        double x; // Get the number x.
        if (!pkGetArgNumber(vm, 0, &x)) return false;
        ((Vector*)obj)->x = x;
        return true;
        
      } else if (strcmp(attrib.string, "y") == 0) {
        double y; // Get the number x.
        if (!pkGetArgNumber(vm, 0, &y)) return false;
        ((Vector*)obj)->y = y;
        return true;
        
      }
    } break;
  }
  
  // If we reached here that means the attribute doesn't exists.
  // Return false to indicate it.
  return false;
}

// The free object callback, called just before the native instance, garbage
// collect.
void freeObj(PKVM* vm, void* instance) {
  Obj* obj = (Obj*)instance;
  // Your cleanups.
  free((void*)obj);
}

/*****************************************************************************/
/* VECTOR MODULE FUNCTIONS REGISTER                                          */
/*****************************************************************************/

// The Vector.new(x, y) function.
void _vecNew(PKVM* vm) {
  double x, y; // The args.
  
  // Get the args from the stack, If it's not number, return.
  if (!pkGetArgNumber(vm, 1, &x)) return;
  if (!pkGetArgNumber(vm, 2, &y)) return;
  
  // Create a new vector.
  Vector* vec = (Vector*)malloc(sizeof(Vector));
  vec->base.type = OBJ_VECTOR;
  vec->x = x, vec->y = y;

  pkReturnInstNative(vm, (void*)vec, OBJ_VECTOR);
}

// The Vector.length(vec) function.
void _vecAdd(PKVM* vm) {
  Vector *v1, *v2;
  if (!pkGetArgInst(vm, 1, OBJ_VECTOR, (void**)&v1)) return;
  if (!pkGetArgInst(vm, 2, OBJ_VECTOR, (void**)&v2)) return;
  
  // Create a new vector.
  Vector* v3 = (Vector*)malloc(sizeof(Vector));
  v3->base.type = OBJ_VECTOR;
  v3->x = v1->x + v2->x;
  v3->y = v1->y + v2->y;  

  pkReturnInstNative(vm, (void*)v3, OBJ_VECTOR);
}

// Register the 'Vector' module and it's functions.
void registerVector(PKVM* vm) {
  PkHandle* vector = pkNewModule(vm, "Vector");

  pkModuleAddFunction(vm, vector, "new",  _vecNew,  2);
  pkModuleAddFunction(vm, vector, "add",  _vecAdd,  2);

  pkReleaseHandle(vm, vector);
}

/*****************************************************************************/
/* POCKET VM CALLBACKS                                                       */
/*****************************************************************************/

// Error report callback.
void reportError(PKVM* vm, PkErrorType type,
                 const char* file, int line,
                 const char* message) {
  fprintf(stderr, "Error: %s\n", message);
}

// print() callback to write stdout.
void stdoutWrite(PKVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}


int main(int argc, char** argv) {

  PkConfiguration config = pkNewConfiguration();
  config.error_fn           = reportError;
  config.write_fn           = stdoutWrite;
  //config.read_fn          = stdinRead;
  config.inst_free_fn       = freeObj;
  config.inst_name_fn       = getObjName;
  config.inst_get_attrib_fn = objGetAttrib;
  config.inst_set_attrib_fn = objSetAttrib;
  //config.load_script_fn   = loadScript;
  //config.resolve_path_fn  = resolvePath;

  PKVM* vm = pkNewVM(&config);
  registerVector(vm);
  
  PkStringPtr source = { code, NULL, NULL };
  PkStringPtr path = { "./some/path/", NULL, NULL };
  
  PkResult result = pkInterpretSource(vm, source, path, NULL/*options*/);
  pkFreeVM(vm);
  
  return result;
}
