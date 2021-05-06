/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "compiler.h"

#include <stdio.h>
#include <string.h>

#include "core.h"
#include "types/gen/byte_buffer.h"
#include "utils.h"
#include "vm.h"

#if DEBUG_DUMP_COMPILED_CODE
  #include "debug.h"
#endif

// The maximum number of variables (or global if compiling top level script)
// to lookup from the compiling context. Also it's limited by it's opcode
// which is using a single byte value to identify the local.
#define MAX_VARIABLES 256

// The maximum number of constant literal a script can contain. Also it's
// limited by it's opcode which is using a short value to identify.
#define MAX_CONSTANTS (1 << 16)

// The maximum address possible to jump. Similar limitation as above.
#define MAX_JUMP (1 << 16)

// Max number of break statement in a loop statement to patch.
#define MAX_BREAK_PATCH 256

// The size of the compiler time error message buffer excluding the file path,
// line number, and function name. Used for `vsprintf` and `vsnprintf` is not
// available in C++98.
#define ERROR_MESSAGE_SIZE 256

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
  TK_SRIGHT,     // >>
  TK_SLEFT,      // <<

  //TODO:
  //TK_SRIGHTEQ  // >>=
  //TK_SLEFTEQ   // <<=
  //TK_MODEQ,    // %=
  //TK_XOREQ,    // ^=

  // Keywords.
  TK_DEF,        // def
  TK_NATIVE,     // native   (C function declaration)
  TK_FUNCTION,   // function (literal function)
  TK_END,        // end

  TK_NULL,       // null
  TK_SELF,       // self
  TK_IN,         // in
  TK_AND,        // and
  TK_OR,         // or
  TK_NOT,        // not / !
  TK_TRUE,       // true
  TK_FALSE,      // false

  TK_DO,         // do
  TK_WHILE,      // while
  TK_FOR,        // for
  TK_IF,         // if
  TK_ELIF,       // elif
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
  { "def",      3, TK_DEF },
  { "native",   6, TK_NATIVE },
  { "function", 8, TK_FUNCTION },
  { "end",      3, TK_END },
  { "null",     4, TK_NULL },
  { "self",     4, TK_SELF },
  { "in",       2, TK_IN },
  { "and",      3, TK_AND },
  { "or",       2, TK_OR },
  { "not",      3, TK_NOT },
  { "true",     4, TK_TRUE },
  { "false",    5, TK_FALSE },
  { "do",       2, TK_DO },
  { "while",    5, TK_WHILE },
  { "for",      3, TK_FOR },
  { "if",       2, TK_IF },
  { "elif",     4, TK_ELIF },
  { "else",     4, TK_ELSE },
  { "break",    5, TK_BREAK },
  { "continue", 8, TK_CONTINUE },
  { "return",   6, TK_RETURN },

  { NULL,       0, (TokenType)(0) }, // Sentinal to mark the end of the array
};

typedef struct {
  MSVM* vm;           //< Owner of the parser (for reporting errors, etc).

  const char* source; //< Currently compiled source.
  const char* path;   //< Path of the source.

  const char* token_start;  //< Start of the currently parsed token.
  const char* current_char; //< Current char position in the source.
  int current_line;         //< Line number of the current char.

  Token previous, current, next; //< Currently parsed tokens.

  bool has_errors;    //< True if any syntex error occured at compile time.
} Parser;

// Compiler Types ////////////////////////////////////////////////////////////

// Precedence parsing references:
// https://en.wikipedia.org/wiki/Shunting-yard_algorithm

typedef enum {
  PREC_NONE,
  PREC_LOWEST,
  PREC_LOGICAL_OR,    // or
  PREC_LOGICAL_AND,   // and
  PREC_LOGICAL_NOT,   // not
  PREC_EQUALITY,      // == !=
  PREC_IN,            // in
  PREC_IS,            // is
  PREC_COMPARISION,   // < > <= >=
  PREC_BITWISE_OR,    // |
  PREC_BITWISE_XOR,   // ^
  PREC_BITWISE_AND,   // &
  PREC_BITWISE_SHIFT, // << >>
  PREC_RANGE,         // ..
  PREC_TERM,          // + -
  PREC_FACTOR,        // * / %
  PREC_UNARY,         // - ! ~
  PREC_CALL,          // ()
  PREC_SUBSCRIPT,     // []
  PREC_ATTRIB,        // .index
  PREC_PRIMARY,
} Precedence;

typedef void (*GrammarFn)(Compiler* compiler, bool can_assign);

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
  int length;       //< Length of the name.
  int depth;        //< The depth the local is defined in.
  int line;         //< The line variable declared for debugging.
} Variable;

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

} Loop;

typedef struct sFunc {

  // Scope of the function. -2 for script body, -1 for top level function and 
  // literal functions will have the scope where it declared.
  int depth;

  // The actual function pointer which is being compiled.
  Function* ptr;

  // If outer function of a literal or the script body function of a script
  // function. Null for script body function.
  struct sFunc* outer_func;

} Func;

// A convinent macro to get the current function.
#define _FN (compiler->func->ptr->fn)

struct Compiler {

  MSVM* vm;
  Parser parser;

  // Current depth the compiler in (-1 means top level) 0 means function
  // level and > 0 is inner scope.
  int scope_depth;

  Variable variables[MAX_VARIABLES]; //< Variables in the current context.
  int var_count;                     //< Number of locals in [variables].
  int global_count;                  //< Number of globals in [variables].

  int stack_size; //< Current size including locals ind temps.=

  Script* script;  //< Current script.
  Loop* loop;      //< Current loop.
  Func* func;      //< Current function.

  // True if the last statement is a new local variable assignment. Because
  // the assignment is different than reqular assignment and use this boolean 
  // to tell the compiler that dont pop it's assigned value because the value
  // itself is the local.
  bool new_local;
};

typedef struct {
  int params;
  int stack;
} OpInfo;

static OpInfo opcode_info[] = {
  #define OPCODE(name, params, stack) { params, stack },
  #include "opcodes.h"
  #undef OPCODE
};

/*****************************************************************************
 * ERROR HANDLERS                                                            *
 *****************************************************************************/

