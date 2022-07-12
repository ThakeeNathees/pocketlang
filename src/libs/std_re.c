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

#include "thirdparty/pikevm/re.c"

#define RE_IGNORECASE 1
#define RE_GLOBAL 2

static RE* _re_init(PKVM* vm, const char** input, uint32_t* len, 
    int flag_argc, int* global) {

  const char *pattern; uint32_t pattern_len;
  if (!pkValidateSlotString(vm, 1, &pattern, &pattern_len)) return NULL;
  if (!pkValidateSlotString(vm, 2, input, len)) return NULL;

  int32_t flags = 0;
  if (pkGetArgc(vm) >= flag_argc) {
    if (!pkValidateSlotInteger(vm, flag_argc, &flags)) return NULL;
  }

  if (global) *global = (flags & RE_GLOBAL) == RE_GLOBAL;
  int insensitive = (flags & RE_IGNORECASE) == RE_IGNORECASE;

  RE* re = re_compile(pattern, insensitive);
  if (!re) {
    pkSetRuntimeError(vm, "Cannot compile the regex pattern.");
    return NULL;
  }

  return re;
}

static void _re_match(PKVM* vm, RE* re, const char *input, uint32_t len, 
    bool global, bool range) {

  pkNewList(vm, 0);
  const char *ptr = input;
  do {
    const char** matches = re_match(re, ptr);
    if (!matches) break;

    for (int i = 0; i < re_max_matches(re); i += 2) {
      if (matches[i] && matches[i + 1]) {
        if (range) pkNewRange(vm, 1, matches[i] - input, matches[i + 1] - input);
        else  pkSetSlotStringLength(vm, 1, matches[i], matches[i + 1] - matches[i]);
        pkListInsert(vm, 0, -1, 1);
      } else {
        if (range) pkSetSlotNull(vm, 1);
        else pkSetSlotStringLength(vm, 1, ptr, 0);
        pkListInsert(vm, 0, -1, 1);
      }
    }
    if (ptr == matches[1]) break; // cannot advance

    ptr = matches[1]; // point to last matched char
  } while (global && ptr < input + len);  
}

DEF(_reMatch,
  "re.match(pattern:String, input:String[, flag:Number]) -> List",
  "Perform a regular expression match and return a list of matches.\n\n"
  "Supported patterns:\n"
  "  ^        Match beginning of a buffer\n"
  "  $        Match end of a buffer\n"
  "  (...)    Grouping and substring capturing\n"
  "  (?:...)  Non-capture grouping\n"
  "  \\s       Match whitespace [ \\t\\n\\r\\f\\v]\n"
  "  \\S       Match non-whitespace [^ \\t\\n\\r\\f\\v]\n"
  "  \\w       Match alphanumeric [a-zA-Z0-9_]\n"
  "  \\W       Match non-alphanumeric [^a-zA-Z0-9_]\n"
  "  \\d       Match decimal digit [0-9]\n"
  "  \\D       Match non-decimal digit [^0-9]\n"
  "  \\n       Match new line character\n"
  "  \\r       Match line feed character\n"
  "  \\f       Match form feed character\n"
  "  \\v       Match vertical tab character\n"
  "  \\t       Match horizontal tab character\n"
  "  \\b       Match backspace character\n"
  "  +        Match one or more times (greedy)\n"
  "  +?       Match one or more times (non-greedy)\n"
  "  *        Match zero or more times (greedy)\n"
  "  *?       Match zero or more times (non-greedy)\n"
  "  ?        Match zero or once (greedy)\n"
  "  ??       Match zero or once (non-greedy)\n"
  "  x|y      Match x or y (alternation operator)\n"
  "  \\meta    Match one of the meta character: ^$().[]*+?|\\\n"
  "  \\xHH     Match byte with hex value 0xHH, e.g. \\x4a\n"
  "  \\<, \\>   Match start-of-word and end-of-word.\n"
  "  [...]    Match any character from set. Ranges like [a-z] are supported\n"
  "  [^...]   Match any character but ones from set\n"
  "  {n}      Matches exactly n times.\n"
  "  {n,}     Matches the preceding character at least n times.\n"
  "  {n,m}    Matches the preceding character at least n and at most m times.\n\n"
  "Flags:\n"
  "  re.I, re.IGNORECASE    Perform case-insensitive matching\n"
  "  re.G, re.GLOBAL        Perform global matching"  
  ) {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 2, 3)) return;

  const char *input; uint32_t len; int global;
  RE* re = _re_init(vm, &input, &len, 3, &global);
  if (!re) return;

  _re_match(vm, re, input, len, global, false);
  re_free(re);
}

DEF(_reRange,
  "re.range(pattern:String, input:String[, flag: Number]) -> List",
  "Perform a regular expression match and return a list of range object.\n"
  "Run help(re.match) to show supported regex patterns."
  ) {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 2, 3)) return;

  const char *input; uint32_t len; int global;
  RE* re = _re_init(vm, &input, &len, 3, &global);
  if (!re) return;

  _re_match(vm, re, input, len, global, true);
  re_free(re);
}

