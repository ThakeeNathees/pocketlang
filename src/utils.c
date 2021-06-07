/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "utils.h"

// Function implementation, see utils.h for description.
int utilPowerOf2Ceil(int n) {
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n++;

  return n;
}

// Function implementation, see utils.h for description.
bool utilIsName(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '_');
}

// Function implementation, see utils.h for description.
bool utilIsDigit(char c) {
  return ('0' <= c && c <= '9');
}

// A union to reinterpret a double as raw bits and back.
typedef union {
  uint64_t bits64;
  uint32_t bits32[2];
  double num;
} _DoubleBitsConv;

// Function implementation, see utils.h for description.
uint64_t utilDoubleToBits(double value) {
  _DoubleBitsConv bits;
  bits.num = value;
  return bits.bits64;
}

// Function implementation, see utils.h for description.
double utilDoubleFromBits(uint64_t value) {
  _DoubleBitsConv bits;
  bits.bits64 = value;
  return bits.num;
}

// Function implementation, see utils.h for description.
uint32_t utilHashBits(uint64_t hash) {
  // From v8's ComputeLongHash() which in turn cites:
  // Thomas Wang, Integer Hash Functions.
  // http://www.concentric.net/~Ttwang/tech/inthash.htm
  hash = ~hash + (hash << 18);
  hash = hash ^ (hash >> 31);
  hash = hash * 21;
  hash = hash ^ (hash >> 11);
  hash = hash + (hash << 6);
  hash = hash ^ (hash >> 22);
  return (uint32_t)(hash & 0x3fffffff);
}

// Function implementation, see utils.h for description.
uint32_t utilHashNumber(double num) {
  // Hash the raw bits of the value.
  return utilHashBits(utilDoubleToBits(num));
}

// Function implementation, see utils.h for description.
uint32_t utilHashString(const char* string) {
  // FNV-1a hash. See: http://www.isthe.com/chongo/tech/comp/fnv/

#define FNV_prime_32_bit 16777619u
#define FNV_offset_basis_32_bit 2166136261u

  uint32_t hash = FNV_offset_basis_32_bit;

  for (const char* c = string; *c != '\0'; c++) {
    hash ^= *c;
    hash *= FNV_prime_32_bit;
  }

  return hash;

#undef FNV_prime_32_bit
#undef FNV_offset_basis_32_bit
}

/****************************************************************************
 * UTF8                                                                     *
 ****************************************************************************/

 // Function implementation, see utils.h for description.
int utf8_encodeBytesCount(int value) {
  if (value <= 0x7f) return 1;
  if (value <= 0x7ff) return 2;
  if (value <= 0xffff) return 3;
  if (value <= 0x10ffff) return 4;

  // if we're here means it's an invalid leading byte
  return 0;
}

// Function implementation, see utils.h for description.
int utf8_decodeBytesCount(uint8_t byte) {

  if ((byte >> 7) == 0b0) return 1;
  if ((byte >> 6) == 0b10) return 1; //< continuation byte
  if ((byte >> 5) == 0b110) return 2;
  if ((byte >> 4) == 0b1110) return 3;
  if ((byte >> 3) == 0b11110) return 4;

  // if we're here means it's an invalid utf8 byte
  return 1;
}

// Function implementation, see utils.h for description.
int utf8_encodeValue(int value, uint8_t* bytes) {

  if (value <= 0x7f) {
    *bytes = value & 0x7f;
    return 1;
  }

  // 2 byte character 110xxxxx 10xxxxxx -> last 6 bits write to 2nd byte and
  // first 5 bit write to first byte
  if (value <= 0x7ff) {
    *(bytes++) = (uint8_t)(0b11000000 | ((value & 0b11111000000) >> 6));
    *(bytes) = (uint8_t)(0b10000000 | ((value & 111111)));
    return 2;
  }

  // 3 byte character 1110xxxx 10xxxxxx 10xxxxxx -> from last, 6 bits write
  // to  3rd byte, next 6 bits write to 2nd byte, and 4 bits to first byte.
  if (value <= 0xffff) {
    *(bytes++) = (uint8_t)(0b11100000 | ((value & 0b1111000000000000) >> 12));
    *(bytes++) = (uint8_t)(0b10000000 | ((value & 0b111111000000) >> 6));
    *(bytes) =   (uint8_t)(0b10000000 | ((value & 0b111111)));
    return 3;
  }

  // 4 byte character 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx -> last 6 bits to
  // to 4th byte, next 6 bits to 3rd byte, next 6 bits to 2nd byte, 3 bits
  // first byte.
  if (value <= 0x10ffff) {
    *(bytes++) = (uint8_t)(0b11110000 | ((value & 0b111000000000000000000) >> 18));
    *(bytes++) = (uint8_t)(0b10000000 | ((value & 0b111111000000000000) >> 12));
    *(bytes++) = (uint8_t)(0b10000000 | ((value & 0b111111000000) >> 6));
    *(bytes)   = (uint8_t)(0b10000000 | ((value & 0b111111)));
    return 4;
  }

  return 0;
}

// Function implementation, see utils.h for description.
int utf8_decodeBytes(uint8_t* bytes, int* value) {

  int continue_bytes = 0;
  int byte_count = 1;
  int _value = 0;

  if ((*bytes & 0b11000000) == 0b10000000) {
    *value = *bytes;
    return byte_count;
  }

  else if ((*bytes & 0b11100000) == 0b11000000) {
    continue_bytes = 1;
    _value = (*bytes & 0b11111);
  }

  else if ((*bytes & 0b11110000) == 0b11100000) {
    continue_bytes = 2;
    _value = (*bytes & 0b1111);
  }

  else if ((*bytes & 0b11111000) == 0b11110000) {
    continue_bytes = 3;
    _value = (*bytes & 0b111);
  }

  else {
    // Invalid leading byte
    return -1;
  }

  // now add the continuation bytes to the _value
  while (continue_bytes--) {
    bytes++, byte_count++;

    if ((*bytes & 0b11000000) != 0b10000000) return -1;

    _value = (_value << 6) | (*bytes & 0b00111111);
  }

  *value = _value;
  return byte_count;
}