static void reportError(Parser* parser, const char* file, int line,
                        const char* fmt, va_list args) {
  MSVM* vm = parser->vm;
  parser->has_errors = true;
  char message[ERROR_MESSAGE_SIZE];
  int length = vsprintf(message, fmt, args);
  ASSERT(length < ERROR_MESSAGE_SIZE, "Error message buffer should not exceed "
    "the buffer");
  vm->config.error_fn(vm, MS_ERROR_COMPILE, file, line, message);
}

// Error caused at the middle of lexing (and TK_ERROR will be lexed insted).
static void lexError(Parser* parser, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  reportError(parser, parser->path, parser->current_line, fmt, args);
  va_end(args);
}

// Error caused when parsing. The associated token assumed to be last consumed
// which is [parser->previous].
static void parseError(Parser* parser, const char* fmt, ...) {

  Token* token = &parser->previous;

  // Lex errors would repored earlier by lexError and lexed a TK_ERROR token.
  if (token->type == TK_ERROR) return;

  va_list args;
  va_start(args, fmt);
  reportError(parser, parser->path, token->line, fmt, args);
  va_end(args);
}

/*****************************************************************************
 * LEXING                                                                    *
 *****************************************************************************/

// Forward declaration of lexer methods.

static char eatChar(Parser* parser);
static void setNextValueToken(Parser* parser, TokenType type, Var value);
static void setNextToken(Parser* parser, TokenType type);
static bool matchChar(Parser* parser, char c);
static bool matchLine(Parser* parser);

static void eatString(Parser* parser, bool single_quote) {
  ByteBuffer buff;
  byteBufferInit(&buff);

  char quote = (single_quote) ? '\'' : '"';

  while (true) {
    char c = eatChar(parser);

    if (c == quote) break;

    if (c == '\0') {
      lexError(parser, "Non terminated string.");

      // Null byte is required by TK_EOF.
      parser->current_char--;
      break;
    }

    if (c == '\\') {
      switch (eatChar(parser)) {
        case '"': byteBufferWrite(&buff, parser->vm, '"'); break;
        case '\'': byteBufferWrite(&buff, parser->vm, '\''); break;
        case '\\': byteBufferWrite(&buff, parser->vm, '\\'); break;
        case 'n': byteBufferWrite(&buff, parser->vm, '\n'); break;
        case 'r': byteBufferWrite(&buff, parser->vm, '\r'); break;
        case 't': byteBufferWrite(&buff, parser->vm, '\t'); break;

        default:
          lexError(parser, "Error: invalid escape character");
          break;
      }
    } else {
      byteBufferWrite(&buff, parser->vm, c);
    }
  }

  // '\0' will be added by varNewSring();
  Var string = VAR_OBJ(&newString(parser->vm, (const char*)buff.data,
    (uint32_t)buff.count)->_super);

  byteBufferClear(&buff, parser->vm);

  setNextValueToken(parser, TK_STRING, string);
}

// Returns the current char of the parser on.
static char peekChar(Parser* parser) {
  return *parser->current_char;
}

// Returns the next char of the parser on.
static char peekNextChar(Parser* parser) {
  if (peekChar(parser) == '\0') return '\0';
  return *(parser->current_char + 1);
}

// Advance the parser by 1 char.
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

  // TODO: hex, binary and scientific literals.

  while (utilIsDigit(peekChar(parser)))
    eatChar(parser);

  if (peekChar(parser) == '.' && utilIsDigit(peekNextChar(parser))) {
    matchChar(parser, '.');
    while (utilIsDigit(peekChar(parser)))
      eatChar(parser);
  }
  errno = 0;
  Var value = VAR_NUM(strtod(parser->token_start, NULL));
  if (errno == ERANGE) {
    const char* start = parser->token_start;
    int len = (int)(parser->current_char - start);
    lexError(parser, "Literal is too large (%.*s)", len, start);
    value = VAR_NUM(0);
  }

  setNextValueToken(parser, TK_NUMBER, value);
}

// Read and ignore chars till it reach new line or EOF.
static void skipLineComment(Parser* parser) {
  char c;
  while ((c = peekChar(parser)) != '\0') {
    eatChar(parser);
    if (c == '\n') return;
  }
}

