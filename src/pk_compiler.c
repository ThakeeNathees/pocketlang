/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include "pk_compiler.h"

#include "pk_core.h"
#include "pk_buffers.h"
#include "pk_utils.h"
#include "pk_vm.h"
#include "pk_debug.h"

// The maximum number of variables (or global if compiling top level script)
// to lookup from the compiling context. Also it's limited by it's opcode
// which is using a single byte value to identify the local.
#define MAX_VARIABLES 256

// The maximum number of functions a script could contain. Also it's limited by
// it's opcode which is using a single byte value to identify.
#define MAX_FUNCTIONS 256

// The maximum number of classes a script could contain. Also it's limited by
// it's opcode which is using a single byte value to identify.
#define MAX_CLASSES 255

// The maximum number of names that were used before defined. Its just the size
// of the Forward buffer of the compiler. Feel free to increase it if it
// require more.
#define MAX_FORWARD_NAMES 256

// The maximum number of constant literal a script can contain. Also it's
// limited by it's opcode which is using a short value to identify.
#define MAX_CONSTANTS (1 << 16)

// The maximum address possible to jump. Similar limitation as above.
#define MAX_JUMP (1 << 16)

// Max number of break statement in a loop statement to patch.
#define MAX_BREAK_PATCH 256

// The name of a literal function.
#define LITERAL_FN_NAME "$(LiteralFn)"

/*****************************************************************************/
/* TOKENS                                                                    */
/*****************************************************************************/

typedef enum {

  TK_ERROR = 0,
  TK_EOF,
  TK_LINE,

  // symbols
  TK_DOT,        // .
  TK_DOTDOT,     // ..
  TK_DOTDOTDOT,  // ...
  TK_COMMA,      // ,
  TK_COLLON,     // :
  TK_SEMICOLLON, // ;
  TK_HASH,       // #
  TK_LPARAN,     // (
  TK_RPARAN,     // )
  TK_LBRACKET,   // [
  TK_RBRACKET,   // ]
  TK_LBRACE,     // {
  TK_RBRACE,     // }
  TK_PERCENT,    // %

  TK_TILD,       // ~
  TK_AMP,        // &
  TK_PIPE,       // |
  TK_CARET,      // ^
  TK_ARROW,      // ->

  TK_PLUS,       // +
  TK_MINUS,      // -
  TK_STAR,       // *
  TK_FSLASH,     // /
  TK_BSLASH,     // \.
  TK_EQ,         // =
  TK_GT,         // >
  TK_LT,         // <

  TK_EQEQ,       // ==
  TK_NOTEQ,      // !=
  TK_GTEQ,       // >=
  TK_LTEQ,       // <=

  TK_PLUSEQ,     // +=
  TK_MINUSEQ,    // -=
  TK_STAREQ,     // *=
  TK_DIVEQ,      // /=
  TK_MODEQ,      // %=

  TK_ANDEQ,      // &=
  TK_OREQ,       // |=
  TK_XOREQ,      // ^=

  TK_SRIGHT,     // >>
  TK_SLEFT,      // <<

  TK_SRIGHTEQ,   // >>=
  TK_SLEFTEQ,    // <<=

  // Keywords.
  TK_MODULE,     // module
  TK_CLASS,      // class
  TK_FROM,       // from
  TK_IMPORT,     // import
  TK_AS,         // as
  TK_DEF,        // def
  TK_NATIVE,     // native (C function declaration)
  TK_FUNC,       // func (literal function)
  TK_END,        // end

  TK_NULL,       // null
  TK_IN,         // in
  TK_AND,        // and
  TK_OR,         // or
  TK_NOT,        // not / !
  TK_TRUE,       // true
  TK_FALSE,      // false

  TK_DO,         // do
  TK_THEN,       // then
  TK_WHILE,      // while
  TK_FOR,        // for
  TK_IF,         // if
  TK_ELSIF,      // elsif
  TK_ELSE,       // else
  TK_BREAK,      // break
  TK_CONTINUE,   // continue
  TK_RETURN,     // return

  TK_NAME,       // identifier

  TK_NUMBER,     // number literal
  TK_STRING,     // string literal

  /* String interpolation (reference wren-lang)
   * but it doesn't support recursive ex: "a \(b + "\(c)")"
   *  "a \(b) c \(d) e"
   * tokenized as:
   *   TK_STR_INTERP  "a "
   *   TK_NAME        b
   *   TK_STR_INTERP  " c "
   *   TK_NAME        d
   *   TK_STRING     " e" */
   // TK_STR_INTERP, //< not yet.

} TokenType;

typedef struct {
  TokenType type;

  const char* start; //< Begining of the token in the source.
  int length;        //< Number of chars of the token.
  int line;          //< Line number of the token (1 based).
  Var value;         //< Literal value of the token.
} Token;

typedef struct {
  const char* identifier;
  int length;
  TokenType tk_type;
} _Keyword;

// List of keywords mapped into their identifiers.
static _Keyword _keywords[] = {
  { "module",   6, TK_MODULE   },
  { "class",    5, TK_CLASS    },
  { "from",     4, TK_FROM     },
  { "import",   6, TK_IMPORT   },
  { "as",       2, TK_AS       },
  { "def",      3, TK_DEF      },
  { "native",   6, TK_NATIVE   },
  { "func",     4, TK_FUNC     },
  { "end",      3, TK_END      },
  { "null",     4, TK_NULL     },
  { "in",       2, TK_IN       },
  { "and",      3, TK_AND      },
  { "or",       2, TK_OR       },
  { "not",      3, TK_NOT      },
  { "true",     4, TK_TRUE     },
  { "false",    5, TK_FALSE    },
  { "do",       2, TK_DO       },
  { "then",     4, TK_THEN     },
  { "while",    5, TK_WHILE    },
  { "for",      3, TK_FOR      },
  { "if",       2, TK_IF       },
  { "elsif",    5, TK_ELSIF    },
  { "else",     4, TK_ELSE     },
  { "break",    5, TK_BREAK    },
  { "continue", 8, TK_CONTINUE },
  { "return",   6, TK_RETURN   },

  { NULL,       0, (TokenType)(0) }, // Sentinel to mark the end of the array
};

/*****************************************************************************/
/* COMPILER INTERNAL TYPES                                                   */
/*****************************************************************************/

// Precedence parsing references:
// https://en.wikipedia.org/wiki/Shunting-yard_algorithm
// http://mathcenter.oxford.emory.edu/site/cs171/shuntingYardAlgorithm/
// http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/

typedef enum {
  PREC_NONE,
  PREC_LOWEST,
  PREC_LOGICAL_OR,    // or
  PREC_LOGICAL_AND,   // and
  PREC_EQUALITY,      // == !=
  PREC_TEST,          // in is
  PREC_COMPARISION,   // < > <= >=
  PREC_BITWISE_OR,    // |
  PREC_BITWISE_XOR,   // ^
  PREC_BITWISE_AND,   // &
  PREC_BITWISE_SHIFT, // << >>
  PREC_RANGE,         // .. ...
  PREC_TERM,          // + -
  PREC_FACTOR,        // * / %
  PREC_UNARY,         // - ! ~ not
  PREC_CHAIN_CALL,    // ->
  PREC_CALL,          // ()
  PREC_SUBSCRIPT,     // []
  PREC_ATTRIB,        // .index
  PREC_PRIMARY,
} Precedence;

typedef void (*GrammarFn)(Compiler* compiler);

typedef struct {
  GrammarFn prefix;
  GrammarFn infix;
  Precedence precedence;
} GrammarRule;

typedef enum {
  DEPTH_SCRIPT = -2, //< Only used for script body function's depth.
  DEPTH_GLOBAL = -1, //< Global variables.
  DEPTH_LOCAL,       //< Local scope. Increase with inner scope.
} Depth;

typedef enum {
  FN_NATIVE,    //< Native C function.
  FN_SCRIPT,    //< Script level functions defined with 'def'.
  FN_LITERAL,   //< Literal functions defined with 'function(){...}'
} FuncType;

typedef struct {
  const char* name; //< Directly points into the source string.
  uint32_t length;  //< Length of the name.
  int depth;        //< The depth the local is defined in.
  int line;         //< The line variable declared for debugging.
} Local;

typedef struct sLoop {

  // Index of the loop's start instruction where the execution will jump
  // back to once it reach the loop end or continue used.
  int start;

  // Index of the jump out address instruction to patch it's value once done
  // compiling the loop.
  int exit_jump;

  // Array of address indexes to patch break address.
  int patches[MAX_BREAK_PATCH];
  int patch_count;

  // The outer loop of the current loop used to set and reset the compiler's
  // current loop context.
  struct sLoop* outer_loop;

  // Depth of the loop, required to pop all the locals in that loop when it
  // met a break/continue statement inside.
  int depth;

} Loop;

// To keep track of names used but not defined yet. This is only used for
// functions, because variables can't be accessed before it ever defined.
typedef struct sForwardName {

  // Index of the short instruction that has the value of the name (in the
  // names buffer of the script).
  int instruction;

  // The function where the name is used, and the instruction is belongs to.
  Fn* func;

  // The name string's pointer in the source.
  const char* name;
  int length;

  // Line number of the name used (required for error message).
  int line;

} ForwardName;

typedef struct sFunc {

  // Scope of the function. -2 for script body, -1 for top level function and
  // literal functions will have the scope where it declared.
  int depth;

  // The actual function pointer which is being compiled.
  Function* ptr;

  // The index of the function in its module.
  int index;

  // If outer function of a literal or the script body function of a script
  // function. Null for script body function.
  struct sFunc* outer_func;

} Func;

// A convenient macro to get the current function.
#define _FN (compiler->func->ptr->fn)

struct Compiler {

  PKVM* vm;
  Compiler* next_compiler;

  // Variables related to parsing.
  const char* source;       //< Currently compiled source (Weak pointer).
  const char* token_start;  //< Start of the currently parsed token.
  const char* current_char; //< Current char position in the source.
  int current_line;         //< Line number of the current char.
  Token previous, current, next; //< Currently parsed tokens.

  bool has_errors;          //< True if any syntex error occurred at.
  bool need_more_lines;     //< True if we need more lines in REPL mode.

  const PkCompileOptions* options; //< To configure the compilation.

  // Current depth the compiler in (-1 means top level) 0 means function
  // level and > 0 is inner scope.
  int scope_depth;

  Local locals[MAX_VARIABLES]; //< Variables in the current context.
  int local_count; //< Number of locals in [locals].

  int stack_size;  //< Current size including locals ind temps.

  Script* script;  //< Current script (a weak pointer).
  Loop* loop;      //< Current loop.
  Func* func;      //< Current function.

  // An array of implicitly forward declared names, which will be resolved once
  // the script is completely compiled.
  ForwardName forwards[MAX_FORWARD_NAMES];
  int forwards_count;

  // True if the last statement is a new local variable assignment. Because
  // the assignment is different than regular assignment and use this boolean
  // to tell the compiler that dont pop it's assigned value because the value
  // itself is the local.
  bool new_local;

  // Will be true when parsing an "l-value" which can be assigned to a value
  // using the assignment operator ('='). ie. 'a = 42' here a is an "l-value"
  // and the 42 is a "r-value" so the assignment is consumed and compiled.
  // Consider '42 = a' where 42 is a "r-value" which cannot be assigned.
  // Similarly 'a = 1 + b = 2' the expression '(1 + b)' is a "r value" and
  // the assignment here is invalid, however 'a = 1 + (b = 2)' is valid because
  // the 'b' is an "l-value" and can be assigned but the '(b = 2)' is a
  // "r-value".
  bool l_value;

  // This will set to true after parsing a call expression, and will be reset
  // to false before calling an infix rule. If this is true, that means the
  // last expression that was parsed with by compileExpression() is a function
  // call. Which is usefull to check if a return expression is function call
  // to perform a tail call optimization.
  bool is_last_call;
};

typedef struct {
  int params;
  int stack;
} OpInfo;

static OpInfo opcode_info[] = {
  #define OPCODE(name, params, stack) { params, stack },
  #include "pk_opcodes.h"
  #undef OPCODE
};

