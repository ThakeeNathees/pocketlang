/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_UTILS_H
#define PK_UTILS_H

#include <stdbool.h>
#include <stdint.h>

// Returns the smallest power of two that is equal to or greater than [n].
// Source : https://github.com/wren-lang/wren/blob/main/src/vm/wren_utils.h#L119
int utilPowerOf2Ceil(int n);

// Returns true if c in [ ' ', '\t', '\n', '\v' ]
bool utilIsSpace(char c);

// Returns true if `c` is [A-Za-z_].
bool utilIsName(char c);

// Returns true if `c` is [0-9].
bool utilIsDigit(char c);

// Returns true if character is a hex digit.
// ie [a-zA-Z0-9].
bool utilIsCharHex(char c);

// Returns the decimal value of the hex digit.
// char should match [a-zA-Z0-9].
uint8_t utilCharHexVal(char c);

// Returns the values hex digit. The value must be 0x0 <= val < 0xf
char utilHexDigit(uint8_t value, bool uppercase);

// Return Reinterpreted bits of the double value.
uint64_t utilDoubleToBits(double value);

// Reinterpret and return double value from bits.
double utilDoubleFromBits(uint64_t value);

// Copied from wren-lang.
uint32_t utilHashBits(uint64_t hash);

// Generates a hash code for [num].
uint32_t utilHashNumber(double num);

// Generate a has code for [string].
uint32_t utilHashString(const char* string);

// Convert the string to number. On success it'll return NULL and set the
// [num] value. Otherwise it'll return a C literal string containing the error
// message.
const char* utilToNumber(const char* str, double* num);

/****************************************************************************
 * UTF8                                                                     *
 ****************************************************************************/

/** @file
 * A tiny UTF-8 utility library.
 *
 *
 * Utf-8 is an elegant character encoding which I just love it's simplicity,
 * and compatibility It's just a wonderful hack of all time. A single byte
 * length utf-8 character is the same as an ASCII character. In case if you
 * don't know about ASCII encoding it's just how a character is represented in
 * a single byte. For an example the character 'A' is 01000001, 'B' is 01000010
 * and so on. The first bit in is always 0 called parity bit, it's a way to
 * check if some of the bits have flipped by noise back in the old age of
 * computers. Parity bit should be equal to the sum of the rest of the bits mod
 * 2. So we have 7 bits to represent ASCII which is 127 different characters.
 * But utf-8 can potentially encode 2,164,864 characters.
 *
 * The length of a utf-8 character would vary from 1 to 4. If it's a single
 * byte character, it's starts with a 0 and rest of the 7 bytes have the
 * value. It's not just like ASCII, it is ASCII (compatable). For the 2 bytes
 * character the first byte starts with 110....., for the 3 bytes character
 * it's starts with 1110.... and for the 4 byte it's 11110... The first byte
 * is called the leading byte and the rest of the bytes of the character is
 * called continuation bytes.
 *
 * example:
 *                  v-- leading byte   v-- continuation byte => 2 bytes
 *             é =  11000011           10101001
 *                  ^^^                ^^
 *                  110 means 2 bytes  10 means continuation
 *
 * (note that the character é is 8 bit long with ANSI encoding)
 *
*/

// Returns the number of bytes the the [value] would take to encode. returns 0
// if the value is invalid utf8 representation.
//
// <pre>
// For single byte character, represented as 0xxxxxxx
// the payload is 7 bytes so the maximum value would be 0x7f
//
// For 2 bytes characters, represented as 110xxxxx 10xxxxxx
// the payload is 11 bits               | xxx xxxx xxxx |
// so the maximum value would be 0x7ff  |  7   f    f   |
//
// For 3 bytes character, represented as 1110xxxx 10xxxxxx 10xxxxxx
// the payload is 16 bits               | xxxx xxxx xxxx xxxx |
// so the maximum value would be 0xffff | f    f    f    f    |
//
// For 4 bytes character, represented as 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
// the payload is 21 bits                     | x xxxx xxxx xxxx xxxx xxxx |
// so the maximum value *SHOULD* be 0x1fffff  | 1 f    f    f    f    f    |
// but in RFC3629 §3 (https://tools.ietf.org/html/rfc3629#section-3) UTF-8 is
// limited to 0x10FFFF to match the limits of UTF-16.
// </pre>
int utf8_encodeBytesCount(int value);

// Returns the number of bytes the the leading [byte] contains. returns 1 if
// the byte is an invalid utf8 leading byte (to skip pass to the next byte).
int utf8_decodeBytesCount(uint8_t byte);

// Encodes the 32 bit value into a byte array which should be a size of 4 and
// returns the number of bytes the value encoded (if invalid returns 0, that
// how many it write to the buffer.
int utf8_encodeValue(int value, uint8_t* bytes);

// Decodes from the leading [byte] and write the value to param [value] and
// returns the number of bytes the value decoded, if invalid write -1 to the
// value.
int utf8_decodeBytes(uint8_t* bytes, int* value);

#endif // PK_UTILS_H
