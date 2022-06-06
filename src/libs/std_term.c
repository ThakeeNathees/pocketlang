/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_AMALGAMATED
#include "libs.h"
#include "gen/ext_term.pk.h"  //<< AMALG_INLINE >>
#endif

#ifdef _WIN32
#include <fcntl.h>
#endif

#define TERM_IMPLEMENT
#include "thirdparty/term/term.h"  //<< AMALG_INLINE >>
#undef TERM_IMPLEMENT

// A reference to the event class, to check is instance of.
static PkHandle* _cls_term_event = NULL;

static void _setSlotVector(PKVM* vm, int slot, int tmp, double x, double y) {

  if (!pkImportModule(vm, "types", slot)) return;
  if (!pkGetAttribute(vm, slot, "Vector", slot)) return;
  if (!pkNewInstance(vm, slot, slot, 0, 0)) return;

  pkSetSlotNumber(vm, tmp, x);
  if (!pkSetAttribute(vm, slot, "x", tmp)) return;
  pkSetSlotNumber(vm, tmp, y);
  if (!pkSetAttribute(vm, slot, "y", tmp)) return;
}

void* _termEventNew(PKVM* vm) {
  term_Event* event = pkRealloc(vm, NULL, sizeof(term_Event));
  event->type = TERM_ET_UNKNOWN;
  return event;
}

void _termEventDelete(PKVM* vm, void* event) {
  pkRealloc(vm, event, 0);
}

PKDEF(_termEventGetter,
  "term.Event@getter() -> Var", "") {
  const char* name;
  if (!pkValidateSlotString(vm, 1, &name, NULL)) return;

  term_Event* event = pkGetSelf(vm);

  if (strcmp(name, "type") == 0) {
    pkSetSlotNumber(vm, 0, (double)event->type);

  } else if (strcmp(name, "keycode") == 0) {
    pkSetSlotNumber(vm, 0, (double)event->key.code);

  } else if (strcmp(name, "ascii") == 0) {
    pkSetSlotNumber(vm, 0, (double)event->key.ascii);

  } else if (strcmp(name, "modifiers") == 0) {
    if (event->type == TERM_ET_KEY_DOWN) {
      pkSetSlotNumber(vm, 0, (double)event->key.modifiers);
    } else {
      pkSetSlotNumber(vm, 0, (double)event->mouse.modifiers);
    }

  } else if (strcmp(name, "button") == 0) {
    pkSetSlotNumber(vm, 0, (double)event->mouse.button);

  } else if (strcmp(name, "pos") == 0) {
    pkReserveSlots(vm, 2);
    _setSlotVector(vm, 0, 1, event->mouse.pos.x, event->mouse.pos.y);

  } else if (strcmp(name, "scroll") == 0) {
    pkSetSlotBool(vm, 0, event->mouse.scroll);
  }

}

