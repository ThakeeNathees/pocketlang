/*****************************************************************************/
/* PUBLIC API                                                                */
/*****************************************************************************/

/*
 * Note that at this point *nix stdout are buffered and windows aren't.
 *
 * *nix systems doesn't support double click at this point, but it's in my
 * TODO. Contributions are wellcome.
 *
 * Window resize events can be enabled by setting NO_WINDOW_RESIZE_EVENT
 * macro value to 1 and recompile, however you have to provide a event
 * callback for resize events in *nix systems.
 *
 */

#include <stdbool.h>

/*
 * A generic Vector type to pass size, position data around.
 */
typedef struct {
  int x;
  int y;
} term_Vec;

/* A macro function to create a vector. */
#define term_vec(x, y) (term_Vec) { (x), (y) }


/*
 * List of keycodes. Note that not all possible keys are listed
 * here however they can be retrieved by from the key event's
 * ascii value.
 */
typedef enum {

  TERM_KEY_UNKNOWN = 0,

  TERM_KEY_0 = '0',
  TERM_KEY_1 = '1',
  TERM_KEY_2 = '2',
  TERM_KEY_3 = '3',
  TERM_KEY_4 = '4',
  TERM_KEY_5 = '5',
  TERM_KEY_6 = '6',
  TERM_KEY_7 = '7',
  TERM_KEY_8 = '8',
  TERM_KEY_9 = '9',

  TERM_KEY_A = 'A',
  TERM_KEY_B = 'B',
  TERM_KEY_C = 'C',
  TERM_KEY_D = 'D',
  TERM_KEY_E = 'E',
  TERM_KEY_F = 'F',
  TERM_KEY_G = 'G',
  TERM_KEY_H = 'H',
  TERM_KEY_I = 'I',
  TERM_KEY_J = 'J',
  TERM_KEY_K = 'K',
  TERM_KEY_L = 'L',
  TERM_KEY_M = 'M',
  TERM_KEY_N = 'N',
  TERM_KEY_O = 'O',
  TERM_KEY_P = 'P',
  TERM_KEY_Q = 'Q',
  TERM_KEY_R = 'R',
  TERM_KEY_S = 'S',
  TERM_KEY_T = 'T',
  TERM_KEY_U = 'U',
  TERM_KEY_V = 'V',
  TERM_KEY_W = 'W',
  TERM_KEY_X = 'X',
  TERM_KEY_Y = 'Y',
  TERM_KEY_Z = 'Z',

  TERM_KEY_ESC,
  TERM_KEY_ENTER,
  TERM_KEY_SPACE,
  TERM_KEY_HOME,
  TERM_KEY_END,
  TERM_KEY_PAGEUP,
  TERM_KEY_PAGEDOWN,
  TERM_KEY_LEFT,
  TERM_KEY_UP,
  TERM_KEY_RIGHT,
  TERM_KEY_DOWN,
  TERM_KEY_INSERT,
  TERM_KEY_DELETE,
  TERM_KEY_BACKSPACE,
  TERM_KEY_TAB,

  TERM_KEY_F1,
  TERM_KEY_F2,
  TERM_KEY_F3,
  TERM_KEY_F4,
  TERM_KEY_F5,
  TERM_KEY_F6,
  TERM_KEY_F7,
  TERM_KEY_F8,
  TERM_KEY_F9,
  TERM_KEY_F10,
  TERM_KEY_F11,
  TERM_KEY_F12,

} term_KeyCode;


/*
 * Event type.
 */
typedef enum {
  TERM_ET_UNKNOWN = 0,
  TERM_ET_KEY_DOWN,
  TERM_ET_DOUBLE_CLICK,
  TERM_ET_MOUSE_DOWN,
  TERM_ET_MOUSE_UP,
  TERM_ET_MOUSE_MOVE,
  TERM_ET_MOUSE_DRAG,
  TERM_ET_MOUSE_SCROLL,
  TERM_ET_RESIZE,
} term_EventType;


/*
 * Key event modifier flags, that contain the ctrl, alt, shift key state.
 */
typedef enum {
  TERM_MD_NONE  = 0x0,
  TERM_MD_CTRL  = (1 << 1),
  TERM_MD_ALT   = (1 << 2),
  TERM_MD_SHIFT = (1 << 3),
} term_Modifiers;