// Will skip multiple new lines.
static void skipNewLines(Parser* parser) {
  matchLine(parser);
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
  parser->next.type = type;
  parser->next.start = parser->token_start;
  parser->next.length = (int)(parser->current_char - parser->token_start);
  parser->next.line = parser->current_line - ((type == TK_LINE) ? 1 : 0);
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
    char c = eatChar(parser);

    switch (c) {
      case ',': setNextToken(parser, TK_COMMA); return;
      case ':': setNextToken(parser, TK_COLLON); return;
      case ';': setNextToken(parser, TK_SEMICOLLON); return;
      case '#': skipLineComment(parser); break;
      case '(': setNextToken(parser, TK_LPARAN); return;
      case ')': setNextToken(parser, TK_RPARAN); return;
      case '[': setNextToken(parser, TK_LBRACKET); return;
      case ']': setNextToken(parser, TK_RBRACKET); return;
      case '{': setNextToken(parser, TK_LBRACE); return;
      case '}': setNextToken(parser, TK_RBRACE); return;
      case '%': setNextToken(parser, TK_PERCENT); return;

      case '~': setNextToken(parser, TK_TILD); return;
      case '&': setNextToken(parser, TK_AMP); return;
      case '|': setNextToken(parser, TK_PIPE); return;
      case '^': setNextToken(parser, TK_CARET); return;

      case '\n': setNextToken(parser, TK_LINE); return;

      case ' ':
      case '\t':
      case '\r': {
        char c = peekChar(parser);
        while (c == ' ' || c == '\t' || c == '\r') {
          eatChar(parser);
          c = peekChar(parser);
        }
        break;
      }

      case '.': // TODO: ".5" should be a valid number.
        setNextTwoCharToken(parser, '.', TK_DOT, TK_DOTDOT);
        return;

      case '=':
        setNextTwoCharToken(parser, '=', TK_EQ, TK_EQEQ);
        return;

      case '!':
        setNextTwoCharToken(parser, '=', TK_NOT, TK_NOTEQ);
        return;

      case '>':
        if (matchChar(parser, '>'))
          setNextToken(parser, TK_SRIGHT);
        else
          setNextTwoCharToken(parser, '=', TK_GT, TK_GTEQ);
        return;

      case '<':
        if (matchChar(parser, '<'))
          setNextToken(parser, TK_SLEFT);
        else
          setNextTwoCharToken(parser, '=', TK_LT, TK_LTEQ);
        return;

      case '+':
        setNextTwoCharToken(parser, '=', TK_PLUS, TK_PLUSEQ);
        return;

      case '-':
        setNextTwoCharToken(parser, '=', TK_MINUS, TK_MINUSEQ);
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
            lexError(parser, "Invalid character %c", c);
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

/*****************************************************************************
 * PARSING                                                                   *
 *****************************************************************************/

 // Initialize the parser.
static void parserInit(Parser* self, MSVM* vm, const char* source,
                       const char* path) {
  self->vm = vm;
  self->source = source;
  self->path = path;
  self->token_start = source;
  self->current_char = source;
  self->current_line = 1;
  self->has_errors = false;

  self->next.type = TK_ERROR;
  self->next.start = NULL;
  self->next.length = 0;
  self->next.line = 1;
  self->next.value = VAR_UNDEFINED;
}

// Returns current token type.
static TokenType peek(Parser* self) {
  return self->current.type;
}

// Returns next token type.
static TokenType peekNext(Parser* self) {
  return self->next.type;
}

// Consume the current token if it's expected and lex for the next token
// and return true otherwise reutrn false.
static bool match(Parser* self, TokenType expected) {
  //ASSERT(expected != TK_LINE, "Can't match TK_LINE.");
  //matchLine(self);

  if (peek(self) != expected) return false;
  lexToken(self);
  return true;
}

// Consume the the current token and if it's not [expected] emits error log
// and continue parsing for more error logs.
static void consume(Parser* self, TokenType expected, const char* err_msg) {
  //ASSERT(expected != TK_LINE, "Can't match TK_LINE.");
  //matchLine(self);

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
static bool matchLine(Parser* parser) {
  if (peek(parser) != TK_LINE) return false;
  while (peek(parser) == TK_LINE)
    lexToken(parser);
  return true;
}

// Match semi collon, multiple new lines or peek 'end' keyword.
static bool matchEndStatement(Parser* parser) {
  if (match(parser, TK_SEMICOLLON)) {
    skipNewLines(parser);
    return true;
  }

  if (matchLine(parser) || peek(parser) == TK_END || peek(parser) == TK_EOF)
    return true;
  return false;
}

// Consume semi collon, multiple new lines or peek 'end' keyword.
static void consumeEndStatement(Parser* parser) {
  if (!matchEndStatement(parser)) {
    parseError(parser, "Expected statement end with newline or ';'.");
  }
}

// Match optional "do" keyword and new lines.
static void consumeStartBlock(Parser* parser) {
  bool consumed = false;

  // Match optional "do".
  if (match(parser, TK_DO))
    consumed = true;

  if (matchLine(parser))
    consumed = true;

  if (!consumed) {
    parseError(parser, "Expected enter block with newline or 'do'.");
  }
}

// Returns a optional compound assignment.
static bool matchAssignment(Parser* parser) {
  if (match(parser, TK_EQ)) return true;
  if (match(parser, TK_PLUSEQ)) return true;
  if (match(parser, TK_MINUSEQ)) return true;
  if (match(parser, TK_STAREQ)) return true;
  if (match(parser, TK_DIVEQ)) return true;
  return false;
}

/*****************************************************************************
 * NAME SEARCH                                                               *
 *****************************************************************************/

// Result type for an identifier definition.
typedef enum {
  NAME_NOT_DEFINED,
  NAME_LOCAL_VAR,  //< Including parameter.
  NAME_GLOBAL_VAR,
  NAME_FUNCTION,
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
  int index;

  // Search through local and global valriables.
  NameDefnType type = NAME_LOCAL_VAR; //< Will change to local.

  // [index] will points to the ith local or ith global (will update).
  index = compiler->var_count - compiler->global_count - 1;

  for (int i = compiler->var_count - 1; i >= 0; i--) {
    Variable* variable = &compiler->variables[i];

    // Literal functions are not closures and ignore it's outer function's
    // local variables.
    if (variable->depth != DEPTH_GLOBAL &&
        compiler->func->depth >= variable->depth) {
      continue;
    }

    if (type == NAME_LOCAL_VAR && variable->depth == DEPTH_GLOBAL) {
      type = NAME_GLOBAL_VAR;
      index = compiler->global_count - 1;
    }

    if (length == variable->length) {
      if (strncmp(variable->name, name, length) == 0) {
        result.type = type;
        result.index = index;
        return result;
      }
    }

    index--;
  }

  // Search through functions.
  index = scriptSearchFunc(compiler->script, name, length);
  if (index != -1) {
    result.type = NAME_FUNCTION;
    result.index = index;
    return result;
  }

  // Search through builtin functions.
  index = findBuiltinFunction(name, length);
  if (index != -1) {
    result.type = NAME_BUILTIN;
    result.index = index;
    return result;
  }

  return result;
}

/*****************************************************************************
 * PARSING GRAMMAR                                                           *
 *****************************************************************************/

// Forward declaration of codegen functions.
static void emitOpcode(Compiler* compiler, Opcode opcode);
static int emitByte(Compiler* compiler, int byte);
static int emitShort(Compiler* compiler, int arg);
static int compilerAddConstant(Compiler* compiler, Var value);

static int compilerAddVariable(Compiler* compiler, const char* name,
  int length, int line);
static int compilerAddConstant(Compiler* compiler, Var value);
static int compileFunction(Compiler* compiler, FuncType fn_type);

// Forward declaration of grammar functions.
static void parsePrecedence(Compiler* compiler, Precedence precedence);
static void compileExpression(Compiler* compiler);

static void exprLiteral(Compiler* compiler, bool can_assign);
static void exprFunc(Compiler* compiler, bool can_assign);
static void exprName(Compiler* compiler, bool can_assign);

static void exprBinaryOp(Compiler* compiler, bool can_assign);
static void exprUnaryOp(Compiler* compiler, bool can_assign);

static void exprGrouping(Compiler* compiler, bool can_assign);
static void exprList(Compiler* compiler, bool can_assign);
static void exprMap(Compiler* compiler, bool can_assign);

static void exprCall(Compiler* compiler, bool can_assign);
static void exprAttrib(Compiler* compiler, bool can_assign);
static void exprSubscript(Compiler* compiler, bool can_assign);

// true, false, null, self.
static void exprValue(Compiler* compiler, bool can_assign);

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
  /* TK_PLUS       */ { NULL,          exprBinaryOp,     PREC_TERM },
  /* TK_MINUS      */ { exprUnaryOp,   exprBinaryOp,     PREC_TERM },
  /* TK_STAR       */ { NULL,          exprBinaryOp,     PREC_FACTOR },
  /* TK_FSLASH     */ { NULL,          exprBinaryOp,     PREC_FACTOR },
  /* TK_BSLASH     */   NO_RULE,
  /* TK_EQ         */   NO_RULE, //    exprAssignment,   PREC_ASSIGNMENT
  /* TK_GT         */ { NULL,          exprBinaryOp,     PREC_COMPARISION },
  /* TK_LT         */ { NULL,          exprBinaryOp,     PREC_COMPARISION },
  /* TK_EQEQ       */ { NULL,          exprBinaryOp,     PREC_EQUALITY },
  /* TK_NOTEQ      */ { NULL,          exprBinaryOp,     PREC_EQUALITY },
  /* TK_GTEQ       */ { NULL,          exprBinaryOp,     PREC_COMPARISION },
  /* TK_LTEQ       */ { NULL,          exprBinaryOp,     PREC_COMPARISION },
  /* TK_PLUSEQ     */   NO_RULE, //    exprAssignment,   PREC_ASSIGNMENT
  /* TK_MINUSEQ    */   NO_RULE, //    exprAssignment,   PREC_ASSIGNMENT
  /* TK_STAREQ     */   NO_RULE, //    exprAssignment,   PREC_ASSIGNMENT
  /* TK_DIVEQ      */   NO_RULE, //    exprAssignment,   PREC_ASSIGNMENT
  /* TK_SRIGHT     */ { NULL,          exprBinaryOp,     PREC_BITWISE_SHIFT },
  /* TK_SLEFT      */ { NULL,          exprBinaryOp,     PREC_BITWISE_SHIFT },
  /* TK_DEF        */   NO_RULE,
  /* TK_EXTERN     */   NO_RULE,
  /* TK_FUNCTION   */ { exprFunc,      NULL,            NO_INFIX },
  /* TK_END        */   NO_RULE,
  /* TK_NULL       */ { exprValue,     NULL,             NO_INFIX },
  /* TK_SELF       */ { exprValue,     NULL,             NO_INFIX },
  /* TK_IN         */ { NULL,          exprBinaryOp,     PREC_IN },
  /* TK_AND        */ { NULL,          exprBinaryOp,     PREC_LOGICAL_AND },
  /* TK_OR         */ { NULL,          exprBinaryOp,     PREC_LOGICAL_OR },
  /* TK_NOT        */ { exprUnaryOp,   NULL,             PREC_LOGICAL_NOT },
  /* TK_TRUE       */ { exprValue,     NULL,             NO_INFIX },
  /* TK_FALSE      */ { exprValue,     NULL,             NO_INFIX },
  /* TK_DO         */   NO_RULE,
  /* TK_WHILE      */   NO_RULE,
  /* TK_FOR        */   NO_RULE,
  /* TK_IF         */   NO_RULE,
  /* TK_ELIF       */   NO_RULE,
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
    emitShort(compiler, index);

  } else {
    if (index < 9) { //< 0..8 locals have single opcode.
      emitOpcode(compiler, (Opcode)(OP_STORE_LOCAL_0 + index));
    } else {
      emitOpcode(compiler, OP_STORE_LOCAL_N);
      emitShort(compiler, index);
    }
  }
}

static void emitPushVariable(Compiler* compiler, int index, bool global) {
  if (global) {
    emitOpcode(compiler, OP_PUSH_GLOBAL);
    emitShort(compiler, index);

  } else {
    if (index < 9) { //< 0..8 locals have single opcode.
      emitOpcode(compiler, (Opcode)(OP_PUSH_LOCAL_0 + index));
    } else {
      emitOpcode(compiler, OP_PUSH_LOCAL_N);
      emitShort(compiler, index);
    }
  }
}

static void exprLiteral(Compiler* compiler, bool can_assign) {
  Token* value = &compiler->parser.previous;
  int index = compilerAddConstant(compiler, value->value);
  emitOpcode(compiler, OP_CONSTANT);
  emitShort(compiler, index);
}

static void exprFunc(Compiler* compiler, bool can_assign) {
  int fn_index = compileFunction(compiler, FN_LITERAL);
  emitOpcode(compiler, OP_PUSH_FN);
  emitShort(compiler, fn_index);
}

// Local/global variables, script/native/builtin functions name.
static void exprName(Compiler* compiler, bool can_assign) {

  Parser* parser = &compiler->parser;

  const char* name_start = parser->previous.start;
  int name_len = parser->previous.length;
  int name_line = parser->previous.line;
  NameSearchResult result = compilerSearchName(compiler, name_start, name_len);

  if (result.type == NAME_NOT_DEFINED) {
    if (can_assign && match(parser, TK_EQ)) {
      int index = compilerAddVariable(compiler, name_start, name_len,
                                      name_line);
      compileExpression(compiler);
      if (compiler->scope_depth == DEPTH_GLOBAL) {
        emitStoreVariable(compiler, index, true);

        // Add the name to the script's global names.
        uint32_t index = scriptAddName(compiler->script, compiler->vm,
                                            name_start, name_len);
        uintBufferWrite(&compiler->script->global_names, compiler->vm, index);

      } else {
        // This will prevent the assignment from poped out from the stack
        // since the assigned value itself is the local and not a temp.
        compiler->new_local = true; 
      }
    } else {
      // TODO: The name could be a function which hasn't been defined at this point.
      //       Implement opcode to push a named variable.
      parseError(parser, "Name \"%.*s\" is not defined.", name_len, name_start);
    }
    return;
  }

  switch (result.type) {
    case NAME_LOCAL_VAR:
    case NAME_GLOBAL_VAR:

      if (can_assign && matchAssignment(parser)) {
        TokenType assignment = parser->previous.type;
        if (assignment != TK_EQ) {
          emitPushVariable(compiler, result.index,
                           result.type == NAME_GLOBAL_VAR);
          compileExpression(compiler);

          switch (assignment) {
            case TK_PLUSEQ: emitOpcode(compiler, OP_ADD); break;
            case TK_MINUSEQ: emitOpcode(compiler, OP_SUBTRACT); break;
            case TK_STAREQ: emitOpcode(compiler, OP_MULTIPLY); break;
            case TK_DIVEQ: emitOpcode(compiler, OP_DIVIDE); break;
            default:
              UNREACHABLE();
              break;
          }

        } else {
          compileExpression(compiler);
        }

        emitStoreVariable(compiler, result.index, result.type == NAME_GLOBAL_VAR);

      } else {
        emitPushVariable(compiler, result.index, result.type == NAME_GLOBAL_VAR);
      }
      return;

    case NAME_FUNCTION:
      emitOpcode(compiler, OP_PUSH_FN);
      emitShort(compiler, result.index);
      return;

    case NAME_BUILTIN:
      emitOpcode(compiler, OP_PUSH_BUILTIN_FN);
      emitShort(compiler, result.index);
      return;

    case NAME_NOT_DEFINED:
      UNREACHABLE(); // Case already handled.
  }
}

static void exprBinaryOp(Compiler* compiler, bool can_assign) {
  TokenType op = compiler->parser.previous.type;
  skipNewLines(&compiler->parser);
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
    case TK_AND:     emitOpcode(compiler, OP_AND);        break;
    case TK_OR:      emitOpcode(compiler, OP_OR);         break;
    default:
      UNREACHABLE();
  }
}

