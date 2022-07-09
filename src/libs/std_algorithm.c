/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <math.h>

#ifndef PK_AMALGAMATED
#include "libs.h"
#include "../core/value.h"
#include "../core/vm.h"
#include "../core/core.h"
#endif

#include "thirdparty/timsort/timsort_m.c"

#define ARG(n) (vm->fiber->ret[n])

int cmpVarAsc(const void * a, const void * b, void * c) {
  PKVM* vm = (PKVM*) c;
  Var l = *((Var*) a), r = *((Var*) b);
  Var isGreater = varGreater(vm, l, r);
  if (VM_HAS_ERROR(vm)) return 0;
  Var isLesser = varLesser(vm, l, r);
  if (VM_HAS_ERROR(vm)) return 0;
  return toBool(isGreater) - toBool(isLesser);
}

int cmpVarDesc(const void * a, const void * b, void * c) {
  PKVM* vm = (PKVM*) c;
  Var l = *((Var*) a), r = *((Var*) b);
  Var isGreater = varGreater(vm, l, r);
  if (VM_HAS_ERROR(vm)) return 0;
  Var isLesser = varLesser(vm, l, r);
  if (VM_HAS_ERROR(vm)) return 0;
  return toBool(isLesser) - toBool(isGreater);
}

int cmpVarCustomAsc(const void * a, const void * b, void * c) {
  PKVM* vm = (PKVM*) c;
  Var l = *((Var*) a), r = *((Var*) b);

  // 0: closure, 1: l, 2: r, 3: ret
  ARG(1) = l; ARG(2) = r;
  pkCallFunction(vm, 0, 2, 1, 3);

  if (pkGetSlotType(vm, 3) != PK_NUMBER || VM_HAS_ERROR(vm)) return 0;
  double ret = pkGetSlotNumber(vm, 3);
  return ret > 0 ? 1 : (ret < 0 ? -1 : 0) ;
}

int cmpVarCustomDesc(const void * a, const void * b, void * c) {
  PKVM* vm = (PKVM*) c;
  Var l = *((Var*) a), r = *((Var*) b);

  // 0: closure, 1: l, 2: r, 3: ret
  ARG(1) = l; ARG(2) = r;
  pkCallFunction(vm, 0, 2, 1, 3);

  if (pkGetSlotType(vm, 3) != PK_NUMBER || VM_HAS_ERROR(vm)) return 0;
  double ret = pkGetSlotNumber(vm, 3);
  return ret < 0 ? 1 : (ret > 0 ? -1 : 0) ;
}

bool checkSlotClosureOrBool(PKVM* vm, int argc, int slot, int* cmpSlot, bool* reverse) {
  if (argc >= slot) {
    if (pkGetSlotType(vm, slot) == PK_BOOL) {
      *reverse = pkGetSlotBool(vm, slot);
      return true;

    } else if (pkGetSlotType(vm, slot) == PK_CLOSURE) {
      *cmpSlot = slot;
      return true;

    } else {

      pkSetRuntimeError(vm, "Expected a 'Bool' or a 'Closure'");
      return false;
    }
  }
  return true;
}

bool checkSlotClosureCmp(PKVM* vm, int cmpSlot) {
  if (!pkGetAttribute(vm, cmpSlot, "arity", 0)) return false;

  if ((int) pkGetSlotNumber(vm, 0) != 2) {
    pkSetRuntimeError(vm, "Expected exactly 2 argument(s) for function cmp.");
    return false;
  }

  return true;
}

