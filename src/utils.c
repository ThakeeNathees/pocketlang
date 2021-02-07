/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "utils.h"

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

bool utilIsName(char c) {
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '_');
}

bool utilIsDigit(char c) {
	return ('0' <= c && c <= '9');
}

/****************************************************************************
 * UTF8                                                                     *
 ****************************************************************************/

#define B1(first) 0b##first
#define B2(first, last) 0b##first##last
#define B3(first, second, last) 0b##first##second##last
#define B4(first, second, third, last) 0b##first##second##third##last

int utf8_encodeBytesCount(int value) {
	if (value <= 0x7f) return 1;
	if (value <= 0x7ff) return 2;
	if (value <= 0xffff) return 3;
	if (value <= 0x10ffff) return 4;

	// if we're here means it's an invalid leading byte
	return 0;
}

int utf8_decodeBytesCount(uint8_t byte) {

	if ((byte >> 7) == 0b0) return 1;
	if ((byte >> 6) == 0b10) return 1; //< continuation byte
	if ((byte >> 5) == 0b110) return 2;
	if ((byte >> 4) == 0b1110) return 3;
	if ((byte >> 3) == 0b11110) return 4;

	// if we're here means it's an invalid utf8 byte
	return 1;
}

int utf8_encodeValue(int value, uint8_t* bytes) {

	if (value <= 0x7f) {
		*bytes = value & 0x7f;
		return 1;
	}

	// 2 byte character 110xxxxx 10xxxxxx -> last 6 bits write to 2nd byte and
	// first 5 bit write to first byte
	if (value <= 0x7ff) {
		*(bytes++) = B2(110, 00000) | ((value & B2(11111, 000000)) >> 6);
		*(bytes) = B2(10, 000000) | ((value & B1(111111)));
		return 2;
	}

	// 3 byte character 1110xxxx 10xxxxxx 10xxxxxx -> from last, 6 bits write
	// to  3rd byte, next 6 bits write to 2nd byte, and 4 bits to first byte.
	if (value <= 0xffff) {
		*(bytes++) = B2(1110, 0000) | ((value & B3(1111, 000000, 000000)) >> 12);
		*(bytes++) = B2(10, 000000) | ((value & B2(111111, 000000)) >> 6);
		*(bytes) = B2(10, 000000) | ((value & B1(111111)));
		return 3;
	}

	// 4 byte character 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx -> last 6 bits to
	// to 4th byte, next 6 bits to 3rd byte, next 6 bits to 2nd byte, 3 bits
	// first byte.
	if (value <= 0x10ffff) {
		*(bytes++) = B2(11110, 000) | ((value & B4(111, 000000, 000000, 000000)) >> 18);
		*(bytes++) = B2(10, 000000) | ((value & B3(111111, 000000, 000000)) >> 12);
		*(bytes++) = B2(10, 000000) | ((value & B2(111111, 000000)) >> 6);
		*(bytes) = B2(10, 000000) | ((value & B1(111111)));
		return 4;
	}

	return 0;
}

int utf8_decodeBytes(uint8_t* bytes, int* value) {

	int continue_bytes = 0;
	int byte_count = 1;
	int _value = 0;

	if ((*bytes & B2(11, 000000)) == B2(10, 000000)) {
		*value = *bytes;
		return byte_count;
	}

	else if ((*bytes & B2(111, 00000)) == B2(110, 00000)) {
		continue_bytes = 1;
		_value = (*bytes & B1(11111));
	}

	else if ((*bytes & B2(1111, 0000)) == B2(1110, 0000)) {
		continue_bytes = 2;
		_value = (*bytes & B1(1111));
	}

	else if ((*bytes & B2(11111, 000)) == B2(11110, 000)) {
		continue_bytes = 3;
		_value = (*bytes & B1(111));
	}

	else {
		// Invalid leading byte
		return -1;
	}

	// now add the continuation bytes to the _value
	while (continue_bytes--) {
		bytes++, byte_count++;

		if ((*bytes & B2(11, 000000)) != B2(10, 000000)) return -1;

		_value = (_value << 6) | (*bytes & B2(00, 111111));
	}

	*value = _value;
	return byte_count;
}

#undef B1
#undef B2
#undef B3
#undef B4