/*
 * Mouse button event's button index.
 */
typedef enum {
  TERM_MB_UNKNOWN = 0,
  TERM_MB_LEFT    = 1,
  TERM_MB_MIDDLE  = 2,
  TERM_MB_RIGHT   = 3,
} term_MouseBtn;


/*
 * Key event.
 */
typedef struct {
  term_KeyCode code;
  char ascii;
  term_Modifiers modifiers;
} term_EventKey;


/*
 * Mouse event.
 */
typedef struct {
  term_MouseBtn button;
  term_Vec pos;
  bool scroll; /* If true down otherwise up. */
  term_Modifiers modifiers;
} term_EventMouse;

/*
 * Event.
 */
typedef struct {
  term_EventType type;
  union {
    term_EventKey key;
    term_EventMouse mouse;
    term_Vec resize;
  };
} term_Event;


/* Returns true if both stdin and stdout are tty like device. */
bool term_isatty();


/*
 * Initialize the terminal. On windows it'll enable the virtual terminal
 * processing, *nix it'll enter non-canonical mode, and won't echo the
 * characters that are inputted.
 *
 * @param capture_events: If true it'll enable input events processing.
 */
void term_init(bool capture_events);


/* Cleans up all the internals. */
void term_cleanup(void);


/*
 * Reads an event. You should initialize terminal with capture_events
 * to enable reading events.
 *
 * @param event: an event pointer that'll updated after reading an event.
 *
 * @return If an event has been read it'll return true.
 */
bool term_read_event(term_Event* event);


/* Create an alternative screen buffer. */
void term_new_screen_buffer();


/*
 * Restore the screen buffer after switching to an alternative buffer with
 * term_new_screen_buffer(). This will also clean entier screen and place
 * the cursor at (0, 0).
 */
void term_restore_screen_buffer();


/* Returns the screen size. */
term_Vec term_getsize();


/* Returns the cursor position in a zero based index coordinate. */
term_Vec term_getposition();


/* Sets the cursor position in a zero based index coordinate. */
void term_setposition(term_Vec pos);


/*****************************************************************************/
/* INTERNAL HEADERS AND MACROS                                               */
/*****************************************************************************/

#ifdef TERM_IMPLEMENT

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #define TERM_SYS_WIN
#else
  #define TERM_SYS_NIX
#endif

/* Platform specific includes */
#if defined(TERM_SYS_WIN)
  #include <windows.h>
#elif defined(TERM_SYS_NIX)
  #include <signal.h>
  #include <termios.h>
  #include <sys/ioctl.h>
#endif

#if defined(_MSC_VER) || (defined(TERM_SYS_WIN) && defined(__TINYC__))
  #include <io.h>
  #define read _read
  #define fileno _fileno
  #define isatty _isatty
#else
  #include <unistd.h>
#endif

/*
 * In older version of windows some terminal attributes are not defined
 * So I'm defining everyting here if they're not already.
 *
 * Note that I have no idea which macros are not defined and not, I'm just
 * re-defining every this looks suspicious.
 */
#ifdef TERM_SYS_WIN

  #ifndef ENABLE_VIRTUALINAL_PROCESSING
    #define ENABLE_VIRTUALINAL_PROCESSING 0x0004
  #endif

  #ifndef ENABLE_EXTENDED_FLAGS
    #define ENABLE_EXTENDED_FLAGS 0x0080
  #endif

  #ifndef FROM_LEFT_1ST_BUTTON_PRESSED
    #define FROM_LEFT_1ST_BUTTON_PRESSED 0x0001
  #endif

  #ifndef RIGHTMOST_BUTTON_PRESSED
    #define RIGHTMOST_BUTTON_PRESSED 0x0002
  #endif

  #ifndef FROM_LEFT_2ND_BUTTON_PRESSED
    #define FROM_LEFT_2ND_BUTTON_PRESSED 0x0004
  #endif

  #ifndef MOUSE_MOVED
    #define MOUSE_MOVED 0x0001
  #endif

  #ifndef DOUBLE_CLICK
    #define DOUBLE_CLICK 0x0002
  #endif

  #ifndef MOUSE_WHEELED
    #define MOUSE_WHEELED 0x0004
  #endif

  #ifndef LEFT_ALT_PRESSED
    #define LEFT_ALT_PRESSED 0x0002
  #endif

  #ifndef RIGHT_ALT_PRESSED
    #define RIGHT_ALT_PRESSED 0x0001
  #endif

  #ifndef RIGHT_CTRL_PRESSED
    #define RIGHT_CTRL_PRESSED 0x0004
  #endif

  #ifndef LEFT_CTRL_PRESSED
    #define LEFT_CTRL_PRESSED 0x0008
  #endif

  #ifndef SHIFT_PRESSED
    #define SHIFT_PRESSED 0x0010
  #endif

