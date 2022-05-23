/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_BUFFERS_TEMPLATE_H
#define PK_BUFFERS_TEMPLATE_H

#ifndef PK_AMALGAMATED
#include "internal.h"
#endif

// The macro 'DECLARE_BUFFER()' emulate the C++ template to declare and define
// different types of buffer objects.

// A buffer of type 'T' will contain a heap allocated array of 'T' with the
// capacity of 'Tbuffer.capacity' as 'T* Tbuffer.data'. When the capacity is
// filled with 'T' values (ie. Tbuffer.count == Tbuffer.capacity) the buffer's
// internal data array will be reallocate to a capacity of 'GROW_FACTOR' times
// it's last capacity.

#define DECLARE_BUFFER(m_name, m_type)                                        \
  typedef struct {                                                            \
    m_type* data;                                                             \
    uint32_t count;                                                           \
    uint32_t capacity;                                                        \
  } pk##m_name##Buffer;                                                       \
                                                                              \
  /* Initialize a new buffer int instance. */                                 \
  void pk##m_name##BufferInit(pk##m_name##Buffer* self);                      \
                                                                              \
  /* Clears the allocated elements from the VM's realloc function. */         \
  void pk##m_name##BufferClear(pk##m_name##Buffer* self, PKVM* vm);           \
                                                                              \
  /* Ensure the capacity is greater than [size], if not resize. */            \
  void pk##m_name##BufferReserve(pk##m_name##Buffer* self, PKVM* vm,          \
                                 size_t size);                                \
                                                                              \
  /* Fill the buffer at the end of it with provided data if the capacity */   \
  /* isn't enough using VM's realloc function. */                             \
  void pk##m_name##BufferFill(pk##m_name##Buffer* self, PKVM* vm,             \
                              m_type data, int count);                        \
                                                                              \
  /* Write to the buffer with provided data at the end of the buffer.*/       \
  void pk##m_name##BufferWrite(pk##m_name##Buffer* self,                      \
                               PKVM* vm, m_type data);                        \
                                                                              \
  /* Concatenate the contents of another buffer at the end of this buffer.*/  \
  void pk##m_name##BufferConcat(pk##m_name##Buffer* self, PKVM* vm,           \
                                pk##m_name##Buffer* other);                   \

// The buffer "template" implementation of different types.
#define DEFINE_BUFFER(m_name, m_type)                                         \
  void pk##m_name##BufferInit(pk##m_name##Buffer* self) {                     \
    self->data = NULL;                                                        \
    self->count = 0;                                                          \
    self->capacity = 0;                                                       \
  }                                                                           \
                                                                              \
  void pk##m_name##BufferClear(pk##m_name##Buffer* self,                      \
              PKVM* vm) {                                                     \
    vmRealloc(vm, self->data, self->capacity * sizeof(m_type), 0);            \
    self->data = NULL;                                                        \
    self->count = 0;                                                          \
    self->capacity = 0;                                                       \
  }                                                                           \
                                                                              \
  void pk##m_name##BufferReserve(pk##m_name##Buffer* self, PKVM* vm,          \
                                 size_t size) {                               \
    if (self->capacity < size) {                                              \
      int capacity = utilPowerOf2Ceil((int)size);                             \
      if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;                   \
      self->data = (m_type*) vmRealloc(vm, self->data,                        \
        self->capacity * sizeof(m_type), capacity * sizeof(m_type));          \
      self->capacity = capacity;                                              \
    }                                                                         \
  }                                                                           \
                                                                              \
  void pk##m_name##BufferFill(pk##m_name##Buffer* self, PKVM* vm,             \
                              m_type data, int count) {                       \
                                                                              \
    pk##m_name##BufferReserve(self, vm, self->count + count);                 \
                                                                              \
    for (int i = 0; i < count; i++) {                                         \
      self->data[self->count++] = data;                                       \
    }                                                                         \
  }                                                                           \
                                                                              \
  void pk##m_name##BufferWrite(pk##m_name##Buffer* self,                      \
                               PKVM* vm, m_type data) {                       \
    pk##m_name##BufferFill(self, vm, data, 1);                                \
  }                                                                           \
                                                                              \
  void pk##m_name##BufferConcat(pk##m_name##Buffer* self, PKVM* vm,           \
                                pk##m_name##Buffer* other) {                  \
    pk##m_name##BufferReserve(self, vm, self->count + other->count);          \
                                                                              \
    memcpy(self->data + self->count,                                          \
           other->data,                                                       \
           other->count * sizeof(m_type));                                    \
    self->count += other->count;                                              \
  }

#endif // PK_BUFFERS_TEMPLATE_H
