/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef BUFFERS_TEMPLATE_H
#define BUFFERS_TEMPLATE_H

#include "common.h"

// The macro 'DECLARE_BUFFER()' emulate the C++ template to declare and define
// different types of buffer objects.

// A buffer of type 'T' will contain a heap allocated array of 'T' with the
// capacity of 'Tbuffer.capacity' as 'T* Tbuffer.data'. When the capacity is
// filled with 'T' values (ie. Tbuffer.count == Tbuffer.capacity) the buffer's
// internal data array will be reallocate to a capacity of 'GROW_FACTOR' times
// it's last capacity.

#define DECLARE_BUFFER(m_name, m_name_l, m_type)                              \
  typedef struct {                                                            \
    m_type* data;                                                             \
    uint32_t count;                                                           \
    uint32_t capacity;                                                        \
  } m_name##Buffer;                                                           \
                                                                              \
  /* Initialize a new buffer int instance. */                                 \
  void m_name_l##BufferInit(m_name##Buffer* self);                            \
                                                                              \
  /* Clears the allocated elementes from the VM's realloc function. */        \
  void m_name_l##BufferClear(m_name##Buffer* self, PKVM* vm);                 \
                                                                              \
  /* Ensure the capacity is greater than [size], if not resize. */            \
  void m_name_l##BufferReserve(m_name##Buffer* self, PKVM* vm, size_t size);  \
                                                                              \
  /* Fill the buffer at the end of it with provided data if the capacity */   \
  /*  isn't enough using VM's realloc function. */                            \
  void m_name_l##BufferFill(m_name##Buffer* self, PKVM* vm,                   \
                             m_type data, int count);                         \
                                                                              \
  /* Write to the buffer with provided data at the end of the buffer.*/       \
  void m_name_l##BufferWrite(m_name##Buffer* self,                            \
                              PKVM* vm, m_type data);                         \


// The buffer "template" implementation of diferent types.

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

#endif // BUFFERS_TEMPLATE_H
