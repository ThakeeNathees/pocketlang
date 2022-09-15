/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */
#include <stdint.h>
#include <time.h>

#ifndef PK_AMALGAMATED
#include "libs.h"
#include "../core/value.h"
#include "../core/vm.h"
#endif

// https://prng.di.unimi.it/splitmix64.c

static uint64_t x; /* The state can be seeded with any value. */

uint64_t next() {
  uint64_t z = (x += 0x9e3779b97f4a7c15);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
  z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
  return z ^ (z >> 31);
}

static void seed(uint64_t n) {
  x = n;
}

DEF(_randomSeed,
  "random.seed(n:Number) -> Null",
  "Initialize the random number generator.") {

  double n = 0;
  if (!pkValidateSlotNumber(vm, 1, &n)) return;
  seed(*(uint64_t*)&n);
}

DEF(_randomRand,
  "random.rand([max:Number | r:Range, isInteger=false]) -> Number",
  "Returns a random number.") {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 0, 2)) return;
  pkReserveSlots(vm, 2);

  bool isInteger = false;
  if (argc == 2) {
    if (!pkValidateSlotBool(vm, 2, &isInteger)) return;
  }

  double min = 0, max = 1.0;
  if (argc >= 1) {
    switch (pkGetSlotType(vm, 1)) {
      case PK_RANGE:
        if (pkGetAttribute(vm, 1, "first", 2)) min = pkGetSlotNumber(vm, 2);
        if (pkGetAttribute(vm, 1, "last", 2)) max = pkGetSlotNumber(vm, 2);
        break;

      case PK_NUMBER:
        max = pkGetSlotNumber(vm, 1);
        break;

      default:
        pkSetRuntimeError(vm, "Expected a number or a range.");
        return;
    }
  }

  uint64_t u = (next() >> 12) | (0x3ffLL << 52);
  double value = (*(double*)&u - 1.0) * (max - min) + min;
  if (isInteger) value = (double)(int32_t) value;
  pkSetSlotNumber(vm, 0, value);
}

// TODO: use function in pocketlang.h instead of vm.h
#define ARG(n) (vm->fiber->ret[n])

DEF(_randomSample,
  "random.sample(list:List) -> Var",
  "Returns a random element from the list.") {

  if (!pkValidateSlotType(vm, 1, PK_LIST)) return;

  List* list = (List*) AS_OBJ(ARG(1));
  if (list->elements.count == 0) {
    pkSetRuntimeError(vm, "Expected a non-empty list.");
    return;
  }

  uint32_t index = (uint32_t) next() % list->elements.count;
  ARG(0) = list->elements.data[index];
}

DEF(_randomShuffle,
  "random.shuffle(list:List) -> List",
  "Shuffles a list.") {

  if (!pkValidateSlotType(vm, 1, PK_LIST)) return;

  PkHandle* handle = pkGetSlotHandle(vm, 1);
  List* list = (List*) AS_OBJ(ARG(1));

  if (list->elements.count <= 1) return; // nothing to do

  for (uint32_t i = list->elements.count - 1; i >= 1; i--) {
    uint32_t j = (uint32_t) next() % (i + 1);
    if (j != i) {
      Var tmp = list->elements.data[i];
      list->elements.data[i] = list->elements.data[j];
      list->elements.data[j] = tmp;
    }
  }

  pkSetSlotHandle(vm, 0, handle);
  pkReleaseHandle(vm, handle);
}

#undef ARG

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

#if defined(_WIN32) || defined(__NT__)
  #include <windows.h>
#endif

void registerModuleRandom(PKVM* vm) {
  #if defined(_WIN32) || defined(__NT__)
    uint64_t ticks;
    QueryPerformanceCounter((LARGE_INTEGER*)&ticks);
    seed(ticks);
  #else
    seed(time(NULL));
  #endif

  PkHandle* random = pkNewModule(vm, "random");

  REGISTER_FN(random, "seed", _randomSeed,  1);
  REGISTER_FN(random, "rand", _randomRand, -1);
  REGISTER_FN(random, "sample", _randomSample, 1);
  REGISTER_FN(random, "shuffle", _randomShuffle, 1);

  pkRegisterModule(vm, random);
  pkReleaseHandle(vm, random);
}
