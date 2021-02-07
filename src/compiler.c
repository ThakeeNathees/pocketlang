/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "compiler.h"

#include "types/name_table.h"
#include "types/gen/byte_buffer.h"
#include "utils.h"
#include "vm.h"

// The maximum number of variables (or global if compiling top level script) 
// to lookup from the compiling context. Also it's limited by it's opcode 
// which is using a single byte value to identify the local.
#define MAX_VARIABLES 256

typedef enum {

	TK_ERROR = 0,
	TK_EOF,
	TK_LINE,

	// symbols
	TK_DOT,        // .
	TK_DOTDOT,     // ..
	TK_COMMA,       // ,
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
	//TK_BANG,       // !  parsed as TK_NOT

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
	// >>= <<=
	//TK_PLUSPLUS,   // ++
	//TK_MINUSMINUS, // --
	//TK_MODEQ,      // %=
	//TK_XOREQ,      // ^=

	// Keywords.
	//TK_TYPE,     // type
	TK_IMPORT,     // import
	TK_ENUM,       // enum
	TK_DEF,        // def
	TK_NATIVE,     // native (C function declaration)
	TK_END,        // end

	TK_NULL,       // null
	TK_SELF,       // self
	TK_IS,         // is
	TK_IN,         // in
	TK_AND,        // and
	TK_OR,         // or
	TK_NOT,        // not
	TK_TRUE,       // true
	TK_FALSE,      // false

	// Type names for is test.
	// TK_NULL already defined.
	TK_BOOL_T,    // Bool
	TK_NUM_T,     // Num
	TK_STRING_T,  // String
	TK_ARRAY_T,   // Array
	TK_MAP_T,     // Map
	TK_RANGE_T,   // Range
	TK_FUNC_T,    // Function 
	TK_OBJ_T,     // Object (self, user data, etc.)

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
	//{ "type",   4, TK_TYPE },
	{ "import",   6,  TK_IMPORT },
	{ "enum",     4,  TK_ENUM },
	{ "def",      3,  TK_DEF },
	{ "native",   6,  TK_NATIVE },
	{ "end",      3,  TK_END },
	{ "null",     4, TK_NULL },
	{ "self",     4, TK_SELF },
	{ "is",       2, TK_IS },
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

	// Type names.
	{ "Bool",     4, TK_BOOL_T },
	{ "Num",      3, TK_NUM_T },
	{ "String",   6, TK_STRING_T },
	{ "Array",    5, TK_ARRAY_T },
	{ "Map",      3, TK_MAP_T },
	{ "Range",    5, TK_RANGE_T },
	{ "Object",   6, TK_OBJ_T },
	{ "Function", 8, TK_FUNC_T },

	{ NULL,      (TokenType)(0) }, // Sentinal to mark the end of the array
};

typedef struct {
	VM* vm;             //< Owner of the parser (for reporting errors, etc).

	const char* source; //< Currently compiled source.

	const char* token_start;  //< Start of the currently parsed token.
	const char* current_char; //< Current char position in the source.
	int current_line;         //< Line number of the current char.

	Token previous, current, next; //< Currently parsed tokens.

	bool has_errors;    //< True if any syntex error occured at compile time.
} Parser;

// Compiler Types ////////////////////////////////////////////////////////////

// Precedence parsing references:
// https://en.wikipedia.org/wiki/Shunting-yard_algorithm
// TODO: I should explicitly state wren-lang as a reference "globaly".