/*****************************************************************************/
/* ERROR HANDLERS                                                            */
/*****************************************************************************/

// Internal error report function of the parseError() function.
static void reportError(Compiler* compiler, const char* file, int line,
                        const char* fmt, va_list args) {

  // On REPL mode only the first error is reported.
  if (compiler->options && compiler->options->repl_mode &&
      compiler->has_errors) {
    return;
  }

  compiler->has_errors = true;

  // If the source is incomplete we're not printing an error message,
  // instead return PK_RESULT_UNEXPECTED_EOF to the host.
  if (compiler->need_more_lines) {
    ASSERT(compiler->options && compiler->options->repl_mode, OOPS);
    return;
  }

  PKVM* vm = compiler->vm;
  if (vm->config.error_fn == NULL) return;

  // TODO: fix the buffer size. A non terminated large string could cause this
  // crash.

  char message[ERROR_MESSAGE_SIZE];
  int length = vsnprintf(message, sizeof(message), fmt, args);
  __ASSERT(length >= 0, "Error message buffer failed at vsnprintf().");
  vm->config.error_fn(vm, PK_ERROR_COMPILE, file, line, message);
}

// Error caused at the middle of lexing (and TK_ERROR will be lexed insted).
static void lexError(Compiler* compiler, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const char* path = compiler->script->path->data;
  reportError(compiler, path, compiler->current_line, fmt, args);
  va_end(args);
}

// Error caused when parsing. The associated token assumed to be last consumed
// which is [compiler->previous].
static void parseError(Compiler* compiler, const char* fmt, ...) {

  Token* token = &compiler->previous;

  // Lex errors would repored earlier by lexError and lexed a TK_ERROR token.
  if (token->type == TK_ERROR) return;

  va_list args;
  va_start(args, fmt);
  const char* path = compiler->script->path->data;
  reportError(compiler, path, token->line, fmt, args);
  va_end(args);
}

// Error caused when trying to resolve forward names (maybe more in the
// future), Which will be called once after compiling the script and thus we
// need to pass the line number the error originated from.
static void resolveError(Compiler* compiler, int line, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const char* path = compiler->script->path->data;
  reportError(compiler, path, line, fmt, args);
  va_end(args);
}

/*****************************************************************************/
/* LEXING                                                                    */
/*****************************************************************************/

// Forward declaration of lexer methods.

static char eatChar(Compiler* compiler);
static void setNextValueToken(Compiler* compiler, TokenType type, Var value);
static void setNextToken(Compiler* compiler, TokenType type);
static bool matchChar(Compiler* compiler, char c);
static bool matchLine(Compiler* compiler);

static void eatString(Compiler* compiler, bool single_quote) {
  pkByteBuffer buff;
  pkByteBufferInit(&buff);

  char quote = (single_quote) ? '\'' : '"';

  while (true) {
    char c = eatChar(compiler);

    if (c == quote) break;

    if (c == '\0') {
      lexError(compiler, "Non terminated string.");

      // Null byte is required by TK_EOF.
      compiler->current_char--;
      break;
    }

    if (c == '\\') {
      switch (eatChar(compiler)) {
        case '"':  pkByteBufferWrite(&buff, compiler->vm, '"'); break;
        case '\'': pkByteBufferWrite(&buff, compiler->vm, '\''); break;
        case '\\': pkByteBufferWrite(&buff, compiler->vm, '\\'); break;
        case 'n':  pkByteBufferWrite(&buff, compiler->vm, '\n'); break;
        case 'r':  pkByteBufferWrite(&buff, compiler->vm, '\r'); break;
        case 't':  pkByteBufferWrite(&buff, compiler->vm, '\t'); break;

        default:
          lexError(compiler, "Error: invalid escape character");
          break;
      }
    } else {
      pkByteBufferWrite(&buff, compiler->vm, c);
    }
  }

  // '\0' will be added by varNewSring();
  Var string = VAR_OBJ(newStringLength(compiler->vm, (const char*)buff.data,
                       (uint32_t)buff.count));

  pkByteBufferClear(&buff, compiler->vm);

  setNextValueToken(compiler, TK_STRING, string);
}

// Returns the current char of the compiler on.
static char peekChar(Compiler* compiler) {
  return *compiler->current_char;
}

// Returns the next char of the compiler on.
static char peekNextChar(Compiler* compiler) {
  if (peekChar(compiler) == '\0') return '\0';
  return *(compiler->current_char + 1);
}

// Advance the compiler by 1 char.
static char eatChar(Compiler* compiler) {
  char c = peekChar(compiler);
  compiler->current_char++;
  if (c == '\n') compiler->current_line++;
  return c;
}

// Complete lexing an identifier name.
static void eatName(Compiler* compiler) {

  char c = peekChar(compiler);
  while (utilIsName(c) || utilIsDigit(c)) {
    eatChar(compiler);
    c = peekChar(compiler);
  }

  const char* name_start = compiler->token_start;

  TokenType type = TK_NAME;

  int length = (int)(compiler->current_char - name_start);
  for (int i = 0; _keywords[i].identifier != NULL; i++) {
    if (_keywords[i].length == length &&
      strncmp(name_start, _keywords[i].identifier, length) == 0) {
      type = _keywords[i].tk_type;
      break;
    }
  }

  setNextToken(compiler, type);
}

// Complete lexing a number literal.
static void eatNumber(Compiler* compiler) {

#define IS_HEX_CHAR(c)            \
  (('0' <= (c) && (c) <= '9')  || \
   ('a' <= (c) && (c) <= 'f'))

#define IS_BIN_CHAR(c) (((c) == '0') || ((c) == '1'))

  Var value = VAR_NULL; // The number value.
  char c = *compiler->token_start;

  // Binary literal.
  if (c == '0' && peekChar(compiler) == 'b') {
    eatChar(compiler); // Consume '0b'

    uint64_t bin = 0;
    c = peekChar(compiler);
    if (!IS_BIN_CHAR(c)) {
      lexError(compiler, "Invalid binary literal.");

    } else {
      do {
        // Consume the next digit.
        c = peekChar(compiler);
        if (!IS_BIN_CHAR(c)) break;
        eatChar(compiler);

        // Check the length of the binary literal.
        int length = (int)(compiler->current_char - compiler->token_start);
        if (length > STR_BIN_BUFF_SIZE - 2) { // -2: '-\0' 0b is in both side.
          lexError(compiler, "Binary literal is too long.");
          break;
        }

        // "Append" the next digit at the end.
        bin = (bin << 1) | (c - '0');

      } while (true);
    }
    value = VAR_NUM((double)bin);

  } else if (c == '0' && peekChar(compiler) == 'x') {
    eatChar(compiler); // Consume '0x'

    uint64_t hex = 0;
    c = peekChar(compiler);

    // The first digit should be either hex digit.
    if (!IS_HEX_CHAR(c)) {
      lexError(compiler, "Invalid hex literal.");

    } else {
      do {
        // Consume the next digit.
        c = peekChar(compiler);
        if (!IS_HEX_CHAR(c)) break;
        eatChar(compiler);

        // Check the length of the binary literal.
        int length = (int)(compiler->current_char - compiler->token_start);
        if (length > STR_HEX_BUFF_SIZE - 2) { // -2: '-\0' 0x is in both side.
          lexError(compiler, "Hex literal is too long.");
          break;
        }

        // "Append" the next digit at the end.
        uint8_t append_val = ('0' <= c && c <= '9')
          ? (uint8_t)(c - '0')
          : (uint8_t)((c - 'a') + 10);
        hex = (hex << 4) | append_val;

      } while (true);

      value = VAR_NUM((double)hex);
    }

  } else { // Regular number literal.
    while (utilIsDigit(peekChar(compiler))) {
      eatChar(compiler);
    }

    if (peekChar(compiler) == '.' && utilIsDigit(peekNextChar(compiler))) {
      matchChar(compiler, '.');
      while (utilIsDigit(peekChar(compiler))) {
        eatChar(compiler);
      }
    }

    // Parse if in scientific notation format (MeN == M * 10 ** N).
    if (matchChar(compiler, 'e') || matchChar(compiler, 'E')) {

      if (peekChar(compiler) == '+' || peekChar(compiler) == '-') {
        eatChar(compiler);
      }

      if (!utilIsDigit(peekChar(compiler))) {
        lexError(compiler, "Invalid number literal.");

      } else { // Eat the exponent.
        while (utilIsDigit(peekChar(compiler))) eatChar(compiler);
      }
    }

    errno = 0;
    value = VAR_NUM(atof(compiler->token_start));
    if (errno == ERANGE) {
      const char* start = compiler->token_start;
      int len = (int)(compiler->current_char - start);
      lexError(compiler, "Number literal is too large (%.*s).", len, start);
      value = VAR_NUM(0);
    }
  }

  setNextValueToken(compiler, TK_NUMBER, value);
#undef IS_BIN_CHAR
#undef IS_HEX_CHAR
}

// Read and ignore chars till it reach new line or EOF.
static void skipLineComment(Compiler* compiler) {
  char c;
  while ((c = peekChar(compiler)) != '\0') {
    // Don't eat new line it's not part of the comment.
    if (c == '\n') return;
    eatChar(compiler);
  }
}

// If the current char is [c] consume it and advance char by 1 and returns
// true otherwise returns false.
static bool matchChar(Compiler* compiler, char c) {
  if (peekChar(compiler) != c) return false;
  eatChar(compiler);
  return true;
}

// If the current char is [c] eat the char and add token two otherwise eat
// append token one.
static void setNextTwoCharToken(Compiler* compiler, char c, TokenType one,
  TokenType two) {
  if (matchChar(compiler, c)) {
    setNextToken(compiler, two);
  } else {
    setNextToken(compiler, one);
  }
}

// Initialize the next token as the type.
static void setNextToken(Compiler* compiler, TokenType type) {
  Token* next = &compiler->next;
  next->type = type;
  next->start = compiler->token_start;
  next->length = (int)(compiler->current_char - compiler->token_start);
  next->line = compiler->current_line - ((type == TK_LINE) ? 1 : 0);
}

// Initialize the next token as the type and assign the value.
static void setNextValueToken(Compiler* compiler, TokenType type, Var value) {
  setNextToken(compiler, type);
  compiler->next.value = value;
}