#endif /* TERM_SYS_WIN */

/* Input read buffer size. */
#define INPUT_BUFF_SZ 256


/* Returns predicate (a <= c <= b). */
#define BETWEEN(a, c, b) ((a) <= (c) && (c) <= (b))

/*****************************************************************************/
/* TYPEDEFINES AND DECLARATIONS                                              */
/*****************************************************************************/


#define _veceq(v1, v2) (((v1.x) == (v2).x) && ((v1).y == (v2).y))

typedef struct {
  
#if defined(TERM_SYS_WIN)
  DWORD outmode, inmode; /* Backup modes. */
  HANDLE h_stdout, h_stdin; /* Handles. */
  
#elif defined(TERM_SYS_NIX)
  struct termios tios; /* Backup modes. */
  uint8_t buff[INPUT_BUFF_SZ]; /* Input buffer. */
  int32_t buffc; /* Buffer element count. */
#endif
  
  term_Vec screensize;
  term_Vec mousepos;
  
  bool capture_events;
  bool initialized;
  
} term_Ctx;


static term_Ctx _ctx;

static void _init();
static void _cleanup(); 
static term_Vec _getsize();

#ifdef TERM_SYS_NIX
static void _handle_resize(int sig);
#endif

static bool _read_event(term_Event* event);


/*****************************************************************************/
/* IMPLEMENTATIONS                                                           */
/*****************************************************************************/


bool term_isatty() {
  return (!!isatty(fileno(stdout))) && (!!isatty(fileno(stdin)));
}


void term_init(bool capture_events) {
  memset(&_ctx, 0, sizeof(term_Ctx));
  _ctx.capture_events = capture_events;
  
  _init();
  
  _ctx.screensize = _getsize();
  _ctx.initialized = true;
  atexit(term_cleanup);
}


void term_cleanup(void) {
  assert(_ctx.initialized);
  _cleanup();
}


bool term_read_event(term_Event* event) {
  return _read_event(event);
}


#if defined(TERM_SYS_WIN)
static void _init() {
  _ctx.h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
  GetConsoleMode(_ctx.h_stdout, &_ctx.outmode);

  _ctx.h_stdin = GetStdHandle(STD_INPUT_HANDLE);
  GetConsoleMode(_ctx.h_stdin, &_ctx.inmode);

  DWORD outmode = (_ctx.outmode | ENABLE_VIRTUALINAL_PROCESSING);
  SetConsoleMode(_ctx.h_stdout, outmode);

  if (_ctx.capture_events) {
    DWORD inmode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
    SetConsoleMode(_ctx.h_stdin, inmode);
  }
}


static void _cleanup() {
  SetConsoleMode(_ctx.h_stdout, _ctx.outmode);
  SetConsoleMode(_ctx.h_stdin, _ctx.inmode);
}