static void exprUnaryOp(Compiler* compiler, bool can_assign) {
  TokenType op = compiler->parser.previous.type;
  skipNewLines(&compiler->parser);
  parsePrecedence(compiler, (Precedence)(PREC_UNARY + 1));

  switch (op) {
    case TK_TILD:  emitOpcode(compiler, OP_BIT_NOT); break;
    case TK_MINUS: emitOpcode(compiler, OP_NEGATIVE); break;
    case TK_NOT:   emitOpcode(compiler, OP_NOT); break;
    default:
      UNREACHABLE();
  }
}

static void exprGrouping(Compiler* compiler, bool can_assign) {
  skipNewLines(&compiler->parser);
  compileExpression(compiler);
  skipNewLines(&compiler->parser);
  consume(&compiler->parser, TK_RPARAN, "Expected ')' after expression ");
}

static void exprList(Compiler* compiler, bool can_assign) {

  emitOpcode(compiler, OP_PUSH_LIST);
  int size_index = emitShort(compiler, 0);

  int size = 0;
  do {
    skipNewLines(&compiler->parser);
    if (peek(&compiler->parser) == TK_RBRACKET) break;

    compileExpression(compiler);
    emitOpcode(compiler, OP_LIST_APPEND);
    size++;

    skipNewLines(&compiler->parser);
  } while (match(&compiler->parser, TK_COMMA));

  skipNewLines(&compiler->parser);
  consume(&compiler->parser, TK_RBRACKET, "Expected ']' after list elements.");

  _FN->opcodes.data[size_index] = (size >> 8) & 0xff;
  _FN->opcodes.data[size_index + 1] = size & 0xff;
}

