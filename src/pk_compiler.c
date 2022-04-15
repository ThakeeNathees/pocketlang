/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "pk_compiler.h"

#include "pk_core.h"
#include "pk_buffers.h"
#include "pk_utils.h"
#include "pk_vm.h"
#include "pk_debug.h"

// The maximum number of locals or global (if compiling top level module)
// to lookup from the compiling context. Also it's limited by it's opcode
// which is using a single byte value to identify the local.
#define MAX_VARIABLES 256

// The maximum number of constant literal a module can contain. Also it's
// limited by it's opcode which is using a short value to identify.
#define MAX_CONSTANTS (1 << 16)

// The maximum number of upvaues a literal function can capture from it's
// enclosing function.
#define MAX_UPVALUES 256

// The maximum number of names that were used before defined. Its just the size
// of the Forward buffer of the compiler. Feel free to increase it if it
// require more.
#define MAX_FORWARD_NAMES 256

// Pocketlang support two types of interpolation.
//
//   1. Name interpolation       ex: "Hello $name!"
//   2. Expression interpolation ex: "Hello ${getName()}!"
//
// Consider a string: "a ${ b "c ${d}" } e" -- Here the depth of 'b' is 1 and
// the depth of 'd' is 2 and so on. The maximum depth an expression can go is
// defined as MAX_STR_INTERP_DEPTH below.
#define MAX_STR_INTERP_DEPTH 8

// The maximum address possible to jump. Similar limitation as above.
#define MAX_JUMP (1 << 16)

// Max number of break statement in a loop statement to patch.
#define MAX_BREAK_PATCH 256

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

  /* String interpolation
   *  "a ${b} c $d e"
   * tokenized as:
   *   TK_STR_INTERP  "a "
   *   TK_NAME        b
   *   TK_STR_INTERP  " c "
   *   TK_NAME        d
   *   TK_STRING     " e" */
   TK_STRING_INTERP,

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

  { NULL,       0, (TokenType)(0) }, // Sentinel to mark the end of the array.
};

/*****************************************************************************/
/* COMPILER INTERNAL TYPES                                                   */
/*****************************************************************************/

// Precedence parsing references:
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
  PREC_RANGE,         // ..
  PREC_TERM,          // + -
  PREC_FACTOR,        // * / %
  PREC_UNARY,         // - ! ~ not
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
  DEPTH_MODULE = -2, //< Only used for module body function's depth.
  DEPTH_GLOBAL = -1, //< Global variables.
  DEPTH_LOCAL,       //< Local scope. Increase with inner scope.
} Depth;

