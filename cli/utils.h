/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include "common.h"

typedef struct {
  uint8_t* data;
  uint32_t count;
  uint32_t capacity;
} ByteBuffer;


/*****************************************************************************/
/* BYTE BUFFER                                                               */
/*****************************************************************************/

// Initialize a new buffer int instance. 
void byteBufferInit(ByteBuffer* buffer);

// Clears the allocated elementes from the VM's realloc function. 
void byteBufferClear(ByteBuffer* buffer);

// Ensure the capacity is greater than [size], if not resize. 
void byteBufferReserve(ByteBuffer* buffer, size_t size);

// Fill the buffer at the end of it with provided data if the capacity 
// isn't enough using VM's realloc function.
void byteBufferFill(ByteBuffer* buffer, uint8_t data, int count);

// Write to the buffer with provided data at the end of the buffer.
void byteBufferWrite(ByteBuffer* buffer, uint8_t data);

// Add all the characters to the buffer, byte buffer can also be used as a
// buffer to write string (like a string stream). Note that this will not
// add a null byte '\0' at the end.
void byteBufferAddString(ByteBuffer* buffer, const char* str, uint32_t length);