// Lex the next token and set it as the next token.
static void lexToken(Compiler* compiler) {
  compiler->previous = compiler->current;
  compiler->current = compiler->next;

  if (compiler->current.type == TK_EOF) return;

  while (peekChar(compiler) != '\0') {
    compiler->token_start = compiler->current_char;
    char c = eatChar(compiler);

    switch (c) {
      case ',': setNextToken(compiler, TK_COMMA); return;
      case ':': setNextToken(compiler, TK_COLLON); return;
      case ';': setNextToken(compiler, TK_SEMICOLLON); return;
      case '#': skipLineComment(compiler); break;
      case '(': setNextToken(compiler, TK_LPARAN); return;
      case ')': setNextToken(compiler, TK_RPARAN); return;
      case '[': setNextToken(compiler, TK_LBRACKET); return;
      case ']': setNextToken(compiler, TK_RBRACKET); return;
      case '{': setNextToken(compiler, TK_LBRACE); return;
      case '}': setNextToken(compiler, TK_RBRACE); return;
      case '%':
        setNextTwoCharToken(compiler, '=', TK_PERCENT, TK_MODEQ);
        return;

      case '~': setNextToken(compiler, TK_TILD); return;

      case '&':
        setNextTwoCharToken(compiler, '=', TK_AMP, TK_ANDEQ);
        return;

      case '|':
        setNextTwoCharToken(compiler, '=', TK_PIPE, TK_OREQ);
        return;

      case '^':
        setNextTwoCharToken(compiler, '=', TK_CARET, TK_XOREQ);
        return;

      case '\n': setNextToken(compiler, TK_LINE); return;

      case ' ':
      case '\t':
      case '\r': {
        c = peekChar(compiler);
        while (c == ' ' || c == '\t' || c == '\r') {
          eatChar(compiler);
          c = peekChar(compiler);
        }
        break;
      }

      case '.':
        if (matchChar(compiler, '.')) {
          if (matchChar(compiler, '.')) {
            setNextToken(compiler, TK_DOTDOTDOT); // '...'
          } else {
            setNextToken(compiler, TK_DOTDOT); // '..'
          }
        } else if (utilIsDigit(peekChar(compiler))) {
          eatChar(compiler);   // Consume the decimal point.
          eatNumber(compiler); // Consume the rest of the number
        } else {
          setNextToken(compiler, TK_DOT);    // '.'
        }
        return;

      case '=':
        setNextTwoCharToken(compiler, '=', TK_EQ, TK_EQEQ);
        return;

      case '!':
        setNextTwoCharToken(compiler, '=', TK_NOT, TK_NOTEQ);
        return;

      case '>':
        if (matchChar(compiler, '>')) {
          if (matchChar(compiler, '=')) {
            setNextToken(compiler, TK_SRIGHTEQ);
          } else {
            setNextToken(compiler, TK_SRIGHT);
          }
        } else {
          setNextTwoCharToken(compiler, '=', TK_GT, TK_GTEQ);
        }
        return;

      case '<':
        if (matchChar(compiler, '<')) {
          if (matchChar(compiler, '=')) {
            setNextToken(compiler, TK_SLEFTEQ);
          } else {
            setNextToken(compiler, TK_SLEFT);
          }
        } else {
          setNextTwoCharToken(compiler, '=', TK_LT, TK_LTEQ);
        }
        return;

      case '+':
        setNextTwoCharToken(compiler, '=', TK_PLUS, TK_PLUSEQ);
        return;

      case '-':
        if (matchChar(compiler, '=')) {
          setNextToken(compiler, TK_MINUSEQ);  // '-='
        } else if (matchChar(compiler, '>')) {
          setNextToken(compiler, TK_ARROW);    // '->'
        } else {
          setNextToken(compiler, TK_MINUS);    // '-'
        }
        return;

      case '*':
        setNextTwoCharToken(compiler, '=', TK_STAR, TK_STAREQ);
        return;

      case '/':
        setNextTwoCharToken(compiler, '=', TK_FSLASH, TK_DIVEQ);
        return;

      case '"': eatString(compiler, false); return;

      case '\'': eatString(compiler, true); return;

      default: {

        if (utilIsDigit(c)) {
          eatNumber(compiler);

        } else if (utilIsName(c)) {
          eatName(compiler);

        } else {
          if (c >= 32 && c <= 126) {
            lexError(compiler, "Invalid character '%c'", c);
          } else {
            lexError(compiler, "Invalid byte 0x%x", (uint8_t)c);
          }
          setNextToken(compiler, TK_ERROR);
        }
        return;
      }
    }
  }

  setNextToken(compiler, TK_EOF);
  compiler->next.start = compiler->current_char;
}

/*****************************************************************************/
/* PARSING                                                                   */
/*****************************************************************************/

// Returns current token type without lexing a new token.
static TokenType peek(Compiler* self) {
  return self->current.type;
}

// Consume the current token if it's expected and lex for the next token
// and return true otherwise return false.
static bool match(Compiler* self, TokenType expected) {
  if (peek(self) != expected) return false;
  lexToken(self);
  return true;
}

// Consume the the current token and if it's not [expected] emits error log
// and continue parsing for more error logs.
static void consume(Compiler* self, TokenType expected, const char* err_msg) {

  lexToken(self);
  if (self->previous.type != expected) {
    parseError(self, "%s", err_msg);

    // If the next token is expected discard the current to minimize
    // cascaded errors and continue parsing.
    if (peek(self) == expected) {
      lexToken(self);
    }
  }
}

// Match one or more lines and return true if there any.
static bool matchLine(Compiler* compiler) {

  bool consumed = false;

  if (peek(compiler) == TK_LINE) {
    while (peek(compiler) == TK_LINE)
      lexToken(compiler);
    consumed = true;
  }

  // If we're running on REPL mode, at the EOF and compile time error occurred,
  // signal the host to get more lines and try re-compiling it.
  if (compiler->options && compiler->options->repl_mode &&
    !compiler->has_errors) {
    if (peek(compiler) == TK_EOF) {
      compiler->need_more_lines = true;
    }
  }

  return consumed;
}

// Will skip multiple new lines.
static void skipNewLines(Compiler* compiler) {
  matchLine(compiler);
}

// Match semi collon, multiple new lines or peek 'end', 'else', 'elsif'
// keywords.
static bool matchEndStatement(Compiler* compiler) {
  if (match(compiler, TK_SEMICOLLON)) {
    skipNewLines(compiler);
    return true;
  }
  if (matchLine(compiler) || peek(compiler) == TK_EOF)
    return true;

  // In the below statement we don't require any new lines or semicolons.
  // 'if cond then stmnt1 elsif cond2 then stmnt2 else stmnt3 end'
  if (peek(compiler) == TK_END || peek(compiler) == TK_ELSE ||
      peek(compiler) == TK_ELSIF)
    return true;

  return false;
}

// Consume semi collon, multiple new lines or peek 'end' keyword.
static void consumeEndStatement(Compiler* compiler) {
  if (!matchEndStatement(compiler)) {
    parseError(compiler, "Expected statement end with '\\n' or ';'.");
  }
}

// Match optional "do" or "then" keyword and new lines.
static void consumeStartBlock(Compiler* compiler, TokenType delimiter) {
  bool consumed = false;

  // Match optional "do" or "then".
  if (delimiter == TK_DO || delimiter == TK_THEN) {
    if (match(compiler, delimiter))
      consumed = true;
  }

  if (matchLine(compiler))
    consumed = true;

  if (!consumed) {
    const char* msg;
    if (delimiter == TK_DO) msg = "Expected enter block with newline or 'do'.";
    else msg = "Expected enter block with newline or 'then'.";
    parseError(compiler, msg);
  }
}

// Returns a optional compound assignment.
static bool matchAssignment(Compiler* compiler) {
  if (match(compiler, TK_EQ))       return true;
  if (match(compiler, TK_PLUSEQ))   return true;
  if (match(compiler, TK_MINUSEQ))  return true;
  if (match(compiler, TK_STAREQ))   return true;
  if (match(compiler, TK_DIVEQ))    return true;
  if (match(compiler, TK_MODEQ))    return true;
  if (match(compiler, TK_ANDEQ))    return true;
  if (match(compiler, TK_OREQ))     return true;
  if (match(compiler, TK_XOREQ))    return true;
  if (match(compiler, TK_SRIGHTEQ)) return true;
  if (match(compiler, TK_SLEFTEQ))  return true;

  return false;
}

/*****************************************************************************/
/* NAME SEARCH                                                               */
/*****************************************************************************/

// Result type for an identifier definition.
typedef enum {
  NAME_NOT_DEFINED,
  NAME_LOCAL_VAR,  //< Including parameter.
  NAME_GLOBAL_VAR,
  NAME_FUNCTION,
  NAME_CLASS,
  NAME_BUILTIN,    //< Native builtin function.
} NameDefnType;

// Identifier search result.
typedef struct {

  NameDefnType type;

  // Index in the variable/function buffer/array.
  int index;

  // The line it declared.
  int line;

} NameSearchResult;

// Will check if the name already defined.
static NameSearchResult compilerSearchName(Compiler* compiler,
  const char* name, uint32_t length) {

  NameSearchResult result;
  result.type = NAME_NOT_DEFINED;

  for (int i = compiler->local_count - 1; i >= 0; i--) {
    Local* local = &compiler->locals[i];
    ASSERT(local->depth != DEPTH_GLOBAL, OOPS);

    // Literal functions are not closures and ignore it's outer function's
    // local variables.
    if (compiler->func->depth >= local->depth) {
      continue;
    }

    if (length == local->length) {
      if (strncmp(local->name, name, length) == 0) {
        result.type = NAME_LOCAL_VAR;
        result.index = i;
        return result;
      }
    }
  }

  int index; // For storing the search result below.

  // Search through globals.
  index = scriptGetGlobals(compiler->script, name, length);
  if (index != -1) {
    result.type = NAME_GLOBAL_VAR;
    result.index = index;
    return result;
  }

  // Search through classes.
  index = scriptGetClass(compiler->script, name, length);
  if (index != -1) {
    result.type = NAME_CLASS;
    result.index = index;
    return result;
  }

  // Search through functions.
  index = scriptGetFunc(compiler->script, name, length);
  if (index != -1) {
    result.type = NAME_FUNCTION;
    result.index = index;
    return result;
  }

  // Search through builtin functions.
  index = findBuiltinFunction(compiler->vm, name, length);
  if (index != -1) {
    result.type = NAME_BUILTIN;
    result.index = index;
    return result;
  }

  return result;
}

/*****************************************************************************/
/* PARSING GRAMMAR                                                           */
/*****************************************************************************/

// Forward declaration of codegen functions.
static void emitOpcode(Compiler* compiler, Opcode opcode);
static int emitByte(Compiler* compiler, int byte);
static int emitShort(Compiler* compiler, int arg);

static void emitLoopJump(Compiler* compiler);
static void emitAssignment(Compiler* compiler, TokenType assignment);
static void emitFunctionEnd(Compiler* compiler);

static void patchJump(Compiler* compiler, int addr_index);
static void patchForward(Compiler* compiler, Fn* fn, int index, int name);

static int compilerAddConstant(Compiler* compiler, Var value);
static int compilerAddVariable(Compiler* compiler, const char* name,
                               uint32_t length, int line);
static void compilerAddForward(Compiler* compiler, int instruction, Fn* fn,
                               const char* name, int length, int line);

// Forward declaration of grammar functions.
static void parsePrecedence(Compiler* compiler, Precedence precedence);
static int compileFunction(Compiler* compiler, FuncType fn_type);
static void compileExpression(Compiler* compiler);

static void exprLiteral(Compiler* compiler);
static void exprFunc(Compiler* compiler);
static void exprName(Compiler* compiler);

static void exprOr(Compiler* compiler);
static void exprAnd(Compiler* compiler);

static void exprChainCall(Compiler* compiler);
static void exprBinaryOp(Compiler* compiler);
static void exprUnaryOp(Compiler* compiler);

static void exprGrouping(Compiler* compiler);
static void exprList(Compiler* compiler);
static void exprMap(Compiler* compiler);

static void exprCall(Compiler* compiler);
static void exprAttrib(Compiler* compiler);
static void exprSubscript(Compiler* compiler);

// true, false, null, self.
static void exprValue(Compiler* compiler);

#define NO_RULE { NULL,          NULL,          PREC_NONE }
#define NO_INFIX PREC_NONE

