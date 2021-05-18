/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "compiler.h"

#include <stdio.h>
#include <string.h>

#include "core.h"
#include "buffers.h"
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
  TK_SRIGHT,     // >>
  TK_SLEFT,      // <<

  //TODO:
  //TK_SRIGHTEQ  // >>=
  //TK_SLEFTEQ   // <<=
  //TK_MODEQ,    // %=
  //TK_XOREQ,    // ^=

  // Keywords.
  TK_FROM,       // from
  TK_IMPORT,     // import
  TK_AS,         // as
  TK_DEF,        // def
  TK_NATIVE,     // native (C function declaration)
  TK_FUNC,       // func (literal function)
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
  TK_THEN,       // then
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
  { "from",     4, TK_FROM },
  { "import",   6, TK_IMPORT },
  { "as",       2, TK_AS },
  { "def",      3, TK_DEF },
  { "native",   6, TK_NATIVE },
  { "func",     4, TK_FUNC },
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
  { "then",     4, TK_THEN },
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
  PREC_CHAIN_CALL,    // ->
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

  // Depth of the loop, required to pop all the locals in that loop when it
  // met a break/continue statement inside.
  int depth;

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

  PKVM* vm;

  // Variables related to parsing.
  const char* source;       //< Currently compiled source (Weak pointer).
  const char* token_start;  //< Start of the currently parsed token.
  const char* current_char; //< Current char position in the source.
  int current_line;         //< Line number of the current char.
  Token previous, current, next; //< Currently parsed tokens.
  bool has_errors;          //< True if any syntex error occured at compile time.

  // Current depth the compiler in (-1 means top level) 0 means function
  // level and > 0 is inner scope.
  int scope_depth;

  Variable variables[MAX_VARIABLES]; //< Variables in the current context.
  int var_count;                     //< Number of locals in [variables].
  int global_count;                  //< Number of globals in [variables].

  int stack_size; //< Current size including locals ind temps.=

  Script* script;  //< Current script (a weak pointer).
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

static void reportError(Compiler* compiler, const char* file, int line,
                        const char* fmt, va_list args) {
  PKVM* vm = compiler->vm;
  compiler->has_errors = true;
  char message[ERROR_MESSAGE_SIZE];
  int length = vsprintf(message, fmt, args);
  ASSERT(length < ERROR_MESSAGE_SIZE, "Error message buffer should not exceed "
    "the buffer");

  if (vm->config.error_fn == NULL) return;
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

/*****************************************************************************
 * LEXING                                                                    *
 *****************************************************************************/

// Forward declaration of lexer methods.

static char eatChar(Compiler* compiler);
static void setNextValueToken(Compiler* compiler, TokenType type, Var value);
static void setNextToken(Compiler* compiler, TokenType type);
static bool matchChar(Compiler* compiler, char c);
static bool matchLine(Compiler* compiler);

static void eatString(Compiler* compiler, bool single_quote) {
  ByteBuffer buff;
  byteBufferInit(&buff);

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
        case '"': byteBufferWrite(&buff, compiler->vm, '"'); break;
        case '\'': byteBufferWrite(&buff, compiler->vm, '\''); break;
        case '\\': byteBufferWrite(&buff, compiler->vm, '\\'); break;
        case 'n': byteBufferWrite(&buff, compiler->vm, '\n'); break;
        case 'r': byteBufferWrite(&buff, compiler->vm, '\r'); break;
        case 't': byteBufferWrite(&buff, compiler->vm, '\t'); break;

        default:
          lexError(compiler, "Error: invalid escape character");
          break;
      }
    } else {
      byteBufferWrite(&buff, compiler->vm, c);
    }
  }

  // '\0' will be added by varNewSring();
  Var string = VAR_OBJ(&newStringLength(compiler->vm, (const char*)buff.data,
    (uint32_t)buff.count)->_super);

  byteBufferClear(&buff, compiler->vm);

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

  // TODO: hex, binary and scientific literals.

  while (utilIsDigit(peekChar(compiler)))
    eatChar(compiler);

  if (peekChar(compiler) == '.' && utilIsDigit(peekNextChar(compiler))) {
    matchChar(compiler, '.');
    while (utilIsDigit(peekChar(compiler)))
      eatChar(compiler);
  }
  errno = 0;
  Var value = VAR_NUM(strtod(compiler->token_start, NULL));
  if (errno == ERANGE) {
    const char* start = compiler->token_start;
    int len = (int)(compiler->current_char - start);
    lexError(compiler, "Literal is too large (%.*s)", len, start);
    value = VAR_NUM(0);
  }

  setNextValueToken(compiler, TK_NUMBER, value);
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