typedef enum {
	PREC_NONE,
	PREC_LOWEST,
	PREC_ASSIGNMENT,    // =
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

typedef struct {
	const char* name; //< Directly points into the source string.
	int length;       //< Length of the name.
	int depth;        //< The depth the local is defined in. (-1 means global)
} Variable;

typedef struct sLoop {

	// Index of the loop's start instruction where the execution will jump
	// back to once it reach the loop end.
	int start;

	// Index of the jump out address instruction to patch it's value once done
	// compiling the loop.
	int exit_jump;

	// Index of the first body instruction. Needed to start patching jump
	// address from which till the loop end.
	int body;

	// The outer loop of the current loop used to set and reset the compiler's
	// current loop context.
	struct sLoop* outer_loop;

} Loop;

struct Compiler {

	VM* vm;
	Parser parser;

	// Current depth the compiler in (-1 means top level) 0 means function
	// level and > 0 is inner scope.
	int scope_depth;

	Variable variables[MAX_VARIABLES]; //< Variables in the current context.
	int var_count;                     //< Number of locals in [variables].

	// TODO: compiler should mark Script* below not to be garbage collected.

	Script* script; //< Current script.
	Loop* loop;     //< Current loop.
	Function* fn;   //< Current function.
};

/*****************************************************************************
 * LEXING                                                                    *
 *****************************************************************************/

// Forward declaration of lexer methods.

static char eatChar(Parser* parser);
static void setNextValueToken(Parser* parser, TokenType type, Var value);
static void setNextToken(Parser* parser, TokenType type);
static bool matchChar(Parser* parser, char c);
static bool matchLine(Parser* parser);

static void eatString(Parser* parser) {
	ByteBuffer buff;
	byteBufferInit(&buff);

	while (true) {
		char c = eatChar(parser);

		if (c == '"') break;

		if (c == '\0') {
			// TODO: syntaxError()

			// Null byte is required by TK_EOF.
			parser->current_char--;
			break;
		}

		if (c == '\\') {
			switch (eatChar(parser)) {
				case '"': byteBufferWrite(&buff, parser->vm, '"'); break;
				case '\\': byteBufferWrite(&buff, parser->vm, '\\'); break;
				case 'n': byteBufferWrite(&buff, parser->vm, '\n'); break;
				case 'r': byteBufferWrite(&buff, parser->vm, '\r'); break;
				case 't': byteBufferWrite(&buff, parser->vm, '\t'); break;

				default:
					// TODO: syntaxError("Error: invalid escape character")
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

	if (matchChar(parser, '.')) {
		while (utilIsDigit(peekChar(parser)))
			eatChar(parser);
	}

	Var value = VAR_NUM(strtod(parser->token_start, NULL));
	setNextValueToken(parser, TK_NUMBER, value);
}

// Read and ignore chars till it reach new line or EOF.
static void skipLineComment(Parser* parser) {
	char c = eatChar(parser);

	while (c != '\n' && c != '\0') {
		c = eatChar(parser);
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
			case '#': setNextToken(parser, TK_HASH); return;
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

			case '"': eatString(parser); return;

			default: {

				if (utilIsDigit(c)) {
					eatNumber(parser);
				} else if (utilIsName(c)) {
					eatName(parser);
				} else {
					if (c >= 32 && c <= 126) {
						// TODO: syntaxError("Invalid character %c", c);
					} else {
						// TODO: syntaxError("Invalid byte 0x%x", (uint8_t)c);
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
static void parserInit(Parser* self, VM* vm, const char* source) {
	self->vm = vm;
	self->source = source;
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
// and return true otherwise reutrn false. It'll skips all the new lines
// inbetween thus matching TK_LINE is invalid.
static bool match(Parser* self, TokenType expected) {
	ASSERT(expected != TK_LINE, "Can't match TK_LINE.");
	matchLine(self);

	if (peek(self) != expected) return false;
	lexToken(self);
	return true;
}

// Match one or more lines and return true if there any.
static bool matchLine(Parser* parser) {
	if (peek(parser) != TK_LINE) return false;
	while (peek(parser) == TK_LINE)
		lexToken(parser);
	return true;
}

// Match semi collon or multiple new lines.
static void matchEndStatement(Parser* parser) {

	// Semi collon must be on the same line.
	if (peek(parser) == TK_SEMICOLLON)
		match(parser, TK_SEMICOLLON);

	matchLine(parser);
}

// Match optional "do" keyword and new lines.
static void matchStartBlock(Parser* parser) {

	// "do" must be on the same line.
	if (peek(parser) == TK_DO)
		match(parser, TK_DO);

	matchLine(parser);
}

// Consume the the current token and if it's not [expected] emits error log
// and continue parsing for more error logs. It'll skips all the new lines
// inbetween thus matching TK_LINE is invald.
static void consume(Parser* self, TokenType expected, const char* err_msg) {
	ASSERT(expected != TK_LINE, "Can't match TK_LINE.");
	matchLine(self);

	lexToken(self);
	if (self->previous.type != expected) {
		// TODO: syntaxError(err_msg);

		// If the next token is expected discard the current to minimize
		// cascaded errors and continue parsing.
		if (peek(self) == expected) {
			lexToken(self);
		}
	}
}

/*****************************************************************************
 * PARSING GRAMMAR                                                           *
 *****************************************************************************/

// Forward declaration of grammar functions.

static void exprAssignment(Compiler* compiler, bool can_assign);

// Bool, Num, String, Null, -and- bool_t, Array_t, String_t, ...
static void exprLiteral(Compiler* compiler, bool can_assign);
static void exprName(Compiler* compiler, bool can_assign);


static void exprBinaryOp(Compiler* compiler, bool can_assign);
static void exprUnaryOp(Compiler* compiler, bool can_assign);

static void exprGrouping(Compiler* compiler, bool can_assign);
static void exprArray(Compiler* compiler, bool can_assign);
static void exprMap(Compiler* compiler, bool can_assign);

static void exprCall(Compiler* compiler, bool can_assign);
static void exprAttrib(Compiler* compiler, bool can_assign);
static void exprSubscript(Compiler* compiler, bool can_assign);

#define NO_RULE { NULL,          NULL,          PREC_NONE }
#define NO_INFIX PREC_NONE

GrammarRule rules[] = {  // Prefix       Infix             Infix Precedence
	/* TK_ERROR      */   NO_RULE,
	/* TK_EOF        */   NO_RULE,
	/* TK_LINE       */   NO_RULE,
	/* TK_DOT        */ { exprAttrib,    NULL,             PREC_ATTRIB },
	/* TK_DOTDOT     */ { NULL,          exprBinaryOp,     PREC_RANGE },
	/* TK_COMMA      */   NO_RULE,
	/* TK_COLLON     */   NO_RULE,
	/* TK_SEMICOLLON */   NO_RULE,
	/* TK_HASH       */   NO_RULE,
	/* TK_LPARAN     */ { exprGrouping,  exprCall,         PREC_CALL },
	/* TK_RPARAN     */   NO_RULE,
	/* TK_LBRACKET   */ { exprArray,     exprSubscript,    PREC_SUBSCRIPT },
	/* TK_RBRACKET   */   NO_RULE,
	/* TK_LBRACE     */ { exprMap,       NULL,             NO_INFIX },
	/* TK_RBRACE     */   NO_RULE,
	/* TK_PERCENT    */ { NULL,          exprBinaryOp,     PREC_FACTOR },
	/* TK_TILD       */ { exprUnaryOp,   NULL,             NO_INFIX },
	/* TK_AMP        */ { NULL,          exprBinaryOp,     PREC_BITWISE_AND },
	/* TK_PIPE       */ { NULL,          exprBinaryOp,     PREC_BITWISE_OR },
	/* TK_CARET      */ { NULL,          exprBinaryOp,     PREC_BITWISE_XOR },
	/* TK_PLUS       */ { NULL,          exprBinaryOp,     PREC_TERM },
	/* TK_MINUS      */ { NULL,          exprBinaryOp,     PREC_TERM },
	/* TK_STAR       */ { NULL,          exprBinaryOp,     PREC_FACTOR },
	/* TK_FSLASH     */ { NULL,          exprBinaryOp,     PREC_FACTOR },
	/* TK_BSLASH     */   NO_RULE,
	/* TK_EQ         */ { NULL,          exprAssignment,   PREC_ASSIGNMENT },
	/* TK_GT         */ { NULL,          exprBinaryOp,     PREC_COMPARISION },
	/* TK_LT         */ { NULL,          exprBinaryOp,     PREC_COMPARISION },
	/* TK_EQEQ       */ { NULL,          exprBinaryOp,     PREC_EQUALITY },
	/* TK_NOTEQ      */ { NULL,          exprBinaryOp,     PREC_EQUALITY },
	/* TK_GTEQ       */ { NULL,          exprBinaryOp,     PREC_COMPARISION },
	/* TK_LTEQ       */ { NULL,          exprBinaryOp,     PREC_COMPARISION },
	/* TK_PLUSEQ     */ { NULL,          exprAssignment,   PREC_ASSIGNMENT },
	/* TK_MINUSEQ    */ { NULL,          exprAssignment,   PREC_ASSIGNMENT },
	/* TK_STAREQ     */ { NULL,          exprAssignment,   PREC_ASSIGNMENT },
	/* TK_DIVEQ      */ { NULL,          exprAssignment,   PREC_ASSIGNMENT },
	/* TK_SRIGHT     */ { NULL,          exprBinaryOp,     PREC_BITWISE_SHIFT },
	/* TK_SLEFT      */ { NULL,          exprBinaryOp,     PREC_BITWISE_SHIFT },
	/* TK_IMPORT     */   NO_RULE,
	/* TK_ENUM       */   NO_RULE,
	/* TK_DEF        */   NO_RULE,
	/* TK_EXTERN     */   NO_RULE,
	/* TK_END        */   NO_RULE,
	/* TK_NULL       */   NO_RULE,
	/* TK_SELF       */   NO_RULE,
	/* TK_IS         */ { NULL,          exprBinaryOp,     PREC_IS },
	/* TK_IN         */ { NULL,          exprBinaryOp,     PREC_IN },
	/* TK_AND        */ { NULL,          exprBinaryOp,     PREC_LOGICAL_AND },
	/* TK_OR         */ { NULL,          exprBinaryOp,     PREC_LOGICAL_OR },
	/* TK_NOT        */ { NULL,          exprUnaryOp,      PREC_LOGICAL_NOT },
	/* TK_TRUE       */ { exprLiteral,   NULL,             NO_INFIX },
	/* TK_FALSE      */ { exprLiteral,   NULL,             NO_INFIX },
	/* TK_BOOL_T     */ { exprLiteral,   NULL,             NO_INFIX },
	/* TK_NUM_T      */ { exprLiteral,   NULL,             NO_INFIX },
	/* TK_STRING_T   */ { exprLiteral,   NULL,             NO_INFIX },
	/* TK_ARRAY_T    */ { exprLiteral,   NULL,             NO_INFIX },
	/* TK_MAP_T      */ { exprLiteral,   NULL,             NO_INFIX },
	/* TK_RANGE_T    */ { exprLiteral,   NULL,             NO_INFIX },
	/* TK_FUNC_T     */ { exprLiteral,   NULL,             NO_INFIX },
	/* TK_OBJ_T      */ { exprLiteral,   NULL,             NO_INFIX },
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

static void exprAssignment(Compiler* compiler, bool can_assign) { /*TODO*/ }

static void exprLiteral(Compiler* compiler, bool can_assign) { /*TODO*/ }
static void exprName(Compiler* compiler, bool can_assign) { /*TODO*/ }


static void exprBinaryOp(Compiler* compiler, bool can_assign) { /*TODO*/ }
static void exprUnaryOp(Compiler* compiler, bool can_assign) { /*TODO*/ }

static void exprGrouping(Compiler* compiler, bool can_assign) { /*TODO*/ }
static void exprArray(Compiler* compiler, bool can_assign) { /*TODO*/ }
static void exprMap(Compiler* compiler, bool can_assign) { /*TODO*/ }

static void exprCall(Compiler* compiler, bool can_assign) { /*TODO*/ }
static void exprAttrib(Compiler* compiler, bool can_assign) { /*TODO*/ }
static void exprSubscript(Compiler* compiler, bool can_assign) { /*TODO*/ }


/*****************************************************************************
 * COMPILING                                                                 *
 *****************************************************************************/

// Used in searching for local variables.
typedef enum {
	SCOPE_ANY = -3,
	SCOPE_CURRENT,
} ScopeType;

// Result type for an identifier definition.
typedef enum {
	NAME_NOT_DEFINED,
	NAME_LOCAL_VAR, //< Including parameter.
	NAME_GLOBAL_VAR,
	NAME_SCRIPT_FN,
} NameDefnType;

// Identifier search result.
typedef struct {

	NameDefnType type;

	// Could be found in one of the imported script or in it's imported script
	// recursively. If true [_extern] will be the script ID.
	bool is_extern;

	// Extern script's ID.
	ID _extern;

	union {
		int local;
		int global;
		int func;
	} index;

} NameSearchResult;

static void compilerInit(Compiler* compiler, VM* vm, const char* source) {
	parserInit(&compiler->parser, vm, source);
	compiler->vm = vm;
	compiler->scope_depth = -1;
	compiler->var_count = 0;
	Loop* loop = NULL;
	Function* fn = NULL;
}

// Search for the name through compiler's variables. Returns -1 if not found.
static int compilerSearchVariables(Compiler* compiler, const char* name,
                                   int length, ScopeType scope) {

	for (int i = 0; i < compiler->var_count; i++) {
		Variable* variable = &compiler->variables[i];
		if (scope == SCOPE_CURRENT &&
			compiler->scope_depth != variable->depth) {
			continue;
		}
		if (variable->length == length &&
			strncmp(variable->name, name, length) == 0) {
			return i;
		}
	}

	return -1;
}

// Will check if the name already defined.
static NameSearchResult compilerSearchName(Compiler* compiler,
                                           const char* name, int length) {
	// TODO:
	NameSearchResult result;
	result.type = NAME_NOT_DEFINED;
	return result;
}

// Add a variable and return it's index to the context. Assumes that the
// variable name is unique and not defined before in the current scope.
static int compilerAddVariable(Compiler* compiler, const char* name,
                                int length) {
	Variable* variable = &compiler->variables[compiler->var_count];
	variable->name = name;
	variable->length = length;
	variable->depth = compiler->scope_depth;
	return compiler->var_count++;
}

static void compileFunction(Compiler* compiler, bool is_native) {

	Parser* parser = &compiler->parser;

	consume(&compiler->parser, TK_NAME, "Expected a function name.");

	const char* name_start = parser->previous.start;
	int name_length = parser->previous.length;
	NameSearchResult result = compilerSearchName(compiler, name_start,
		name_length);

	if (result.type != NAME_NOT_DEFINED) {
		// TODO: multiple definition error();
	}

	int index = nameTableAdd(&compiler->script->function_names, compiler->vm,
		name_start, name_length);

	Function* func = newFunction(compiler->vm, nameTableGet(
		&compiler->script->function_names, index), compiler->script, is_native);

	vmPushTempRef(compiler->vm, &func->_super);
	functionBufferWrite(&compiler->script->functions, compiler->vm, func);
	vmPopTempRef(compiler->vm);

	compiler->fn = func;

	consume(parser, TK_LPARAN, "Expected '(' after function name.");

	compiler->scope_depth++; // Parameter scope.

	// Compile parameter list.
	while (match(parser, TK_NAME)) {
		int predef = compilerSearchVariables(compiler, parser->previous.start,
			parser->previous.length, SCOPE_CURRENT);
		if (predef != -1) {
			// TODO: error("Multiple definition of a parameter");
		}
		match(parser, TK_COMMA);
	}

	consume(parser, TK_RPARAN, "Expected ')' after parameters end.");
	matchEndStatement(parser);

	if (is_native) { // Done here.
		compiler->scope_depth--; // Parameter scope.
		compiler->fn = NULL;
		return;
	}

	// TODO: Compile body.

	compiler->scope_depth--; // Parameter scope.
	compiler->fn = NULL;
}

Script* compileSource(VM* vm, const char* source) {

	// Skip utf8 BOM if there is any.
	if (strncmp(source, "\xEF\xBB\xBF", 3) == 0) source += 3;

	Compiler compiler;
	compilerInit(&compiler, vm, source);

	Script* script = newScript(vm);
	compiler.script = script;

	// Parser pointer for quick access.
	Parser* parser = &compiler.parser;

	// Lex initial tokens. current <-- next.
	lexToken(parser);
	lexToken(parser);
	skipNewLines(parser);

	while (!match(parser, TK_EOF)) {
		
		if (match(parser, TK_NATIVE)) {
			compileFunction(&compiler, true);

		} else if (match(parser, TK_DEF)) {
			compileFunction(&compiler, false);

		} else if (match(parser, TK_IMPORT)) {
			// TODO:

		} else {
			// name = value # Variable defn.
			// name()       # statement
		}
	}
}
