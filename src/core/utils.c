/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_AMALGAMATED
#include "utils.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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

bool utilIsSpace(char c) {
  return (c == ' ' || c == '\t' || c == '\n' || c == '\v');
}

// Function implementation, see utils.h for description.
bool utilIsName(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '_');
}

// Function implementation, see utils.h for description.
bool utilIsDigit(char c) {
  return ('0' <= c && c <= '9');
}

#define _BETWEEN(a, c, b) (((a) <= (c)) && ((c) <= (b)))
bool utilIsCharHex(char c) {
  return (_BETWEEN('0', c, '9')
    || _BETWEEN('a', c, 'z')
    || _BETWEEN('A', c, 'Z'));
}

uint8_t utilCharHexVal(char c) {
  assert(utilIsCharHex(c));

  if (_BETWEEN('0', c, '9')) {
    return c - '0';
  } else if (_BETWEEN('a', c, 'z')) {
    return c - 'a' + 10;
  } else if (_BETWEEN('A', c, 'Z')) {
    return c - 'A' + 10;
  }

  assert(false); // Unreachable.
  return 0;
}

char utilHexDigit(uint8_t value, bool uppercase) {
  assert(_BETWEEN(0x0, value, 0xf));

  if (_BETWEEN(0, value, 9)) return '0' + value;

  return (uppercase)
    ? 'A' + (value - 10)
    : 'a' + (value - 10);

}
#undef _BETWEEN

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

const char* utilToNumber(const char* str, double* num) {
#define IS_HEX_CHAR(c)            \
  (('0' <= (c) && (c) <= '9')  || \
   ('a' <= (c) && (c) <= 'f'))

#define IS_BIN_CHAR(c) (((c) == '0') || ((c) == '1'))
#define IS_DIGIT(c) (('0' <= (c)) && ((c) <= '9'))
#define INVALID_NUMERIC_STRING "Invalid numeric string."

  assert((str != NULL) && (num != NULL));
  uint32_t length = (uint32_t) strlen(str);

  // Consume the sign.
  double sign = +1;
  if (*str == '-') {
    sign = -1;
    str++;
  } else if (*str == '+') {
    str++;
  }

  // Binary String.
  if (length >= 3 &&
      ((strncmp(str, "0b", 2) == 0) || (strncmp(str, "0B", 2) == 0))) {

    uint64_t bin = 0;
    const char* c = str + 2;

    if (*c == '\0') return INVALID_NUMERIC_STRING;

    do {
      if (*c == '\0') {
        *num = sign * bin;
        return NULL;
      };

      if (!IS_BIN_CHAR(*c)) return INVALID_NUMERIC_STRING;
      if ((c - str) > (68 /*STR_BIN_BUFF_SIZE*/ - 2)) { // -2 for '-, \0'.
        return "Binary literal is too long.";
      }

      bin = (bin << 1) | (*c - '0');
      c++;

    } while (true);
  }

  // Hex String.
  if (length >= 3 &&
      ((strncmp(str, "0x", 2) == 0) || (strncmp(str, "0X", 2) == 0))) {

    uint64_t hex = 0;
    const char* c = str + 2;

    if (*c == '\0') return INVALID_NUMERIC_STRING;

    do {
      if (*c == '\0') {
        *num = sign * hex;
        return NULL;
      };

      if (!IS_HEX_CHAR(*c)) return INVALID_NUMERIC_STRING;
      if ((c - str) > (20 /*STR_HEX_BUFF_SIZE*/ - 2)) { // -2 for '-, \0'.
        return "Hex literal is too long.";
      }

      uint8_t digit = ('0' <= *c && *c <= '9')
        ? (uint8_t)(*c - '0')
        : (uint8_t)((*c - 'a') + 10);

      hex = (hex << 4) | digit;
      c++;

    } while (true);
  }

  // Regular number.
  if (*str == '\0') return INVALID_NUMERIC_STRING;

  const char* c = str;
  do {

    while (IS_DIGIT(*c)) c++;
    if (*c == '.') { // TODO: allowing "1." as a valid float.?
      c++;
      while (IS_DIGIT(*c)) c++;
    }

    if (*c == '\0') break; // Done.

    if (*c == 'e' || *c == 'E') {
      c++;
      if (*c == '+' || *c == '-') c++;

      if (!IS_DIGIT(*c)) return INVALID_NUMERIC_STRING;
      while (IS_DIGIT(*c)) c++;
      if (*c != '\0') return INVALID_NUMERIC_STRING;
    }

    if (*c != '\0') return INVALID_NUMERIC_STRING;

  } while (false);

  errno = 0;
  *num = atof(str) * sign;
  if (errno == ERANGE) return "Numeric string is too long.";

  return NULL;

#undef INVALID_NUMERIC_STRING
#undef IS_DIGIT
#undef IS_HEX_CHAR
#undef IS_BIN_CHAR
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
    *(bytes++) = (uint8_t)(0b11110000 | ((value & (0b111    << 18)) >> 18));
    *(bytes++) = (uint8_t)(0b10000000 | ((value & (0b111111 << 12)) >> 12));
    *(bytes++) = (uint8_t)(0b10000000 | ((value & (0b111111 << 6))  >> 6));
    *(bytes)   = (uint8_t)(0b10000000 | ((value &  0b111111)));
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
