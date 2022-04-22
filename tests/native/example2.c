/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#error Native interface is being refactored and will be completed soon.

// This is an example on how to write your own custom type (Vector here) and
// bind it with with the pocket VM.

#include <pocketlang.h>

#include <stdlib.h> /* For malloc */
#include <stdio.h>  /* For printf */
#include <string.h> /* For strncmp */
#include <math.h>   /* For sqrt */

// The script we're using to test the native Vector type.
static const char* code =
  "  from vector import Vec2               \n"
  "  print('Class     = $Vec2')            \n"
  "                                        \n"
  "  v1 = Vec2(1, 2)                       \n"
  "  print('v1        = $v1')              \n"
  "  print()                               \n"
  "                                        \n"
  "  print('v1.x      = ${v1.x}')          \n"
  "  print('v1.y      = ${v1.y}')          \n"
  "  print('v1.length = ${v1.length}')     \n"
  "  print()                               \n"
  "                                        \n"
  "  v1.x = 3; v1.y = 4;                   \n"
  "  print('v1.x      = ${v1.x}')          \n"
  "  print('v1.y      = ${v1.y}')          \n"
  "  print('v1.length = ${v1.length}')     \n"
  "  print()                               \n"
  "                                        \n"
  "  v2 = Vec2(5, 6)                       \n"
  "  v3 = v1.add(v2)                       \n"
  "  print('v3        = ${v3}')            \n"
  "  print('v3.x      = ${v3.x}')          \n"
  "  print('v3.y      = ${v3.y}')          \n"
  "                                        \n"
  ;

/*****************************************************************************/
/* VECTOR MODULE FUNCTIONS REGISTER                                          */
/*****************************************************************************/

typedef struct {
  double x, y;
} Vector;

// Native instance allocation callback.
void* _newVec() {
  Vector* vec = (Vector*)malloc(sizeof(Vector));
  vec->x = 0;
  vec->y = 0;
  return vec;
}

// Native instance de-allocatoion callback.
void _deleteVec(void* vector) {
  free(vector);
}

// Vec2 'add' method.
void _vec2Add(PKVM* vm) {
  Vector* self = (Vector*)pkGetSelf(vm);
  // FIXME:
  // Temproarly it's not possible to get vector from the args since the native
  // interface is being refactored. Will be implemented soon.
}

// Register the 'Vector' module and it's functions.
void registerVector(PKVM* vm) {
  PkHandle* vector = pkNewModule(vm, "vector");

  PkHandle* Vec2 = pkNewClass(vm, "Vec2", NULL /*Base Class*/,
                              vector, _newVec, _deleteVec);
  pkClassAddMethod(vm, Vec2, "add", _vec2Add, 1);
  pkReleaseHandle(vm, Vec2);

  pkRegisterModule(vm, vector);
  pkReleaseHandle(vm, vector);
}

/*****************************************************************************/
/* POCKET VM CALLBACKS                                                       */
/*****************************************************************************/

void stdoutCallback(PKVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}

int main(int argc, char** argv) {

  PkConfiguration config = pkNewConfiguration();
  config.stdout_write = stdoutCallback;

  PKVM* vm = pkNewVM(&config);
  registerVector(vm);
  
  PkStringPtr source = { .string = code };
  PkStringPtr path = { .string = "./some/path/" };
  
  PkResult result = pkInterpretSource(vm, source, path, NULL/*options*/);
  pkFreeVM(vm);
  
  return result;
}
