/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include "utils.h"

// The factor by which a buffer will grow when it's capacity reached.
#define GROW_FACTOR 2

// The initial minimum capacity of a buffer to allocate.
#define MIN_CAPACITY 8

static inline int powerOf2Ceil(int n) {
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n++;
  return n;
}

 /*****************************************************************************/
 /* BYTE BUFFER IMPLEMENTATION                                                */
 /*****************************************************************************/

void byteBufferInit(ByteBuffer* buffer) {
  buffer->data = NULL;
  buffer->count = 0;
  buffer->capacity = 0;
}

void byteBufferClear(ByteBuffer* buffer) {
  free(buffer->data);
  buffer->data = NULL;
  buffer->count = 0;
  buffer->capacity = 0;
}

void byteBufferReserve(ByteBuffer* buffer, size_t size) {
  if (buffer->capacity < size) {
    int capacity = powerOf2Ceil((int)size);
    if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;
    buffer->data = (uint8_t*)realloc(buffer->data, capacity * sizeof(uint8_t));
    buffer->capacity = capacity;
  }
}

void byteBufferFill(ByteBuffer* buffer, uint8_t data, int count) {
  byteBufferReserve(buffer, buffer->count + count);
  for (int i = 0; i < count; i++) {
    buffer->data[buffer->count++] = data;
  }
}

void byteBufferWrite(ByteBuffer* buffer, uint8_t data) {
  byteBufferFill(buffer, data, 1);
}

void byteBufferAddString(ByteBuffer* buffer, const char* str,
                         uint32_t length) {
  byteBufferReserve(buffer, buffer->count + length);
  for (uint32_t i = 0; i < length; i++) {
    buffer->data[buffer->count++] = *(str++);
  }
}