GrammarRule rules[] = {  // Prefix       Infix             Infix Precedence
  /* TK_ERROR      */   NO_RULE,
  /* TK_EOF        */   NO_RULE,
  /* TK_LINE       */   NO_RULE,
  /* TK_DOT        */ { NULL,          exprAttrib,       PREC_ATTRIB },
  /* TK_DOTDOT     */ { NULL,          exprBinaryOp,     PREC_RANGE },
  /* TK_DOTDOTDOT  */ { NULL,          exprBinaryOp,     PREC_RANGE },
  /* TK_COMMA      */   NO_RULE,
  /* TK_COLLON     */   NO_RULE,
  /* TK_SEMICOLLON */   NO_RULE,
  /* TK_HASH       */   NO_RULE,
  /* TK_LPARAN     */ { exprGrouping,  exprCall,         PREC_CALL },
  /* TK_RPARAN     */   NO_RULE,
  /* TK_LBRACKET   */ { exprList,      exprSubscript,    PREC_SUBSCRIPT },
  /* TK_RBRACKET   */   NO_RULE,
  /* TK_LBRACE     */ { exprMap,       NULL,             NO_INFIX },
  /* TK_RBRACE     */   NO_RULE,
  /* TK_PERCENT    */ { NULL,          exprBinaryOp,     PREC_FACTOR },
  /* TK_TILD       */ { exprUnaryOp,   NULL,             NO_INFIX },
  /* TK_AMP        */ { NULL,          exprBinaryOp,     PREC_BITWISE_AND },
  /* TK_PIPE       */ { NULL,          exprBinaryOp,     PREC_BITWISE_OR },
  /* TK_CARET      */ { NULL,          exprBinaryOp,     PREC_BITWISE_XOR },
  /* TK_ARROW      */ { NULL,          exprChainCall,    PREC_CHAIN_CALL },
  /* TK_PLUS       */ { NULL,          exprBinaryOp,     PREC_TERM },
  /* TK_MINUS      */ { exprUnaryOp,   exprBinaryOp,     PREC_TERM },
  /* TK_STAR       */ { NULL,          exprBinaryOp,     PREC_FACTOR },
  /* TK_FSLASH     */ { NULL,          exprBinaryOp,     PREC_FACTOR },
  /* TK_BSLASH     */   NO_RULE,
  /* TK_EQ         */   NO_RULE,
  /* TK_GT         */ { NULL,          exprBinaryOp,     PREC_COMPARISION },
  /* TK_LT         */ { NULL,          exprBinaryOp,     PREC_COMPARISION },
  /* TK_EQEQ       */ { NULL,          exprBinaryOp,     PREC_EQUALITY },
  /* TK_NOTEQ      */ { NULL,          exprBinaryOp,     PREC_EQUALITY },
  /* TK_GTEQ       */ { NULL,          exprBinaryOp,     PREC_COMPARISION },
  /* TK_LTEQ       */ { NULL,          exprBinaryOp,     PREC_COMPARISION },
  /* TK_PLUSEQ     */   NO_RULE,
  /* TK_MINUSEQ    */   NO_RULE,
  /* TK_STAREQ     */   NO_RULE,
  /* TK_DIVEQ      */   NO_RULE,
  /* TK_MODEQ      */   NO_RULE,
  /* TK_ANDEQ      */   NO_RULE,
  /* TK_OREQ       */   NO_RULE,
  /* TK_XOREQ      */   NO_RULE,
  /* TK_SRIGHT     */ { NULL,          exprBinaryOp,     PREC_BITWISE_SHIFT },
  /* TK_SLEFT      */ { NULL,          exprBinaryOp,     PREC_BITWISE_SHIFT },
  /* TK_SRIGHTEQ   */   NO_RULE,
  /* TK_SLEFTEQ    */   NO_RULE,
  /* TK_MODULE     */   NO_RULE,
  /* TK_CLASS      */   NO_RULE,
  /* TK_FROM       */   NO_RULE,
  /* TK_IMPORT     */   NO_RULE,
  /* TK_AS         */   NO_RULE,
  /* TK_DEF        */   NO_RULE,
  /* TK_EXTERN     */   NO_RULE,
  /* TK_FUNC       */ { exprFunc,      NULL,             NO_INFIX },
  /* TK_END        */   NO_RULE,
  /* TK_NULL       */ { exprValue,     NULL,             NO_INFIX },
  /* TK_IN         */ { NULL,          exprBinaryOp,     PREC_TEST },
  /* TK_AND        */ { NULL,          exprAnd,          PREC_LOGICAL_AND },
  /* TK_OR         */ { NULL,          exprOr,           PREC_LOGICAL_OR },
  /* TK_NOT        */ { exprUnaryOp,   NULL,             PREC_UNARY },
  /* TK_TRUE       */ { exprValue,     NULL,             NO_INFIX },
  /* TK_FALSE      */ { exprValue,     NULL,             NO_INFIX },
  /* TK_DO         */   NO_RULE,
  /* TK_THEN       */   NO_RULE,
  /* TK_WHILE      */   NO_RULE,
  /* TK_FOR        */   NO_RULE,
  /* TK_IF         */   NO_RULE,
  /* TK_ELSIF      */   NO_RULE,
  /* TK_ELSE       */   NO_RULE,
  /* TK_BREAK      */   NO_RULE,
  /* TK_CONTINUE   */   NO_RULE,
  /* TK_RETURN     */   NO_RULE,
  /* TK_NAME       */ { exprName,      NULL,             NO_INFIX },
  /* TK_NUMBER     */ { exprLiteral,   NULL,             NO_INFIX },
  /* TK_STRING     */ { exprLiteral,   NULL,             NO_INFIX },
};

static GrammarRule* getRule(TokenType type) {
  return &(rules[(int)type]);
}

// Emit variable store.
static void emitStoreVariable(Compiler* compiler, int index, bool global) {
  if (global) {
    emitOpcode(compiler, OP_STORE_GLOBAL);
    emitByte(compiler, index);

  } else {
    if (index < 9) { //< 0..8 locals have single opcode.
      emitOpcode(compiler, (Opcode)(OP_STORE_LOCAL_0 + index));
    } else {
      emitOpcode(compiler, OP_STORE_LOCAL_N);
      emitByte(compiler, index);
    }
  }
}

static void emitPushVariable(Compiler* compiler, int index, bool global) {
  if (global) {
    emitOpcode(compiler, OP_PUSH_GLOBAL);
    emitByte(compiler, index);

  } else {
    if (index < 9) { //< 0..8 locals have single opcode.
      emitOpcode(compiler, (Opcode)(OP_PUSH_LOCAL_0 + index));
    } else {
      emitOpcode(compiler, OP_PUSH_LOCAL_N);
      emitByte(compiler, index);
    }
  }
}

static void exprLiteral(Compiler* compiler) {
  Token* value = &compiler->previous;
  int index = compilerAddConstant(compiler, value->value);
  emitOpcode(compiler, OP_PUSH_CONSTANT);
  emitShort(compiler, index);

  compiler->is_last_call = false;
}

static void exprFunc(Compiler* compiler) {
  int fn_index = compileFunction(compiler, FN_LITERAL);
  emitOpcode(compiler, OP_PUSH_FN);
  emitByte(compiler, fn_index);

  compiler->is_last_call = false;
}

// Local/global variables, script/native/builtin functions name.
static void exprName(Compiler* compiler) {

  const char* start = compiler->previous.start;
  int length = compiler->previous.length;
  int line = compiler->previous.line;
  NameSearchResult result = compilerSearchName(compiler, start, length);

  if (result.type == NAME_NOT_DEFINED) {
    if (compiler->l_value && match(compiler, TK_EQ)) {
      skipNewLines(compiler);

      int index = compilerAddVariable(compiler, start, length, line);

      // Compile the assigned value.
      compileExpression(compiler);

      // Store the value to the variable.
      if (compiler->scope_depth == DEPTH_GLOBAL) {
        emitStoreVariable(compiler, index, true);

      } else {
        // This will prevent the assignment from being popped out from the
        // stack since the assigned value itself is the local and not a temp.
        compiler->new_local = true;
        emitStoreVariable(compiler, index, false);
      }
    } else {

      // The name could be a function which hasn't been defined at this point.
      if (peek(compiler) == TK_LPARAN) {
        emitOpcode(compiler, OP_PUSH_FN);
        int index = emitByte(compiler, 0xff);
        compilerAddForward(compiler, index, _FN, start, length, line);
      } else {
        parseError(compiler, "Name '%.*s' is not defined.", length, start);
      }
    }

  } else {

    switch (result.type) {
      case NAME_LOCAL_VAR:
      case NAME_GLOBAL_VAR: {
        const bool is_global = result.type == NAME_GLOBAL_VAR;

        if (compiler->l_value && matchAssignment(compiler)) {
          skipNewLines(compiler);

          TokenType assignment = compiler->previous.type;
          if (assignment != TK_EQ) {
            emitPushVariable(compiler, result.index, is_global);
            compileExpression(compiler);
            emitAssignment(compiler, assignment);

          } else {
            compileExpression(compiler);
          }

          emitStoreVariable(compiler, result.index, is_global);

        } else {
          emitPushVariable(compiler, result.index, is_global);
        }
        break;
      }

      case NAME_FUNCTION:
        emitOpcode(compiler, OP_PUSH_FN);
        emitByte(compiler, result.index);
        break;

      case NAME_CLASS:
        emitOpcode(compiler, OP_PUSH_TYPE);
        emitByte(compiler, result.index);
        break;

      case NAME_BUILTIN:
        emitOpcode(compiler, OP_PUSH_BUILTIN_FN);
        emitByte(compiler, result.index);
        break;

      case NAME_NOT_DEFINED:
        UNREACHABLE(); // Case already handled.
    }
  }

  compiler->is_last_call = false;
}

/*           a or b:             |        a and b:
                                 |
            (...)                |           (...)
        .-- jump_if [offset]     |       .-- jump_if_not [offset]
        |   (...)                |       |   (...)
        |-- jump_if [offset]     |       |-- jump_if_not [offset]
        |   push false           |       |   push true
     .--+-- jump [offset]        |    .--+-- jump [offset]
     |  '-> push true            |    |  '-> push false
     '----> (...)                |    '----> (...)
*/

void exprOr(Compiler* compiler) {
  emitOpcode(compiler, OP_JUMP_IF);
  int true_offset_a = emitShort(compiler, 0xffff); //< Will be patched.

  parsePrecedence(compiler, PREC_LOGICAL_OR);
  emitOpcode(compiler, OP_JUMP_IF);
  int true_offset_b = emitShort(compiler, 0xffff); //< Will be patched.

  emitOpcode(compiler, OP_PUSH_FALSE);
  emitOpcode(compiler, OP_JUMP);
  int end_offset = emitShort(compiler, 0xffff); //< Will be patched.

  patchJump(compiler, true_offset_a);
  patchJump(compiler, true_offset_b);
  emitOpcode(compiler, OP_PUSH_TRUE);

  patchJump(compiler, end_offset);

  compiler->is_last_call = false;
}

void exprAnd(Compiler* compiler) {
  emitOpcode(compiler, OP_JUMP_IF_NOT);
  int false_offset_a = emitShort(compiler, 0xffff); //< Will be patched.

  parsePrecedence(compiler, PREC_LOGICAL_AND);
  emitOpcode(compiler, OP_JUMP_IF_NOT);
  int false_offset_b = emitShort(compiler, 0xffff); //< Will be patched.

  emitOpcode(compiler, OP_PUSH_TRUE);
  emitOpcode(compiler, OP_JUMP);
  int end_offset = emitShort(compiler, 0xffff); //< Will be patched.

  patchJump(compiler, false_offset_a);
  patchJump(compiler, false_offset_b);
  emitOpcode(compiler, OP_PUSH_FALSE);

  patchJump(compiler, end_offset);

  compiler->is_last_call = false;
}

static void exprChainCall(Compiler* compiler) {
  skipNewLines(compiler);
  parsePrecedence(compiler, (Precedence)(PREC_CHAIN_CALL + 1));
  emitOpcode(compiler, OP_SWAP); // Swap the data with the function.

  int argc = 1; // The initial data.

  if (match(compiler, TK_LBRACE)) {
    if (!match(compiler, TK_RBRACE)) {
      do {
        skipNewLines(compiler);
        compileExpression(compiler);
        skipNewLines(compiler);
        argc++;
      } while (match(compiler, TK_COMMA));
      consume(compiler, TK_RBRACE, "Expected '}' after chain call "
                                   "parameter list.");
    }
  }

  // TODO: ensure argc < 256 (MAX_ARGC) 1byte.

  emitOpcode(compiler, OP_CALL);
  emitByte(compiler, argc);

  compiler->is_last_call = false;
}

static void exprBinaryOp(Compiler* compiler) {
  TokenType op = compiler->previous.type;
  skipNewLines(compiler);
  parsePrecedence(compiler, (Precedence)(getRule(op)->precedence + 1));

  switch (op) {
    case TK_DOTDOT:    emitOpcode(compiler, OP_RANGE);   break;
    case TK_DOTDOTDOT: emitOpcode(compiler, OP_RANGE);   break;
    case TK_PERCENT:   emitOpcode(compiler, OP_MOD);        break;
    case TK_AMP:       emitOpcode(compiler, OP_BIT_AND);    break;
    case TK_PIPE:      emitOpcode(compiler, OP_BIT_OR);     break;
    case TK_CARET:     emitOpcode(compiler, OP_BIT_XOR);    break;
    case TK_PLUS:      emitOpcode(compiler, OP_ADD);        break;
    case TK_MINUS:     emitOpcode(compiler, OP_SUBTRACT);   break;
    case TK_STAR:      emitOpcode(compiler, OP_MULTIPLY);   break;
    case TK_FSLASH:    emitOpcode(compiler, OP_DIVIDE);     break;
    case TK_GT:        emitOpcode(compiler, OP_GT);         break;
    case TK_LT:        emitOpcode(compiler, OP_LT);         break;
    case TK_EQEQ:      emitOpcode(compiler, OP_EQEQ);       break;
    case TK_NOTEQ:     emitOpcode(compiler, OP_NOTEQ);      break;
    case TK_GTEQ:      emitOpcode(compiler, OP_GTEQ);       break;
    case TK_LTEQ:      emitOpcode(compiler, OP_LTEQ);       break;
    case TK_SRIGHT:    emitOpcode(compiler, OP_BIT_RSHIFT); break;
    case TK_SLEFT:     emitOpcode(compiler, OP_BIT_LSHIFT); break;
    case TK_IN:        emitOpcode(compiler, OP_IN);         break;
    default:
      UNREACHABLE();
  }

  compiler->is_last_call = false;
}

