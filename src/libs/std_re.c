/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_AMALGAMATED
#include "libs.h"
#include "../core/value.h"
#include "../core/vm.h"
#endif

// https://github.com/cesanta/slre
#include "thirdparty/slre/slre.h"
#include "thirdparty/slre/slre.c"

typedef struct slre_cap capture;

#define MAX_CAPTURES 256
#define RE_IGNORECASE SLRE_IGNORE_CASE // 1
#define RE_GLOBAL 2

static void _re_error(PKVM* vm, int result) {
  switch (result) {
    case SLRE_UNEXPECTED_QUANTIFIER:
      pkSetRuntimeError(vm, "unexpected quantifier"); break;
    case SLRE_UNBALANCED_BRACKETS:
      pkSetRuntimeError(vm, "unbalanced brackets"); break;
    case SLRE_INTERNAL_ERROR:
      pkSetRuntimeError(vm, "internal error"); break;
    case SLRE_INVALID_CHARACTER_SET:
      pkSetRuntimeError(vm, "invalid character_set"); break;
    case SLRE_INVALID_METACHARACTER:
      pkSetRuntimeError(vm, "invalid metacharacter"); break;
    case SLRE_CAPS_ARRAY_TOO_SMALL:
      pkSetRuntimeError(vm, "caps array too small"); break;
    case SLRE_TOO_MANY_BRANCHES:
      pkSetRuntimeError(vm, "too many branches"); break;
    case SLRE_TOO_MANY_BRACKETS:
      pkSetRuntimeError(vm, "too many brackets"); break;
  }
}

static bool _re_init(PKVM* vm, const char** pattern, const char** input,
    uint32_t* pattern_len, uint32_t* input_len, capture** caps) {

  if (!pkValidateSlotString(vm, 1, pattern, pattern_len)) return false;
  if (!pkValidateSlotString(vm, 2, input, input_len)) return false;

  // let result[0] be entire match
  if (*pattern_len != 0 && *pattern[0] == '^') {
    pkSetSlotStringFmt(vm, 1, "^(%.*s)", *pattern_len - 1, *pattern + 1);
  } else {
    pkSetSlotStringFmt(vm, 1, "(%.*s)", *pattern_len, *pattern);
  }
  if (!pkValidateSlotString(vm, 1, pattern, pattern_len)) return false;

  *caps = (capture*) pkRealloc(vm, NULL, sizeof(capture) * MAX_CAPTURES);
  ASSERT(*caps != NULL, "pkRealloc failed.");
  memset(*caps, 0, sizeof(capture) * MAX_CAPTURES);
  return true;
}

static void _re_exit(PKVM* vm, capture* caps) {
  pkRealloc(vm, caps, 0);
}

DEF(_reMatch,
  "re.match(pattern:String, input:String[, flag:Number]) -> List",
  "Perform a regular expression match and return a list of matches. Supported pattern:\n"
  "  ^       Match beginning of a buffer\n"
  "  $       Match end of a buffer\n"
  "  ()      Grouping and substring capturing\n"
  "  \\s      Match whitespace\n"
  "  \\S      Match non-whitespace\n"
  "  \\w      Match alphanumeric [a-zA-Z0-9_]\n"
  "  \\W      Match non-alphanumeric\n"
  "  \\d      Match decimal digit\n"
  "  \\D      Match non-decimal digit\n"
  "  \\n      Match new line character\n"
  "  \\r      Match line feed character\n"
  "  \\f      Match form feed character\n"
  "  \\v      Match vertical tab character\n"
  "  \\t      Match horizontal tab character\n"
  "  \\b      Match backspace character\n"
  "  +       Match one or more times (greedy)\n"
  "  +?      Match one or more times (non-greedy)\n"
  "  *       Match zero or more times (greedy)\n"
  "  *?      Match zero or more times (non-greedy)\n"
  "  ?       Match zero or once (greedy)\n"
  "  x|y     Match x or y (alternation operator)\n"
  "  \\meta   Match one of the meta character: ^$().[]*+?|\\\n"
  "  \\xHH    Match byte with hex value 0xHH, e.g. \\x4a\n"
  "  [...]   Match any character from set. Ranges like [a-z] are supported\n"
  "  [^...]  Match any character but ones from set\n\n"
  "Flags:\n"
  "  re.I, re.IGNORECASE    Perform case-insensitive matching\n"
  "  re.G, re.GLOBAL        Perform global matching"
  ) {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 2, 3)) return;

  int32_t flags = 0;
  if (argc == 3) {
    if (!pkValidateSlotInteger(vm, 3, &flags)) return;
  }

  const char *pattern, *input;
  uint32_t pattern_len, input_len;
  capture* caps;
  if (!_re_init(vm, &pattern, &input, &pattern_len, &input_len, &caps)) return;

  pkReserveSlots(vm, 5);
  pkNewList(vm, 0);

  int i = 0;
  while (input_len > 0) {
    i = slre_match(pattern, input, input_len, caps,
      MAX_CAPTURES, flags & SLRE_IGNORE_CASE);

    if (i < 0 || i > input_len) break;
    input += i;
    input_len -= i;

    for(int j = 0; j < MAX_CAPTURES && caps[j].ptr != NULL; j++) {
      pkSetSlotStringLength(vm, 4, caps[j].ptr, caps[j].len);
      pkListInsert(vm, 0, -1, 4);
      caps[j].ptr = NULL; // reset the ptr after the string is stored
    }

    if ((flags & RE_GLOBAL) != RE_GLOBAL) break;
  }
  _re_error(vm, i);
  _re_exit(vm, caps);
}