void _registerEnums(PKVM* vm, PkHandle* term) {
  pkReserveSlots(vm, 1);
  pkSetSlotHandle(vm, 0, term);

  pkSetSlotNumber(vm, 1, TERM_KEY_UNKNOWN); pkSetAttribute(vm, 0, "KEY_UNKNOWN", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_0); pkSetAttribute(vm, 0, "KEY_0", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_1); pkSetAttribute(vm, 0, "KEY_1", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_2); pkSetAttribute(vm, 0, "KEY_2", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_3); pkSetAttribute(vm, 0, "KEY_3", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_4); pkSetAttribute(vm, 0, "KEY_4", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_5); pkSetAttribute(vm, 0, "KEY_5", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_6); pkSetAttribute(vm, 0, "KEY_6", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_7); pkSetAttribute(vm, 0, "KEY_7", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_8); pkSetAttribute(vm, 0, "KEY_8", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_9); pkSetAttribute(vm, 0, "KEY_9", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_A); pkSetAttribute(vm, 0, "KEY_A", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_B); pkSetAttribute(vm, 0, "KEY_B", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_C); pkSetAttribute(vm, 0, "KEY_C", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_D); pkSetAttribute(vm, 0, "KEY_D", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_E); pkSetAttribute(vm, 0, "KEY_E", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F); pkSetAttribute(vm, 0, "KEY_F", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_G); pkSetAttribute(vm, 0, "KEY_G", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_H); pkSetAttribute(vm, 0, "KEY_H", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_I); pkSetAttribute(vm, 0, "KEY_I", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_J); pkSetAttribute(vm, 0, "KEY_J", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_K); pkSetAttribute(vm, 0, "KEY_K", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_L); pkSetAttribute(vm, 0, "KEY_L", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_M); pkSetAttribute(vm, 0, "KEY_M", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_N); pkSetAttribute(vm, 0, "KEY_N", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_O); pkSetAttribute(vm, 0, "KEY_O", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_P); pkSetAttribute(vm, 0, "KEY_P", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_Q); pkSetAttribute(vm, 0, "KEY_Q", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_R); pkSetAttribute(vm, 0, "KEY_R", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_S); pkSetAttribute(vm, 0, "KEY_S", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_T); pkSetAttribute(vm, 0, "KEY_T", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_U); pkSetAttribute(vm, 0, "KEY_U", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_V); pkSetAttribute(vm, 0, "KEY_V", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_W); pkSetAttribute(vm, 0, "KEY_W", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_X); pkSetAttribute(vm, 0, "KEY_X", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_Y); pkSetAttribute(vm, 0, "KEY_Y", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_Z); pkSetAttribute(vm, 0, "KEY_Z", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_ESC); pkSetAttribute(vm, 0, "KEY_ESC", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_ENTER); pkSetAttribute(vm, 0, "KEY_ENTER", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_SPACE); pkSetAttribute(vm, 0, "KEY_SPACE", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_HOME); pkSetAttribute(vm, 0, "KEY_HOME", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_END); pkSetAttribute(vm, 0, "KEY_END", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_PAGEUP); pkSetAttribute(vm, 0, "KEY_PAGEUP", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_PAGEDOWN); pkSetAttribute(vm, 0, "KEY_PAGEDOWN", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_LEFT); pkSetAttribute(vm, 0, "KEY_LEFT", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_UP); pkSetAttribute(vm, 0, "KEY_UP", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_RIGHT); pkSetAttribute(vm, 0, "KEY_RIGHT", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_DOWN); pkSetAttribute(vm, 0, "KEY_DOWN", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_INSERT); pkSetAttribute(vm, 0, "KEY_INSERT", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_DELETE); pkSetAttribute(vm, 0, "KEY_DELETE", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_BACKSPACE); pkSetAttribute(vm, 0, "KEY_BACKSPACE", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_TAB); pkSetAttribute(vm, 0, "KEY_TAB", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F1); pkSetAttribute(vm, 0, "KEY_F1", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F2); pkSetAttribute(vm, 0, "KEY_F2", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F3); pkSetAttribute(vm, 0, "KEY_F3", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F4); pkSetAttribute(vm, 0, "KEY_F4", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F5); pkSetAttribute(vm, 0, "KEY_F5", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F6); pkSetAttribute(vm, 0, "KEY_F6", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F7); pkSetAttribute(vm, 0, "KEY_F7", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F8); pkSetAttribute(vm, 0, "KEY_F8", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F9); pkSetAttribute(vm, 0, "KEY_F9", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F10); pkSetAttribute(vm, 0, "KEY_F10", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F11); pkSetAttribute(vm, 0, "KEY_F11", 1);
  pkSetSlotNumber(vm, 1, TERM_KEY_F12); pkSetAttribute(vm, 0, "KEY_F12", 1);

  pkSetSlotNumber(vm, 1, TERM_MB_UNKNOWN); pkSetAttribute(vm, 0, "BUTTON_UNKNOWN", 1);
  pkSetSlotNumber(vm, 1, TERM_MB_LEFT); pkSetAttribute(vm, 0, "BUTTON_LEFT", 1);
  pkSetSlotNumber(vm, 1, TERM_MB_MIDDLE); pkSetAttribute(vm, 0, "BUTTON_MIDDLE", 1);
  pkSetSlotNumber(vm, 1, TERM_MB_RIGHT); pkSetAttribute(vm, 0, "BUTTON_RIGHT", 1);

  pkSetSlotNumber(vm, 1, TERM_MD_NONE); pkSetAttribute(vm, 0, "MD_NONE", 1);
  pkSetSlotNumber(vm, 1, TERM_MD_CTRL); pkSetAttribute(vm, 0, "MD_CTRL", 1);
  pkSetSlotNumber(vm, 1, TERM_MD_ALT); pkSetAttribute(vm, 0, "MD_ALT", 1);
  pkSetSlotNumber(vm, 1, TERM_MD_SHIFT); pkSetAttribute(vm, 0, "MD_SHIFT", 1);

  pkSetSlotNumber(vm, 1, TERM_ET_UNKNOWN); pkSetAttribute(vm, 0, "EVENT_UNKNOWN", 1);
  pkSetSlotNumber(vm, 1, TERM_ET_KEY_DOWN); pkSetAttribute(vm, 0, "EVENT_KEY_DOWN", 1);
  pkSetSlotNumber(vm, 1, TERM_ET_RESIZE); pkSetAttribute(vm, 0, "EVENT_RESIZE", 1);
  pkSetSlotNumber(vm, 1, TERM_ET_DOUBLE_CLICK); pkSetAttribute(vm, 0, "EVENT_DOUBLE_CLICK", 1);
  pkSetSlotNumber(vm, 1, TERM_ET_MOUSE_DOWN); pkSetAttribute(vm, 0, "EVENT_MOUSE_DOWN", 1);
  pkSetSlotNumber(vm, 1, TERM_ET_MOUSE_UP); pkSetAttribute(vm, 0, "EVENT_MOUSE_UP", 1);
  pkSetSlotNumber(vm, 1, TERM_ET_MOUSE_MOVE); pkSetAttribute(vm, 0, "EVENT_MOUSE_MOVE", 1);
  pkSetSlotNumber(vm, 1, TERM_ET_MOUSE_DRAG); pkSetAttribute(vm, 0, "EVENT_MOUSE_DRAG", 1);
  pkSetSlotNumber(vm, 1, TERM_ET_MOUSE_SCROLL); pkSetAttribute(vm, 0, "EVENT_MOUSE_SCROLL", 1);

}