DEF(algorithmSort,
  "sort(list:List[, cmp:Closure, reverse=false]) -> List",
  "Sort a [list] by TimSort algorithm.") {

  int argc = pkGetArgc(vm);
  int cmpSlot = 0;
  bool reverse = false;

  if (!pkCheckArgcRange(vm, argc, 1, 3)) return;
  if (!pkValidateSlotType(vm, 1, PK_LIST)) return;

  if (!checkSlotClosureOrBool(vm, argc, 2, &cmpSlot, &reverse)) return;
  if (!checkSlotClosureOrBool(vm, argc, 3, &cmpSlot, &reverse)) return;
  if (cmpSlot != 0 && !checkSlotClosureCmp(vm, cmpSlot)) return;

  PkHandle* handle = pkGetSlotHandle(vm, 1);
  List* list = (List*) AS_OBJ(ARG(1));
  int (*cmp)(const void *, const void *, void *);

  if (list->elements.count >= 2) {
    if (cmpSlot != 0) {
      ARG(0) = ARG(cmpSlot);
      pkReserveSlots(vm, 4);
      cmp = reverse ? cmpVarCustomDesc : cmpVarCustomAsc;

    } else {
      cmp = reverse ? cmpVarDesc : cmpVarAsc;
    }

    timsort_r(list->elements.data, list->elements.count, sizeof(Var), cmp, vm);
  }

  pkSetSlotHandle(vm, 0, handle);
  pkReleaseHandle(vm, handle);
}

DEF(algorithmIsSorted,
  "isSorted(list:List[, cmp:Closure, reverse=false]) -> Bool",
  "Checks to see whether [list] is already sorted.") {

  int argc = pkGetArgc(vm);
  int cmpSlot = 0;
  bool reverse = false;

  if (!pkCheckArgcRange(vm, argc, 1, 3)) return;
  if (!pkValidateSlotType(vm, 1, PK_LIST)) return;

  if (!checkSlotClosureOrBool(vm, argc, 2, &cmpSlot, &reverse)) return;
  if (!checkSlotClosureOrBool(vm, argc, 3, &cmpSlot, &reverse)) return;
  if (cmpSlot != 0 && !checkSlotClosureCmp(vm, cmpSlot)) return;

  List* list = (List*) AS_OBJ(ARG(1));
  int (*cmp)(const void *, const void *, void *);
  bool result = true;

  if (list->elements.count >= 2) {
    if (cmpSlot != 0) {
      ARG(0) = ARG(cmpSlot);
      pkReserveSlots(vm, 4);
      cmp = reverse ? cmpVarCustomDesc : cmpVarCustomAsc;

    } else {
      cmp = reverse ? cmpVarDesc : cmpVarAsc;
    }

    for (uint32_t i = 0; i < list->elements.count - 1; i++) {
      if (cmp(&list->elements.data[i], &list->elements.data[i + 1], vm) > 0) {
        result = false;
        break;
      }
    }
  }

  pkSetSlotBool(vm, 0, result);
}

int32_t bSearch(PKVM* vm, List* list, Var* key, comparator cmp) {
  int32_t count = list->elements.count;

  if (count <= 0) return -1;
  if (count == 1) {
    if (cmp(&list->elements.data[0], key, vm) == 0) return 0;
    return -1;
  }

  int32_t a = 0, b = count, cmpRes;
  while (a < b) {
    int32_t mid = (a + b) >> 1;
    cmpRes = cmp(&list->elements.data[mid], key, vm);
    if (cmpRes == 0)
      return mid;

    if (cmpRes < 0) a = mid + 1;
    else b = mid;
  }

  if (a >= count || cmp(&list->elements.data[a], key, vm) != 0)
    return -1;

  return a;
}

DEF(algorithmBinarySearch,
  "binarySearch(list:List, key:Var[, cmp:Closure]) -> Number",
  "Binary search for key in [list]. Return the index of key or -1 if not found. "
  "Assumes that list is sorted.") {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 2, 3)) return;
  if (!pkValidateSlotType(vm, 1, PK_LIST)) return;

  List* list = (List*) AS_OBJ(ARG(1));
  Var* key = &(ARG(2));
  comparator cmp;

  if (argc == 3) {
    if (!pkValidateSlotType(vm, 3, PK_CLOSURE)) return;
    if (!checkSlotClosureCmp(vm, 3)) return;

    ARG(0) = ARG(3);
    pkReserveSlots(vm, 4);
    cmp = cmpVarCustomAsc;
  }
  else {
    cmp = cmpVarAsc;
  }

  pkSetSlotNumber(vm, 0, (double) bSearch(vm, list, key, cmp));
}

