/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "modules.h"

// A DUMMY MODULE TO TEST NATIVE INTERFACE AND CLASSES.

typedef struct {
  double val;
} Dummy;

void* _newDummy() {
  Dummy* dummy = NEW_OBJ(Dummy);
  ASSERT(dummy != NULL, "malloc failed.");
  dummy->val = 0;
  return dummy;
}

void _deleteDummy(void* ptr) {
  Dummy* dummy = (Dummy*)ptr;
  FREE_OBJ(dummy);
}

DEF(_dummyGetter, "") {
  const char* name = pkGetSlotString(vm, 1, NULL);
  Dummy* self = (Dummy*)pkGetSelf(vm);
  if (strcmp("val", name) == 0) {
    pkSetSlotNumber(vm, 0, self->val);
    return;
  }
}

DEF(_dummySetter, "") {
  const char* name = pkGetSlotString(vm, 1, NULL);
  Dummy* self = (Dummy*)pkGetSelf(vm);
  if (strcmp("val", name) == 0) {
    double val;
    if (!pkValidateSlotNumber(vm, 2, &val)) return;
    self->val = val;
    return;
  }
}

DEF(_dummyEq, "") {

  // TODO: Currently there is no way of getting another native instance
  // So, it's impossible to check self == other. So for now checking with
  // number.
  double value;
  if (!pkValidateSlotNumber(vm, 1, &value)) return;

  Dummy* self = (Dummy*)pkGetSelf(vm);
  pkSetSlotBool(vm, 0, value == self->val);
}

DEF(_dummyGt, "") {

  // TODO: Currently there is no way of getting another native instance
  // So, it's impossible to check self == other. So for now checking with
  // number.
  double value;
  if (!pkValidateSlotNumber(vm, 1, &value)) return;

  Dummy* self = (Dummy*)pkGetSelf(vm);
  pkSetSlotBool(vm, 0, self->val > value);
}

DEF(_dummyMethod,
  "Dummy.a_method(n1:num, n2:num) -> num\n"
  "A dummy method to check dummy method calls. Will take 2 number arguments "
  "and return the multiplication.") {

  double n1, n2;
  if (!pkValidateSlotNumber(vm, 1, &n1)) return;
  if (!pkValidateSlotNumber(vm, 2, &n2)) return;
  pkSetSlotNumber(vm, 0, n1 * n2);

}

void registerModuleDummy(PKVM* vm) {
  PkHandle* dummy = pkNewModule(vm, "dummy");

  PkHandle* cls_dummy = pkNewClass(vm, "Dummy", NULL, dummy,
                                   _newDummy, _deleteDummy);
  pkClassAddMethod(vm, cls_dummy, "@getter",  _dummyGetter, 1);
  pkClassAddMethod(vm, cls_dummy, "@setter",  _dummySetter, 2);
  pkClassAddMethod(vm, cls_dummy, "==",       _dummyEq,     1);
  pkClassAddMethod(vm, cls_dummy, ">",        _dummyGt,     1);
  pkClassAddMethod(vm, cls_dummy, "a_method", _dummyMethod, 2);
  pkReleaseHandle(vm, cls_dummy);

  pkRegisterModule(vm, dummy);
  pkReleaseHandle(vm, dummy);
}
