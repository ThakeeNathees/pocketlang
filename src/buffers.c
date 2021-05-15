/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "buffers.h"

#include "utils.h"
#include "vm.h"

#define DEFINE_BUFFER(M__NAME, M__NAME_L, M__TYPE)                            \
  void M__NAME_L##BufferInit(M__NAME##Buffer* self) {                         \
    self->data = NULL;                                                        \
    self->count = 0;                                                          \
    self->capacity = 0;                                                       \
  }                                                                           \
                                                                              \
  void M__NAME_L##BufferClear(M__NAME##Buffer* self,                          \
              PKVM* vm) {                                                     \
    vmRealloc(vm, self->data, self->capacity * sizeof(M__TYPE), 0);           \
    self->data = NULL;                                                        \
    self->count = 0;                                                          \
    self->capacity = 0;                                                       \
  }                                                                           \
                                                                              \
  void M__NAME_L##BufferFill(M__NAME##Buffer* self, PKVM* vm,                 \
              M__TYPE data, int count) {                                      \
                                                                              \
    if (self->capacity < self->count + count) {                               \
      int capacity = utilPowerOf2Ceil((int)self->count + count);              \
      if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;                   \
      self->data = (M__TYPE*)vmRealloc(vm, self->data,                        \
        self->capacity * sizeof(M__TYPE), capacity * sizeof(M__TYPE));        \
      self->capacity = capacity;                                              \
    }                                                                         \
                                                                              \
    for (int i = 0; i < count; i++) {                                         \
      self->data[self->count++] = data;                                       \
    }                                                                         \
  }                                                                           \
                                                                              \
  void M__NAME_L##BufferWrite(M__NAME##Buffer* self,                          \
              PKVM* vm, M__TYPE data) {                                       \
    M__NAME_L##BufferFill(self, vm, data, 1);                                 \
  }                                                                           \


DEFINE_BUFFER(Uint, uint, uint32_t)
DEFINE_BUFFER(Byte, byte, uint8_t)
DEFINE_BUFFER(Var, var, Var)
DEFINE_BUFFER(String, string, String*)
DEFINE_BUFFER(Function, function, Function*)

#undef DEFINE_BUFFER
