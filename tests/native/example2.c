/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

// This is an example on how to write your own custom type (Vector here) and
// bind it with with the pocket VM.

#include <pocketlang.h>

#include <stdlib.h> /* For malloc */
#include <string.h> /* For strncmp */
#include <math.h>   /* For sqrt */

// The script we're using to test the native Vector type.
static const char* code =
  "  from vector import Vec2               \n"
  "  print('Class     = $Vec2')            \n"
  "                                        \n"
  "  v1 = Vec2(1, 2)                       \n"
  "  print('v1        = $v1')              \n"
  "  print('v1.length = ${v1.length}')     \n"
  "  print()                               \n"
  "                                        \n"
  "  v1.x = 3; v1.y = 4;                   \n"
  "  print('v1        = $v1')              \n"
  "  print('v1.length = ${v1.length}')     \n"
  "  print()                               \n"
  "                                        \n"
  "  v2 = Vec2(5, 6)                       \n"
  "  print('v2        = $v2')              \n"
  "  v3 = v1 + v2                          \n"
  "  print('v3        = $v3')              \n"
  "                                        \n"
  ;

/*****************************************************************************/
/* VECTOR MODULE FUNCTIONS REGISTER                                          */
/*****************************************************************************/

typedef struct {
  double x, y;
} Vector;

// Native instance allocation callback.
void* _newVec(PKVM* vm) {
  Vector* vec = pkRealloc(vm, NULL, sizeof(Vector));
  vec->x = 0;
  vec->y = 0;
  return vec;
}

// Native instance de-allocatoion callback.
void _deleteVec(PKVM* vm, void* vec) {
  pkRealloc(vm, vec, 0);
}

void _vecGetter(PKVM* vm) {
  const char* name = pkGetSlotString(vm, 1, NULL);
  Vector* self = (Vector*)pkGetSelf(vm);
  if (strcmp("x", name) == 0) {
    pkSetSlotNumber(vm, 0, self->x);
    return;
  } else if (strcmp("y", name) == 0) {
    pkSetSlotNumber(vm, 0, self->y);
    return;
  } else if (strcmp("length", name) == 0) {
    double length = sqrt(pow(self->x, 2) + pow(self->y, 2));
    pkSetSlotNumber(vm, 0, length);
    return;
  }

}

void _vecSetter(PKVM* vm) {
  const char* name = pkGetSlotString(vm, 1, NULL);
  Vector* self = (Vector*)pkGetSelf(vm);
  if (strcmp("x", name) == 0) {
    double x;
    if (!pkValidateSlotNumber(vm, 2, &x)) return;
    self->x = x;
    return;
  } else if (strcmp("y", name) == 0) {
    double y;
    if (!pkValidateSlotNumber(vm, 2, &y)) return;
    self->y = y;
    return;
  }
}

void _vecInit(PKVM* vm) {
  double x, y;
  if (!pkValidateSlotNumber(vm, 1, &x)) return;
  if (!pkValidateSlotNumber(vm, 2, &y)) return;

  Vector* self = (Vector*) pkGetSelf(vm);
  self->x = x;
  self->y = y;
}

// Vec2 '+' operator method.
void _vecAdd(PKVM* vm) {
  Vector* self = (Vector*) pkGetSelf(vm);

  pkReserveSlots(vm, 5); // Now we have slots [0, 1, 2, 3, 4].

  pkPlaceSelf(vm, 2);   // slot[2] = self
  pkGetClass(vm, 2, 2); // slot[2] = Vec2 class.

  // slot[1] is slot[2] == other is Vec2 ?
  if (!pkValidateSlotInstanceOf(vm, 1, 2)) return;
  Vector* other = (Vector*) pkGetSlotNativeInstance(vm, 1);

  // slot[3] = new.x
  pkSetSlotNumber(vm, 3, self->x + other->x);

  // slot[4] = new.y
  pkSetSlotNumber(vm, 4, self->y + other->y);

  // slot[0] = Vec2(slot[3], slot[4]) => return value.
  if (!pkNewInstance(vm, 2, 0, 2, 3)) return;
}

void _vecStr(PKVM* vm) {
  Vector* self = (Vector*)pkGetSelf(vm);
  pkSetSlotStringFmt(vm, 0, "[%g, %g]", self->x, self->y);
}

// Register the 'Vector' module and it's functions.
void registerVector(PKVM* vm) {
  PkHandle* vector = pkNewModule(vm, "vector");

  PkHandle* Vec2 = pkNewClass(vm, "Vec2", NULL /*Base Class*/,
                              vector, _newVec, _deleteVec);

  pkClassAddMethod(vm, Vec2, "@getter", _vecGetter, 1);
  pkClassAddMethod(vm, Vec2, "@setter", _vecSetter, 2);
  pkClassAddMethod(vm, Vec2, "_init",   _vecInit,   2);
  pkClassAddMethod(vm, Vec2, "_str",    _vecStr,    0);
  pkClassAddMethod(vm, Vec2, "+",       _vecAdd,    1);
  pkReleaseHandle(vm, Vec2);

  pkRegisterModule(vm, vector);
  pkReleaseHandle(vm, vector);
}

/*****************************************************************************/
/* POCKET VM CALLBACKS                                                       */
/*****************************************************************************/

int main(int argc, char** argv) {

  PKVM* vm = pkNewVM(NULL);
  registerVector(vm);
  pkRunString(vm, code);
  pkFreeVM(vm);
  
  return 0;
}