// Will skip multiple new lines.
static void skipNewLines(Compiler* compiler) {
  matchLine(compiler);
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
  compiler->next.type = type;
  compiler->next.start = compiler->token_start;
  compiler->next.length = (int)(compiler->current_char - compiler->token_start);
  compiler->next.line = compiler->current_line - ((type == TK_LINE) ? 1 : 0);
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
      case '%': setNextToken(compiler, TK_PERCENT); return;

      case '~': setNextToken(compiler, TK_TILD); return;
      case '&': setNextToken(compiler, TK_AMP); return;
      case '|': setNextToken(compiler, TK_PIPE); return;
      case '^': setNextToken(compiler, TK_CARET); return;

      case '\n': setNextToken(compiler, TK_LINE); return;

      case ' ':
      case '\t':
      case '\r': {
        char c = peekChar(compiler);
        while (c == ' ' || c == '\t' || c == '\r') {
          eatChar(compiler);
          c = peekChar(compiler);
        }
        break;
      }

      case '.': // TODO: ".5" should be a valid number.
        setNextTwoCharToken(compiler, '.', TK_DOT, TK_DOTDOT);
        return;

      case '=':
        setNextTwoCharToken(compiler, '=', TK_EQ, TK_EQEQ);
        return;

      case '!':
        setNextTwoCharToken(compiler, '=', TK_NOT, TK_NOTEQ);
        return;

      case '>':
        if (matchChar(compiler, '>'))
          setNextToken(compiler, TK_SRIGHT);
        else
          setNextTwoCharToken(compiler, '=', TK_GT, TK_GTEQ);
        return;

      case '<':
        if (matchChar(compiler, '<'))
          setNextToken(compiler, TK_SLEFT);
        else
          setNextTwoCharToken(compiler, '=', TK_LT, TK_LTEQ);
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
            lexError(compiler, "Invalid character %c", c);
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

/*****************************************************************************
 * PARSING                                                                   *
 *****************************************************************************/

// Returns current token type.
static TokenType peek(Compiler* self) {
  return self->current.type;
}

// Returns next token type.
static TokenType peekNext(Compiler* self) {
  return self->next.type;
}

// Consume the current token if it's expected and lex for the next token
// and return true otherwise reutrn false.
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
  if (peek(compiler) != TK_LINE) return false;
  while (peek(compiler) == TK_LINE)
    lexToken(compiler);
  return true;
}

// Match semi collon, multiple new lines or peek 'end' keyword.
static bool matchEndStatement(Compiler* compiler) {
  if (match(compiler, TK_SEMICOLLON)) {
    skipNewLines(compiler);
    return true;
  }

  if (matchLine(compiler) || peek(compiler) == TK_END || peek(compiler) == TK_EOF)
    return true;
  return false;
}