PKDEF(_termInit,
  "term.init(capture_events:Bool) -> Null",
  "Initialize terminal with raw mode for tui applications, set "
  "[capture_events] true to enable event handling.") {
  bool capture_events;
  if (!pkValidateSlotBool(vm, 1, &capture_events)) return;
  term_init(capture_events);
}

PKDEF(_termCleanup,
  "term.cleanup() -> Null",
  "Cleanup and resotre the last terminal state.") {
  term_cleanup();
}

PKDEF(_termIsatty,
  "term.isatty() -> Bool",
  "Returns true if both stdin and stdout are tty.") {
  pkSetSlotBool(vm, 0, term_isatty());
}

PKDEF(_termNewScreenBuffer,
  "term.new_screen_buffer() -> Null",
  "Switch to an alternative screen buffer.") {
  term_new_screen_buffer();
}

PKDEF(_termRestoreScreenBuffer,
  "term.restore_screen_buffer() -> Null",
  "Restore the alternative buffer which was created with "
  "term.new_screen_buffer()") {
  term_restore_screen_buffer();
}

PKDEF(_termGetSize,
  "term.getsize() -> types.Vector",
  "Returns the screen size.") {
  pkReserveSlots(vm, 2);
  term_Vec size = term_getsize();
  _setSlotVector(vm, 0, 1, size.x, size.y);
}

PKDEF(_termGetPosition,
  "term.getposition() -> types.Vector",
  "Returns the cursor position in the screen on a zero based coordinate.") {
  pkReserveSlots(vm, 2);
  term_Vec pos = term_getposition();
  _setSlotVector(vm, 0, 1, pos.x, pos.y);
}

PKDEF(_termSetPosition,
  "term.setposition(pos:types.Vector | {x, y}) -> Null",
  "Set cursor position at the [position] in the screen no a zero"
  "based coordinate.") {
  double x, y;

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 1, 2)) return;

  if (argc == 1) {
    pkReserveSlots(vm, 3);
    if (!pkGetAttribute(vm, 1, "x", 2)) return;
    if (!pkValidateSlotNumber(vm, 2, &x)) return;

    if (!pkGetAttribute(vm, 1, "y", 2)) return;
    if (!pkValidateSlotNumber(vm, 2, &y)) return;
  } else {
    if (!pkValidateSlotNumber(vm, 1, &x)) return;
    if (!pkValidateSlotNumber(vm, 2, &y)) return;
  }

  term_Vec pos = term_vec((int)x, (int)y);
  term_setposition(pos);
}

PKDEF(_termReadEvent,
  "term.read_event(event:term.Event) -> Bool",
  "Read an event and update the argument [event] and return true."
  "If no event was read it'll return false.") {
  pkReserveSlots(vm, 3);
  pkSetSlotHandle(vm, 2, _cls_term_event);
  if (!pkValidateSlotInstanceOf(vm, 1, 2)) return;

  term_Event* event = pkGetSlotNativeInstance(vm, 1);
  pkSetSlotBool(vm, 0, term_read_event(event));
}

PKDEF(_termBinaryMode,
  "term.binary_mode() -> Null",
  "On windows it'll set stdout to binary mode, on other platforms this "
  "function won't make make any difference.") {
  #ifdef _WIN32
    (void) _setmode(_fileno(stdout), _O_BINARY);
  #endif
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleTerm(PKVM* vm) {
  PkHandle* term = pkNewModule(vm, "term");

  _registerEnums(vm, term);
  REGISTER_FN(term, "init", _termInit, 1);
  REGISTER_FN(term, "cleanup", _termCleanup, 0);
  REGISTER_FN(term, "isatty", _termIsatty, 0);
  REGISTER_FN(term, "new_screen_buffer", _termNewScreenBuffer, 0);
  REGISTER_FN(term, "restore_screen_buffer", _termRestoreScreenBuffer, 0);
  REGISTER_FN(term, "getsize", _termGetSize, 0);
  REGISTER_FN(term, "getposition", _termGetPosition, 0);
  REGISTER_FN(term, "setposition", _termSetPosition, -1);
  REGISTER_FN(term, "read_event", _termReadEvent, 1);

  _cls_term_event = pkNewClass(vm, "Event", NULL, term,
                              _termEventNew, _termEventDelete,
  "The terminal event type, that'll be used at term.read_event function to "
  "fetch events.");

  ADD_METHOD(_cls_term_event, "@getter", _termEventGetter, 1);

  pkModuleAddSource(vm, term, ext_term_pk);

  // This is required for language server. Since we need to send '\r\n' to
  // the lsp client but windows will change '\n' to '\r\n' and it'll become
  // '\r\r\n', binary mode will prevent this.
  REGISTER_FN(term, "binary_mode", _termBinaryMode, 0);

  pkRegisterModule(vm, term);
  pkReleaseHandle(vm, term);
}

void cleanupModuleTerm(PKVM* vm) {
  if (_cls_term_event) pkReleaseHandle(vm, _cls_term_event);
}