DEF(_reTest,
  "re.test(pattern:String, input:String[, flag:Number]) -> Bool",
  "Perform a regular expression match and return true of false.\n"
  "Run help(re.match) for details."
  ) {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 2, 3)) return;

  int32_t flags = 0;
  if (argc == 3) {
    if (!pkValidateSlotInteger(vm, 3, &flags)) return;
  }

  const char *pattern, *input;
  uint32_t pattern_len, input_len;
  capture* caps;
  if (!_re_init(vm, &pattern, &input, &pattern_len, &input_len, &caps)) return;

  int i = slre_match(pattern, input, input_len, caps,
    MAX_CAPTURES, flags & SLRE_IGNORE_CASE);

  pkSetSlotBool(vm, 0, !(i < 0 || i > input_len));

  _re_error(vm, i);
  _re_exit(vm, caps);
}

DEF(_reReplace,
  "re.replace(pattern:String, input:String, by:String[, count:Number]) -> String",
  "Perform a regular expression search and replace and return a new string.\n"
  "Run help(re.match) for details."
  ) {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 3, 4)) return;

  int32_t count = -1;
  if (argc == 4) {
    if (!pkValidateSlotInteger(vm, 4, &count)) return;
  }

  const char *pattern, *input, *by;
  uint32_t pattern_len, input_len, by_len;
  capture* caps;
  if (!_re_init(vm, &pattern, &input, &pattern_len, &input_len, &caps)) return;
  if (!pkValidateSlotString(vm, 3, &by, &by_len)) return;

  pkByteBuffer buff;
  pkByteBufferInit(&buff);

  int i = 0;
  while (input_len > 0 && count != 0) {
    i = slre_match(pattern, input, input_len, caps,
      MAX_CAPTURES, 0);

    if (i < 0 || i > input_len) break;

    if (caps[0].ptr - input != 0) {
      pkByteBufferAddString(&buff, vm, input, caps[0].ptr - input);
    }
    pkByteBufferAddString(&buff, vm, by, by_len);

    input += i;
    input_len -= i;

    for(int j = 0; j < MAX_CAPTURES && caps[j].ptr != NULL; j++) {
      caps[j].ptr = NULL; // reset the ptr after the string is stored
    }

    if (count > 0) count--;
  }
  if (input_len != 0) {
    pkByteBufferAddString(&buff, vm, input, input_len);
  }

  String* str = newStringLength(vm, (const char*)buff.data, buff.count);
  pkByteBufferClear(&buff, vm);
  vm->fiber->ret[0] = VAR_OBJ(str);

  _re_error(vm, i);
  _re_exit(vm, caps);
}

DEF(_reSplit,
  "re.split(pattern:String, input:String[, count:Number]) -> List",
  "Split string by a regular expression.\n"
  "Run help(re.match) for details."
  ) {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 2, 3)) return;

  int32_t count = -1;
  if (argc == 3) {
    if (!pkValidateSlotInteger(vm, 3, &count)) return;
  }

  const char *pattern, *input, *by;
  uint32_t pattern_len, input_len, by_len;
  capture* caps;
  if (!_re_init(vm, &pattern, &input, &pattern_len, &input_len, &caps)) return;

  pkReserveSlots(vm, 5);
  pkNewList(vm, 0);

  int i = 0;
  while (input_len > 0 && count != 0) {
    i = slre_match(pattern, input, input_len, caps,
      MAX_CAPTURES, 0);

    if (i < 0 || i > input_len) break;

    pkSetSlotStringLength(vm, 4, input, caps[0].ptr - input);
    pkListInsert(vm, 0, -1, 4);

    input += i;
    input_len -= i;

    for(int j = 0; j < MAX_CAPTURES && caps[j].ptr != NULL; j++) {
      caps[j].ptr = NULL; // reset the ptr after the string is stored
    }

    if (count > 0) count--;
  }
  pkSetSlotStringLength(vm, 4, input, input_len);
  pkListInsert(vm, 0, -1, 4);

  _re_error(vm, i);
  _re_exit(vm, caps);
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleRe(PKVM* vm) {
  PkHandle* re = pkNewModule(vm, "re");

  pkReserveSlots(vm, 2);
  pkSetSlotHandle(vm, 0, re);
  pkSetSlotNumber(vm, 1, RE_IGNORECASE);
  pkSetAttribute(vm, 0, "I", 1);
  pkSetAttribute(vm, 0, "IGNORECASE", 1);
  pkSetSlotNumber(vm, 1, RE_GLOBAL);
  pkSetAttribute(vm, 0, "G", 1);
  pkSetAttribute(vm, 0, "GLOBAL", 1);

  REGISTER_FN(re, "match", _reMatch, -1);
  REGISTER_FN(re, "test", _reTest, -1);
  REGISTER_FN(re, "replace", _reReplace, -1);
  REGISTER_FN(re, "split", _reSplit, -1);

  pkRegisterModule(vm, re);
  pkReleaseHandle(vm, re);
}