static void exprMap(Compiler* compiler, bool can_assign) {
  emitOpcode(compiler, OP_PUSH_MAP);

  do {
    skipNewLines(&compiler->parser);
    if (peek(&compiler->parser) == TK_RBRACE) break;

    compileExpression(compiler);
    consume(&compiler->parser, TK_COLLON, "Expected ':' after map's key.");
    compileExpression(compiler);

    emitOpcode(compiler, OP_MAP_INSERT);

    skipNewLines(&compiler->parser);
  } while (match(&compiler->parser, TK_COMMA));

  skipNewLines(&compiler->parser);
  consume(&compiler->parser, TK_RBRACE, "Expected '}' after map elements.");
}

static void exprCall(Compiler* compiler, bool can_assign) {

  // Compile parameters.
  int argc = 0;
  if (!match(&compiler->parser, TK_RPARAN)) {
    do {
      skipNewLines(&compiler->parser);
      compileExpression(compiler);
      skipNewLines(&compiler->parser);
      argc++;
    } while (match(&compiler->parser, TK_COMMA));
    consume(&compiler->parser, TK_RPARAN, "Expected ')' after parameter list.");
  }

  emitOpcode(compiler, OP_CALL);
  emitShort(compiler, argc);
}

static void exprAttrib(Compiler* compiler, bool can_assign) {
  Parser* parser = &compiler->parser;
  consume(parser, TK_NAME, "Expected an attribute name after '.'.");
  const char* name = parser->previous.start;
  int length = parser->previous.length;

  // Store the name in script's names.
  String* string = newString(compiler->vm, name, length);
  vmPushTempRef(compiler->vm, &string->_super);
  stringBufferWrite(&compiler->script->names, compiler->vm, string);
  vmPopTempRef(compiler->vm);

  int index = (int)compiler->script->names.count - 1;

  if (can_assign && matchAssignment(parser)) {

    TokenType assignment = parser->previous.type;
    if (assignment != TK_EQ) {
      emitOpcode(compiler, OP_GET_ATTRIB_AOP);
      emitShort(compiler, index);
      compileExpression(compiler);

      switch (assignment) {
        case TK_PLUSEQ: emitOpcode(compiler, OP_ADD); break;
        case TK_MINUSEQ: emitOpcode(compiler, OP_SUBTRACT); break;
        case TK_STAREQ: emitOpcode(compiler, OP_MULTIPLY); break;
        case TK_DIVEQ: emitOpcode(compiler, OP_DIVIDE); break;
        default:
          UNREACHABLE();
          break;
      }
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

static void exprSubscript(Compiler* compiler, bool can_assign) {
  Parser* parser = &compiler->parser;
  compileExpression(compiler);
  consume(parser, TK_RBRACKET, "Expected ']' after subscription ends.");

  if (can_assign && matchAssignment(parser)) {

    TokenType assignment = parser->previous.type;
    if (assignment != TK_EQ) {
      emitOpcode(compiler, OP_GET_SUBSCRIPT_AOP);
      compileExpression(compiler);

      switch (assignment) {
        case TK_PLUSEQ: emitOpcode(compiler, OP_ADD); break;
        case TK_MINUSEQ: emitOpcode(compiler, OP_SUBTRACT); break;
        case TK_STAREQ: emitOpcode(compiler, OP_MULTIPLY); break;
        case TK_DIVEQ: emitOpcode(compiler, OP_DIVIDE); break;
        default:
          UNREACHABLE();
          break;
      }
    } else {
      compileExpression(compiler);
    }

    emitOpcode(compiler, OP_SET_SUBSCRIPT);

  } else {
    emitOpcode(compiler, OP_GET_SUBSCRIPT);
  }
}

static void exprValue(Compiler* compiler, bool can_assign) {
  TokenType op = compiler->parser.previous.type;
  switch (op) {
    case TK_NULL:  emitOpcode(compiler, OP_PUSH_NULL); return;
    case TK_SELF:  emitOpcode(compiler, OP_PUSH_SELF); return;
    case TK_TRUE:  emitOpcode(compiler, OP_PUSH_TRUE); return;
    case TK_FALSE: emitOpcode(compiler, OP_PUSH_FALSE); return;
    default:
      UNREACHABLE();
  }
}

static void parsePrecedence(Compiler* compiler, Precedence precedence) {
  lexToken(&compiler->parser);
  GrammarFn prefix = getRule(compiler->parser.previous.type)->prefix;

  if (prefix == NULL) {
    parseError(&compiler->parser, "Expected an expression.");
    return;
  }

  bool can_assign = precedence <= PREC_LOWEST;
  prefix(compiler, can_assign);

  while (getRule(compiler->parser.current.type)->precedence >= precedence) {
    lexToken(&compiler->parser);
    GrammarFn infix = getRule(compiler->parser.previous.type)->infix;
    infix(compiler, can_assign);
  }
}

/*****************************************************************************
 * COMPILING                                                                 *
 *****************************************************************************/

static void compilerInit(Compiler* compiler, MSVM* vm, const char* source,
                         const char* path) {
  parserInit(&compiler->parser, vm, source, path);
  compiler->vm = vm;
  vm->compiler = compiler;
  compiler->scope_depth = DEPTH_GLOBAL;
  compiler->var_count = 0;
  compiler->global_count = 0;
  compiler->stack_size = 0;
  compiler->loop = NULL;
  compiler->func = NULL;
  compiler->script = NULL;
  compiler->new_local = false;
}

// Add a variable and return it's index to the context. Assumes that the
// variable name is unique and not defined before in the current scope.
static int compilerAddVariable(Compiler* compiler, const char* name,
                                int length, int line) {
  Variable* variable = &compiler->variables[compiler->var_count];
  variable->name = name;
  variable->length = length;
  variable->depth = compiler->scope_depth;
  variable->line = line;
  if (variable->depth == DEPTH_GLOBAL) compiler->global_count++;
  return compiler->var_count++;
}

// Add a literal constant to scripts literals and return it's index.
static int compilerAddConstant(Compiler* compiler, Var value) {
  VarBuffer* literals = &compiler->script->literals;

  for (uint32_t i = 0; i < literals->count; i++) {
    if (isValuesSame(literals->data[i], value)) {
      return i;
    }
  }

  // Add new constant to script.
  if (literals->count < MAX_CONSTANTS) {
    varBufferWrite(literals, compiler->vm, value);
  } else {
    parseError(&compiler->parser, "A script should contain at most %d "
      "unique constants.", MAX_CONSTANTS);
  }
  return (int)literals->count - 1;
}

// Enters inside a block.
static void compilerEnterBlock(Compiler* compiler) {
  compiler->scope_depth++;
}

// Exits a block.
static void compilerExitBlock(Compiler* compiler) {
  ASSERT(compiler->scope_depth > (int)DEPTH_GLOBAL, "Cannot exit toplevel.");

  while (compiler->var_count > 0 && compiler->variables[
      compiler->var_count - 1].depth >= compiler->scope_depth) {
    compiler->var_count--;
    compiler->stack_size--;
  }
  compiler->scope_depth--;
}

/*****************************************************************************
 * COMPILING (EMIT BYTECODE)                                                 *
 *****************************************************************************/

// Emit a single byte and return it's index.
static int emitByte(Compiler* compiler, int byte) {

  byteBufferWrite(&_FN->opcodes, compiler->vm,
                    (uint8_t)byte);
  uintBufferWrite(&_FN->oplines, compiler->vm,
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

  compiler->stack_size += opcode_info[opcode].stack;
  if (compiler->stack_size > _FN->stack_size) {
    _FN->stack_size = compiler->stack_size;
  }
}

// Emits a constant value if it doesn't exists on the current script it'll
// make one.
static void emitConstant(Compiler* compiler, Var value) {
  int index = compilerAddConstant(compiler, value);
  emitOpcode(compiler, OP_CONSTANT);
  emitShort(compiler, index);
}

// Update the jump offset.
static void patchJump(Compiler* compiler, int addr_index) {
  int jump_to = (int)_FN->opcodes.count - addr_index - 2;
  ASSERT(jump_to < MAX_JUMP, "Too large address offset to jump to.");

  _FN->opcodes.data[addr_index] = (jump_to >> 8) & 0xff;
  _FN->opcodes.data[addr_index + 1] = jump_to & 0xff;
}

// Jump back to the start of the loop.
static void emitLoopJump(Compiler* compiler) {
  emitOpcode(compiler, OP_LOOP);
  int offset = (int)_FN->opcodes.count - compiler->loop->start + 2;
  emitShort(compiler, offset);
}

 /****************************************************************************
  * COMPILING (PARSE TOPLEVEL)                                               *
  ****************************************************************************/

typedef enum {
  BLOCK_FUNC,
  BLOCK_LOOP,
  BLOCK_IF,
  BLOCK_ELIF,
  BLOCK_ELSE,
} BlockType;

static void compileStatement(Compiler* compiler);
static void compileBlockBody(Compiler* compiler, BlockType type);

// Compile a function and return it's index in the script's function buffer.
static int compileFunction(Compiler* compiler, FuncType fn_type) {

  Parser* parser = &compiler->parser;

  const char* name;
  int name_length;

  if (fn_type != FN_LITERAL) {
    consume(parser, TK_NAME, "Expected a function name.");
    name = parser->previous.start;
    name_length = parser->previous.length;
    NameSearchResult result = compilerSearchName(compiler, name,
                                                 name_length);
    if (result.type != NAME_NOT_DEFINED) {
      parseError(&compiler->parser, "Name %.*s already exists.", name_length,
                 name);
      // Not returning here as to complete for skip cascaded errors.
    }

  } else {
    name = "[FunctionLiteral]";
    name_length = (int)strlen(name);
  }
  
  Function* func = newFunction(compiler->vm, name, name_length,
                               compiler->script, fn_type == FN_NATIVE);
  int fn_index = (int)compiler->script->functions.count - 1;

  Func curr_func;
  curr_func.outer_func = compiler->func;
  curr_func.ptr = func;
  curr_func.depth = compiler->scope_depth;

  compiler->func = &curr_func;

  int argc = 0;
  compilerEnterBlock(compiler); // Parameter depth.

  // Parameter list is optional.
  if (match(parser, TK_LPARAN) && !match(parser, TK_RPARAN)) {
    do {
      skipNewLines(parser);

      consume(parser, TK_NAME, "Expected a parameter name.");
      argc++;

      const char* param_name = parser->previous.start;
      int param_len = parser->previous.length;

      bool predefined = false;
      for (int i = compiler->var_count - 1; i >= 0; i--) {
        Variable* variable = &compiler->variables[i];
        if (compiler->scope_depth != variable->depth) break;
        if (variable->length == param_len &&
          strncmp(variable->name, param_name, param_len) == 0) {
          predefined = true;
          break;
        }
      }
      if (predefined)
        parseError(parser, "Multiple definition of a parameter");

      compilerAddVariable(compiler, param_name, param_len,
        compiler->parser.previous.line);

    } while (match(parser, TK_COMMA));

    consume(parser, TK_RPARAN, "Expected ')' after parameter list.");
  }

  func->arity = argc;

  if (fn_type != FN_NATIVE) {
    compileBlockBody(compiler, BLOCK_FUNC);
  }
  
  consume(parser, TK_END, "Expected 'end' after function definition end.");

  emitOpcode(compiler,  OP_PUSH_NULL);
  emitOpcode(compiler, OP_RETURN);
  emitOpcode(compiler, OP_END);

  compilerExitBlock(compiler); // Parameter depth.

#if DEBUG_DUMP_COMPILED_CODE
  dumpInstructions(compiler->vm, compiler->func->ptr);
#endif
  compiler->func = compiler->func->outer_func;

  return fn_index;
}

// Finish a block body.
static void compileBlockBody(Compiler* compiler, BlockType type) {

  compilerEnterBlock(compiler);

  if (type != BLOCK_ELSE && type != BLOCK_ELIF) {
    consumeStartBlock(&compiler->parser);
    skipNewLines(&compiler->parser);
  }

  bool if_body = (type == BLOCK_IF) || (type == BLOCK_ELIF);

  TokenType next = peek(&compiler->parser);
  while (!(next == TK_END || next == TK_EOF || (
    if_body && (next == TK_ELSE || next == TK_ELIF)))) {

    compileStatement(compiler);
    skipNewLines(&compiler->parser);

    next = peek(&compiler->parser);
  }

  compilerExitBlock(compiler);
}

// Compiles an expression. An expression will result a value on top of the
// stack.
static void compileExpression(Compiler* compiler) {
  parsePrecedence(compiler, PREC_LOWEST);
}

static void compileIfStatement(Compiler* compiler) {

  skipNewLines(&compiler->parser);
  compileExpression(compiler); //< Condition.
  emitOpcode(compiler, OP_JUMP_IF_NOT);
  int ifpatch = emitShort(compiler, 0xffff); //< Will be patched.

  compileBlockBody(compiler, BLOCK_IF);

  // Elif statement's don't consume 'end' after they end since it's treated as
  // else and if they require 2 'end' statements. But we're omitting the 'end'
  // for the 'else' since it'll consumed by the 'if'.
  bool elif = false;

  if (peek(&compiler->parser) == TK_ELIF) {
    elif = true;
    // Override the elif to if so that it'll be parsed as a new if statement
    // and that's why we're not consuming it here.
    compiler->parser.current.type = TK_IF;

    // Jump pass else.
    emitOpcode(compiler, OP_JUMP);
    int exit_jump = emitShort(compiler, 0xffff); //< Will be patched.

    patchJump(compiler, ifpatch);
    compileBlockBody(compiler, BLOCK_ELIF);
    patchJump(compiler, exit_jump);

  } else if (match(&compiler->parser, TK_ELSE)) {

    // Jump pass else.
    emitOpcode(compiler, OP_JUMP);
    int exit_jump = emitShort(compiler, 0xffff); //< Will be patched.

    patchJump(compiler, ifpatch);
    compileBlockBody(compiler, BLOCK_ELSE);
    patchJump(compiler, exit_jump);

  } else {
    patchJump(compiler, ifpatch);
  }

  if (!elif) {
    skipNewLines(&compiler->parser);
    consume(&compiler->parser, TK_END, "Expected 'end' after statement end.");
  }
}

static void compileWhileStatement(Compiler* compiler) {
  Loop loop;
  loop.start = (int)_FN->opcodes.count;
  loop.patch_count = 0;
  loop.outer_loop = compiler->loop;
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

  skipNewLines(&compiler->parser);
  consume(&compiler->parser, TK_END, "Expected 'end' after statement end.");
}

static void compileForStatement(Compiler* compiler) {
  compilerEnterBlock(compiler);

  Parser* parser = &compiler->parser;
  consume(parser, TK_NAME, "Expected an iterator name.");

  // Unlike functions local variable could shadow a name.
  const char* iter_name = parser->previous.start;
  int iter_len = parser->previous.length;
  int iter_line = parser->previous.line;

  consume(parser, TK_IN, "Expected 'in' after iterator name.");

  // Compile and store container.
  int container = compilerAddVariable(compiler, "@container", 10, iter_line);
  compileExpression(compiler);

  // Add iterator to locals. It would initially be null and once the loop
  // started it'll be an increasing integer indicating that the current 
  // loop is nth.
  int iterator = compilerAddVariable(compiler, "@iterator", 9, iter_line);
  emitOpcode(compiler, OP_PUSH_NULL);

  // Add the iteration value. It'll be updated to each element in an array of
  // each character in a string etc.
  int iter_value = compilerAddVariable(compiler, iter_name, iter_len,
                                       iter_line);
  emitOpcode(compiler, OP_PUSH_NULL);

  Loop loop;
  loop.start = (int)_FN->opcodes.count;
  loop.patch_count = 0;
  loop.outer_loop = compiler->loop;
  compiler->loop = &loop;

  // Compile next iteration.
  emitOpcode(compiler, OP_ITER);
  int forpatch = emitShort(compiler, 0xffff);

  compileBlockBody(compiler, BLOCK_LOOP);

  emitLoopJump(compiler);
  patchJump(compiler, forpatch);

  // Patch break statement.
  for (int i = 0; i < compiler->loop->patch_count; i++) {
    patchJump(compiler, compiler->loop->patches[i]);
  }
  compiler->loop = loop.outer_loop;

  skipNewLines(&compiler->parser);
  consume(&compiler->parser, TK_END, "Expected 'end' after statement end.");
  compilerExitBlock(compiler); //< Iterator scope.
}

// Compiles a statement. Assignment could be an assignment statement or a new
// variable declaration, which will be handled.
static void compileStatement(Compiler* compiler) {
  Parser* parser = &compiler->parser;

  if (match(parser, TK_BREAK)) {
    if (compiler->loop == NULL) {
      parseError(parser, "Cannot use 'break' outside a loop.");
      return;
    }

    ASSERT(compiler->loop->patch_count < MAX_BREAK_PATCH,
      "Too many break statements (" STRINGIFY(MAX_BREAK_PATCH) ")." );

    consumeEndStatement(parser);
    emitOpcode(compiler, OP_JUMP);
    int patch = emitByte(compiler, 0xffff); //< Will be patched.
    compiler->loop->patches[compiler->loop->patch_count++] = patch;

  } else if (match(parser, TK_CONTINUE)) {
    if (compiler->loop == NULL) {
      parseError(parser, "Cannot use 'continue' outside a loop.");
      return;
    }

    consumeEndStatement(parser);
    emitLoopJump(compiler);

  } else if (match(parser, TK_RETURN)) {

    if (compiler->scope_depth == DEPTH_GLOBAL) {
      parseError(parser, "Invalid 'return' outside a function.");
      return;
    }

    if (matchEndStatement(parser)) {
      emitOpcode(compiler, OP_PUSH_NULL);
      emitOpcode(compiler, OP_RETURN);
    } else {
      compileExpression(compiler); //< Return value is at stack top.
      consumeEndStatement(parser);
      emitOpcode(compiler, OP_RETURN);
    }
  } else if (match(parser, TK_IF)) {
    compileIfStatement(compiler);

  } else if (match(parser, TK_WHILE)) {
    compileWhileStatement(compiler);

  } else if (match(parser, TK_FOR)) {
    compileForStatement(compiler);

  } else {
    compiler->new_local = false;
    compileExpression(compiler);
    consumeEndStatement(parser);
    if (!compiler->new_local) {
      emitOpcode(compiler, OP_POP);
    }
    compiler->new_local = false;
  }
}

Script* compileSource(MSVM* vm, const char* path) {

  MSLoadScriptResult res = vm->config.load_script_fn(vm, path);
  if (res.is_failed) // FIXME:
    vm->config.error_fn(vm, MS_ERROR_COMPILE, NULL, -1,
      "file load source failed.");
  const char* source = res.source;

  // Skip utf8 BOM if there is any.
  if (strncmp(source, "\xEF\xBB\xBF", 3) == 0) source += 3;

  Compiler compiler;
  compilerInit(&compiler, vm, source, path);

  Script* script = newScript(vm);
  compiler.script = script;

  Func curr_fn;
  curr_fn.depth = DEPTH_SCRIPT;
  curr_fn.ptr = script->body;
  curr_fn.outer_func = NULL;
  compiler.func = &curr_fn;

  // Parser pointer for quick access.
  Parser* parser = &compiler.parser;

  // Lex initial tokens. current <-- next.
  lexToken(parser);
  lexToken(parser);
  skipNewLines(parser);

  while (!match(parser, TK_EOF)) {

    if (match(parser, TK_NATIVE)) {
      compileFunction(&compiler, FN_NATIVE);

    } else if (match(parser, TK_DEF)) {
      compileFunction(&compiler, FN_SCRIPT);

    } else {
      compileStatement(&compiler);
    }

    skipNewLines(parser);
  }

  emitOpcode(&compiler, OP_PUSH_NULL);
  emitOpcode(&compiler, OP_RETURN);
  emitOpcode(&compiler, OP_END);

  // Source done callback.
  if (vm->config.load_script_done_fn != NULL)
    vm->config.load_script_done_fn(vm, path, res.user_data);

  // Create script globals.
  for (int i = 0; i < compiler.var_count; i++) {
    ASSERT(compiler.variables[i].depth == (int)DEPTH_GLOBAL, OOPS);
    varBufferWrite(&script->globals, vm, VAR_NULL);
  }

  vm->compiler = NULL;

#if DEBUG_DUMP_COMPILED_CODE
  dumpInstructions(vm, script->body);
#endif

  if (compiler.parser.has_errors) return NULL;
  return script;
}

///////////////////////////////////////////////////////////////////////////////

void compilerMarkObjects(Compiler* compiler, MSVM* vm) {
  
  // Mark the script which is currently being compiled.
  grayObject(&compiler->script->_super, vm);

  // Mark the string literals (they haven't added to the script's literal
  // buffer yet).
  grayValue(compiler->parser.current.value, vm);
  grayValue(compiler->parser.previous.value, vm);
  grayValue(compiler->parser.next.value, vm);
}