static void exprUnaryOp(Compiler* compiler) {
  TokenType op = compiler->previous.type;
  skipNewLines(compiler);
  parsePrecedence(compiler, (Precedence)(PREC_UNARY + 1));

  switch (op) {
    case TK_TILD:  emitOpcode(compiler, OP_BIT_NOT); break;
    case TK_MINUS: emitOpcode(compiler, OP_NEGATIVE); break;
    case TK_NOT:   emitOpcode(compiler, OP_NOT); break;
    default:
      UNREACHABLE();
  }

  compiler->is_last_call = false;
}

static void exprGrouping(Compiler* compiler) {
  skipNewLines(compiler);
  compileExpression(compiler);
  skipNewLines(compiler);
  consume(compiler, TK_RPARAN, "Expected ')' after expression.");

  compiler->is_last_call = false;
}

static void exprList(Compiler* compiler) {

  emitOpcode(compiler, OP_PUSH_LIST);
  int size_index = emitShort(compiler, 0);

  int size = 0;
  do {
    skipNewLines(compiler);
    if (peek(compiler) == TK_RBRACKET) break;

    compileExpression(compiler);
    emitOpcode(compiler, OP_LIST_APPEND);
    size++;

    skipNewLines(compiler);
  } while (match(compiler, TK_COMMA));

  skipNewLines(compiler);
  consume(compiler, TK_RBRACKET, "Expected ']' after list elements.");

  _FN->opcodes.data[size_index] = (size >> 8) & 0xff;
  _FN->opcodes.data[size_index + 1] = size & 0xff;

  compiler->is_last_call = false;
}

static void exprMap(Compiler* compiler) {
  emitOpcode(compiler, OP_PUSH_MAP);

  do {
    skipNewLines(compiler);
    if (peek(compiler) == TK_RBRACE) break;

    compileExpression(compiler);
    consume(compiler, TK_COLLON, "Expected ':' after map's key.");
    compileExpression(compiler);

    emitOpcode(compiler, OP_MAP_INSERT);

    skipNewLines(compiler);
  } while (match(compiler, TK_COMMA));

  skipNewLines(compiler);
  consume(compiler, TK_RBRACE, "Expected '}' after map elements.");

  compiler->is_last_call = false;
}

static void exprCall(Compiler* compiler) {

  // Compile parameters.
  int argc = 0;
  if (!match(compiler, TK_RPARAN)) {
    do {
      skipNewLines(compiler);
      compileExpression(compiler);
      skipNewLines(compiler);
      argc++;
    } while (match(compiler, TK_COMMA));
    consume(compiler, TK_RPARAN, "Expected ')' after parameter list.");
  }

  emitOpcode(compiler, OP_CALL);
  emitByte(compiler, argc);

  compiler->is_last_call = true;
}

static void exprAttrib(Compiler* compiler) {
  consume(compiler, TK_NAME, "Expected an attribute name after '.'.");
  const char* name = compiler->previous.start;
  int length = compiler->previous.length;

  // Store the name in script's names.
  int index = scriptAddName(compiler->script, compiler->vm, name, length);

  if (compiler->l_value && matchAssignment(compiler)) {
    skipNewLines(compiler);

    TokenType assignment = compiler->previous.type;
    if (assignment != TK_EQ) {
      emitOpcode(compiler, OP_GET_ATTRIB_KEEP);
      emitShort(compiler, index);
      compileExpression(compiler);
      emitAssignment(compiler, assignment);
    } else {
      compileExpression(compiler);
    }

    emitOpcode(compiler, OP_SET_ATTRIB);
    emitShort(compiler, index);

  } else {
    emitOpcode(compiler, OP_GET_ATTRIB);
    emitShort(compiler, index);
  }

  compiler->is_last_call = false;
}

static void exprSubscript(Compiler* compiler) {
  compileExpression(compiler);
  consume(compiler, TK_RBRACKET, "Expected ']' after subscription ends.");

  if (compiler->l_value && matchAssignment(compiler)) {
    skipNewLines(compiler);

    TokenType assignment = compiler->previous.type;
    if (assignment != TK_EQ) {
      emitOpcode(compiler, OP_GET_SUBSCRIPT_KEEP);
      compileExpression(compiler);
      emitAssignment(compiler, assignment);

    } else {
      compileExpression(compiler);
    }

    emitOpcode(compiler, OP_SET_SUBSCRIPT);

  } else {
    emitOpcode(compiler, OP_GET_SUBSCRIPT);
  }
  compiler->is_last_call = false;
}

static void exprValue(Compiler* compiler) {
  TokenType op = compiler->previous.type;
  switch (op) {
    case TK_NULL:  emitOpcode(compiler, OP_PUSH_NULL);  break;
    case TK_TRUE:  emitOpcode(compiler, OP_PUSH_TRUE);  break;
    case TK_FALSE: emitOpcode(compiler, OP_PUSH_FALSE); break;
    default:
      UNREACHABLE();
  }

  compiler->is_last_call = false;
}

static void parsePrecedence(Compiler* compiler, Precedence precedence) {
  lexToken(compiler);
  GrammarFn prefix = getRule(compiler->previous.type)->prefix;

  if (prefix == NULL) {
    parseError(compiler, "Expected an expression.");
    return;
  }

  // Reset to false and this will set to true by the exprCall() function,
  // If the next infix is call '('.
  compiler->is_last_call = false;
  compiler->l_value = precedence <= PREC_LOWEST;

  prefix(compiler);

  while (getRule(compiler->current.type)->precedence >= precedence) {
    lexToken(compiler);
    GrammarFn infix = getRule(compiler->previous.type)->infix;
    infix(compiler);
  }
}

/*****************************************************************************/
/* COMPILING                                                                 */
/*****************************************************************************/

static void compilerInit(Compiler* compiler, PKVM* vm, const char* source,
                         Script* script, const PkCompileOptions* options) {

  compiler->vm = vm;
  compiler->next_compiler = NULL;

  compiler->source = source;
  compiler->script = script;
  compiler->token_start = source;
  compiler->has_errors = false;
  compiler->need_more_lines = false;
  compiler->options = options;

  compiler->current_char = source;
  compiler->current_line = 1;
  compiler->next.type = TK_ERROR;
  compiler->next.start = NULL;
  compiler->next.length = 0;
  compiler->next.line = 1;
  compiler->next.value = VAR_UNDEFINED;

  compiler->scope_depth = DEPTH_GLOBAL;
  compiler->local_count = 0;
  compiler->stack_size = 0;

  compiler->loop = NULL;
  compiler->func = NULL;

  compiler->forwards_count = 0;
  compiler->new_local = false;
  compiler->is_last_call = false;
}

// Add a variable and return it's index to the context. Assumes that the
// variable name is unique and not defined before in the current scope.
static int compilerAddVariable(Compiler* compiler, const char* name,
                                uint32_t length, int line) {

  // TODO: should I validate the name for pre-defined, etc?

  // Check if maximum variable count is reached.
  bool max_vars_reached = false;
  const char* var_type = ""; // For max variables reached error message.
  if (compiler->scope_depth == DEPTH_GLOBAL) {
    if (compiler->script->globals.count >= MAX_VARIABLES) {
      max_vars_reached = true;
      var_type = "globals";
    }
  } else {
    if (compiler->local_count >= MAX_VARIABLES) {
      max_vars_reached = true;
      var_type = "locals";
    }
  }
  if (max_vars_reached) {
    parseError(compiler, "A script should contain at most %d %s.",
               MAX_VARIABLES, var_type);
    return -1;
  }

  // Add the variable and return it's index.

  if (compiler->scope_depth == DEPTH_GLOBAL) {
    return (int)scriptAddGlobal(compiler->vm, compiler->script,
                                name, length, VAR_NULL);
  } else {
    Local* local = &compiler->locals [compiler->local_count];
    local->name = name;
    local->length = length;
    local->depth = compiler->scope_depth;
    local->line = line;
    return compiler->local_count++;
  }

  UNREACHABLE();
}

static void compilerAddForward(Compiler* compiler, int instruction, Fn* fn,
                               const char* name, int length, int line) {
  if (compiler->forwards_count == MAX_FORWARD_NAMES) {
    parseError(compiler, "A script should contain at most %d implicit forward "
               "function declarations.", MAX_FORWARD_NAMES);
    return;
  }

  ForwardName* forward = &compiler->forwards[compiler->forwards_count++];
  forward->instruction = instruction;
  forward->func = fn;
  forward->name = name;
  forward->length = length;
  forward->line = line;
}

// Add a literal constant to scripts literals and return it's index.
static int compilerAddConstant(Compiler* compiler, Var value) {
  pkVarBuffer* literals = &compiler->script->literals;

  for (uint32_t i = 0; i < literals->count; i++) {
    if (isValuesSame(literals->data[i], value)) {
      return i;
    }
  }

  // Add new constant to script.
  if (literals->count < MAX_CONSTANTS) {
    pkVarBufferWrite(literals, compiler->vm, value);
  } else {
    parseError(compiler, "A script should contain at most %d "
               "unique constants.", MAX_CONSTANTS);
  }
  return (int)literals->count - 1;
}

// Enters inside a block.
static void compilerEnterBlock(Compiler* compiler) {
  compiler->scope_depth++;
}

// Change the stack size by the [num], if it's positive, the stack will
// grow otherwise it'll shrink.
static void compilerChangeStack(Compiler* compiler, int num) {
  compiler->stack_size += num;

  // If the compiler has error (such as undefined name, that will not popped
  // because of the semantic error but it'll be popped once the expression
  // parsing is done.
  if (!compiler->has_errors) ASSERT(compiler->stack_size >= 0, OOPS);

  if (compiler->stack_size > _FN->stack_size) {
    _FN->stack_size = compiler->stack_size;
  }
}

// Write instruction to pop all the locals at the current [depth] or higher,
// but it won't change the stack size of locals count because this function
// is called by break/continue statements at the middle of a scope, so we need
// those locals till the scope ends. This will returns the number of locals
// that were popped.
static int compilerPopLocals(Compiler* compiler, int depth) {
  ASSERT(depth > (int)DEPTH_GLOBAL, "Cannot pop global variables.");

  int local = compiler->local_count - 1;
  while (local >= 0 && compiler->locals[local].depth >= depth) {

    // Note: Do not use emitOpcode(compiler, OP_POP);
    // Because this function is called at the middle of a scope (break,
    // continue). So we need the pop instruction here but we still need the
    // locals to continue parsing the next statements in the scope. They'll be
    // popped once the scope is ended.
    emitByte(compiler, OP_POP);

    local--;
  }
  return (compiler->local_count - 1) - local;
}

// Exits a block.
static void compilerExitBlock(Compiler* compiler) {
  ASSERT(compiler->scope_depth > (int)DEPTH_GLOBAL, "Cannot exit toplevel.");

  // Discard all the locals at the current scope.
  int popped = compilerPopLocals(compiler, compiler->scope_depth);
  compiler->local_count -= popped;
  compiler->stack_size -= popped;
  compiler->scope_depth--;
}

static void compilerPushFunc(Compiler* compiler, Func* fn,
                             Function* func, int index) {
  fn->outer_func = compiler->func;
  fn->ptr = func;
  fn->depth = compiler->scope_depth;
  fn->index = index;
  compiler->func = fn;
}

