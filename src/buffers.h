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


DECLARE_BUFFER(Uint, uint, uint32_t)
DECLARE_BUFFER(Byte, byte, uint8_t)
DECLARE_BUFFER(Var, var, Var)
DECLARE_BUFFER(String, string, String*)
DECLARE_BUFFER(Function, function, Function*)

// Add all the characters to the buffer, byte buffer can also be used as a
// buffer to write string (like a string stream).
void ByteBufferAddString(ByteBuffer* self, PKVM* vm, const char* str,
                         uint32_t length);

#undef DECLARE_BUFFER

#endif // BUFFERS_TEMPLATE_H