DEF(_reTest,
  "re.test(pattern:String, input:String[, flag: Number]) -> Bool",
  "Perform a regular expression match and return true or false.\n"
  "Run help(re.match) to show supported regex patterns."
  ) {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 2, 3)) return;

  const char *input; uint32_t len;
  RE* re = _re_init(vm, &input, &len, 3, NULL);
  if (!re) return;

  const char** matches = re_match(re, input);

  pkSetSlotBool(vm, 0, matches != NULL);

  re_free(re);
}

DEF(_reReplace,
  "re.replace(pattern:String, input:String, [by:String|Closure, count:Number, flag:Number]) -> String",
  "Replaces [pattern] in [input] by the string [by].\n"
  "Run help(re.match) to show supported regex patterns."
  ) {
    

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 2, 5)) return;

  int callback = 0; const char *by = NULL; uint32_t by_len = 0;
  if (argc >= 3) {
    PkVarType type = pkGetSlotType(vm, 3);
    if (type == PK_CLOSURE) {
      callback = 3;
      int arity;
      if (!pkGetAttribute(vm, 3, "arity", 0) ||
        pkGetSlotType(vm, 0) != PK_NUMBER ||
        pkGetSlotNumber(vm, 0) != 1) {
          pkSetRuntimeError(vm, "Expected exactly 1 argument for callback function.");
          return;
      }

    } else if (type == PK_STRING) {
      by = pkGetSlotString(vm, 3, &by_len);

    } else {
      pkSetRuntimeError(vm, "Expected a 'String' or a 'Closure' at slot 3.");
      return;
    }
  }

  int32_t count = -1;
  if (argc >= 4) {
    if (!pkValidateSlotInteger(vm, 4, &count)) return;
  }

  const char *input; uint32_t len;
  RE* re = _re_init(vm, &input, &len, 5, NULL);
  if (!re) return;

  pkByteBuffer buff;
  pkByteBufferInit(&buff);

  const char *ptr = input;
  while (ptr < input + len && count != 0) {
    const char** matches = re_match(re, ptr);
    if (!matches || !matches[0] || !matches[1] || matches[0] == matches[1]) break;

    if (matches[0] - ptr != 0) {
      pkByteBufferAddString(&buff, vm, ptr, matches[0] - ptr);
    }
    if (callback) {
      pkSetSlotStringLength(vm, 0, matches[0], matches[1] - matches[0]);
      pkCallFunction(vm, callback, 1, 0, 0);
      if (pkGetSlotType(vm, 0) == PK_STRING) {
        by = pkGetSlotString(vm, 0, &by_len);
        pkByteBufferAddString(&buff, vm, by, by_len);
      }
    } else if (by) {
      pkByteBufferAddString(&buff, vm, by, by_len);
    }

    ptr = matches[1]; // point to last matched char
    if (count > 0) count--;
  }
  if (ptr < input + len) {
    pkByteBufferAddString(&buff, vm, ptr, input + len - ptr);
  }

  String* str = newStringLength(vm, (const char*)buff.data, buff.count);
  pkByteBufferClear(&buff, vm);
  vm->fiber->ret[0] = VAR_OBJ(str);

  re_free(re);
}

DEF(_reSplit,
  "re.split(pattern:String, input:String[, count:Number, flag:Number]) -> List",
  "Split string by a regular expression.\n"
  "Run help(re.match) to show supported regex patterns."
  ) {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 2, 4)) return;

  int32_t count = -1;
  if (argc >= 3) {
    if (!pkValidateSlotInteger(vm, 3, &count)) return;
  }

  const char *input; uint32_t len;
  RE* re = _re_init(vm, &input, &len, 4, NULL);
  if (!re) return;

  pkNewList(vm, 0);

  const char *ptr = input;
  while (ptr < input + len && count != 0) {
    const char** matches = re_match(re, ptr);
    if (!matches || !matches[0] || !matches[1] || matches[0] == matches[1]) break;

    pkSetSlotStringLength(vm, 1, ptr, matches[0] - ptr);
    pkListInsert(vm, 0, -1, 1);

    ptr = matches[1]; // point to last matched char
    if (count > 0) count--;
  }
  pkSetSlotStringLength(vm, 1, ptr, input + len - ptr);
  pkListInsert(vm, 0, -1, 1);

  re_free(re);
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
  REGISTER_FN(re, "range", _reRange, -1);
  REGISTER_FN(re, "test", _reTest, -1);
  REGISTER_FN(re, "replace", _reReplace, -1);
  REGISTER_FN(re, "split", _reSplit, -1);

  pkRegisterModule(vm, re);
  pkReleaseHandle(vm, re);
}