DEF(algorithmReverse,
  "reverse(list:List[, range:Range]) -> List",
  "Reverse a [list].") {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 1, 2)) return;
  if (!pkValidateSlotType(vm, 1, PK_LIST)) return;

  PkHandle* handle = pkGetSlotHandle(vm, 1);
  List* list = (List*) AS_OBJ(ARG(1));
  uint32_t count = list->elements.count;

  do {
    int32_t first = 0, last = count - 1;

    if (argc == 2) {
      if (!pkValidateSlotType(vm, 2, PK_RANGE)) break;
      Range* r = (Range*)AS_OBJ(ARG(2));

      if ((floor(r->from) != r->from) || (floor(r->to) != r->to)) {
        pkSetRuntimeError(vm, "Expected a whole number.");
        break;
      }

      first = (int32_t)r->from;
      last = (int32_t)r->to;
      if (first < 0) first = count + first;
      if (last < 0) last = count + last;
      if (last < first) {
        int32_t tmp = last;
        last = first;
        first = tmp;
      }

      if (first < 0 || last >= count) {
        pkSetRuntimeError(vm, "List index out of bound.");
        break;
      }
    }

    while (first < last) {
      Var tmp = list->elements.data[first];
      list->elements.data[first] = list->elements.data[last];
      list->elements.data[last] = tmp;
      last--; first++;
    }

  } while (false);

  pkSetSlotHandle(vm, 0, handle);
  pkReleaseHandle(vm, handle);
}

static void _callfn(PKVM* vm, pkNativeFn fn) {
  int argc = pkGetArgc(vm);
  pkReserveSlots(vm, argc + 1);
  for (int i = argc; i > 0; i--) {
    ARG(i + 1) = ARG(i);
  }
  pkPlaceSelf(vm, 1);

  vm->fiber->sp += 1; // is there a bettery way?
  fn(vm);
  vm->fiber->sp -= 1;
}

DEF(listSort,
  "List.sort([cmp:Closure, reverse=false]) -> List",
  "Sort the [list] by TimSort algorithm.") {
  if (!pkCheckArgcRange(vm, pkGetArgc(vm), 0, 2)) return;
  _callfn(vm, algorithmSort);
}

DEF(listIsSorted,
  "List.isSorted([cmp:Closure, reverse=false]) -> Bool",
  "Checks to see whether [list] is already sorted.") {
  if (!pkCheckArgcRange(vm, pkGetArgc(vm), 0, 2)) return;
  _callfn(vm, algorithmIsSorted);
}

DEF(listBinarySearch,
  "List.binarySearch(key:Var[, cmp:Closure]) -> Number",
  "Binary search for key in [list]. Return the index of key or -1 if not found. "
  "Assumes that list is sorted.") {
  if (!pkCheckArgcRange(vm, pkGetArgc(vm), 1, 2)) return;
  _callfn(vm, algorithmBinarySearch);
}

DEF(listReverse,
  "List.reverse([range:Range]) -> List",
  "Reverse the [list].") {
  if (!pkCheckArgcRange(vm, pkGetArgc(vm), 0, 1)) return;
  _callfn(vm, algorithmReverse);
}

#undef ARG

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleAlgorithm(PKVM* vm) {
  PkHandle* algorithm = pkNewModule(vm, "algorithm");

  REGISTER_FN(algorithm, "sort", algorithmSort, -1);
  REGISTER_FN(algorithm, "isSorted", algorithmIsSorted, -1);
  REGISTER_FN(algorithm, "binarySearch", algorithmBinarySearch, -1);
  REGISTER_FN(algorithm, "reverse", algorithmReverse, -1);

  // Wrap algorithm functions to methods of List.
  // These methods can be call without `import algorithm`.
  pkReserveSlots(vm, 1);
  pkNewList(vm, 0);
  pkGetClass(vm, 0, 0);
  PkHandle* clsList = pkGetSlotHandle(vm, 0);
  ADD_METHOD(clsList, "sort", listSort, -1);
  ADD_METHOD(clsList, "isSorted", listIsSorted, -1);
  ADD_METHOD(clsList, "reverse", listReverse, -1);
  ADD_METHOD(clsList, "binarySearch", listBinarySearch, -1);
  pkReleaseHandle(vm, clsList);

  pkRegisterModule(vm, algorithm);
  pkReleaseHandle(vm, algorithm);
}