static void compilerPopFunc(Compiler* compiler) {
  compiler->func = compiler->func->outer_func;
}

/*****************************************************************************/
/* COMPILING (EMIT BYTECODE)                                                 */
/*****************************************************************************/

// Emit a single byte and return it's index.
static int emitByte(Compiler* compiler, int byte) {

  pkByteBufferWrite(&_FN->opcodes, compiler->vm,
                    (uint8_t)byte);
  pkUintBufferWrite(&_FN->oplines, compiler->vm,
                   compiler->previous.line);
  return (int)_FN->opcodes.count - 1;
}

// Emit 2 bytes argument as big indian. return it's starting index.
static int emitShort(Compiler* compiler, int arg) {
  emitByte(compiler, (arg >> 8) & 0xff);
  return emitByte(compiler, arg & 0xff) - 1;
}

// Emits an instruction and update stack size (variable stack size opcodes
// should be handled).
static void emitOpcode(Compiler* compiler, Opcode opcode) {
  emitByte(compiler, (int)opcode);
  compilerChangeStack(compiler, opcode_info[opcode].stack);
}

// Jump back to the start of the loop.
static void emitLoopJump(Compiler* compiler) {
  emitOpcode(compiler, OP_LOOP);
  int offset = (int)_FN->opcodes.count - compiler->loop->start + 2;
  emitShort(compiler, offset);
}

static void emitAssignment(Compiler* compiler, TokenType assignment) {
  switch (assignment) {
    case TK_PLUSEQ:   emitOpcode(compiler, OP_ADD);        break;
    case TK_MINUSEQ:  emitOpcode(compiler, OP_SUBTRACT);   break;
    case TK_STAREQ:   emitOpcode(compiler, OP_MULTIPLY);   break;
    case TK_DIVEQ:    emitOpcode(compiler, OP_DIVIDE);     break;
    case TK_MODEQ:    emitOpcode(compiler, OP_MOD);        break;
    case TK_ANDEQ:    emitOpcode(compiler, OP_BIT_AND);    break;
    case TK_OREQ:     emitOpcode(compiler, OP_BIT_OR);     break;
    case TK_XOREQ:    emitOpcode(compiler, OP_BIT_XOR);    break;
    case TK_SRIGHTEQ: emitOpcode(compiler, OP_BIT_RSHIFT); break;
    case TK_SLEFTEQ:  emitOpcode(compiler, OP_BIT_LSHIFT); break;
    default:
      UNREACHABLE();
      break;
  }
}

static void emitFunctionEnd(Compiler* compiler) {

  // Don't use emitOpcode(compiler, OP_RETURN); Because it'll recude the stack
  // size by -1, (return value will be popped). We really don't have to pop the
  // return value of the function, when returning from the end of the function,
  // because there'll always be a null value at the base of the current call
  // frame as the return value of the function.
  emitByte(compiler, OP_RETURN);

  emitOpcode(compiler, OP_END);
}

// Update the jump offset.
static void patchJump(Compiler* compiler, int addr_index) {
  int offset = (int)_FN->opcodes.count - (addr_index + 2 /*bytes index*/);
  ASSERT(offset < MAX_JUMP, "Too large address offset to jump to.");

  _FN->opcodes.data[addr_index] = (offset >> 8) & 0xff;
  _FN->opcodes.data[addr_index + 1] = offset & 0xff;
}

static void patchForward(Compiler* compiler, Fn* fn, int index, int name) {
  fn->opcodes.data[index] = name & 0xff;
}

/*****************************************************************************/
/* COMPILING (PARSE TOPLEVEL)                                                */
/*****************************************************************************/

typedef enum {
  BLOCK_FUNC,
  BLOCK_LOOP,
  BLOCK_IF,
  BLOCK_ELSE,
} BlockType;

static void compileStatement(Compiler* compiler);
static void compileBlockBody(Compiler* compiler, BlockType type);

// Compile a type and return it's index in the script's types buffer.
static int compileType(Compiler* compiler) {

  // Consume the name of the type.
  consume(compiler, TK_NAME, "Expected a type name.");
  const char* name = compiler->previous.start;
  int name_len = compiler->previous.length;
  NameSearchResult result = compilerSearchName(compiler, name, name_len);
  if (result.type != NAME_NOT_DEFINED) {
    parseError(compiler, "Name '%.*s' already exists.", name_len, name);
  }

  // Create a new type.
  Class* type = newClass(compiler->vm, compiler->script,
                         name, (uint32_t)name_len);
  type->ctor->arity = 0;

  // Check count exceeded.
  int fn_index = (int)compiler->script->functions.count - 1;
  if (fn_index == MAX_FUNCTIONS) {
    parseError(compiler, "A script should contain at most %d functions.",
               MAX_FUNCTIONS);
  }

  int ty_index = (int)(compiler->script->classes.count - 1);
  if (ty_index == MAX_CLASSES) {
    parseError(compiler, "A script should contain at most %d types.",
               MAX_CLASSES);
  }

  // Compile the constructor function.
  ASSERT(compiler->func->ptr == compiler->script->body, OOPS);
  Func curr_fn;
  compilerPushFunc(compiler, &curr_fn, type->ctor, fn_index);
  compilerEnterBlock(compiler);

  // Push an instance on the stack.
  emitOpcode(compiler, OP_PUSH_INSTANCE);
  emitByte(compiler, ty_index);

  skipNewLines(compiler);
  TokenType next = peek(compiler);
  while (next != TK_END && next != TK_EOF) {

    // Compile field name.
    consume(compiler, TK_NAME, "Expected a type name.");
    const char* f_name = compiler->previous.start;
    int f_len = compiler->previous.length;

    uint32_t f_index = scriptAddName(compiler->script, compiler->vm,
                                     f_name, f_len);

    // TODO: Add a string compare macro.
    String* new_name = compiler->script->names.data[f_index];
    for (uint32_t i = 0; i < type->field_names.count; i++) {
      String* prev = compiler->script->names.data[type->field_names.data[i]];
      if (new_name->hash == prev->hash && new_name->length == prev->length &&
          memcmp(new_name->data, prev->data, prev->length) == 0) {
        parseError(compiler, "Class field with name '%s' already exists.",
                   new_name->data);
      }
    }

    pkUintBufferWrite(&type->field_names, compiler->vm, f_index);

    // Consume the assignment expression.
    consume(compiler, TK_EQ, "Expected an assignment after field name.");
    compileExpression(compiler); // Assigned value.
    consumeEndStatement(compiler);

    // At this point the stack top would be the expression.
    emitOpcode(compiler, OP_INST_APPEND);

    skipNewLines(compiler);
    next = peek(compiler);
  }
  consume(compiler, TK_END, "Expected 'end' after type declaration end.");

  compilerExitBlock(compiler);

  // At this point, the stack top would be the constructed instance. Return it.
  emitFunctionEnd(compiler);

  compilerPopFunc(compiler);

  return -1; // TODO;
}

// Compile a function and return it's index in the script's function buffer.
static int compileFunction(Compiler* compiler, FuncType fn_type) {

  const char* name;
  int name_length;

  if (fn_type != FN_LITERAL) {
    consume(compiler, TK_NAME, "Expected a function name.");
    name = compiler->previous.start;
    name_length = compiler->previous.length;
    NameSearchResult result = compilerSearchName(compiler, name, name_length);
    if (result.type != NAME_NOT_DEFINED) {
      parseError(compiler, "Name '%.*s' already exists.", name_length, name);
    }

  } else {
    name = LITERAL_FN_NAME;
    name_length = (int)strlen(name);
  }

  Function* func = newFunction(compiler->vm, name, name_length,
                               compiler->script, fn_type == FN_NATIVE, NULL);
  int fn_index = (int)compiler->script->functions.count - 1;
  if (fn_index == MAX_FUNCTIONS) {
    parseError(compiler, "A script should contain at most %d functions.",
               MAX_FUNCTIONS);
  }

  Func curr_fn;
  compilerPushFunc(compiler, &curr_fn, func, fn_index);

  int argc = 0;
  compilerEnterBlock(compiler); // Parameter depth.

  // Parameter list is optional.
  if (match(compiler, TK_LPARAN) && !match(compiler, TK_RPARAN)) {
    do {
      skipNewLines(compiler);

      consume(compiler, TK_NAME, "Expected a parameter name.");
      argc++;

      const char* param_name = compiler->previous.start;
      uint32_t param_len = compiler->previous.length;

      // TODO: move this to a functions.
      bool predefined = false;
      for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (compiler->scope_depth != local->depth) break;
        if (local->length == param_len &&
            strncmp(local->name, param_name, param_len) == 0) {
          predefined = true;
          break;
        }
      }
      if (predefined) {
        parseError(compiler, "Multiple definition of a parameter.");
      }

      compilerAddVariable(compiler, param_name, param_len,
                          compiler->previous.line);

    } while (match(compiler, TK_COMMA));

    consume(compiler, TK_RPARAN, "Expected ')' after parameter list.");
  }

  func->arity = argc;
  compilerChangeStack(compiler, argc);

  if (fn_type != FN_NATIVE) {
    compileBlockBody(compiler, BLOCK_FUNC);

    // Tail call optimization disabled at debug mode.
    if (compiler->options && !compiler->options->debug) {
      if (compiler->is_last_call) {
        ASSERT(_FN->opcodes.count >= 3, OOPS); // OP_CALL, argc, OP_POP
        ASSERT(_FN->opcodes.data[_FN->opcodes.count - 1] == OP_POP, OOPS);
        ASSERT(_FN->opcodes.data[_FN->opcodes.count - 3] == OP_CALL, OOPS);
        _FN->opcodes.data[_FN->opcodes.count - 3] = OP_TAIL_CALL;
      }
    }

    consume(compiler, TK_END, "Expected 'end' after function definition end.");
    compilerExitBlock(compiler); // Parameter depth.
    emitFunctionEnd(compiler);

  } else {
    compilerExitBlock(compiler); // Parameter depth.
  }

#if DEBUG_DUMP_COMPILED_CODE
  pkByteBuffer buff;
  pkByteBufferInit(&buff);
  dumpFunctionCode(compiler->vm, compiler->func->ptr, &buff);
  printf("%s", buff.data);
  pkByteBufferClear(&buff, compiler->vm);
#endif

  compilerPopFunc(compiler);

  return fn_index;
}

// Finish a block body.
static void compileBlockBody(Compiler* compiler, BlockType type) {

  compilerEnterBlock(compiler);

  if (type == BLOCK_IF) {
    consumeStartBlock(compiler, TK_THEN);
    skipNewLines(compiler);

  } else if (type == BLOCK_ELSE) {
    skipNewLines(compiler);

  } else if (type == BLOCK_FUNC) {
    // Function body doesn't require a 'do' or 'then' delimiter to enter.
    skipNewLines(compiler);

  } else {
    // For/While loop block body delimiter is 'do'.
    consumeStartBlock(compiler, TK_DO);
    skipNewLines(compiler);
  }

  TokenType next = peek(compiler);
  while (!(next == TK_END || next == TK_EOF || (
    (type == BLOCK_IF) && (next == TK_ELSE || next == TK_ELSIF)))) {

    compileStatement(compiler);
    skipNewLines(compiler);

    next = peek(compiler);
  }

  compilerExitBlock(compiler);
}

