/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef BUFFERS_TEMPLATE_H
#define BUFFERS_TEMPLATE_H

#include "common.h"
#include "include/pocketlang.h"

#define DECLARE_BUFFER(M__NAME, M__NAME_L, M__TYPE)                           \
  typedef struct {                                                            \
    M__TYPE* data;                                                            \
    uint32_t count;                                                           \
    uint32_t capacity;                                                        \
  } M__NAME##Buffer;                                                          \
                                                                              \
  /* Initialize a new buffer int instance. */                                 \
  void M__NAME_L##BufferInit(M__NAME##Buffer* self);                          \
                                                                              \
  /* Clears the allocated elementes from the VM's realloc function. */        \
  void M__NAME_L##BufferClear(M__NAME##Buffer* self,                          \
              PKVM* vm);                                                      \
                                                                              \
  /* Fill the buffer at the end of it with provided data if the capacity */   \
  /*  isn't enough using VM's realloc function. */                            \
  void M__NAME_L##BufferFill(M__NAME##Buffer* self, PKVM* vm,                 \
              M__TYPE data, int count);                                       \
                                                                              \
  /* Write to the buffer with provided data at the end of the buffer.*/       \
  void M__NAME_L##BufferWrite(M__NAME##Buffer* self,                          \
              PKVM* vm, M__TYPE data);                                        \


DECLARE_BUFFER(Uint, uint, uint32_t)
DECLARE_BUFFER(Byte, byte, uint8_t)
DECLARE_BUFFER(Var, var, Var)
DECLARE_BUFFER(String, string, String*)
DECLARE_BUFFER(Function, function, Function*)

#undef DECLARE_BUFFER

#endif // BUFFERS_TEMPLATE_H