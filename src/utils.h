/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef UTILS_H
#define UTILS_H

#include "common.h"

// Returns the smallest power of two that is equal to or greater than [n].
// Copyied from : https://github.com/wren-lang/wren/blob/main/src/vm/wren_utils.h#L119
// Reference    : http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2Float
int utilPowerOf2Ceil(int n);

// Returns true if `c` is [A-Za-z_].
bool utilIsName(char c);

// Returns true if `c` is [0-9].
bool utilIsDigit(char c);

#endif // UTILS_H


/****************************************************************************
 * UTF8                                                                     *
 ****************************************************************************/


#ifndef UTF8_H
#define UTF8_H

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
 * check if  some of the bits have flipped by noice back in the old age of
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
 * <pre>
 * example:
 *                  v-- leading byte   v-- continuation byte => 2 bytes
 *             é =  11000011           10101001
 *                  ^^^                ^^
 *                  110 means 2 bytes  10 means continuation
 *
 * (note that the character é is 8 bit long with ANSI encoding)
 * </pre>
 *
 * USAGE:
 *     // define imlpementation only a single *.c source file like this
 *     #define UTF8_IMPLEMENT
 *     #include "utf8.h"
*/

#include <stdint.h>

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


#endif // UTF8_H