#elif defined(TERM_SYS_NIX)
static void _init() {
  tcgetattr(fileno(stdin), &_ctx.tios);
  
  struct termios raw = _ctx.tios;
  
  /*
   * ECHO   : It won't print character as we type.
   * ICANON : Disable canonical mode, inputs will
   *          be byte by byte instead of line by line.
   * ISIG   : Disable Ctrl+C, Ctrl+Z
   * IXON   : Disable Ctrl+S, Ctrl+Q
   * IEXTEN : Disable Ctrl+V
   * ICRNL  : Fix Ctrl+M. It won't convert '\r' into '\n' anymore.
   * OPOST  : It won't convert '\n' into '\r\n' anymore.
   * BRKINT : Disable break condition that'll send a SIGINT.
   */
  raw.c_lflag &= ~(ECHO | ICANON);
  if (_ctx.capture_events) {
    /*raw.c_oflag &= ~(OPOST);*/
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT);
    raw.c_lflag &= ~(ISIG | IEXTEN);
  }
  
  /*
   * VMIN  : Minimum number of bytes should be read before return from read().
   * VTIME : Maximum amount of time to be wait before read() returns.
   *         1 unit is 100 of a second (ie. vtime = n => 1/100 s)
   *
   * However on windows running WSL, VTIME won't work, it'll wait till an
   * input is read.
   */
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  
  tcsetattr(fileno(stdin), TCSAFLUSH, &raw);
  
  /* Enable mouse events. */
  if (_ctx.capture_events) {
    fprintf(stdout, "\x1b[?1003h\x1b[?1006h");
    fflush(stdout);
  }
  
  /* Handle resize events. */
  signal(SIGWINCH, _handle_resize);

}


static void _cleanup() {
  
  /* Disable mouse events. */
  if (_ctx.capture_events) {
    fprintf(stdout, "\x1b[?1003l\x1b[?1006l\x1b[?25h");
  }
  
  tcsetattr(fileno(stdin), TCSAFLUSH, &_ctx.tios);
}


void _handle_resize(int sig) {
  term_Vec newsize = _getsize();
  if (!_veceq(_ctx.screensize, newsize)) {
    return;
  }
  _ctx.screensize = newsize;

#if 0 /* TODO: This event should be dispatch or sent to some kind of an event queue. */
  term_Event event;
  memset(&event, 0, sizeof(term_Event));
  event.type = TERM_ET_RESIZE;
  event.resize = _ctx.screensize;

  #error "Dispatch the event with a callback or something."
  your_callback(&event);
#endif
}

#endif /* TERM_SYS_NIX */


void term_new_screen_buffer() {
  fprintf(stdout, "\x1b[?1049h");
}


void term_restore_screen_buffer() {
  fprintf(stdout, "\x1b[H\x1b[J"); /* Clear screen and go to (0, 0). */
  fprintf(stdout, "\x1b[?1049l");
}


term_Vec term_getposition() {
    term_Vec pos;

  #if defined(TERM_SYS_WIN)
  CONSOLE_SCREEN_BUFFER_INFO binfo;
  GetConsoleScreenBufferInfo(_ctx.h_stdout, &binfo);
  pos.x = binfo.dwCursorPosition.X;
  pos.y = binfo.dwCursorPosition.Y;
  return pos;

  #elif defined(TERM_SYS_NIX)

  struct termios tio;
  if (tcgetattr(fileno(stdin), &tio) != 0) {
    assert(false && "tcgetattr(stdin) failed.");
  }
  
  tcsetattr(fileno(stdin), TCSANOW, &tio);
  assert(((tio.c_lflag & (ICANON | ECHO)) == 0)
         && "Did you forget to call term_init()");
  

  /* 
   * Request cursor position. stdin will be in the for of ESC[n;mR
   * here where n is the row and m is the column. (1 based).
   */
  fprintf(stdout, "\x1b[6n");

  if (getchar() != '\x1b' || getchar() != '[') {
    assert(false && "getchar() failed in getposition()");
  }

  pos.x = 0; pos.y = 0;
  int* p = &pos.y;

  while (true) {
    int c = getchar();

    if (c == EOF) {
      assert(false && "getchar() failed in getposition()");
    }

    if (c == ';') p = &pos.x;
    if (c == 'R') break;


    if (BETWEEN('0', c, '9')) {
      *p = *p * 10 + (c - '0');
    }
  }

  /* Since column, row numbers are 1 based substract 1 for 0 based. */
  pos.x--; pos.y--;

  #endif /* TERM_SYS_NIX */

  return pos;
}


void term_setposition(term_Vec pos) {
  fprintf(stdout, "\x1b[%i;%iH", pos.y + 1, pos.x + 1);
}


