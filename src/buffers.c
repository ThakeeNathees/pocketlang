/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "buffers.h"

#include "utils.h"
#include "vm.h"

// The buffer "template" implementation of diferent types, defined in the
// buffers.h header.

#define DEFINE_BUFFER(m_name, m_name_l, m_type)                               \
  void m_name_l##BufferInit(m_name##Buffer* self) {                           \
    self->data = NULL;                                                        \
    self->count = 0;                                                          \
    self->capacity = 0;                                                       \
  }                                                                           \
                                                                              \
  void m_name_l##BufferClear(m_name##Buffer* self,                            \
              PKVM* vm) {                                                     \
    vmRealloc(vm, self->data, self->capacity * sizeof(m_type), 0);            \
    self->data = NULL;                                                        \
    self->count = 0;                                                          \
    self->capacity = 0;                                                       \
  }                                                                           \
                                                                              \
  void m_name_l##BufferReserve(m_name##Buffer* self, PKVM* vm, size_t size) { \
    if (self->capacity < size) {                                              \
      int capacity = utilPowerOf2Ceil((int)size);                             \
      if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;                   \
      self->data = (m_type*)vmRealloc(vm, self->data,                         \
        self->capacity * sizeof(m_type), capacity * sizeof(m_type));          \
      self->capacity = capacity;                                              \
    }                                                                         \
  }                                                                           \
                                                                              \
  void m_name_l##BufferFill(m_name##Buffer* self, PKVM* vm,                   \
              m_type data, int count) {                                       \
                                                                              \
    m_name_l##BufferReserve(self, vm, self->count + count);                   \
                                                                              \
    for (int i = 0; i < count; i++) {                                         \
      self->data[self->count++] = data;                                       \
    }                                                                         \
  }                                                                           \
                                                                              \
  void m_name_l##BufferWrite(m_name##Buffer* self,                            \
              PKVM* vm, m_type data) {                                        \
    m_name_l##BufferFill(self, vm, data, 1);                                  \
  }                                                                           \

void ByteBufferAddString(ByteBuffer* self, PKVM* vm, const char* str,
                         uint32_t length) {
  byteBufferReserve(self, vm, self->count + length);
  for (uint32_t i = 0; i < length; i++) {
    self->data[self->count++] = *(str++);
  }
}

DEFINE_BUFFER(Uint, uint, uint32_t)
DEFINE_BUFFER(Byte, byte, uint8_t)
DEFINE_BUFFER(Var, var, Var)
DEFINE_BUFFER(String, string, String*)
DEFINE_BUFFER(Function, function, Function*)

#undef DEFINE_BUFFER
