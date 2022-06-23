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

// https://xoroshiro.di.unimi.it/xoroshiro128plus.c
/*  Written in 2016-2018 by David Blackman and Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.

See <http://creativecommons.org/publicdomain/zero/1.0/>. */

static inline uint64_t rotl(const uint64_t x, int k) {
  return (x << k) | (x >> (64 - k));
}

static uint64_t s[2];

static uint64_t next(void) {
  const uint64_t s0 = s[0];
  uint64_t s1 = s[1];
  const uint64_t result = s0 + s1;

  s1 ^= s0;
  s[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16); // a, b
  s[1] = rotl(s1, 37); // c

  return result;
}

static void jump(void) {
  static const uint64_t JUMP[] = { 0xdf900294d8f554a5, 0x170865df4b3201fc };

  uint64_t s0 = 0;
  uint64_t s1 = 0;
  for(int i = 0; i < sizeof JUMP / sizeof *JUMP; i++)
    for(int b = 0; b < 64; b++) {
      if (JUMP[i] & UINT64_C(1) << b) {
        s0 ^= s[0];
        s1 ^= s[1];
      }
      next();
    }

  s[0] = s0;
  s[1] = s1;
}

static void seed(int32_t n) {
  s[0] = n >> 16;
  s[1] = (n & 0xffff) + 1;
  jump();
}

DEF(_randomSeed,
  "random.seed(n:Number) -> Null",
  "Initialize the random number generator.") {

  int32_t n = 0;
  if (!pkValidateSlotInteger(vm, 1, &n)) return;
  seed(n);
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

  if (pkGetSlotType(vm, 1) != PK_LIST) {
    pkSetRuntimeError(vm, "Expected a non-empty list.");
    return;
  }

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

  if (pkGetSlotType(vm, 1) != PK_LIST) {
    pkSetRuntimeError(vm, "Expected a list.");
    return;
  }

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

void registerModuleRandom(PKVM* vm) {
  seed(time(NULL));

  PkHandle* random = pkNewModule(vm, "random");

  REGISTER_FN(random, "seed", _randomSeed,  1);
  REGISTER_FN(random, "rand", _randomRand, -1);
  REGISTER_FN(random, "sample", _randomSample, 1);
  REGISTER_FN(random, "shuffle", _randomShuffle, 1);

  pkRegisterModule(vm, random);
  pkReleaseHandle(vm, random);
}