static term_Vec _getsize() {
  term_Vec size;

#if defined(TERM_SYS_WIN)
  CONSOLE_SCREEN_BUFFER_INFO binfo;
  GetConsoleScreenBufferInfo(_ctx.h_stdout, &binfo);
  size.x = binfo.srWindow.Right - binfo.srWindow.Left + 1;
  size.y = binfo.srWindow.Bottom - binfo.srWindow.Top + 1;

#elif defined(TERM_SYS_NIX)
  struct winsize wsize;
  ioctl(fileno(stdout), TIOCGWINSZ, &wsize);
  size.x = wsize.ws_col;
  size.y = wsize.ws_row;
#endif

  return size;
}


term_Vec term_getsize() {
  return _ctx.screensize;
}


/*****************************************************************************/
/* INPUT PROCESSING                                                          */
/*****************************************************************************/

#if defined(TERM_SYS_WIN)


/*
 * Convert windows virtual keys to term_KeyCodes. Returns false if the key
 * should be ignored as an event (ex: shift, ctrl, ...).
 */
static bool _toTermKeyCode(WORD vk, term_KeyCode * kc) {

  if (0x30 <= vk && vk <= 0x39) { *kc = (term_KeyCode)TERM_KEY_0 + (vk - 0x30); return true; }
  if (0x60 <= vk && vk <= 0x69) { *kc = (term_KeyCode)TERM_KEY_0 + (vk - 0x60); return true; }
  if (0x41 <= vk && vk <= 0x5a) { *kc = (term_KeyCode)TERM_KEY_A + (vk - 0x41); return true; }
  if (0x70 <= vk && vk <= 0x7b) { *kc = (term_KeyCode)TERM_KEY_F1 + (vk - 0x70); return true; }

  /*
   * Note that shift, ctrl, alt keys are returned as TERM_KEY_UNKNOWN
   *since *nix systems don't support them.
   */
  switch (vk) {
  case VK_BACK: *kc = TERM_KEY_BACKSPACE; break;
  case VK_TAB: *kc = TERM_KEY_TAB; break;
  case VK_RETURN: *kc = TERM_KEY_ENTER; break;
  case VK_ESCAPE: *kc = TERM_KEY_ESC; break;
  case VK_SPACE: *kc = TERM_KEY_SPACE; break;
  case VK_PRIOR: *kc = TERM_KEY_PAGEUP; break;
  case VK_NEXT: *kc = TERM_KEY_PAGEDOWN; break;
  case VK_END: *kc = TERM_KEY_END; break;
  case VK_HOME: *kc = TERM_KEY_HOME; break;
  case VK_LEFT: *kc = TERM_KEY_LEFT; break;
  case VK_RIGHT: *kc = TERM_KEY_RIGHT; break;
  case VK_UP: *kc = TERM_KEY_UP; break;
  case VK_DOWN: *kc = TERM_KEY_DOWN; break;
  case VK_INSERT: *kc = TERM_KEY_INSERT; break;
  case VK_DELETE: *kc = TERM_KEY_DELETE; break;

  case VK_SHIFT:
  case VK_CONTROL:
  case VK_MENU:
  case VK_PAUSE:
  case VK_CAPITAL: /* Capslock. */
    return false;
  }

  return true;
}