// Import a file at the given path (first it'll be resolved from the current
// path) and return it as a script pointer. And it'll emit opcodes to push
// that script to the stack.
static Script* importFile(Compiler* compiler, const char* path) {
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);

  PKVM* vm = compiler->vm;

  // Resolve the path.
  PkStringPtr resolved = { path, NULL, NULL };
  if (vm->config.resolve_path_fn != NULL) {
    resolved = vm->config.resolve_path_fn(vm, compiler->script->path->data,
                                          path);
  }

  if (resolved.string == NULL) {
    parseError(compiler, "Cannot resolve path '%s' from '%s'", path,
               compiler->script->path->data);
  }

  // Create new string for the resolved path. And free the resolved path.
  int index = (int)scriptAddName(compiler->script, compiler->vm,
                           resolved.string, (uint32_t)strlen(resolved.string));
  String* path_name = compiler->script->names.data[index];
  if (resolved.on_done != NULL) resolved.on_done(vm, resolved);

  // Check if the script already exists.
  Var entry = mapGet(vm->scripts, VAR_OBJ(path_name));
  if (!IS_UNDEF(entry)) {
    ASSERT(AS_OBJ(entry)->type == OBJ_SCRIPT, OOPS);

    // Push the script on the stack.
    emitOpcode(compiler, OP_IMPORT);
    emitShort(compiler, index);
    return (Script*)AS_OBJ(entry);
  }

  // The script not exists, make sure we have the script loading api function.
  if (vm->config.load_script_fn == NULL) {
    parseError(compiler, "Cannot import. The hosting application haven't "
               "registered the script loading API");
    return NULL;
  }

  // Load the script at the path.
  PkStringPtr source = vm->config.load_script_fn(vm, path_name->data);
  if (source.string == NULL) {
    parseError(compiler, "Error loading script at \"%s\"", path_name->data);
    return NULL;
  }

  // Make a new script and to compile it.
  Script* scr = newScript(vm, path_name, false);
  vmPushTempRef(vm, &scr->_super); // scr.
  mapSet(vm, vm->scripts, VAR_OBJ(path_name), VAR_OBJ(scr));
  vmPopTempRef(vm); // scr.

  // Push the script on the stack.
  emitOpcode(compiler, OP_IMPORT);
  emitShort(compiler, index);

  // Option for the compilation, even if we're running on repl mode the
  // imported script cannot run on repl mode.
  PkCompileOptions options = pkNewCompilerOptions();
  if (compiler->options) options = *compiler->options;
  options.repl_mode = false;

  // Compile the source to the script and clean the source.
  PkResult result = compile(vm, scr, source.string, &options);
  if (source.on_done != NULL) source.on_done(vm, source);

  if (result != PK_RESULT_SUCCESS) {
    parseError(compiler, "Compilation of imported script '%s' failed",
               path_name->data);
  }

  return scr;
}

// Import the core library from the vm's core_libs and it'll emit opcodes to
// push that script to the stack.
static Script* importCoreLib(Compiler* compiler, const char* name_start,
                             int name_length) {
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);

  // Add the name to the script's name buffer, we need it as a key to the
  // vm's script cache.
  int index = (int)scriptAddName(compiler->script, compiler->vm,
                                 name_start, name_length);
  String* module = compiler->script->names.data[index];

  Var entry = mapGet(compiler->vm->core_libs, VAR_OBJ(module));
  if (IS_UNDEF(entry)) {
    parseError(compiler, "No module named '%s' exists.", module->data);
    return NULL;
  }

  // Push the script on the stack.
  emitOpcode(compiler, OP_IMPORT);
  emitShort(compiler, index);

  ASSERT(AS_OBJ(entry)->type == OBJ_SCRIPT, OOPS);
  return (Script*)AS_OBJ(entry);
}

// Push the imported script on the stack and return the pointer. It could be
// either core library or a local import.
static inline Script* compilerImport(Compiler* compiler) {
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);

  // Get the script (from core libs or vm's cache or compile new one).
  // And push it on the stack.
  if (match(compiler, TK_NAME)) { //< Core library.
    return importCoreLib(compiler, compiler->previous.start,
                         compiler->previous.length);

  } else if (match(compiler, TK_STRING)) { //< Local library.
    Var var_path = compiler->previous.value;
    ASSERT(IS_OBJ_TYPE(var_path, OBJ_STRING), OOPS);
    String* path = (String*)AS_OBJ(var_path);
    return importFile(compiler, path->data);
  }

  // Invalid token after import/from keyword.
  parseError(compiler, "Expected a module name or path to import.");
  return NULL;
}

// Search for the name, and return it's index in the globals. If it's not
// exists in the globals it'll add a variable to the globals entry and return.
// But If the name is predefined function (cannot be modified). It'll set error
// and return -1.
static int compilerImportName(Compiler* compiler, int line,
                              const char* name, uint32_t length) {
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);

  NameSearchResult result = compilerSearchName(compiler, name, length);
  switch (result.type) {
    case NAME_NOT_DEFINED:
      return compilerAddVariable(compiler, name, length, line);

    case NAME_LOCAL_VAR:
      UNREACHABLE();

    case NAME_GLOBAL_VAR:
      return result.index;

    case NAME_FUNCTION:
    case NAME_CLASS:
    case NAME_BUILTIN:
      parseError(compiler, "Name '%.*s' already exists.", length, name);
      return -1;
  }

  UNREACHABLE();
}

// This will called by the compilerImportAll() function to import a single
// entry from the imported script. (could be a function or global variable).
static void compilerImportSingleEntry(Compiler* compiler,
                                      const char* name, uint32_t length) {

  // Special names are begins with '$' like function body (only for now).
  // Skip them.
  if (name[0] == '$') return;

  // Line number of the variables which will be bind to the imported symbol.
  int line = compiler->previous.line;

  // Add the name to the **current** script's name buffer.
  int name_index = (int)scriptAddName(compiler->script, compiler->vm,
                                      name, length);

  // Get the function from the script.
  emitOpcode(compiler, OP_GET_ATTRIB_KEEP);
  emitShort(compiler, name_index);

  int index = compilerImportName(compiler, line, name, length);
  if (index != -1) emitStoreVariable(compiler, index, true);
  emitOpcode(compiler, OP_POP);
}

// Import all from the script, which is also would be at the top of the stack
// before executing the below instructions.
static void compilerImportAll(Compiler* compiler, Script* script) {

  ASSERT(script != NULL, OOPS);
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);

  // Import all types.
  for (uint32_t i = 0; i < script->classes.count; i++) {
    uint32_t name_ind = script->classes.data[i]->name;
    String* name = script->names.data[name_ind];
    compilerImportSingleEntry(compiler, name->data, name->length);
  }

  // Import all functions.
  for (uint32_t i = 0; i < script->functions.count; i++) {
    const char* name = script->functions.data[i]->name;
    uint32_t length = (uint32_t)strlen(name);
    compilerImportSingleEntry(compiler, name, length);
  }

  // Import all globals.
  for (uint32_t i = 0; i < script->globals.count; i++) {
    ASSERT(i < script->global_names.count, OOPS);
    ASSERT(script->global_names.data[i] < script->names.count, OOPS);
    const String* name = script->names.data[script->global_names.data[i]];

    compilerImportSingleEntry(compiler, name->data, name->length);
  }
}

// from module import symbol [as alias [, symbol2 [as alias]]]
static void compileFromImport(Compiler* compiler) {
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);

  // Import the library and push it on the stack. If the import failed
  // lib_from would be NULL.
  Script* lib_from = compilerImport(compiler);

  // At this point the script would be on the stack before executing the next
  // instruction.
  consume(compiler, TK_IMPORT, "Expected keyword 'import'.");

  if (match(compiler, TK_STAR)) {
    // from math import *
    if (lib_from) compilerImportAll(compiler, lib_from);

  } else {
    do {
      // Consume the symbol name to import from the script.
      consume(compiler, TK_NAME, "Expected symbol to import.");
      const char* name = compiler->previous.start;
      uint32_t length = (uint32_t)compiler->previous.length;
      int line = compiler->previous.line;

      // Add the name of the symbol to the names buffer.
      int name_index = (int)scriptAddName(compiler->script, compiler->vm,
                                          name, length);

      // Don't pop the lib since it'll be used for the next entry.
      emitOpcode(compiler, OP_GET_ATTRIB_KEEP);
      emitShort(compiler, name_index); //< Name of the attrib.

      // Check if it has an alias.
      if (match(compiler, TK_AS)) {
        // Consuming it'll update the previous token which would be the name of
        // the binding variable.
        consume(compiler, TK_NAME, "Expected a name after 'as'.");
      }

      // Set the imported symbol binding name, which wold be in the last token
      // consumed by the first one or after the as keyword.
      name = compiler->previous.start;
      length = (uint32_t)compiler->previous.length;
      line = compiler->previous.line;

      // Get the variable to bind the imported symbol, if we already have a
      // variable with that name override it, otherwise use a new variable.
      int var_index = compilerImportName(compiler, line, name, length);
      if (var_index != -1) emitStoreVariable(compiler, var_index, true);
      emitOpcode(compiler, OP_POP);

    } while (match(compiler, TK_COMMA) && (skipNewLines(compiler), true));
  }

  // Done getting all the attributes, now pop the lib from the stack.
  emitOpcode(compiler, OP_POP);

  // Always end the import statement.
  consumeEndStatement(compiler);
}

static void compileRegularImport(Compiler* compiler) {
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);

  do {

    // Import the library and push it on the stack. If it cannot import,
    // the lib would be null, but we're not terminating here, just continue
    // parsing for cascaded errors.
    Script* lib = compilerImport(compiler);

    // variable to bind the imported script.
    int var_index = -1;

    // Check if it has an alias, if so bind the variable with that name.
    if (match(compiler, TK_AS)) {
      // Consuming it'll update the previous token which would be the name of
      // the binding variable.
      consume(compiler, TK_NAME, "Expected a name after 'as'.");

      // Get the variable to bind the imported symbol, if we already have a
      // variable with that name override it, otherwise use a new variable.
      const char* name = compiler->previous.start;
      int length = compiler->previous.length, line = compiler->previous.line;
      var_index = compilerImportName(compiler, line, name, length);

    } else {
      // If it has a module name use it as binding variable.
      // Core libs names are it's module name but for local libs it's optional
      // to define a module name for a script.
      if (lib && lib->module != NULL) {

        // Get the variable to bind the imported symbol, if we already have a
        // variable with that name override it, otherwise use a new variable.
        const char* name = lib->module->data;
        uint32_t length = lib->module->length;
        int line = compiler->previous.line;
        var_index = compilerImportName(compiler, line, name, length);

      } else {
        // -- Nothing to do here --
        // Importing from path which doesn't have a module name. Import
        // everything of it. and bind to a variables.
      }
    }

    if (var_index != -1) {
      emitStoreVariable(compiler, var_index, true);
      emitOpcode(compiler, OP_POP);

    } else {
      if (lib) compilerImportAll(compiler, lib);
      // Done importing everything from lib now pop the lib.
      emitOpcode(compiler, OP_POP);
    }

  } while (match(compiler, TK_COMMA) && (skipNewLines(compiler), true));

  consumeEndStatement(compiler);
}

// Compiles an expression. An expression will result a value on top of the
// stack.
static void compileExpression(Compiler* compiler) {
  parsePrecedence(compiler, PREC_LOWEST);
}

static void compileIfStatement(Compiler* compiler, bool elsif) {

  skipNewLines(compiler);
  compileExpression(compiler); //< Condition.
  emitOpcode(compiler, OP_JUMP_IF_NOT);
  int ifpatch = emitShort(compiler, 0xffff); //< Will be patched.

  compileBlockBody(compiler, BLOCK_IF);

  if (match(compiler, TK_ELSIF)) {

    // Jump pass else.
    emitOpcode(compiler, OP_JUMP);
    int exit_jump = emitShort(compiler, 0xffff); //< Will be patched.

    // if (false) jump here.
    patchJump(compiler, ifpatch);

    compilerEnterBlock(compiler);
    compileIfStatement(compiler, true);
    compilerExitBlock(compiler);

    patchJump(compiler, exit_jump);

  } else if (match(compiler, TK_ELSE)) {

    // Jump pass else.
    emitOpcode(compiler, OP_JUMP);
    int exit_jump = emitShort(compiler, 0xffff); //< Will be patched.

    patchJump(compiler, ifpatch);
    compileBlockBody(compiler, BLOCK_ELSE);
    patchJump(compiler, exit_jump);

  } else {
    patchJump(compiler, ifpatch);
  }

  // elsif will not consume the 'end' keyword as it'll be leaved to be consumed
  // by it's 'if'.
  if (!elsif) {
    skipNewLines(compiler);
    consume(compiler, TK_END, "Expected 'end' after statement end.");
  }
}