typedef struct {
  const char* name; //< Directly points into the source string.
  uint32_t length;  //< Length of the name.
  int depth;        //< The depth the local is defined in.
  bool is_upvalue;  //< Is this an upvalue for a nested function.
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

// ForwardName is used for globals that are accessed before defined inside
// a local scope.
// TODO: Since function and class global variables are initialized at the
//       compile time we can allow access to them at the global scope.
typedef struct sForwardName {

  // Index of the short instruction that has the value of the global's name
  // (in the names buffer of the module).
  int instruction;

  // The function where the name is used, and the instruction is belongs to.
  Fn* func;

  // The name string's pointer in the source.
  const char* name;
  int length;

  // Line number of the name used (required for error message).
  int line;

} ForwardName;

// This struct is used to keep track about the information of the upvaues for
// the current function to generate opcodes to capture them.
typedef struct sUpvalueInfo {

  // If it's true the extrenal local belongs to the immediate enclosing
  // function and the bellow [index] refering at the locals of that function.
  // If it's false the external local of the upvalue doesn't belongs to the
  // immediate enclosing function and the [index] will refering to the upvalues
  // array of the enclosing function.
  bool is_immediate;

  // Index of the upvalue's external local variable, in the local or upvalues
  // array of the enclosing function.
  int index;

} UpvalueInfo;

typedef struct sFunc {

  // Scope of the function. -2 for module body function, -1 for top level
  // function and literal functions will have the scope where it declared.
  int depth;

  Local locals[MAX_VARIABLES]; //< Variables in the current context.
  int local_count; //< Number of locals in [locals].

  UpvalueInfo upvalues[MAX_UPVALUES]; //< Upvalues in the current context.

  int stack_size;  //< Current size including locals ind temps.

  // The actual function pointer which is being compiled.
  Function* ptr;

  // If outer function of this function, for top level function the outer
  // function will be the module's body function.
  struct sFunc* outer_func;

} Func;

// A convenient macro to get the current function.
#define _FN (compiler->func->ptr->fn)

// The context of the parsing phase for the compiler.
typedef struct sParser {

  // Parser need a reference of the PKVM to allocate strings (for string
  // literals in the source) and to report error if there is any.
  PKVM* vm;

  // The [source] and the [file_path] are pointers to an allocated string.
  // The parser doesn't keep references to that objects (to prevent them
  // from garbage collected). It's the compiler's responsibility to keep the
  // strings alive alive as long as the parser is alive.

  const char* source;       //< Currently compiled source.
  const char* file_path;    //< Path of the module (for reporting errors).
  const char* token_start;  //< Start of the currently parsed token.
  const char* current_char; //< Current char position in the source.
  int current_line;         //< Line number of the current char.
  Token previous, current, next; //< Currently parsed tokens.

  // The current depth of the string interpolation. 0 means we're not inside
  // an interpolated string.
  int si_depth;

  // If we're parsing an interpolated string and found a TK_RBRACE (ie. '}')
  // we need to know if that's belongs to the expression we're parsing, or the
  // end of the current interpolation.
  //
  // To achieve that We need to keep track of the number of open brace at the
  // current depth. If we don't have any open brace then the TK_RBRACE token
  // is consumed to end the interpolation.
  //
  // If we're inside an interpolated string (ie. si_depth > 0)
  // si_open_brace[si_depth - 1] will return the number of open brace at the
  // current depth.
  int si_open_brace[MAX_STR_INTERP_DEPTH];

  // Since we're supporting both quotes (single and double), we need to keep
  // track of the qoute the interpolation is surrounded by to properly
  // terminate the string.
  // here si_quote[si_depth - 1] will return the surrunded quote of the
  // expression at current depth.
  char si_quote[MAX_STR_INTERP_DEPTH];

  // When we're parsing a name interpolated string (ie. "Hello $name!") we
  // have to keep track of where the name ends to start the interpolation
  // from there. The below value [si_name_end] will be NULL if we're not
  // parsing a name interpolated string, otherwise it'll points to the end of
  // the name.
  //
  // Also we're using [si_name_quote] to store the quote of the string to
  // properly terminate.
  const char* si_name_end;
  char si_name_quote;

  // An array of implicitly forward declared names, which will be resolved once
  // the module is completely compiled.
  ForwardName forwards[MAX_FORWARD_NAMES];
  int forwards_count;

  bool repl_mode;       //< True if compiling for REPL.
  bool has_errors;      //< True if any syntex error occurred at.
  bool need_more_lines; //< True if we need more lines in REPL mode.

} Parser;

struct Compiler {

  // The parser of the compiler which contains all the parsing context for the
  // current compilation.
  Parser parser;

  // Each module will be compiled with it's own compiler and a module is
  // imported, a new compiler is created for that module and it'll be added to
  // the linked list of compilers at the begining. PKVM will use this compiler
  // reference as a root object (objects which won't garbage collected) and
  // the chain of compilers will be marked at the marking phase.
  //
  // Here is how the chain change when a new compiler (compiler_3) created.
  //
  //     PKVM -> compiler_2 -> compiler_1 -> NULL
  //
  //     PKVM -> compiler_3 -> compiler_2 -> compiler_1 -> NULL
  //
  Compiler* next_compiler;

  const PkCompileOptions* options; //< To configure the compilation.

  Module* module;  //< Current module that's being compiled.
  Loop* loop;      //< Current loop the we're parsing.
  Func* func;      //< Current function we're parsing.

  // Current depth the compiler in (-1 means top level) 0 means function
  // level and > 0 is inner scope.
  int scope_depth;

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

  // This value will be true after parsing a call expression, for every other
  // Expressions it'll be false. This is **ONLY** to be used when compiling a
  // return statement to check if the last parsed expression is a call to
  // perform a tail call optimization (anywhere else this below boolean is
  // meaningless).
  bool is_last_call;

  // Since the compiler manually call some builtin functions we need to cache
  // the index of the functions in order to prevent search for them each time.
  int bifn_list_join;
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
/* INITALIZATION FUNCTIONS                                                   */
/*****************************************************************************/

// FIXME:
// This forward declaration can be removed once the interpolated string's
// "list_join" function replaced with BUILD_STRING opcode. (The declaration
// needed at compiler initialization function to find the "list_join" function.
static int findBuiltinFunction(const PKVM* vm,
                               const char* name, uint32_t length);

// This should be called once the compiler initialized (to access it's fields).
static void parserInit(Parser* parser, PKVM* vm, Compiler* compiler,
                       const char* source, const char* path) {

  parser->vm = vm;

  parser->source = source;
  parser->file_path = path;
  parser->token_start = parser->source;
  parser->current_char = parser->source;
  parser->current_line = 1;

  parser->next.type = TK_ERROR;
  parser->next.start = NULL;
  parser->next.length = 0;
  parser->next.line = 1;
  parser->next.value = VAR_UNDEFINED;

  parser->si_depth = 0;
  parser->si_name_end = NULL;
  parser->si_name_quote = '\0';

  parser->forwards_count = 0;

  parser->repl_mode = !!(compiler->options && compiler->options->repl_mode);
  parser->has_errors = false;
  parser->need_more_lines = false;
}

static void compilerInit(Compiler* compiler, PKVM* vm, const char* source,
                         Module* module, const PkCompileOptions* options) {

  compiler->next_compiler = NULL;

  compiler->module = module;
  compiler->options = options;

  compiler->scope_depth = DEPTH_GLOBAL;

  compiler->loop = NULL;
  compiler->func = NULL;

  compiler->new_local = false;
  compiler->is_last_call = false;

  parserInit(&compiler->parser, vm, compiler, source, module->path->data);

  // Cache the required built functions.
  compiler->bifn_list_join = findBuiltinFunction(vm, "list_join", 9);
  ASSERT(compiler->bifn_list_join >= 0, OOPS);
}

/*****************************************************************************/
/* ERROR HANDLERS                                                            */
/*****************************************************************************/

// Internal error report function for lexing and parsing.
static void reportError(Parser* parser, const char* file, int line,
                        const char* fmt, va_list args) {

  // On REPL mode only the first error is reported.
  if (parser->repl_mode && parser->has_errors) {
    return;
  }

  parser->has_errors = true;

  // If the source is incomplete we're not printing an error message,
  // instead return PK_RESULT_UNEXPECTED_EOF to the host.
  if (parser->need_more_lines) {
    ASSERT(parser->repl_mode, OOPS);
    return;
  }

  if (parser->vm->config.error_fn == NULL) return;

  // TODO: fix the buffer size. A non terminated large string could cause this
  // crash.

  char message[ERROR_MESSAGE_SIZE];
  int length = vsnprintf(message, sizeof(message), fmt, args);
  __ASSERT(length >= 0, "Error message buffer failed at vsnprintf().");
  parser->vm->config.error_fn(parser->vm, PK_ERROR_COMPILE,
                              file, line, message);
}

// Error caused at the middle of lexing (and TK_ERROR will be lexed instead).
static void lexError(Parser* parser, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  reportError(parser, parser->file_path, parser->current_line, fmt, args);
  va_end(args);
}

// Error caused when parsing. The associated token assumed to be last consumed
// which is [parser->previous].
static void parseError(Compiler* compiler, const char* fmt, ...) {

  Token* token = &(compiler->parser.previous);

  // Lex errors would reported earlier by lexError and lexed a TK_ERROR token.
  if (token->type == TK_ERROR) return;

  va_list args;
  va_start(args, fmt);
  reportError(&(compiler->parser), compiler->parser.file_path,
              token->line, fmt, args);
  va_end(args);
}

// Error caused when trying to resolve forward names (maybe more in the
// future), Which will be called once after compiling the module and thus we
// need to pass the line number the error originated from.
static void resolveError(Compiler* compiler, int line, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  reportError(&(compiler->parser),
              compiler->parser.file_path,
              line, fmt, args);
  va_end(args);
}

// Check if the given [index] is greater than or equal to the maximum constants
// that a module can contain and report an error.
static void checkMaxConstantsReached(Compiler* compiler, int index) {
  ASSERT(index >= 0, OOPS);
  if (index >= MAX_CONSTANTS) {
    parseError(compiler, "A module should contain at most %d "
      "unique constants.", MAX_CONSTANTS);
  }
}

/*****************************************************************************/
/* LEXING                                                                    */
/*****************************************************************************/

// Forward declaration of lexer methods.

static char peekChar(Parser* parser);
static char peekNextChar(Parser* parser);
static char eatChar(Parser* parser);
static void setNextValueToken(Parser* parser, TokenType type, Var value);
static void setNextToken(Parser* parser, TokenType type);
static bool matchChar(Parser* parser, char c);

static void eatString(Parser* parser, bool single_quote) {
  pkByteBuffer buff;
  pkByteBufferInit(&buff);

  char quote = (single_quote) ? '\'' : '"';

  // For interpolated string it'll be TK_STRING_INTERP.
  TokenType tk_type = TK_STRING;

  while (true) {
    char c = eatChar(parser);

    if (c == quote) break;

    if (c == '\0') {
      lexError(parser, "Non terminated string.");

      // Null byte is required by TK_EOF.
      parser->current_char--;
      break;
    }

    if (c == '$') {
      if (parser->si_depth < MAX_STR_INTERP_DEPTH) {
        tk_type = TK_STRING_INTERP;

        char c = peekChar(parser);
        if (c == '{') { // Expression interpolation (ie. "${expr}").
          eatChar(parser);
          parser->si_depth++;
          parser->si_quote[parser->si_depth - 1] = quote;
          parser->si_open_brace[parser->si_depth - 1] = 0;

        } else { // Name Interpolation.
          if (!utilIsName(c)) {
            lexError(parser, "Expected '{' or identifier after '$'.");

          } else { // Name interpolation (ie. "Hello $name!").

            // The pointer [ptr] will points to the character at where the
            // interpolated string ends. (ie. the next character after name
            // ends).
            const char* ptr = parser->current_char;
            while (utilIsName(*(ptr)) || utilIsDigit(*(ptr))) {
              ptr++;
            }
            parser->si_name_end = ptr;
            parser->si_name_quote = quote;
          }
        }

      } else {
        lexError(parser, "Maximum interpolation level reached (can only "
                 "interpolate upto depth %d).", MAX_STR_INTERP_DEPTH);
      }
      break;
    }

    if (c == '\\') {
      switch (eatChar(parser)) {
        case '"':  pkByteBufferWrite(&buff, parser->vm, '"'); break;
        case '\'': pkByteBufferWrite(&buff, parser->vm, '\''); break;
        case '\\': pkByteBufferWrite(&buff, parser->vm, '\\'); break;
        case 'n':  pkByteBufferWrite(&buff, parser->vm, '\n'); break;
        case 'r':  pkByteBufferWrite(&buff, parser->vm, '\r'); break;
        case 't':  pkByteBufferWrite(&buff, parser->vm, '\t'); break;

        // '$' In pocketlang string is used for interpolation.
        case '$':  pkByteBufferWrite(&buff, parser->vm, '$'); break;

        default:
          lexError(parser, "Error: invalid escape character");
          break;
      }
    } else {
      pkByteBufferWrite(&buff, parser->vm, c);
    }
  }

  // '\0' will be added by varNewSring();
  Var string = VAR_OBJ(newStringLength(parser->vm, (const char*)buff.data,
                       (uint32_t)buff.count));

  pkByteBufferClear(&buff, parser->vm);

  setNextValueToken(parser, tk_type, string);
}

// Returns the current char of the compiler on.
static char peekChar(Parser* parser) {
  return *parser->current_char;
}

// Returns the next char of the compiler on.
static char peekNextChar(Parser* parser) {
  if (peekChar(parser) == '\0') return '\0';
  return *(parser->current_char + 1);
}

// Advance the compiler by 1 char.
static char eatChar(Parser* parser) {
  char c = peekChar(parser);
  parser->current_char++;
  if (c == '\n') parser->current_line++;
  return c;
}

// Complete lexing an identifier name.
static void eatName(Parser* parser) {

  char c = peekChar(parser);
  while (utilIsName(c) || utilIsDigit(c)) {
    eatChar(parser);
    c = peekChar(parser);
  }

  const char* name_start = parser->token_start;

  TokenType type = TK_NAME;

  int length = (int)(parser->current_char - name_start);
  for (int i = 0; _keywords[i].identifier != NULL; i++) {
    if (_keywords[i].length == length &&
      strncmp(name_start, _keywords[i].identifier, length) == 0) {
      type = _keywords[i].tk_type;
      break;
    }
  }

  setNextToken(parser, type);
}

// Complete lexing a number literal.
static void eatNumber(Parser* parser) {

#define IS_HEX_CHAR(c)            \
  (('0' <= (c) && (c) <= '9')  || \
   ('a' <= (c) && (c) <= 'f'))

#define IS_BIN_CHAR(c) (((c) == '0') || ((c) == '1'))

  Var value = VAR_NULL; // The number value.
  char c = *parser->token_start;

  // Binary literal.
  if (c == '0' && peekChar(parser) == 'b') {
    eatChar(parser); // Consume '0b'

    uint64_t bin = 0;
    c = peekChar(parser);
    if (!IS_BIN_CHAR(c)) {
      lexError(parser, "Invalid binary literal.");

    } else {
      do {
        // Consume the next digit.
        c = peekChar(parser);
        if (!IS_BIN_CHAR(c)) break;
        eatChar(parser);

        // Check the length of the binary literal.
        int length = (int)(parser->current_char - parser->token_start);
        if (length > STR_BIN_BUFF_SIZE - 2) { // -2: '-\0' 0b is in both side.
          lexError(parser, "Binary literal is too long.");
          break;
        }

        // "Append" the next digit at the end.
        bin = (bin << 1) | (c - '0');

      } while (true);
    }
    value = VAR_NUM((double)bin);

  } else if (c == '0' && peekChar(parser) == 'x') {
    eatChar(parser); // Consume '0x'

    uint64_t hex = 0;
    c = peekChar(parser);

    // The first digit should be either hex digit.
    if (!IS_HEX_CHAR(c)) {
      lexError(parser, "Invalid hex literal.");

    } else {
      do {
        // Consume the next digit.
        c = peekChar(parser);
        if (!IS_HEX_CHAR(c)) break;
        eatChar(parser);

        // Check the length of the binary literal.
        int length = (int)(parser->current_char - parser->token_start);
        if (length > STR_HEX_BUFF_SIZE - 2) { // -2: '-\0' 0x is in both side.
          lexError(parser, "Hex literal is too long.");
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
    while (utilIsDigit(peekChar(parser))) {
      eatChar(parser);
    }

    if (peekChar(parser) == '.' && utilIsDigit(peekNextChar(parser))) {
      matchChar(parser, '.');
      while (utilIsDigit(peekChar(parser))) {
        eatChar(parser);
      }
    }

    // Parse if in scientific notation format (MeN == M * 10 ** N).
    if (matchChar(parser, 'e') || matchChar(parser, 'E')) {

      if (peekChar(parser) == '+' || peekChar(parser) == '-') {
        eatChar(parser);
      }

      if (!utilIsDigit(peekChar(parser))) {
        lexError(parser, "Invalid number literal.");

      } else { // Eat the exponent.
        while (utilIsDigit(peekChar(parser))) eatChar(parser);
      }
    }

    errno = 0;
    value = VAR_NUM(atof(parser->token_start));
    if (errno == ERANGE) {
      const char* start = parser->token_start;
      int len = (int)(parser->current_char - start);
      lexError(parser, "Number literal is too large (%.*s).", len, start);
      value = VAR_NUM(0);
    }
  }

  setNextValueToken(parser, TK_NUMBER, value);
#undef IS_BIN_CHAR
#undef IS_HEX_CHAR
}

// Read and ignore chars till it reach new line or EOF.
static void skipLineComment(Parser* parser) {
  char c;
  while ((c = peekChar(parser)) != '\0') {
    // Don't eat new line it's not part of the comment.
    if (c == '\n') return;
    eatChar(parser);
  }
}

// If the current char is [c] consume it and advance char by 1 and returns
// true otherwise returns false.
static bool matchChar(Parser* parser, char c) {
  if (peekChar(parser) != c) return false;
  eatChar(parser);
  return true;
}

// If the current char is [c] eat the char and add token two otherwise eat
// append token one.
static void setNextTwoCharToken(Parser* parser, char c, TokenType one,
  TokenType two) {
  if (matchChar(parser, c)) {
    setNextToken(parser, two);
  } else {
    setNextToken(parser, one);
  }
}

// Initialize the next token as the type.
static void setNextToken(Parser* parser, TokenType type) {
  Token* next = &parser->next;
  next->type = type;
  next->start = parser->token_start;
  next->length = (int)(parser->current_char - parser->token_start);
  next->line = parser->current_line - ((type == TK_LINE) ? 1 : 0);
}

// Initialize the next token as the type and assign the value.
static void setNextValueToken(Parser* parser, TokenType type, Var value) {
  setNextToken(parser, type);
  parser->next.value = value;
}

// Lex the next token and set it as the next token.
static void lexToken(Parser* parser) {
  parser->previous = parser->current;
  parser->current = parser->next;

  if (parser->current.type == TK_EOF) return;

  while (peekChar(parser) != '\0') {
    parser->token_start = parser->current_char;

    // If we're parsing a name interpolation and the current character is where
    // the name end, continue parsing the string.
    //
    //        "Hello $name!"
    //                    ^-- si_name_end
    //
    if (parser->si_name_end != NULL) {
      if (parser->current_char == parser->si_name_end) {
        parser->si_name_end = NULL;
        eatString(parser, parser->si_name_quote == '\'');
        return;
      } else {
        ASSERT(parser->current_char < parser->si_name_end, OOPS);
      }
    }

    char c = eatChar(parser);
    switch (c) {

      case '{': {

        // If we're inside an interpolation, increase the open brace count
        // of the current depth.
        if (parser->si_depth > 0) {
          parser->si_open_brace[parser->si_depth - 1]++;
        }
        setNextToken(parser, TK_LBRACE);
        return;
      }

      case '}': {
        // If we're inside of an interpolated string.
        if (parser->si_depth > 0) {

          // No open braces, then end the expression and complete the string.
          if (parser->si_open_brace[parser->si_depth - 1] == 0) {

            char quote = parser->si_quote[parser->si_depth - 1];
            parser->si_depth--; //< Exit the depth.
            eatString(parser, quote == '\'');
            return;

          } else { // Decrease the open brace at the current depth.
            parser->si_open_brace[parser->si_depth - 1]--;
          }
        }

        setNextToken(parser, TK_RBRACE);
        return;
      }

      case ',': setNextToken(parser, TK_COMMA); return;
      case ':': setNextToken(parser, TK_COLLON); return;
      case ';': setNextToken(parser, TK_SEMICOLLON); return;
      case '#': skipLineComment(parser); break;
      case '(': setNextToken(parser, TK_LPARAN); return;
      case ')': setNextToken(parser, TK_RPARAN); return;
      case '[': setNextToken(parser, TK_LBRACKET); return;
      case ']': setNextToken(parser, TK_RBRACKET); return;
      case '%':
        setNextTwoCharToken(parser, '=', TK_PERCENT, TK_MODEQ);
        return;

      case '~': setNextToken(parser, TK_TILD); return;

      case '&':
        setNextTwoCharToken(parser, '=', TK_AMP, TK_ANDEQ);
        return;

      case '|':
        setNextTwoCharToken(parser, '=', TK_PIPE, TK_OREQ);
        return;

      case '^':
        setNextTwoCharToken(parser, '=', TK_CARET, TK_XOREQ);
        return;

      case '\n': setNextToken(parser, TK_LINE); return;

      case ' ':
      case '\t':
      case '\r': {
        c = peekChar(parser);
        while (c == ' ' || c == '\t' || c == '\r') {
          eatChar(parser);
          c = peekChar(parser);
        }
        break;
      }

      case '.':
        if (matchChar(parser, '.')) {
          setNextToken(parser, TK_DOTDOT); // '..'
        } else if (utilIsDigit(peekChar(parser))) {
          eatChar(parser);   // Consume the decimal point.
          eatNumber(parser); // Consume the rest of the number
        } else {
          setNextToken(parser, TK_DOT);    // '.'
        }
        return;

      case '=':
        setNextTwoCharToken(parser, '=', TK_EQ, TK_EQEQ);
        return;

      case '!':
        setNextTwoCharToken(parser, '=', TK_NOT, TK_NOTEQ);
        return;

      case '>':
        if (matchChar(parser, '>')) {
          if (matchChar(parser, '=')) {
            setNextToken(parser, TK_SRIGHTEQ);
          } else {
            setNextToken(parser, TK_SRIGHT);
          }
        } else {
          setNextTwoCharToken(parser, '=', TK_GT, TK_GTEQ);
        }
        return;

      case '<':
        if (matchChar(parser, '<')) {
          if (matchChar(parser, '=')) {
            setNextToken(parser, TK_SLEFTEQ);
          } else {
            setNextToken(parser, TK_SLEFT);
          }
        } else {
          setNextTwoCharToken(parser, '=', TK_LT, TK_LTEQ);
        }
        return;

      case '+':
        setNextTwoCharToken(parser, '=', TK_PLUS, TK_PLUSEQ);
        return;

      case '-':
        if (matchChar(parser, '=')) {
          setNextToken(parser, TK_MINUSEQ);  // '-='
        } else if (matchChar(parser, '>')) {
          setNextToken(parser, TK_ARROW);    // '->'
        } else {
          setNextToken(parser, TK_MINUS);    // '-'
        }
        return;

      case '*':
        setNextTwoCharToken(parser, '=', TK_STAR, TK_STAREQ);
        return;

      case '/':
        setNextTwoCharToken(parser, '=', TK_FSLASH, TK_DIVEQ);
        return;

      case '"': eatString(parser, false); return;

      case '\'': eatString(parser, true); return;

      default: {

        if (utilIsDigit(c)) {
          eatNumber(parser);

        } else if (utilIsName(c)) {
          eatName(parser);

        } else {
          if (c >= 32 && c <= 126) {
            lexError(parser, "Invalid character '%c'", c);
          } else {
            lexError(parser, "Invalid byte 0x%x", (uint8_t)c);
          }
          setNextToken(parser, TK_ERROR);
        }
        return;
      }
    }
  }

  setNextToken(parser, TK_EOF);
  parser->next.start = parser->current_char;
}

/*****************************************************************************/
/* PARSING                                                                   */
/*****************************************************************************/

// Returns current token type without lexing a new token.
static TokenType peek(Compiler* compiler) {
  return compiler->parser.current.type;
}

// Consume the current token if it's expected and lex for the next token
// and return true otherwise return false.
static bool match(Compiler* compiler, TokenType expected) {
  if (peek(compiler) != expected) return false;
  lexToken(&(compiler->parser));
  return true;
}

// Consume the the current token and if it's not [expected] emits error log
// and continue parsing for more error logs.
static void consume(Compiler* compiler, TokenType expected,
                    const char* err_msg) {

  lexToken(&(compiler->parser));
  if (compiler->parser.previous.type != expected) {
    parseError(compiler, "%s", err_msg);

    // If the next token is expected discard the current to minimize
    // cascaded errors and continue parsing.
    if (peek(compiler) == expected) {
      lexToken(&(compiler->parser));
    }
  }
}

// Match one or more lines and return true if there any.
static bool matchLine(Compiler* compiler) {

  bool consumed = false;

  if (peek(compiler) == TK_LINE) {
    while (peek(compiler) == TK_LINE)
      lexToken(&(compiler->parser));
    consumed = true;
  }

  // If we're running on REPL mode, at the EOF and compile time error occurred,
  // signal the host to get more lines and try re-compiling it.
  if (compiler->parser.repl_mode && !compiler->parser.has_errors) {
    if (peek(compiler) == TK_EOF) {
      compiler->parser.need_more_lines = true;
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
/* NAME SEARCH (AT COMPILATION PHASE)                                        */
/*****************************************************************************/

// Find the builtin function name and returns it's index in the builtins array
// if not found returns -1.
static int findBuiltinFunction(const PKVM* vm,
                               const char* name, uint32_t length) {
  for (int i = 0; i < vm->builtins_count; i++) {
    uint32_t bfn_length = (uint32_t)strlen(vm->builtins[i]->fn->name);
    if (bfn_length != length) continue;
    if (strncmp(name, vm->builtins[i]->fn->name, length) == 0) {
      return i;
    }
  }
  return -1;
}

// Find the local with the [name] in the given function [func] and return
// it's index, if not found returns -1.
static int findLocal(Func* func, const char* name, uint32_t length) {
  ASSERT(func != NULL, OOPS);
  for (int i = 0; i < func->local_count; i++) {
    if (func->locals[i].length != length) continue;
    if (strncmp(func->locals[i].name, name, length) == 0) {
      return i;
    }
  }
  return -1;
}

// Add the upvalue to the given function and return it's index, if the upvalue
// already present in the function's upvalue array it'll return it.
static int addUpvalue(Compiler* compiler, Func* func,
                      int index, bool is_immediate) {

  // Search the upvalue in the existsing upvalues array.
  for (int i = 0; i < func->ptr->upvalue_count; i++) {
    UpvalueInfo info = func->upvalues[i];
    if (info.index == index && info.is_immediate == is_immediate) {
      return i;
    }
  }

  if (func->ptr->upvalue_count == MAX_UPVALUES) {
    parseError(compiler, "A function cannot capture more thatn %d upvalues.",
               MAX_UPVALUES);
    return -1;
  }

  func->upvalues[func->ptr->upvalue_count].index = index;
  func->upvalues[func->ptr->upvalue_count].is_immediate = is_immediate;
  return func->ptr->upvalue_count++;
}

// Search for an upvalue with the given [name] for the current function [func].
// If an upvalue found, it'll add the upvalue info to the upvalue infor array
// of the [func] and return the index of the upvalue in the current function's
// upvalues array.
static int findUpvalue(Compiler* compiler, Func* func, const char* name,
                       uint32_t length) {
  // TODO:
  // check if the function is a method of a class and return -1 for them as
  // well (once methods implemented).
  //
  // Toplevel functions cannot have upvalues.
  if (func->depth <= DEPTH_GLOBAL) return -1;

  // Search in the immediate enclosing function's locals.
  int index = findLocal(func->outer_func, name, length);
  if (index != -1) {

    // Mark the locals as an upvalue to close it when it goes out of the scope.
    func->outer_func->locals[index].is_upvalue = true;

    // Add upvalue to the function and return it's index.
    return addUpvalue(compiler, func, index, true);
  }

  // Recursively search for the upvalue in the outer function. If we found one
  // all the outer function in the chain would have captured the upvalue for
  // the local, we can add it to the current function as non-immediate upvalue.
  index = findUpvalue(compiler, func->outer_func, name, length);

  if (index != -1) {
    return addUpvalue(compiler, func, index, false);
  }

  // If we reached here, the upvalue doesn't exists.
  return -1;
}

// Result type for an identifier definition.
typedef enum {
  NAME_NOT_DEFINED,
  NAME_LOCAL_VAR,  //< Including parameter.
  NAME_UPVALUE,    //< Local to an enclosing function.
  NAME_GLOBAL_VAR,
  NAME_BUILTIN_FN,    //< Native builtin function.
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

  int index; // For storing the search result below.

  // Search through locals.
  index = findLocal(compiler->func, name, length);
  if (index != -1) {
    result.type = NAME_LOCAL_VAR;
    result.index = index;
    return result;
  }

  // Search through upvalues.
  index = findUpvalue(compiler, compiler->func, name, length);
  if (index != -1) {
    result.type = NAME_UPVALUE;
    result.index = index;
    return result;
  }

  // Search through globals.
  index = moduleGetGlobalIndex(compiler->module, name, length);
  if (index != -1) {
    result.type = NAME_GLOBAL_VAR;
    result.index = index;
    return result;
  }

  // Search through builtin functions.
  index = findBuiltinFunction(compiler->parser.vm, name, length);
  if (index != -1) {
    result.type = NAME_BUILTIN_FN;
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
static void emitAssignedOp(Compiler* compiler, TokenType assignment);
static void emitFunctionEnd(Compiler* compiler);

static void patchJump(Compiler* compiler, int addr_index);
static void patchListSize(Compiler* compiler, int size_index, int size);
static void patchForward(Compiler* compiler, Fn* fn, int index, int name);

static int compilerAddConstant(Compiler* compiler, Var value);
static int compilerAddVariable(Compiler* compiler, const char* name,
                               uint32_t length, int line);
static void compilerAddForward(Compiler* compiler, int instruction, Fn* fn,
                               const char* name, int length, int line);
static void compilerChangeStack(Compiler* compiler, int num);

// Forward declaration of grammar functions.
static void parsePrecedence(Compiler* compiler, Precedence precedence);
static void compileFunction(Compiler* compiler, bool is_literal);
static void compileExpression(Compiler* compiler);

static void exprLiteral(Compiler* compiler);
static void exprInterpolation(Compiler* compiler);
static void exprFunc(Compiler* compiler);
static void exprName(Compiler* compiler);

static void exprOr(Compiler* compiler);
static void exprAnd(Compiler* compiler);

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
  /* TK_ARROW      */   NO_RULE,
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
  /* TK_STRING_INTERP */ { exprInterpolation, NULL,      NO_INFIX },
};

static GrammarRule* getRule(TokenType type) {
  return &(rules[(int)type]);
}

// FIXME:
// This function is used by the import system, remove this function (and move
// it to emitStoreName()) after import system refactored.
//
// Store the value at the stack top to the global at the [index].
static void emitStoreGlobal(Compiler* compiler, int index) {
  emitOpcode(compiler, OP_STORE_GLOBAL);
  emitByte(compiler, index);
}

// Emit opcode to push the named value at the [index] in it's array.
static void emitPushName(Compiler* compiler, NameDefnType type, int index) {
  ASSERT(index >= 0, OOPS);

  switch (type) {
    case NAME_NOT_DEFINED:
      UNREACHABLE();

    case NAME_LOCAL_VAR:
      if (index < 9) { //< 0..8 locals have single opcode.
        emitOpcode(compiler, (Opcode)(OP_PUSH_LOCAL_0 + index));
      } else {
        emitOpcode(compiler, OP_PUSH_LOCAL_N);
        emitByte(compiler, index);
      }
      return;

    case NAME_UPVALUE:
      emitOpcode(compiler, OP_PUSH_UPVALUE);
      emitByte(compiler, index);
      return;

    case NAME_GLOBAL_VAR:
      emitOpcode(compiler, OP_PUSH_GLOBAL);
      emitByte(compiler, index);
      return;

    case NAME_BUILTIN_FN:
      emitOpcode(compiler, OP_PUSH_BUILTIN_FN);
      emitByte(compiler, index);
      return;
  }
}

// Emit opcode to store the stack top value to the named value at the [index]
// in it's array.
static void emitStoreName(Compiler* compiler, NameDefnType type, int index) {
  ASSERT(index >= 0, OOPS);

  switch (type) {
    case NAME_NOT_DEFINED:
    case NAME_BUILTIN_FN:
      UNREACHABLE();

    case NAME_LOCAL_VAR:
      if (index < 9) { //< 0..8 locals have single opcode.
        emitOpcode(compiler, (Opcode)(OP_STORE_LOCAL_0 + index));
      } else {
        emitOpcode(compiler, OP_STORE_LOCAL_N);
        emitByte(compiler, index);
      }
      return;

    case NAME_UPVALUE:
      emitOpcode(compiler, OP_STORE_UPVALUE);
      emitByte(compiler, index);
      return;

    case NAME_GLOBAL_VAR:
      emitStoreGlobal(compiler, index);
      return;
  }
}

static void exprLiteral(Compiler* compiler) {
  Token* value = &compiler->parser.previous;
  int index = compilerAddConstant(compiler, value->value);
  emitOpcode(compiler, OP_PUSH_CONSTANT);
  emitShort(compiler, index);
}

// Consider the bellow string.
//
//     "Hello $name!"
//
// This will be compiled as:
//
//     list_join(["Hello ", name, "!"])
//
static void exprInterpolation(Compiler* compiler) {
  emitOpcode(compiler, OP_PUSH_BUILTIN_FN);
  emitByte(compiler, compiler->bifn_list_join);

  emitOpcode(compiler, OP_PUSH_LIST);
  int size_index = emitShort(compiler, 0);

  int size = 0;
  do {
    // Push the string on the stack and append it to the list.
    exprLiteral(compiler);
    emitOpcode(compiler, OP_LIST_APPEND);
    size++;

    // Compile the expression and append it to the list.
    skipNewLines(compiler);
    compileExpression(compiler);
    emitOpcode(compiler, OP_LIST_APPEND);
    size++;
    skipNewLines(compiler);
  } while (match(compiler, TK_STRING_INTERP));

  // The last string is not TK_STRING_INTERP but it would be
  // TK_STRING. Apped it.
  // Optimize case last string could be empty. Skip it.
  consume(compiler, TK_STRING, "Non terminated interpolated string.");
  if (compiler->parser.previous.type == TK_STRING /* != if syntax error. */) {
    ASSERT(IS_OBJ_TYPE(compiler->parser.previous.value, OBJ_STRING), OOPS);
    String* str = (String*)AS_OBJ(compiler->parser.previous.value);
    if (str->length != 0) {
      exprLiteral(compiler);
      emitOpcode(compiler, OP_LIST_APPEND);
      size++;
    }
  }

  patchListSize(compiler, size_index, size);

  // Call the list_join function (which is at the stack top).
  emitOpcode(compiler, OP_CALL);
  emitByte(compiler, 1);

  // After the above call, the lits and the "list_join" function will be popped
  // from the stack and a string will be pushed. The so the result stack effect
  // is -1.
  compilerChangeStack(compiler, -1);

}

static void exprFunc(Compiler* compiler) {
  compileFunction(compiler, true);
}

static void exprName(Compiler* compiler) {

  const char* start = compiler->parser.previous.start;
  int length = compiler->parser.previous.length;
  int line = compiler->parser.previous.line;
  NameSearchResult result = compilerSearchName(compiler, start, length);

  if (compiler->l_value && matchAssignment(compiler)) {
    TokenType assignment = compiler->parser.previous.type;
    skipNewLines(compiler);

    // Type of the name that's being assigned. Could only be local, global
    // or an upvalue.
    NameDefnType name_type = result.type;
    int index = result.index; // Index of the name in it's array.

    // Will be set to true if the name is a new local.
    bool new_local = false;

    if (assignment == TK_EQ) { // name = (expr);

      // Assignment to builtin functions will override the name and it'll
      // become a local or global variable. Note that if the names is a global
      // which hasent defined yet we treat that as a local (no global keyword
      // like python does) and it's recommented to define all the globals
      // before entering a local scope.

      if (result.type == NAME_NOT_DEFINED || result.type == NAME_BUILTIN_FN) {
        name_type = (compiler->scope_depth == DEPTH_GLOBAL)
                    ? NAME_GLOBAL_VAR
                    : NAME_LOCAL_VAR;
        index = compilerAddVariable(compiler, start, length, line);

        // We cannot set `compiler->new_local = true;` here since there is an
        // expression after the assignment pending. We'll update it once the
        // expression is compiled.
        if (name_type == NAME_LOCAL_VAR) {
          new_local = true;
        }
      }

      // Compile the assigned value.
      compileExpression(compiler);

    } else { // name += / -= / *= ... = (expr);
      if (result.type == NAME_NOT_DEFINED) {
        parseError(compiler, "Name '%.*s' is not defined.", length, start);
      }

      // Push the named value.
      emitPushName(compiler, name_type, index);

      // Compile the RHS of the assigned operation.
      compileExpression(compiler);

      // Do the arithmatic operation of the assignment.
      emitAssignedOp(compiler, assignment);
    }

    // If it's a new local we don't have to store it, it's already at it's
    // stack slot.
    if (new_local) {
      // This will prevent the assignment from being popped out from the
      // stack since the assigned value itself is the local and not a temp.
      compiler->new_local = true;

      // Ensure the local variable's index is equals to the stack top index.
      // If the compiler has errors, we cannot and don't have to assert.
      ASSERT(compiler->parser.has_errors ||
             (compiler->func->stack_size - 1) == index, OOPS);
    } else {
      // The assigned value or the result of the operator will be at the top of
      // the stack by now. Store it.
      emitStoreName(compiler, name_type, index);
    }

  } else { // Just the name and no assignment followed by.

    // The name could be a global value which hasn't been defined at this
    // point. We add an implicit forward declaration and once this expression
    // executed the value could be initialized only if the expression is at
    // a local depth.
    if (result.type == NAME_NOT_DEFINED) {
      if (compiler->scope_depth == DEPTH_GLOBAL) {
        parseError(compiler, "Name '%.*s' is not defined.", length, start);
      } else {
        emitOpcode(compiler, OP_PUSH_GLOBAL);
        int index = emitByte(compiler, 0xff);
        compilerAddForward(compiler, index, _FN, start, length, line);
      }
    } else {
      emitPushName(compiler, result.type, result.index);
    }
  }

}

// Compiling (expr a) or (expr b)
//
//            (expr a)
//             |  At this point (expr a) is at the stack top.
//             V
//        .-- (OP_OR [offset])
//        |    |  if true short circuit and skip (expr b)
//        |    |  otherwise pop (expr a) and continue.
//        |    V
//        |   (expr b)
//        |    |  At this point (expr b) is at the stack top.
//        |    V
//        '->  (...)
//              At this point stack top would be
//              either (expr a) or (expr b)
//
// Compiling 'and' expression is also similler but we jump if the (expr a) is
// false.

void exprOr(Compiler* compiler) {
  emitOpcode(compiler, OP_OR);
  int orpatch = emitShort(compiler, 0xffff); //< Will be patched.
  parsePrecedence(compiler, PREC_LOGICAL_OR);
  patchJump(compiler, orpatch);
}

void exprAnd(Compiler* compiler) {
  emitOpcode(compiler, OP_AND);
  int andpatch = emitShort(compiler, 0xffff); //< Will be patched.
  parsePrecedence(compiler, PREC_LOGICAL_AND);
  patchJump(compiler, andpatch);
}

static void exprBinaryOp(Compiler* compiler) {
  TokenType op = compiler->parser.previous.type;
  skipNewLines(compiler);
  parsePrecedence(compiler, (Precedence)(getRule(op)->precedence + 1));

  switch (op) {
    case TK_DOTDOT:  emitOpcode(compiler, OP_RANGE);      break;
    case TK_PERCENT: emitOpcode(compiler, OP_MOD);        break;
    case TK_AMP:     emitOpcode(compiler, OP_BIT_AND);    break;
    case TK_PIPE:    emitOpcode(compiler, OP_BIT_OR);     break;
    case TK_CARET:   emitOpcode(compiler, OP_BIT_XOR);    break;
    case TK_PLUS:    emitOpcode(compiler, OP_ADD);        break;
    case TK_MINUS:   emitOpcode(compiler, OP_SUBTRACT);   break;
    case TK_STAR:    emitOpcode(compiler, OP_MULTIPLY);   break;
    case TK_FSLASH:  emitOpcode(compiler, OP_DIVIDE);     break;
    case TK_GT:      emitOpcode(compiler, OP_GT);         break;
    case TK_LT:      emitOpcode(compiler, OP_LT);         break;
    case TK_EQEQ:    emitOpcode(compiler, OP_EQEQ);       break;
    case TK_NOTEQ:   emitOpcode(compiler, OP_NOTEQ);      break;
    case TK_GTEQ:    emitOpcode(compiler, OP_GTEQ);       break;
    case TK_LTEQ:    emitOpcode(compiler, OP_LTEQ);       break;
    case TK_SRIGHT:  emitOpcode(compiler, OP_BIT_RSHIFT); break;
    case TK_SLEFT:   emitOpcode(compiler, OP_BIT_LSHIFT); break;
    case TK_IN:      emitOpcode(compiler, OP_IN);         break;
    default:
      UNREACHABLE();
  }
}

static void exprUnaryOp(Compiler* compiler) {
  TokenType op = compiler->parser.previous.type;
  skipNewLines(compiler);
  parsePrecedence(compiler, (Precedence)(PREC_UNARY + 1));

  switch (op) {
    case TK_TILD:  emitOpcode(compiler, OP_BIT_NOT); break;
    case TK_MINUS: emitOpcode(compiler, OP_NEGATIVE); break;
    case TK_NOT:   emitOpcode(compiler, OP_NOT); break;
    default:
      UNREACHABLE();
  }
}

static void exprGrouping(Compiler* compiler) {
  skipNewLines(compiler);
  compileExpression(compiler);
  skipNewLines(compiler);
  consume(compiler, TK_RPARAN, "Expected ')' after expression.");
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

  patchListSize(compiler, size_index, size);
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

  // After the call the arguments will be popped and the callable
  // will be replaced with the return value.
  compilerChangeStack(compiler, -argc);
}

static void exprAttrib(Compiler* compiler) {
  consume(compiler, TK_NAME, "Expected an attribute name after '.'.");
  const char* name = compiler->parser.previous.start;
  int length = compiler->parser.previous.length;

  // Store the name in module's names buffer.
  int index = moduleAddName(compiler->module, compiler->parser.vm,
                            name, length);

  if (compiler->l_value && matchAssignment(compiler)) {
    TokenType assignment = compiler->parser.previous.type;
    skipNewLines(compiler);

    if (assignment != TK_EQ) {
      emitOpcode(compiler, OP_GET_ATTRIB_KEEP);
      emitShort(compiler, index);
      compileExpression(compiler);
      emitAssignedOp(compiler, assignment);
    } else {
      compileExpression(compiler);
    }

    emitOpcode(compiler, OP_SET_ATTRIB);
    emitShort(compiler, index);

  } else {
    emitOpcode(compiler, OP_GET_ATTRIB);
    emitShort(compiler, index);
  }
}

static void exprSubscript(Compiler* compiler) {
  compileExpression(compiler);
  consume(compiler, TK_RBRACKET, "Expected ']' after subscription ends.");

  if (compiler->l_value && matchAssignment(compiler)) {
    TokenType assignment = compiler->parser.previous.type;
    skipNewLines(compiler);

    if (assignment != TK_EQ) {
      emitOpcode(compiler, OP_GET_SUBSCRIPT_KEEP);
      compileExpression(compiler);
      emitAssignedOp(compiler, assignment);

    } else {
      compileExpression(compiler);
    }

    emitOpcode(compiler, OP_SET_SUBSCRIPT);

  } else {
    emitOpcode(compiler, OP_GET_SUBSCRIPT);
  }
}

static void exprValue(Compiler* compiler) {
  TokenType op = compiler->parser.previous.type;
  switch (op) {
    case TK_NULL:  emitOpcode(compiler, OP_PUSH_NULL);  break;
    case TK_TRUE:  emitOpcode(compiler, OP_PUSH_TRUE);  break;
    case TK_FALSE: emitOpcode(compiler, OP_PUSH_FALSE); break;
    default:
      UNREACHABLE();
  }
}

static void parsePrecedence(Compiler* compiler, Precedence precedence) {
  lexToken(&(compiler->parser));
  GrammarFn prefix = getRule(compiler->parser.previous.type)->prefix;

  if (prefix == NULL) {
    parseError(compiler, "Expected an expression.");
    return;
  }

  compiler->l_value = precedence <= PREC_LOWEST;
  prefix(compiler);

  // The above expression cannot be a call '(', since call is an infix
  // operator. But could be true (ex: x = f()). we set is_last_call to false
  // here and if the next infix operator is call this will be set to true
  // once the call expression is parsed.
  compiler->is_last_call = false;

  while (getRule(compiler->parser.current.type)->precedence >= precedence) {
    lexToken(&(compiler->parser));

    TokenType op = compiler->parser.previous.type;
    GrammarFn infix = getRule(op)->infix;

    infix(compiler);

    // TK_LPARAN '(' as infix is the call operator.
    compiler->is_last_call = (op == TK_LPARAN);

  }
}

/*****************************************************************************/
/* COMPILING                                                                 */
/*****************************************************************************/

// Add a variable and return it's index to the context. Assumes that the
// variable name is unique and not defined before in the current scope.
static int compilerAddVariable(Compiler* compiler, const char* name,
                                uint32_t length, int line) {

  // TODO: should I validate the name for pre-defined, etc?

  // Check if maximum variable count is reached.
  bool max_vars_reached = false;
  const char* var_type = ""; // For max variables reached error message.
  if (compiler->scope_depth == DEPTH_GLOBAL) {
    if (compiler->module->globals.count >= MAX_VARIABLES) {
      max_vars_reached = true;
      var_type = "globals";
    }
  } else {
    if (compiler->func->local_count >= MAX_VARIABLES) {
      max_vars_reached = true;
      var_type = "locals";
    }
  }
  if (max_vars_reached) {
    parseError(compiler, "A module should contain at most %d %s.",
               MAX_VARIABLES, var_type);
    return -1;
  }

  // Add the variable and return it's index.

  if (compiler->scope_depth == DEPTH_GLOBAL) {
    return (int)moduleAddGlobal(compiler->parser.vm, compiler->module,
                                name, length, VAR_NULL);
  } else {
    Local* local = &compiler->func->locals[compiler->func->local_count];
    local->name = name;
    local->length = length;
    local->depth = compiler->scope_depth;
    local->is_upvalue = false;
    local->line = line;
    return compiler->func->local_count++;
  }

  UNREACHABLE();
  return -1;
}

static void compilerAddForward(Compiler* compiler, int instruction, Fn* fn,
                               const char* name, int length, int line) {
  if (compiler->parser.forwards_count == MAX_FORWARD_NAMES) {
    parseError(compiler, "A module should contain at most %d implicit forward "
               "function declarations.", MAX_FORWARD_NAMES);
    return;
  }

  ForwardName* forward = &compiler->parser.forwards[
                           compiler->parser.forwards_count++];
  forward->instruction = instruction;
  forward->func = fn;
  forward->name = name;
  forward->length = length;
  forward->line = line;
}

// Add a literal constant to module literals and return it's index.
static int compilerAddConstant(Compiler* compiler, Var value) {
  pkVarBuffer* constants = &compiler->module->constants;

  uint32_t index = moduleAddConstant(compiler->parser.vm,
                                     compiler->module, value);
  checkMaxConstantsReached(compiler, index);
  return (int)index;
}

// Enters inside a block.
static void compilerEnterBlock(Compiler* compiler) {
  compiler->scope_depth++;
}

// Change the stack size by the [num], if it's positive, the stack will
// grow otherwise it'll shrink.
static void compilerChangeStack(Compiler* compiler, int num) {
  compiler->func->stack_size += num;

  // If the compiler has error (such as undefined name), that will not popped
  // because of the semantic error but it'll be popped once the expression
  // parsing is done. So it's possible for negative size in error.
  ASSERT(compiler->parser.has_errors || compiler->func->stack_size >= 0, OOPS);

  if (compiler->func->stack_size > _FN->stack_size) {
    _FN->stack_size = compiler->func->stack_size;
  }
}

// Write instruction to pop all the locals at the current [depth] or higher,
// but it won't change the stack size of locals count because this function
// is called by break/continue statements at the middle of a scope, so we need
// those locals till the scope ends. This will returns the number of locals
// that were popped.
static int compilerPopLocals(Compiler* compiler, int depth) {
  ASSERT(depth > (int)DEPTH_GLOBAL, "Cannot pop global variables.");

  int local = compiler->func->local_count - 1;
  while (local >= 0 && compiler->func->locals[local].depth >= depth) {

    // Note: Do not use emitOpcode(compiler, OP_POP);
    // Because this function is called at the middle of a scope (break,
    // continue). So we need the pop instruction here but we still need the
    // locals to continue parsing the next statements in the scope. They'll be
    // popped once the scope is ended.

    if (compiler->func->locals[local].is_upvalue) {
      emitByte(compiler, OP_CLOSE_UPVALUE);
    } else {
      emitByte(compiler, OP_POP);
    }

    local--;
  }
  return (compiler->func->local_count - 1) - local;
}

// Exits a block.
static void compilerExitBlock(Compiler* compiler) {
  ASSERT(compiler->scope_depth > (int)DEPTH_GLOBAL, "Cannot exit toplevel.");

  // Discard all the locals at the current scope.
  int popped = compilerPopLocals(compiler, compiler->scope_depth);
  compiler->func->local_count -= popped;
  compiler->func->stack_size -= popped;
  compiler->scope_depth--;
}

static void compilerPushFunc(Compiler* compiler, Func* fn,
                             Function* func) {
  fn->outer_func = compiler->func;
  fn->local_count = 0;
  fn->stack_size = 0;
  fn->ptr = func;
  fn->depth = compiler->scope_depth;
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

  pkByteBufferWrite(&_FN->opcodes, compiler->parser.vm,
                    (uint8_t)byte);
  pkUintBufferWrite(&_FN->oplines, compiler->parser.vm,
                   compiler->parser.previous.line);
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
  // If the opcode is OP_CALL the compiler should change the stack size
  // manually because we don't know that here.
  compilerChangeStack(compiler, opcode_info[opcode].stack);
}

// Jump back to the start of the loop.
static void emitLoopJump(Compiler* compiler) {
  emitOpcode(compiler, OP_LOOP);
  int offset = (int)_FN->opcodes.count - compiler->loop->start + 2;
  emitShort(compiler, offset);
}

static void emitAssignedOp(Compiler* compiler, TokenType assignment) {
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

  // Don't use emitOpcode(compiler, OP_RETURN); Because it'll reduce the stack
  // size by -1, (return value will be popped). This return is implictly added
  // by the compiler.
  // Since we're returning from the end of the function, there'll always be a
  // null value at the base of the current call frame the reserved return value
  // slot.
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

// Update the size value for OP_PUSH_LIST instruction.
static void patchListSize(Compiler* compiler, int size_index, int size) {
  _FN->opcodes.data[size_index] = (size >> 8) & 0xff;
  _FN->opcodes.data[size_index + 1] = size & 0xff;
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

// Compile a class and return it's index in the module's types buffer.
static void compileClass(Compiler* compiler) {

  // Consume the name of the type.
  consume(compiler, TK_NAME, "Expected a type name.");
  const char* name = compiler->parser.previous.start;
  int name_len = compiler->parser.previous.length;

  // Create a new class.
  int cls_index, ctor_index;
  Class* cls = newClass(compiler->parser.vm, compiler->module,
                        name, (uint32_t)name_len, &cls_index, &ctor_index);
  cls->ctor->fn->arity = 0;

  // FIXME:
  // Temproary patch for moving functions and classes to constant buffer.
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);
  int index = compilerAddVariable(compiler,
                                  compiler->parser.previous.start,
                                  compiler->parser.previous.length,
                                  compiler->parser.previous.line);
  moduleSetGlobal(compiler->module, index, VAR_OBJ(cls));

  // Check count exceeded.
  checkMaxConstantsReached(compiler, cls_index);
  checkMaxConstantsReached(compiler, ctor_index);

  // Compile the constructor function.
  ASSERT(compiler->func->ptr == compiler->module->body->fn, OOPS);
  Func curr_fn;
  compilerPushFunc(compiler, &curr_fn, cls->ctor->fn);
  compilerEnterBlock(compiler);

  // Push an instance on the stack.
  emitOpcode(compiler, OP_PUSH_INSTANCE);
  emitShort(compiler, cls_index);

  skipNewLines(compiler);
  TokenType next = peek(compiler);
  while (next != TK_END && next != TK_EOF) {

    // Compile field name.
    consume(compiler, TK_NAME, "Expected a type name.");
    const char* f_name = compiler->parser.previous.start;
    int f_len = compiler->parser.previous.length;

    uint32_t f_index = moduleAddName(compiler->module, compiler->parser.vm,
                                     f_name, f_len);

    String* new_name = compiler->module->names.data[f_index];
    for (uint32_t i = 0; i < cls->field_names.count; i++) {
      String* prev = compiler->module->names.data[cls->field_names.data[i]];
      if (IS_STR_EQ(new_name, prev)) {
        parseError(compiler, "Class field with name '%s' already exists.",
                   new_name->data);
      }
    }

    pkUintBufferWrite(&cls->field_names, compiler->parser.vm, f_index);

    // Consume the assignment expression.
    consume(compiler, TK_EQ, "Expected an assignment after field name.");
    compileExpression(compiler); // Assigned value.
    consumeEndStatement(compiler);

    // At this point the stack top would be the expression.
    emitOpcode(compiler, OP_INST_APPEND);

    skipNewLines(compiler);
    next = peek(compiler);
  }
  consume(compiler, TK_END, "Expected 'end' after a class declaration end.");

  // The instance pushed by the OP_PUSH_INSTANCE instruction is at the top
  // of the stack, return it (Constructor will return the instance). Note that
  // the emitFunctionEnd function will also add a return instruction but that's
  // for functions which doesn't return anything explicitly. This return won't
  // change compiler's stack size because it won't pop the return value.
  emitOpcode(compiler, OP_RETURN);

  compilerExitBlock(compiler);
  emitFunctionEnd(compiler);
  compilerPopFunc(compiler);
}

// Compile a function and return it's index in the module's function buffer.
static void compileFunction(Compiler* compiler, bool is_literal) {

  const char* name;
  int name_length;

  if (!is_literal) {
    consume(compiler, TK_NAME, "Expected a function name.");
    name = compiler->parser.previous.start;
    name_length = compiler->parser.previous.length;

  } else {
    name = LITERAL_FN_NAME;
    name_length = (int)strlen(name);
  }

  int fn_index;
  Function* func = newFunction(compiler->parser.vm, name, name_length,
                               compiler->module, false, NULL, &fn_index);
  checkMaxConstantsReached(compiler, fn_index);

  if (!is_literal) {
    ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);
    int name_line = compiler->parser.previous.line;
    int g_index = compilerAddVariable(compiler, name, name_length, name_line);

    vmPushTempRef(compiler->parser.vm, &func->_super); // func.
    Closure* closure = newClosure(compiler->parser.vm, func);
    moduleSetGlobal(compiler->module, g_index, VAR_OBJ(closure));
    vmPopTempRef(compiler->parser.vm); // func.
  }

  Func curr_fn;
  compilerPushFunc(compiler, &curr_fn, func);

  int argc = 0;
  compilerEnterBlock(compiler); // Parameter depth.

  // Parameter list is optional.
  if (match(compiler, TK_LPARAN) && !match(compiler, TK_RPARAN)) {
    do {
      skipNewLines(compiler);

      consume(compiler, TK_NAME, "Expected a parameter name.");
      argc++;

      const char* param_name = compiler->parser.previous.start;
      uint32_t param_len = compiler->parser.previous.length;

      // TODO: move this to a functions.
      bool predefined = false;
      for (int i = compiler->func->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->func->locals[i];
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
                          compiler->parser.previous.line);

    } while (match(compiler, TK_COMMA));

    consume(compiler, TK_RPARAN, "Expected ')' after parameter list.");
  }

  func->arity = argc;
  compilerChangeStack(compiler, argc);

  compileBlockBody(compiler, BLOCK_FUNC);

  consume(compiler, TK_END, "Expected 'end' after function definition end.");
  compilerExitBlock(compiler); // Parameter depth.
  emitFunctionEnd(compiler);

#if DUMP_BYTECODE
  // FIXME:
  // Forward patch are pending so we can't dump constant value that
  // needs to be patched.
  //dumpFunctionCode(compiler->parser.vm, compiler->func->ptr);
#endif

  compilerPopFunc(compiler);

  // Note: After the above compilerPopFunc() call, now we're at the outer
  // function of this function, and the bellow emit calls will write to the
  // outer function. If it's a literal function, we need to push a closure
  // of it on the stack.
  if (is_literal) {
    emitOpcode(compiler, OP_PUSH_CLOSURE);
    emitShort(compiler, fn_index);

    // Capture the upvalues when the closure is created.
    for (int i = 0; i < curr_fn.ptr->upvalue_count; i++) {
      emitByte(compiler, (curr_fn.upvalues[i].is_immediate) ? 1 : 0);
      emitByte(compiler, curr_fn.upvalues[i].index);
    }
  }
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
// path) and return it as a module pointer. And it'll emit opcodes to push
// that module to the stack.
static Module* importFile(Compiler* compiler, const char* path) {
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);

  PKVM* vm = compiler->parser.vm;

  // Resolve the path.
  PkStringPtr resolved = { path, NULL, NULL };
  if (vm->config.resolve_path_fn != NULL) {
    resolved = vm->config.resolve_path_fn(vm, compiler->module->path->data,
                                          path);
  }

  if (resolved.string == NULL) {
    parseError(compiler, "Cannot resolve path '%s' from '%s'", path,
               compiler->module->path->data);
  }

  // Create new string for the resolved path. And free the resolved path.
  int index = (int)moduleAddName(compiler->module, compiler->parser.vm,
                           resolved.string, (uint32_t)strlen(resolved.string));
  String* path_name = compiler->module->names.data[index];
  if (resolved.on_done != NULL) resolved.on_done(vm, resolved);

  // Check if the script already compiled and cached in the PKVM.
  Var entry = mapGet(vm->modules, VAR_OBJ(path_name));
  if (!IS_UNDEF(entry)) {
    ASSERT(IS_OBJ_TYPE(entry, OBJ_MODULE), OOPS);

    // Push the compiled script on the stack.
    emitOpcode(compiler, OP_IMPORT);
    emitShort(compiler, index);
    return (Module*)AS_OBJ(entry);
  }

  // The script not exists in the VM, make sure we have the script loading
  // api function.
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

  // Make a new module and to compile it.
  Module* module = newModule(vm, path_name, false);
  vmPushTempRef(vm, &module->_super); // scr.
  mapSet(vm, vm->modules, VAR_OBJ(path_name), VAR_OBJ(module));
  vmPopTempRef(vm); // scr.

  // Push the compiled script on the stack.
  emitOpcode(compiler, OP_IMPORT);
  emitShort(compiler, index);

  // Even if we're running on repl mode the imported module cannot run on
  // repl mode.
  PkCompileOptions options = pkNewCompilerOptions();
  if (compiler->options) options = *compiler->options;
  options.repl_mode = false;

  // Compile the source to the module and clean the source.
  PkResult result = compile(vm, module, source.string, &options);
  if (source.on_done != NULL) source.on_done(vm, source);

  if (result != PK_RESULT_SUCCESS) {
    parseError(compiler, "Compilation of imported script '%s' failed",
               path_name->data);
  }

  return module;
}

// Import the native module from the PKVM's core_libs and it'll emit opcodes
// to push that module to the stack.
static Module* importCoreLib(Compiler* compiler, const char* name_start,
                             int name_length) {
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);

  // Add the name to the module's name buffer, we need it as a key to the
  // PKVM's module cache.
  int index = (int)moduleAddName(compiler->module, compiler->parser.vm,
                                 name_start, name_length);
  String* module_name = compiler->module->names.data[index];

  Var entry = mapGet(compiler->parser.vm->core_libs, VAR_OBJ(module_name));
  if (IS_UNDEF(entry)) {
    parseError(compiler, "No module named '%s' exists.", module_name->data);
    return NULL;
  }

  // Push the module on the stack.
  emitOpcode(compiler, OP_IMPORT);
  emitShort(compiler, index);

  ASSERT(IS_OBJ_TYPE(entry, OBJ_MODULE), OOPS);
  return (Module*)AS_OBJ(entry);
}

// Push the imported module on the stack and return the pointer. It could be
// either core library or a local import.
static inline Module* compilerImport(Compiler* compiler) {
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);

  // Get the module (from native libs or VM's cache or compile new one).
  // And push it on the stack.
  if (match(compiler, TK_NAME)) { //< Core library.
    return importCoreLib(compiler, compiler->parser.previous.start,
                         compiler->parser.previous.length);

  } else if (match(compiler, TK_STRING)) { //< Local library.
    Var var_path = compiler->parser.previous.value;
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

    // TODO:
    // Make it possible to override any name (ie. the syntax `print = 1`
    // should pass) and allow imported entries to have the same name of
    // builtin functions.
    case NAME_BUILTIN_FN:
      parseError(compiler, "Name '%.*s' already exists.", length, name);
      return -1;
  }

  UNREACHABLE();
  return -1;
}

// This will called by the compilerImportAll() function to import a single
// entry from the imported module. (could be a function or global variable).
static void compilerImportSingleEntry(Compiler* compiler,
                                      const char* name, uint32_t length) {

  // Special names are begins with '@' (implicit main function, literal
  // functions etc) skip them.
  if (name[0] == SPECIAL_NAME_CHAR) return;

  // Line number of the variables which will be bind to the imported symbol.
  int line = compiler->parser.previous.line;

  // Add the name to the **current** module's name buffer.
  int name_index = (int)moduleAddName(compiler->module, compiler->parser.vm,
                                      name, length);

  // Get the global/function/class from the module.
  emitOpcode(compiler, OP_GET_ATTRIB_KEEP);
  emitShort(compiler, name_index);

  int index = compilerImportName(compiler, line, name, length);
  if (index != -1) emitStoreGlobal(compiler, index);
  emitOpcode(compiler, OP_POP);
}

// Import all from the module, which is also would be at the top of the stack
// before executing the below instructions.
static void compilerImportAll(Compiler* compiler, Module* module) {

  ASSERT(module != NULL, OOPS);
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);

  // Import all globals.
  ASSERT(module->global_names.count == module->globals.count, OOPS);
  for (uint32_t i = 0; i < module->globals.count; i++) {
    ASSERT(module->global_names.data[i] < module->names.count, OOPS);
    const String* name = module->names.data[module->global_names.data[i]];

    compilerImportSingleEntry(compiler, name->data, name->length);
  }
}

// from module import symbol [as alias [, symbol2 [as alias]]]
static void compileFromImport(Compiler* compiler) {
  ASSERT(compiler->scope_depth == DEPTH_GLOBAL, OOPS);

  // Import the library and push it on the stack. If the import failed
  // lib_from would be NULL.
  Module* lib_from = compilerImport(compiler);

  // At this point the module would be on the stack before executing the next
  // instruction.
  consume(compiler, TK_IMPORT, "Expected keyword 'import'.");

  if (match(compiler, TK_STAR)) {
    // from math import *
    if (lib_from) compilerImportAll(compiler, lib_from);

  } else {
    do {
      // Consume the symbol name to import from the module.
      consume(compiler, TK_NAME, "Expected symbol to import.");
      const char* name = compiler->parser.previous.start;
      uint32_t length = (uint32_t)compiler->parser.previous.length;
      int line = compiler->parser.previous.line;

      // Add the name of the symbol to the names buffer.
      int name_index = (int)moduleAddName(compiler->module,
                                          compiler->parser.vm,
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
      name = compiler->parser.previous.start;
      length = (uint32_t)compiler->parser.previous.length;
      line = compiler->parser.previous.line;

      // Get the variable to bind the imported symbol, if we already have a
      // variable with that name override it, otherwise use a new variable.
      int var_index = compilerImportName(compiler, line, name, length);
      if (var_index != -1) emitStoreGlobal(compiler, var_index);
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
    Module* lib = compilerImport(compiler);

    // variable to bind the imported module.
    int var_index = -1;

    // Check if it has an alias, if so bind the variable with that name.
    if (match(compiler, TK_AS)) {
      // Consuming it'll update the previous token which would be the name of
      // the binding variable.
      consume(compiler, TK_NAME, "Expected a name after 'as'.");

      // Get the variable to bind the imported symbol, if we already have a
      // variable with that name override it, otherwise use a new variable.
      const char* name = compiler->parser.previous.start;
      int length = compiler->parser.previous.length;
      int line = compiler->parser.previous.line;
      var_index = compilerImportName(compiler, line, name, length);

    } else {
      // If it has a module name use it as binding variable.
      // Core libs names are it's module name but for local libs it's optional
      // to define a module name for a module.
      if (lib && lib->name != NULL) {

        // Get the variable to bind the imported symbol, if we already have a
        // variable with that name override it, otherwise use a new variable.
        const char* name = lib->name->data;
        uint32_t length = lib->name->length;
        int line = compiler->parser.previous.line;
        var_index = compilerImportName(compiler, line, name, length);

      } else {
        // -- Nothing to do here --
        // Importing from path which doesn't have a module name. Import
        // everything of it. and bind to a variables.
        NO_OP;
      }
    }

    if (var_index != -1) {
      emitStoreGlobal(compiler, var_index);
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
  const char* iter_name = compiler->parser.previous.start;
  int iter_len = compiler->parser.previous.length;
  int iter_line = compiler->parser.previous.line;

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

  // is_temporary will be set to true if the statement is an temporary
  // expression, it'll used to be pop from the stack.
  bool is_temporary = false;

  // This will be set to true if the statement is an expression. It'll used to
  // print it's value when running in REPL mode.
  bool is_expression = false;

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

      // If the last expression parsed with compileExpression() is a call
      // is_last_call would be true by now.
      if (compiler->is_last_call) {
        // Tail call optimization disabled at debug mode.
        if (compiler->options && !compiler->options->debug) {
          ASSERT(_FN->opcodes.count >= 2, OOPS); // OP_CALL, argc
          ASSERT(_FN->opcodes.data[_FN->opcodes.count - 2] == OP_CALL, OOPS);
          _FN->opcodes.data[_FN->opcodes.count - 2] = OP_TAIL_CALL;
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
    if (!compiler->new_local) is_temporary = true;

    compiler->new_local = false;
  }

  // If running REPL mode, print the expression's evaluated value.
  if (compiler->options && compiler->options->repl_mode &&
      compiler->func->ptr == compiler->module->body->fn &&
      is_expression /*&& compiler->scope_depth == DEPTH_GLOBAL*/) {
    emitOpcode(compiler, OP_REPL_PRINT);
  }

  if (is_temporary) emitOpcode(compiler, OP_POP);
}

// Compile statements that are only valid at the top level of the module. Such
// as import statement, function define, and if we're running REPL mode top
// level expression's evaluated value will be printed.
static void compileTopLevelStatement(Compiler* compiler) {

  // At the top level the stack size should be 0, before and after compiling
  // a top level statement, since there aren't any locals at the top level.
  ASSERT(compiler->parser.has_errors || compiler->func->stack_size == 0, OOPS);

  if (match(compiler, TK_CLASS)) {
    compileClass(compiler);

  } else if (match(compiler, TK_DEF)) {
    compileFunction(compiler, false);

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

  // At the top level the stack size should be 0, before and after compiling
  // a top level statement, since there aren't any locals at the top level.
  ASSERT(compiler->parser.has_errors || compiler->func->stack_size == 0, OOPS);

}

PkResult compile(PKVM* vm, Module* module, const char* source,
                 const PkCompileOptions* options) {

  // Skip utf8 BOM if there is any.
  if (strncmp(source, "\xEF\xBB\xBF", 3) == 0) source += 3;

  Compiler _compiler;
  Compiler* compiler = &_compiler; //< Compiler pointer for quick access.
  compilerInit(compiler, vm, source, module, options);

  // If compiling for an imported module the vm->compiler would be the compiler
  // of the module that imported this module. Add the all the compilers into a
  // link list.
  compiler->next_compiler = vm->compiler;
  vm->compiler = compiler;

  // If the module doesn't has a body by default, it's probably was created by
  // the native api function (pkNewModule() that'll return a module without a
  // main function) so just create and add the function here.
  if (module->body == NULL) moduleAddMain(vm, module);

  // If we're compiling for a module that was already compiled (when running
  // REPL or evaluating an expression) we don't need the old main anymore.
  // just use the globals and functions of the module and use a new body func.
  pkByteBufferClear(&module->body->fn->fn->opcodes, vm);

  // Remember the count of constants, names, and globals, If the compilation
  // failed discard all of them and roll back.
  uint32_t constants_count = module->constants.count;
  uint32_t names_count = module->names.count;
  uint32_t globals_count = module->globals.count;

  Func curr_fn;
  compilerPushFunc(compiler, &curr_fn, module->body->fn);

  // At the begining the compiler's scope will be DEPTH_GLOBAL and that'll be
  // set to to the current functions depth. Override for the body function.
  curr_fn.depth = DEPTH_MODULE;

  // Lex initial tokens. current <-- next.
  lexToken(&(compiler->parser));
  lexToken(&(compiler->parser));
  skipNewLines(compiler);

  if (match(compiler, TK_MODULE)) {

    // If the module running a REPL or compiled multiple times by hosting
    // application module attribute might already set. In that case make it
    // Compile error.
    if (module->name != NULL) {
      parseError(compiler, "Module name already defined.");

    } else {
      consume(compiler, TK_NAME, "Expected a name for the module.");
      const char* name = compiler->parser.previous.start;
      uint32_t len = compiler->parser.previous.length;
      module->name = newStringLength(vm, name, len);
      consumeEndStatement(compiler);
    }
  }

  while (!match(compiler, TK_EOF)) {
    compileTopLevelStatement(compiler);
    skipNewLines(compiler);
  }

  emitFunctionEnd(compiler);

  // Resolve forward names (function names that are used before defined).
  for (int i = 0; i < compiler->parser.forwards_count; i++) {
    ForwardName* forward = &compiler->parser.forwards[i];
    const char* name = forward->name;
    int length = forward->length;
    int index = moduleGetGlobalIndex(compiler->module, name, (uint32_t)length);
    if (index != -1) {
      patchForward(compiler, forward->func, forward->instruction, index);
    } else {
      // need_more_lines is only true for unexpected EOF errors. For syntax
      // errors it'll be false by now but. Here it's a semantic errors, so
      // we're overriding it to false.
      compiler->parser.need_more_lines = false;
      resolveError(compiler, forward->line, "Name '%.*s' is not defined.",
                   length, name);
    }
  }

  vm->compiler = compiler->next_compiler;

  // If compilation failed, discard all the invalid functions and globals.
  if (compiler->parser.has_errors) {
    module->constants.count = constants_count;
    module->names.count = names_count;
    module->globals.count = module->global_names.count = globals_count;
  }

#if DUMP_BYTECODE
  dumpFunctionCode(compiler->parser.vm, module->body);
#endif

  // Return the compilation result.
  if (compiler->parser.has_errors) {
    if (compiler->parser.repl_mode && compiler->parser.need_more_lines) {
      return PK_RESULT_UNEXPECTED_EOF;
    }
    return PK_RESULT_COMPILE_ERROR;
  }
  return PK_RESULT_SUCCESS;
}

PkResult pkCompileModule(PKVM* vm, PkHandle* module_handle, PkStringPtr source,
                         const PkCompileOptions* options) {
  __ASSERT(module_handle != NULL, "Argument module was NULL.");
  __ASSERT(IS_OBJ_TYPE(module_handle->value, OBJ_MODULE),
           "Given handle is not a module.");
  Module* module = (Module*)AS_OBJ(module_handle->value);

  PkResult result = compile(vm, module, source.string, options);
  if (source.on_done) source.on_done(vm, source);
  return result;
}

void compilerMarkObjects(PKVM* vm, Compiler* compiler) {

  // Mark the module which is currently being compiled.
  markObject(vm, &compiler->module->_super);

  // Mark the string literals (they haven't added to the module's literal
  // buffer yet).
  markValue(vm, compiler->parser.current.value);
  markValue(vm, compiler->parser.previous.value);
  markValue(vm, compiler->parser.next.value);

  if (compiler->next_compiler != NULL) {
    compilerMarkObjects(vm, compiler->next_compiler);
  }
}