static bool _read_event(term_Event* event) {
  memset(event, 0, sizeof(term_Event));
  event->type = TERM_ET_UNKNOWN;

  DWORD count;
  if (!GetNumberOfConsoleInputEvents(_ctx.h_stdin, &count)) {
    /* TODO: error handle api ("GetNumberOfConsoleInputEvents() failed."). */
    return false;
  }

  if (count == 0) return false;

  INPUT_RECORD ir;
  if (!ReadConsoleInput(_ctx.h_stdin, &ir, 1, &count)) {
    /* TODO: error handle api ("ReadConsoleInput() failed."). */
    return false;
  }

  switch (ir.EventType) {
    case KEY_EVENT: {
      KEY_EVENT_RECORD* ker = &ir.Event.KeyEvent;

      /* Key up event not available in *nix systems. So we're ignoring here as well. */
      if (!ker->bKeyDown) return false;

      if (!_toTermKeyCode(ker->wVirtualKeyCode, &event->key.code)) return false;

      event->type = TERM_ET_KEY_DOWN;
      event->key.ascii = ker->uChar.AsciiChar;

      if ((ker->dwControlKeyState & LEFT_ALT_PRESSED) || (ker->dwControlKeyState & RIGHT_ALT_PRESSED))
        event->key.modifiers |= TERM_MD_ALT;
      if ((ker->dwControlKeyState & LEFT_CTRL_PRESSED) || (ker->dwControlKeyState & RIGHT_CTRL_PRESSED))
        event->key.modifiers |= TERM_MD_CTRL;
      if (ker->dwControlKeyState & SHIFT_PRESSED)
        event->key.modifiers |= TERM_MD_SHIFT;

    } break;

    case MOUSE_EVENT: {

      MOUSE_EVENT_RECORD* mer = &ir.Event.MouseEvent;

      static DWORD last_state = 0; /* Last state to compare if a new button pressed. */
      bool pressed = mer->dwButtonState; /* If any state is on pressed will be true. */

      /* Xor will give != 0 if any button changed. */
      DWORD change = last_state ^ mer->dwButtonState;

      if (change != 0) {
        /*
         * What if the mouse doesn't have the middle button and right
         * button is the second button?.
         */
        if (change & FROM_LEFT_1ST_BUTTON_PRESSED) {
          pressed = mer->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED;
          event->mouse.button = TERM_MB_LEFT;

        } else if (change & RIGHTMOST_BUTTON_PRESSED) {
          pressed = mer->dwButtonState & RIGHTMOST_BUTTON_PRESSED;
          event->mouse.button = TERM_MB_RIGHT;

        } else if (change & FROM_LEFT_2ND_BUTTON_PRESSED) {
          pressed = mer->dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED;
          event->mouse.button = TERM_MB_MIDDLE;
        }
      }
      last_state = mer->dwButtonState;

      event->mouse.pos.x = mer->dwMousePosition.X;
      event->mouse.pos.y = mer->dwMousePosition.Y;

      if (mer->dwEventFlags == 0) {
        event->type = (pressed) ? TERM_ET_MOUSE_DOWN : TERM_ET_MOUSE_UP;

      } else if (mer->dwEventFlags & MOUSE_MOVED) {
        if (_veceq(_ctx.mousepos, event->mouse.pos)) {
          return false;
        }
        event->type = (pressed) ? TERM_ET_MOUSE_DRAG : TERM_ET_MOUSE_MOVE;

      } else if (mer->dwEventFlags & MOUSE_WHEELED) {
        event->type = TERM_ET_MOUSE_SCROLL;
        event->mouse.scroll = (mer->dwButtonState & 0xFF000000) ? true : false;

      } else if (mer->dwEventFlags & DOUBLE_CLICK) {
        event->type = TERM_ET_DOUBLE_CLICK;
      }

      _ctx.mousepos.x = event->mouse.pos.x;
      _ctx.mousepos.y = event->mouse.pos.y;

      if ((mer->dwControlKeyState & LEFT_ALT_PRESSED) || (mer->dwControlKeyState & RIGHT_ALT_PRESSED))
        event->mouse.modifiers |= TERM_MD_ALT;
      if ((mer->dwControlKeyState & LEFT_CTRL_PRESSED) || (mer->dwControlKeyState & RIGHT_CTRL_PRESSED))
        event->mouse.modifiers |= TERM_MD_CTRL;
      if (mer->dwControlKeyState & SHIFT_PRESSED)
        event->mouse.modifiers |= TERM_MD_SHIFT;

    } break;

    case WINDOW_BUFFER_SIZE_EVENT: {
      WINDOW_BUFFER_SIZE_RECORD* wbs = &ir.Event.WindowBufferSizeEvent;
      event->type = TERM_ET_RESIZE;
      term_Vec newsize = term_vec(wbs->dwSize.X, wbs->dwSize.Y);
      if (_ctx.screensize.x == newsize.x && _ctx.screensize.y == newsize.y) return false;
      _ctx.screensize = newsize;
      event->resize = _ctx.screensize;
    } break;

    /* Not handling as it's not available in *nix. */
    case MENU_EVENT:
    case FOCUS_EVENT:
      return false;
  }

  return event->type != TERM_ET_UNKNOWN;
}

#elif defined(TERM_SYS_NIX)