static void compileWhileStatement(Compiler* compiler) {
  Loop loop;
  loop.start = (int)_FN->opcodes.count;
  loop.patch_count = 0;
  loop.outer_loop = compiler->loop;
  loop.depth = compiler->scope_depth;
  compiler->loop = &loop;

  compileExpression(compiler); //< Condition.
  emitOpcode(compiler, OP_JUMP_IF_NOT);
  int whilepatch = emitShort(compiler, 0xffff); //< Will be patched.

  compileBlockBody(compiler, BLOCK_LOOP);

  emitLoopJump(compiler);
  patchJump(compiler, whilepatch);

  // Patch break statement.
  for (int i = 0; i < compiler->loop->patch_count; i++) {
    patchJump(compiler, compiler->loop->patches[i]);
  }
  compiler->loop = loop.outer_loop;

  skipNewLines(compiler);
  consume(compiler, TK_END, "Expected 'end' after statement end.");
}

static void compileForStatement(Compiler* compiler) {
  compilerEnterBlock(compiler);
  consume(compiler, TK_NAME, "Expected an iterator name.");

  // Unlike functions local variable could shadow a name.
  const char* iter_name = compiler->previous.start;
  int iter_len = compiler->previous.length;
  int iter_line = compiler->previous.line;

  consume(compiler, TK_IN, "Expected 'in' after iterator name.");

  // Compile and store sequence.
  compilerAddVariable(compiler, "@Sequence", 9, iter_line); // Sequence
  compileExpression(compiler);

  // Add iterator to locals. It's an increasing integer indicating that the
  // current loop is nth starting from 0.
  compilerAddVariable(compiler, "@iterator", 9, iter_line); // Iterator.
  emitOpcode(compiler, OP_PUSH_0);

  // Add the iteration value. It'll be updated to each element in an array of
  // each character in a string etc.
  compilerAddVariable(compiler, iter_name, iter_len, iter_line); // Iter value.
  emitOpcode(compiler, OP_PUSH_NULL);

  // Start the iteration, and check if the sequence is iterable.
  emitOpcode(compiler, OP_ITER_TEST);

  Loop loop;
  loop.start = (int)_FN->opcodes.count;
  loop.patch_count = 0;
  loop.outer_loop = compiler->loop;
  loop.depth = compiler->scope_depth;
  compiler->loop = &loop;

  // Compile next iteration.
  emitOpcode(compiler, OP_ITER);
  int forpatch = emitShort(compiler, 0xffff);

  compileBlockBody(compiler, BLOCK_LOOP);

  emitLoopJump(compiler); //< Loop back to iteration.
  patchJump(compiler, forpatch); //< Patch exit iteration address.

  // Patch break statement.
  for (int i = 0; i < compiler->loop->patch_count; i++) {
    patchJump(compiler, compiler->loop->patches[i]);
  }
  compiler->loop = loop.outer_loop;

  skipNewLines(compiler);
  consume(compiler, TK_END, "Expected 'end' after statement end.");
  compilerExitBlock(compiler); //< Iterator scope.
}

// Compiles a statement. Assignment could be an assignment statement or a new
// variable declaration, which will be handled.
static void compileStatement(Compiler* compiler) {

  // is_temproary will be set to true if the statement is an temporary
  // expression, it'll used to be pop from the stack.
  bool is_temproary = false;

  // This will be set to true if the statement is an expression. It'll used to
  // print it's value when running in REPL mode.
  bool is_expression = false;

  // If the statement is call, this will be set to true.
  compiler->is_last_call = false;

  if (match(compiler, TK_BREAK)) {
    if (compiler->loop == NULL) {
      parseError(compiler, "Cannot use 'break' outside a loop.");
      return;
    }

    ASSERT(compiler->loop->patch_count < MAX_BREAK_PATCH,
      "Too many break statements (" STRINGIFY(MAX_BREAK_PATCH) ")." );

    consumeEndStatement(compiler);
    // Pop all the locals at the loop's body depth.
    compilerPopLocals(compiler, compiler->loop->depth + 1);

    emitOpcode(compiler, OP_JUMP);
    int patch = emitShort(compiler, 0xffff); //< Will be patched.
    compiler->loop->patches[compiler->loop->patch_count++] = patch;

  } else if (match(compiler, TK_CONTINUE)) {
    if (compiler->loop == NULL) {
      parseError(compiler, "Cannot use 'continue' outside a loop.");
      return;
    }

    consumeEndStatement(compiler);
    // Pop all the locals at the loop's body depth.
    compilerPopLocals(compiler, compiler->loop->depth + 1);

    emitLoopJump(compiler);

  } else if (match(compiler, TK_RETURN)) {

    if (compiler->scope_depth == DEPTH_GLOBAL) {
      parseError(compiler, "Invalid 'return' outside a function.");
      return;
    }

    if (matchEndStatement(compiler)) {
      emitOpcode(compiler, OP_PUSH_NULL);
      emitOpcode(compiler, OP_RETURN);
    } else {
      compileExpression(compiler); //< Return value is at stack top.

      // Tail call optimization disabled at debug mode.
      if (compiler->options && !compiler->options->debug) {
        if (compiler->is_last_call) {
          ASSERT(_FN->opcodes.count >= 2, OOPS); // OP_CALL, argc
          ASSERT(_FN->opcodes.data[_FN->opcodes.count - 2] == OP_CALL, OOPS);
          _FN->opcodes.data[_FN->opcodes.count - 2] = OP_TAIL_CALL;

          // Now it's a return statement, not call.
          compiler->is_last_call = false;
        }
      }

      consumeEndStatement(compiler);
      emitOpcode(compiler, OP_RETURN);
    }
  } else if (match(compiler, TK_IF)) {
    compileIfStatement(compiler, false);

  } else if (match(compiler, TK_WHILE)) {
    compileWhileStatement(compiler);

  } else if (match(compiler, TK_FOR)) {
    compileForStatement(compiler);

  } else {
    compiler->new_local = false;
    compileExpression(compiler);
    consumeEndStatement(compiler);

    is_expression = true;
    if (!compiler->new_local) is_temproary = true;

    compiler->new_local = false;
  }

  // If running REPL mode, print the expression's evaluated value.
  if (compiler->options && compiler->options->repl_mode &&
      compiler->func->ptr == compiler->script->body &&
      is_expression /*&& compiler->scope_depth == DEPTH_GLOBAL*/) {
    emitOpcode(compiler, OP_REPL_PRINT);
  }

  if (is_temproary) emitOpcode(compiler, OP_POP);
}

// Compile statements that are only valid at the top level of the script. Such
// as import statement, function define, and if we're running REPL mode top
// level expression's evaluated value will be printed.
static void compileTopLevelStatement(Compiler* compiler) {

  // If the statement is call, this will be set to true.
  compiler->is_last_call = false;

  if (match(compiler, TK_CLASS)) {
    compileType(compiler);

  } else if (match(compiler, TK_NATIVE)) {
    compileFunction(compiler, FN_NATIVE);

  } else if (match(compiler, TK_DEF)) {
    compileFunction(compiler, FN_SCRIPT);

  } else if (match(compiler, TK_FROM)) {
    compileFromImport(compiler);

  } else if (match(compiler, TK_IMPORT)) {
    compileRegularImport(compiler);

  } else if (match(compiler, TK_MODULE)) {
    parseError(compiler, "Module name must be the first statement "
                          "of the script.");

  } else {
    compileStatement(compiler);
  }
}

PkResult compile(PKVM* vm, Script* script, const char* source,
                 const PkCompileOptions* options) {

  // Skip utf8 BOM if there is any.
  if (strncmp(source, "\xEF\xBB\xBF", 3) == 0) source += 3;

  Compiler _compiler;
  Compiler* compiler = &_compiler; //< Compiler pointer for quick access.
  compilerInit(compiler, vm, source, script, options);

  // If compiling for an imported script the vm->compiler would be the compiler
  // of the script that imported this script. Add the all the compilers into a
  // link list.
  compiler->next_compiler = vm->compiler;
  vm->compiler = compiler;

  // If the script doesn't has a body by default, it's probably was created by
  // the native api function (pkNewModule() that'll return a module without a
  // main function) so just create and add the function here.
  if (script->body == NULL) scriptAddMain(vm, script);

  // If we're compiling for a script that was already compiled (when running
  // REPL or evaluating an expression) we don't need the old main anymore.
  // just use the globals and functions of the script and use a new body func.
  pkByteBufferClear(&script->body->fn->opcodes, vm);

  // Remember the count of the globals, functions and types, If the compilation
  // failed discard all the globals and functions added by the compilation.
  uint32_t globals_count = script->globals.count;
  uint32_t functions_count = script->functions.count;
  uint32_t types_count = script->classes.count;

  Func curr_fn;
  curr_fn.depth = DEPTH_SCRIPT;
  curr_fn.ptr = script->body;
  curr_fn.outer_func = NULL;
  compiler->func = &curr_fn;

  // Lex initial tokens. current <-- next.
  lexToken(compiler);
  lexToken(compiler);
  skipNewLines(compiler);

  if (match(compiler, TK_MODULE)) {

    // If the script running a REPL or compiled multiple times by hosting
    // application module attribute might already set. In that case make it
    // Compile error.
    if (script->module != NULL) {
      parseError(compiler, "Module name already defined.");

    } else {
      consume(compiler, TK_NAME, "Expected a name for the module.");
      const char* name = compiler->previous.start;
      uint32_t len = compiler->previous.length;
      script->module = newStringLength(vm, name, len);
      consumeEndStatement(compiler);
    }
  }

  while (!match(compiler, TK_EOF)) {
    compileTopLevelStatement(compiler);
    skipNewLines(compiler);
  }

  emitFunctionEnd(compiler);

  // Resolve forward names (function names that are used before defined).
  for (int i = 0; i < compiler->forwards_count; i++) {
    ForwardName* forward = &compiler->forwards[i];
    const char* name = forward->name;
    int length = forward->length;
    int index = scriptGetFunc(script, name, (uint32_t)length);
    if (index != -1) {
      patchForward(compiler, forward->func, forward->instruction, index);
    } else {
      // need_more_lines is only true for unexpected EOF errors. For syntax
      // errors it'll be false by now but. Here it's a semantic errors, so
      // we're overriding it to false.
      compiler->need_more_lines = false;
      resolveError(compiler, forward->line, "Name '%.*s' is not defined.",
                   length, name);
    }
  }

  vm->compiler = compiler->next_compiler;

  // If compilation failed, discard all the invalid functions and globals.
  if (compiler->has_errors) {
    script->globals.count = script->global_names.count = globals_count;
    script->functions.count = functions_count;
    script->classes.count = types_count;
  }

#if DEBUG_DUMP_COMPILED_CODE
  pkByteBuffer buff;
  pkByteBufferInit(&buff);
  dumpFunctionCode(compiler->vm, script->body, &buff);
  printf("%s", buff.data);
  pkByteBufferClear(&buff, vm);
#endif

  // Return the compilation result.
  if (compiler->has_errors) {
    if (compiler->options && compiler->options->repl_mode &&
        compiler->need_more_lines) {
      return PK_RESULT_UNEXPECTED_EOF;
    }
    return PK_RESULT_COMPILE_ERROR;
  }
  return PK_RESULT_SUCCESS;
}

PkResult pkCompileModule(PKVM* vm, PkHandle* module, PkStringPtr source,
                         const PkCompileOptions* options) {
  __ASSERT(module != NULL, "Argument module was NULL.");
  Var scr = module->value;
  __ASSERT(IS_OBJ_TYPE(scr, OBJ_SCRIPT), "Given handle is not a module");
  Script* script = (Script*)AS_OBJ(scr);

  PkResult result = compile(vm, script, source.string, options);
  if (source.on_done) source.on_done(vm, source);
  return result;
}

void compilerMarkObjects(PKVM* vm, Compiler* compiler) {

  // Mark the script which is currently being compiled.
  markObject(vm, &compiler->script->_super);

  // Mark the string literals (they haven't added to the script's literal
  // buffer yet).
  markValue(vm, compiler->current.value);
  markValue(vm, compiler->previous.value);
  markValue(vm, compiler->next.value);

  if (compiler->next_compiler != NULL) {
    compilerMarkObjects(vm, compiler->next_compiler);
  }
}