// Consume semi collon, multiple new lines or peek 'end' keyword.
static void consumeEndStatement(Compiler* compiler) {
  if (!matchEndStatement(compiler)) {
    parseError(compiler, "Expected statement end with newline or ';'.");
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
  if (match(compiler, TK_EQ)) return true;
  if (match(compiler, TK_PLUSEQ)) return true;
  if (match(compiler, TK_MINUSEQ)) return true;
  if (match(compiler, TK_STAREQ)) return true;
  if (match(compiler, TK_DIVEQ)) return true;
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
  index = findBuiltinFunction(compiler->vm, name, length);
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
static void patchJump(Compiler* compiler, int addr_index);
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

static void exprOr(Compiler* compiler, bool can_assign);
static void exprAnd(Compiler* compiler, bool can_assign);

static void exprChainCall(Compiler* compiler, bool can_assign);
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
  /* TK_ARROW      */ { NULL,          exprChainCall,    PREC_CHAIN_CALL },
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
  /* TK_FROM       */   NO_RULE,
  /* TK_IMPORT     */   NO_RULE,
  /* TK_AS         */   NO_RULE,
  /* TK_DEF        */   NO_RULE,
  /* TK_EXTERN     */   NO_RULE,
  /* TK_FUNC       */ { exprFunc,      NULL,             NO_INFIX },
  /* TK_END        */   NO_RULE,
  /* TK_NULL       */ { exprValue,     NULL,             NO_INFIX },
  /* TK_SELF       */ { exprValue,     NULL,             NO_INFIX },
  /* TK_IN         */ { NULL,          exprBinaryOp,     PREC_IN },
  /* TK_AND        */ { NULL,          exprAnd,          PREC_LOGICAL_AND },
  /* TK_OR         */ { NULL,          exprOr,           PREC_LOGICAL_OR },
  /* TK_NOT        */ { exprUnaryOp,   NULL,             PREC_LOGICAL_NOT },
  /* TK_TRUE       */ { exprValue,     NULL,             NO_INFIX },
  /* TK_FALSE      */ { exprValue,     NULL,             NO_INFIX },
  /* TK_DO         */   NO_RULE,
  /* TK_THEN       */   NO_RULE,
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
  Token* value = &compiler->previous;
  int index = compilerAddConstant(compiler, value->value);
  emitOpcode(compiler, OP_PUSH_CONSTANT);
  emitShort(compiler, index);
}

static void exprFunc(Compiler* compiler, bool can_assign) {
  int fn_index = compileFunction(compiler, FN_LITERAL);
  emitOpcode(compiler, OP_PUSH_FN);
  emitShort(compiler, fn_index);
}

// Local/global variables, script/native/builtin functions name.
static void exprName(Compiler* compiler, bool can_assign) {

  const char* name_start = compiler->previous.start;
  int name_len = compiler->previous.length;
  int name_line = compiler->previous.line;
  NameSearchResult result = compilerSearchName(compiler, name_start, name_len);

  if (result.type == NAME_NOT_DEFINED) {
    if (can_assign && match(compiler, TK_EQ)) {
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
        emitStoreVariable(compiler, (index - compiler->global_count), false);
      }
    } else {
      // TODO: The name could be a function which hasn't been defined at this point.
      //       Implement opcode to push a named variable.
      parseError(compiler, "Name '%.*s' is not defined.", name_len, name_start);
    }
    return;
  }

  switch (result.type) {
    case NAME_LOCAL_VAR:
    case NAME_GLOBAL_VAR: {

      if (can_assign && matchAssignment(compiler)) {
        TokenType assignment = compiler->previous.type;
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
    }

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

void exprOr(Compiler* compiler, bool can_assign) {
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
}

void exprAnd(Compiler* compiler, bool can_assign) {
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
}

static void exprChainCall(Compiler* compiler, bool can_assign) {
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
      consume(compiler, TK_RBRACE, "Expected '}' after chain call"
                                            "parameter list.");
    }
  }

  emitOpcode(compiler, OP_CALL);
  emitShort(compiler, argc);
}

static void exprBinaryOp(Compiler* compiler, bool can_assign) {
  TokenType op = compiler->previous.type;
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

static void exprUnaryOp(Compiler* compiler, bool can_assign) {
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
}

static void exprGrouping(Compiler* compiler, bool can_assign) {
  skipNewLines(compiler);
  compileExpression(compiler);
  skipNewLines(compiler);
  consume(compiler, TK_RPARAN, "Expected ')' after expression ");
}

static void exprList(Compiler* compiler, bool can_assign) {

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
}

static void exprMap(Compiler* compiler, bool can_assign) {
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

static void exprCall(Compiler* compiler, bool can_assign) {

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
  emitShort(compiler, argc);
}

static void exprAttrib(Compiler* compiler, bool can_assign) {
  consume(compiler, TK_NAME, "Expected an attribute name after '.'.");
  const char* name = compiler->previous.start;
  int length = compiler->previous.length;

  // Store the name in script's names.
  int index = scriptAddName(compiler->script, compiler->vm, name, length);

  if (can_assign && matchAssignment(compiler)) {

    TokenType assignment = compiler->previous.type;
    if (assignment != TK_EQ) {
      emitOpcode(compiler, OP_GET_ATTRIB_KEEP);
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
  compileExpression(compiler);
  consume(compiler, TK_RBRACKET, "Expected ']' after subscription ends.");

  if (can_assign && matchAssignment(compiler)) {

    TokenType assignment = compiler->previous.type;
    if (assignment != TK_EQ) {
      emitOpcode(compiler, OP_GET_SUBSCRIPT_KEEP);
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
  TokenType op = compiler->previous.type;
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
  lexToken(compiler);
  GrammarFn prefix = getRule(compiler->previous.type)->prefix;

  if (prefix == NULL) {
    parseError(compiler, "Expected an expression.");
    return;
  }

  bool can_assign = precedence <= PREC_LOWEST;
  prefix(compiler, can_assign);

  while (getRule(compiler->current.type)->precedence >= precedence) {
    lexToken(compiler);
    GrammarFn infix = getRule(compiler->previous.type)->infix;
    infix(compiler, can_assign);
  }
}

/*****************************************************************************
 * COMPILING                                                                 *
 *****************************************************************************/

static void compilerInit(Compiler* compiler, PKVM* vm, const char* source,
                         Script* script) {
  
  compiler->vm = vm;

  compiler->source = source;
  compiler->script = script;
  compiler->token_start = source;
  compiler->has_errors = false;

  compiler->current_char = source;
  compiler->current_line = 1;
  compiler->next.type = TK_ERROR;
  compiler->next.start = NULL;
  compiler->next.length = 0;
  compiler->next.line = 1;
  compiler->next.value = VAR_UNDEFINED;

  compiler->scope_depth = DEPTH_GLOBAL;
  compiler->var_count = 0;
  compiler->global_count = 0;
  compiler->stack_size = 0;

  compiler->loop = NULL;
  compiler->func = NULL;
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
    parseError(compiler, "A script should contain at most %d "
      "unique constants.", MAX_CONSTANTS);
  }
  return (int)literals->count - 1;
}

// Enters inside a block.
static void compilerEnterBlock(Compiler* compiler) {
  compiler->scope_depth++;
}

// Pop all the locals at the [depth] or highter.
static void compilerPopLocals(Compiler* compiler, int depth) {
  ASSERT(depth > (int)DEPTH_GLOBAL, "Cannot pop global variables.");

  int local = compiler->var_count - 1;
  while (local >= 0 && compiler->variables[local].depth >= depth) {
    emitOpcode(compiler, OP_POP);
    compiler->var_count--;
    compiler->stack_size--;
    local--;
  }
}

// Exits a block.
static void compilerExitBlock(Compiler* compiler) {
  ASSERT(compiler->scope_depth > (int)DEPTH_GLOBAL, "Cannot exit toplevel.");

  // Discard all the locals at the current scope.
  compilerPopLocals(compiler, compiler->scope_depth);
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

  compiler->stack_size += opcode_info[opcode].stack;
  if (compiler->stack_size > _FN->stack_size) {
    _FN->stack_size = compiler->stack_size;
  }
}

// Emits a constant value if it doesn't exists on the current script it'll
// make one.
static void emitConstant(Compiler* compiler, Var value) {
  int index = compilerAddConstant(compiler, value);
  emitOpcode(compiler, OP_PUSH_CONSTANT);
  emitShort(compiler, index);
}

// Update the jump offset.
static void patchJump(Compiler* compiler, int addr_index) {
  int offset = (int)_FN->opcodes.count - addr_index - 2;
  ASSERT(offset < MAX_JUMP, "Too large address offset to jump to.");

  _FN->opcodes.data[addr_index] = (offset >> 8) & 0xff;
  _FN->opcodes.data[addr_index + 1] = offset & 0xff;
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

  const char* name;
  int name_length;

  if (fn_type != FN_LITERAL) {
    consume(compiler, TK_NAME, "Expected a function name.");
    name = compiler->previous.start;
    name_length = compiler->previous.length;
    NameSearchResult result = compilerSearchName(compiler, name,
                                                 name_length);
    if (result.type != NAME_NOT_DEFINED) {
      parseError(compiler, "Name '%.*s' already exists.", name_length,
                 name);
      // Not returning here as to complete for skip cascaded errors.
    }

  } else {
    name = "$(LiteralFn)";
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
  if (match(compiler, TK_LPARAN) && !match(compiler, TK_RPARAN)) {
    do {
      skipNewLines(compiler);

      consume(compiler, TK_NAME, "Expected a parameter name.");
      argc++;

      const char* param_name = compiler->previous.start;
      int param_len = compiler->previous.length;

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
        parseError(compiler, "Multiple definition of a parameter");

      compilerAddVariable(compiler, param_name, param_len,
        compiler->previous.line);

    } while (match(compiler, TK_COMMA));

    consume(compiler, TK_RPARAN, "Expected ')' after parameter list.");
  }

  func->arity = argc;

  if (fn_type != FN_NATIVE) {
    compileBlockBody(compiler, BLOCK_FUNC);
    consume(compiler, TK_END, "Expected 'end' after function definition end.");
  
    emitOpcode(compiler,  OP_PUSH_NULL);
    emitOpcode(compiler, OP_RETURN);
    emitOpcode(compiler, OP_END);
  }

  compilerExitBlock(compiler); // Parameter depth.

#if DEBUG_DUMP_COMPILED_CODE
  dumpFunctionCode(compiler->vm, compiler->func->ptr);
#endif
  compiler->func = compiler->func->outer_func;

  return fn_index;
}

// Finish a block body.
static void compileBlockBody(Compiler* compiler, BlockType type) {

  compilerEnterBlock(compiler);

  if (type == BLOCK_IF) {
    consumeStartBlock(compiler, TK_THEN);
    skipNewLines(compiler);

  } else if (type == BLOCK_ELIF) {
    // Do nothing, because this will be parsed as a new if statement.
    // and it's condition hasn't parsed yet.

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

  bool if_body = (type == BLOCK_IF) || (type == BLOCK_ELIF);

  TokenType next = peek(compiler);
  while (!(next == TK_END || next == TK_EOF || (
    if_body && (next == TK_ELSE || next == TK_ELIF)))) {

    compileStatement(compiler);
    skipNewLines(compiler);

    next = peek(compiler);
  }

  compilerExitBlock(compiler);
}

typedef enum {
  IMPORT_REGULAR,      //< Regular import
  IMPORT_FROM_LIB,     //< Import the lib for `from` import.
  IMPORT_FROM_SYMBOL,  //< Entry of a `from` import.
  // TODO: from os import *
} ImportEntryType;

// Single entry of a comma seperated import statement. The instructions will
// import the and assign to the corresponding variables.
static void _compilerImportEntry(Compiler* compiler, ImportEntryType ie_type) {

  const char* name = NULL;
  uint32_t length = 0;
  int entry = -1; //< Imported library variable index.

  if (match(compiler, TK_NAME)) {
    name = compiler->previous.start;
    length = compiler->previous.length;

    uint32_t name_line = compiler->previous.line;

    // >> from os import clock
    //    Here `os` is temporary don't add to variable.
    if (ie_type != IMPORT_FROM_LIB) {
      entry = compilerAddVariable(compiler, name, length, name_line);
    }

    int index = (int)scriptAddName(compiler->script, compiler->vm,
      name, length);

    // >> from os import clock
    //    Here clock is not imported but get attribute of os.
    if (ie_type != IMPORT_FROM_SYMBOL) {
      emitOpcode(compiler, OP_IMPORT);
      emitShort(compiler, index); //< Name of the lib.
    } else {
      // Don't pop the lib since it'll be used for the next entry.
      emitOpcode(compiler, OP_GET_ATTRIB_KEEP);
      emitShort(compiler, index); //< Name of the attrib.
    }

  } else {
    TODO; // import and push the lib to the stack.
  }

  // >> from os import clock
  //    Here the os is imported and not bound to any variable, just temp.
  if (ie_type == IMPORT_FROM_LIB) return;

  // Store to the variable.
  if (match(compiler, TK_AS)) {
    consume(compiler, TK_NAME, "Expected a name after as.");
    // TODO: validate the name (maybe can't be predefined?).
    compiler->variables[entry].name = compiler->previous.start;
    compiler->variables[entry].length = compiler->previous.length;
    // TODO: add the name to global_names ??
  }

  emitStoreVariable(compiler, entry, true);
  emitOpcode(compiler, OP_POP);
}

// The 'import' statement compilation. It's inspired by the python's import
// statement. It could be multiple import libs comma seperated and they can
// be aliased with 'as' keyword. Using from keyword it's possible to import
// some of the members of the lib.
//
// example. import lib
//          import lib1, lib2 as l2
//          from lib import func1, func2 as f2
// 
static void compileImportStatement(Compiler* compiler, bool is_from) {
  
  if (is_from) {
    _compilerImportEntry(compiler, IMPORT_FROM_LIB);
    consume(compiler, TK_IMPORT, "Expected keyword 'import'.");

    do {
      _compilerImportEntry(compiler, IMPORT_FROM_SYMBOL);
    } while (match(compiler, TK_COMMA));

    // Done getting all the attributes, now pop the lib from the stack.
    emitOpcode(compiler, OP_POP);

  } else {
    do {
      //skipNewLines(compiler);
      _compilerImportEntry(compiler, IMPORT_REGULAR);
    } while (match(compiler, TK_COMMA));
  }

  if (peek(compiler) != TK_EOF) {
    consume(compiler, TK_LINE, "Expected EOL after import statement.");
  }
}

// Compiles an expression. An expression will result a value on top of the
// stack.
static void compileExpression(Compiler* compiler) {
  parsePrecedence(compiler, PREC_LOWEST);
}

static void compileIfStatement(Compiler* compiler) {

  skipNewLines(compiler);
  compileExpression(compiler); //< Condition.
  emitOpcode(compiler, OP_JUMP_IF_NOT);
  int ifpatch = emitShort(compiler, 0xffff); //< Will be patched.

  compileBlockBody(compiler, BLOCK_IF);

  // Elif statement's don't consume 'end' after they end since it's treated as
  // else and if they require 2 'end' statements. But we're omitting the 'end'
  // for the 'else' since it'll consumed by the 'if'.
  bool elif = false;

  if (peek(compiler) == TK_ELIF) {
    elif = true;
    // Override the elif to if so that it'll be parsed as a new if statement
    // and that's why we're not consuming it here.
    compiler->current.type = TK_IF;

    // Jump pass else.
    emitOpcode(compiler, OP_JUMP);
    int exit_jump = emitShort(compiler, 0xffff); //< Will be patched.

    patchJump(compiler, ifpatch);
    compileBlockBody(compiler, BLOCK_ELIF);
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

  if (!elif) {
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
  loop.depth = compiler->scope_depth;
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

  skipNewLines(compiler);
  consume(compiler, TK_END, "Expected 'end' after statement end.");
  compilerExitBlock(compiler); //< Iterator scope.
}

// Compiles a statement. Assignment could be an assignment statement or a new
// variable declaration, which will be handled.
static void compileStatement(Compiler* compiler) {

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
    int patch = emitByte(compiler, 0xffff); //< Will be patched.
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
      consumeEndStatement(compiler);
      emitOpcode(compiler, OP_RETURN);
    }
  } else if (match(compiler, TK_IF)) {
    compileIfStatement(compiler);

  } else if (match(compiler, TK_WHILE)) {
    compileWhileStatement(compiler);

  } else if (match(compiler, TK_FOR)) {
    compileForStatement(compiler);

  } else {
    compiler->new_local = false;
    compileExpression(compiler);
    consumeEndStatement(compiler);
    if (!compiler->new_local) {
      // Pop the temp.
      emitOpcode(compiler, OP_POP);
    }
    compiler->new_local = false;
  }
}

bool compile(PKVM* vm, Script* script, const char* source) {

  // Skip utf8 BOM if there is any.
  if (strncmp(source, "\xEF\xBB\xBF", 3) == 0) source += 3;

  Compiler _compiler;
  Compiler* compiler = &_compiler; //< Compiler pointer for quick access.
  compilerInit(compiler, vm, source, script);

  // If compiling for an imported script the vm->compiler would be the compiler
  // of the script that imported this script. create a reference to it and
  // update the compiler when we're done with this one.
  Compiler* last_compiler = vm->compiler;
  vm->compiler = compiler;

  Func curr_fn;
  curr_fn.depth = DEPTH_SCRIPT;
  curr_fn.ptr = script->body;
  curr_fn.outer_func = NULL;
  compiler->func = &curr_fn;

  // Lex initial tokens. current <-- next.
  lexToken(compiler);
  lexToken(compiler);
  skipNewLines(compiler);

  while (!match(compiler, TK_EOF)) {

    if (match(compiler, TK_NATIVE)) {
      compileFunction(compiler, FN_NATIVE);

    } else if (match(compiler, TK_DEF)) {
      compileFunction(compiler, FN_SCRIPT);

    } else if (match(compiler, TK_FROM)) {
      compileImportStatement(compiler, true);

    } else if (match(compiler, TK_IMPORT)) {
      compileImportStatement(compiler, false);

    } else {
      compileStatement(compiler);
    }

    skipNewLines(compiler);
  }

  emitOpcode(compiler, OP_PUSH_NULL);
  emitOpcode(compiler, OP_RETURN);
  emitOpcode(compiler, OP_END);

  // Create script globals.
  for (int i = 0; i < compiler->var_count; i++) {
    ASSERT(compiler->variables[i].depth == (int)DEPTH_GLOBAL, OOPS);
    varBufferWrite(&script->globals, vm, VAR_NULL);
    // TODO: add the names to global_names table.
  }

  vm->compiler = last_compiler;

#if DEBUG_DUMP_COMPILED_CODE
  dumpFunctionCode(vm, script->body);
#endif

  // Return true if success.
  return !(compiler->has_errors);
}

///////////////////////////////////////////////////////////////////////////////

void compilerMarkObjects(Compiler* compiler, PKVM* vm) {
  
  // Mark the script which is currently being compiled.
  grayObject(&compiler->script->_super, vm);

  // Mark the string literals (they haven't added to the script's literal
  // buffer yet).
  grayValue(compiler->current.value, vm);
  grayValue(compiler->previous.value, vm);
  grayValue(compiler->next.value, vm);
}