/* Returns the length of an escape sequence. */
static int _escape_length(const char* buff, uint32_t size) {
  int length = 0;

  while (length < size) {
    char c = buff[length++];

    if (BETWEEN('a', c, 'z') || BETWEEN('A', c, 'Z') || (c == '~')) {
      if (c == 'O' && length < size) {
        c = buff[length];
        if (BETWEEN('A', c, 'D') || BETWEEN('P', c, 'S') || c == 'F' || c == 'H') {
          return length + 1;
        }
      }

      return length;

    } else if (c == '\x1b') {
      return length;
    }
  }

  return length;
}


static void _key_event(char c, term_Event* event) {
  event->type = TERM_ET_KEY_DOWN;
  event->key.ascii = c;

  /* Note: Ctrl+M and <enter> both reads as '\r'. */
  if (c == '\r') { event->key.code = TERM_KEY_ENTER; return; }
  if (c == 127) { event->key.code = TERM_KEY_BACKSPACE; return; }
  if (c == 9) { event->key.code = TERM_KEY_TAB; return; }
  if (c == 32) { event->key.code = TERM_KEY_SPACE; return; }

  event->key.code = (term_KeyCode) c;

  /* Ctrl + key */
  if (1 <= c && c <= 26) {
    event->key.modifiers |= TERM_MD_CTRL;
    event->key.code = (term_KeyCode)('A' + (c - 1));

  } else if (BETWEEN('a', c, 'z') || BETWEEN('A', c, 'Z') || BETWEEN('0', c, '9')) {
    event->key.code = (term_KeyCode) toupper(c);
    if (BETWEEN('A', c, 'Z')) event->key.modifiers |= TERM_MD_SHIFT;
  }
}


/* Reference: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Mouse-Tracking */
static void _mouse_event(const char* buff, uint32_t count, term_Event * event) {

  /* buff = cb ; cx ; cy m|M */

  const char* c = buff;
  if (!(BETWEEN('0', *c, '9'))) return;

  int cb = 0, cx = 0, cy = 0;
  char m;

  while (BETWEEN('0', *c, '9')) cb = cb * 10 + (*c++ - '0');
  if (*c++ != ';') return;
  while (BETWEEN('0', *c, '9')) cx = cx * 10 + (*c++ - '0');
  if (*c++ != ';') return;
  while (BETWEEN('0', *c, '9')) cy = cy * 10 + (*c++ - '0');

  m = *c++;
  if (m != 'm' && m != 'M') return;

  /*
   * low two bits = button information.
   * next three bits = modifiers.
   * next bits = event type.
   */
  int low = cb & 0b11;
  int high = (cb & 0b11100) >> 2;
  int type = cb >> 5;

  /* Note that the modifiers won't work/incorrect in WSL. */
  if (high & 0b001) event->mouse.modifiers |= TERM_MD_SHIFT;
  if (high & 0b010) event->mouse.modifiers |= TERM_MD_ALT;
  if (high & 0b100) event->mouse.modifiers |= TERM_MD_CTRL;

  event->mouse.pos.x = cx - 1;
  event->mouse.pos.y = cy - 1;

  switch (type) {
    case 0: {
      event->type = (m == 'M') ? TERM_ET_MOUSE_DOWN : TERM_ET_MOUSE_UP;
      event->mouse.button = (term_MouseBtn)(low + 1);
    } break;

    case 1: {
      if (low == 0b11) {  /* Mouse move. */
        event->type = TERM_ET_MOUSE_MOVE;

      } else { /* Drag. */
        event->type = TERM_ET_MOUSE_DRAG;
        event->mouse.button = (term_MouseBtn)(low + 1);
      }
    } break;

    case 2: { /* Scroll. */
      event->type = TERM_ET_MOUSE_SCROLL;

      if (low == 0) event->mouse.scroll = false;
      else if (low == 1) event->mouse.scroll = true;

    } break;
  }
}


void _parse_escape_sequence(const char* buff, uint32_t count, term_Event* event) {
  assert(buff[0] == '\x1b');

  if (count == 1) {
    event->type = TERM_ET_KEY_DOWN;
    event->key.ascii = *buff;
    event->key.code = TERM_KEY_ESC;
    return;
  }

  if (count == 2) {
    _key_event(buff[1], event);
    event->key.modifiers |= TERM_MD_ALT;
    return;
  }


  #define _SET_KEY(keycode)            \
    do {                               \
      event->type = TERM_ET_KEY_DOWN;  \
      event->key.code = keycode;       \
    } while (false)

  #define _MATCH(str) \
    ((count >= strlen(str) + 1) && (strncmp(buff + 1, (str), strlen(str)) == 0))

  if (_MATCH("[<")) _mouse_event(buff + 3, count - 3, event);

  else if (_MATCH("[A") || _MATCH("OA")) _SET_KEY(TERM_KEY_UP);
  else if (_MATCH("[B") || _MATCH("OB")) _SET_KEY(TERM_KEY_DOWN);
  else if (_MATCH("[C") || _MATCH("OC")) _SET_KEY(TERM_KEY_RIGHT);
  else if (_MATCH("[D") || _MATCH("OD")) _SET_KEY(TERM_KEY_LEFT);
  else if (_MATCH("[5~") || _MATCH("[[5~")) _SET_KEY(TERM_KEY_PAGEUP);
  else if (_MATCH("[6~") || _MATCH("[[6~")) _SET_KEY(TERM_KEY_PAGEDOWN);

  else if (_MATCH("[H") || _MATCH("OH") || _MATCH("[1~") || _MATCH("[[7~")) _SET_KEY(TERM_KEY_HOME);
  else if (_MATCH("[F") || _MATCH("OF") || _MATCH("[4~") || _MATCH("[[8~")) _SET_KEY(TERM_KEY_END);

  else if (_MATCH("[2~")) _SET_KEY(TERM_KEY_INSERT);
  else if (_MATCH("[3~")) _SET_KEY(TERM_KEY_DELETE);

  else if (_MATCH("OP") || _MATCH("[11~")) _SET_KEY(TERM_KEY_F1);
  else if (_MATCH("OQ") || _MATCH("[12~")) _SET_KEY(TERM_KEY_F2);
  else if (_MATCH("OR") || _MATCH("[13~")) _SET_KEY(TERM_KEY_F3);
  else if (_MATCH("OS") || _MATCH("[14~")) _SET_KEY(TERM_KEY_F4);

  else if (_MATCH("[15~")) _SET_KEY(TERM_KEY_F5);
  else if (_MATCH("[17~")) _SET_KEY(TERM_KEY_F6);
  else if (_MATCH("[18~")) _SET_KEY(TERM_KEY_F7);
  else if (_MATCH("[19~")) _SET_KEY(TERM_KEY_F8);
  else if (_MATCH("[20~")) _SET_KEY(TERM_KEY_F9);
  else if (_MATCH("[21~")) _SET_KEY(TERM_KEY_F10);
  else if (_MATCH("[23~")) _SET_KEY(TERM_KEY_F11);
  else if (_MATCH("[24~")) _SET_KEY(TERM_KEY_F12);

  #undef _MATCH
  #undef _SET_KEY
}


static void _buff_shift(uint32_t length) {
  assert(_ctx.buff != NULL);
  if (length < _ctx.buffc) {
    memmove(_ctx.buff, _ctx.buff + length, _ctx.buffc - length);
    _ctx.buffc -= length;
  } else {
    _ctx.buffc = 0;
  }
}


static bool _read_event(term_Event* event) {

  memset(event, 0, sizeof(term_Event));
  event->type = TERM_ET_UNKNOWN;

  int count = read(fileno(stdin), _ctx.buff + _ctx.buffc, INPUT_BUFF_SZ - _ctx.buffc);
  if (count == 0) return false;

  _ctx.buffc += count;

  int event_length = 1; /* Num of character for the event in the buffer. */

  if (*_ctx.buff == '\x1b') {
    event_length = _escape_length(_ctx.buff + 1, _ctx.buffc - 1) + 1;
    _parse_escape_sequence(_ctx.buff, event_length, event);
    if (event->type == TERM_ET_MOUSE_MOVE) {
      if (_veceq(_ctx.mousepos, event->mouse.pos)) {
        _buff_shift(event_length);
        return false;
      }
      _ctx.mousepos = event->mouse.pos;
    }

  } else {
    _key_event(_ctx.buff[0], event);
  }

  _buff_shift(event_length);

  return event->type != TERM_ET_UNKNOWN;
}

#endif /* TERM_SYS_NIX */

#endif /* TERM_IMPLEMENT */
