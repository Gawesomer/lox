#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

struct Parser {
	struct Token current;
	struct Token previous;
	bool had_error;
	bool panic_mode;
};

enum Precedence {
	PREC_NONE,
	PREC_ASSIGNMENT,  // =
	PREC_TERNARY,     // ?:
	PREC_OR,          // or
	PREC_AND,         // and
	PREC_EQUALITY,    // == !=
	PREC_COMPARISON,  // < > <= >=
	PREC_TERM,        // + -
	PREC_FACTOR,      // * /
	PREC_UNARY,       // ! -
	PREC_CALL,        // . ()
	PREC_PRIMARY
};

struct ParseRule {
	void (*prefix)(void);
	void (*infix)(void);
	enum Precedence precedence;
};

struct Parser parser;
struct Chunk *compiling_chunk;

static struct Chunk *current_chunk(void)
{
	return compiling_chunk;
}

static void error_at(struct Token *token, const char *message)
{
	if (parser.panic_mode)
		return;
	parser.panic_mode = true;
	fprintf(stderr, "[line %d] Error", token->line);

	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	} else if (token->type == TOKEN_ERROR) {
		// Nothing.
	} else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}

	fprintf(stderr, ": %s\n", message);
	parser.had_error = true;
}

static void error(const char *message)
{
	error_at(&parser.previous, message);
}

static void error_at_current(const char *message)
{
	error_at(&parser.current, message);
}

static void advance(void)
{
	parser.previous = parser.current;

	for (;;) {
		parser.current = scan_token();
		if (parser.current.type != TOKEN_ERROR)
			break;

		error_at_current(parser.current.start);
	}
}

static void consume(enum TokenType type, const char *message)
{
	if (parser.current.type == type) {
		advance();
		return;
	}

	error_at_current(message);
}

static void emit_byte(uint8_t byte)
{
	write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2)
{
	emit_byte(byte1);
	emit_byte(byte2);
}

static void emit_return(void)
{
	emit_byte(OP_RETURN);
}

static int make_constant(Value value)
{
	int constant = write_constant(current_chunk(), value, parser.previous.line);

	if (constant > 0xFFFFFF) {
		error("Too many constants in one chunk.");
		return 0;
	}

	return constant;
}

static void emit_constant(Value value)
{
	make_constant(value);
}

static void end_compiler(void)
{
	emit_return();
#ifdef DEBUG_PRINT_CODE
	if (!parser.had_error)
		disassemble_chunk(current_chunk(), "code");
#endif
}

static void expression(void);
static struct ParseRule *get_rule(enum TokenType type);
static void parse_precedence(enum Precedence precedence);

static void binary(void)
{
	enum TokenType operator_type = parser.previous.type;
	struct ParseRule *rule = get_rule(operator_type);

	parse_precedence((enum Precedence)(rule->precedence + 1));

	switch (operator_type) {
	case TOKEN_BANG_EQUAL:
		emit_bytes(OP_EQUAL, OP_NOT);
		break;
	case TOKEN_EQUAL_EQUAL:
		emit_byte(OP_EQUAL);
		break;
	case TOKEN_GREATER:
		emit_byte(OP_GREATER);
		break;
	case TOKEN_GREATER_EQUAL:
		emit_bytes(OP_LESS, OP_NOT);
		break;
	case TOKEN_LESS:
		emit_byte(OP_LESS);
		break;
	case TOKEN_LESS_EQUAL:
		emit_bytes(OP_GREATER, OP_NOT);
		break;
	case TOKEN_PLUS:
		emit_byte(OP_ADD);
		break;
	case TOKEN_MINUS:
		emit_byte(OP_SUBTRACT);
		break;
	case TOKEN_STAR:
		emit_byte(OP_MULTIPLY);
		break;
	case TOKEN_SLASH:
		emit_byte(OP_DIVIDE);
		break;
	default:
		return;  // Unreachable.
	}
}

static void literal(void)
{
	switch (parser.previous.type) {
	case TOKEN_FALSE:
		emit_byte(OP_FALSE);
		break;
	case TOKEN_NIL:
		emit_byte(OP_NIL);
		break;
	case TOKEN_TRUE:
		emit_byte(OP_TRUE);
		break;
	default:
		return;  // Unreachable.
	}
}

static void ternary(void)
{
	struct ParseRule *rule = get_rule(parser.previous.type);

	parse_precedence((enum Precedence)(rule->precedence));
	consume(TOKEN_COLON, "Expect ':' after '?' operator.");
	parse_precedence((enum Precedence)(rule->precedence));
	// TODO: Compile ternary
}

static void grouping(void)
{
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(void)
{
	double value = strtod(parser.previous.start, NULL);

	emit_constant(NUMBER_VAL(value));
}

static void string(void)
{
	emit_constant(OBJ_VAL(copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

static void unary(void)
{
	enum TokenType operator_type = parser.previous.type;

	// Compile the operand.
	parse_precedence(PREC_UNARY);

	// Emit the operator instruction.
	switch (operator_type) {
	case TOKEN_BANG:
		emit_byte(OP_NOT);
		break;
	case TOKEN_MINUS:
		emit_byte(OP_NEGATE);
		break;
	default:
		return;  // Unreachable.
	}
}

struct ParseRule rules[] = {
	[TOKEN_LEFT_PAREN]    = {grouping, NULL,    PREC_NONE},
	[TOKEN_RIGHT_PAREN]   = {NULL,     NULL,    PREC_NONE},
	[TOKEN_LEFT_BRACE]    = {NULL,     NULL,    PREC_NONE},
	[TOKEN_RIGHT_BRACE]   = {NULL,     NULL,    PREC_NONE},
	[TOKEN_PLUS]          = {NULL,     binary,  PREC_TERM},
	[TOKEN_MINUS]         = {unary,    binary,  PREC_TERM},
	[TOKEN_STAR]          = {NULL,     binary,  PREC_FACTOR},
	[TOKEN_SLASH]         = {NULL,     binary,  PREC_FACTOR},
	[TOKEN_COMMA]         = {NULL,     NULL,    PREC_NONE},
	[TOKEN_DOT]           = {NULL,     NULL,    PREC_NONE},
	[TOKEN_EROTEME]       = {NULL,     ternary, PREC_TERNARY},
	[TOKEN_SEMICOLON]     = {NULL,     NULL,    PREC_NONE},
	[TOKEN_COLON]         = {NULL,     NULL,    PREC_NONE},
	[TOKEN_BANG]          = {unary,    NULL,    PREC_NONE},
	[TOKEN_BANG_EQUAL]    = {NULL,     binary,  PREC_EQUALITY},
	[TOKEN_EQUAL]         = {NULL,     NULL,    PREC_NONE},
	[TOKEN_EQUAL_EQUAL]   = {NULL,     binary,  PREC_EQUALITY},
	[TOKEN_GREATER]       = {NULL,     binary,  PREC_COMPARISON},
	[TOKEN_GREATER_EQUAL] = {NULL,     binary,  PREC_COMPARISON},
	[TOKEN_LESS]          = {NULL,     binary,  PREC_COMPARISON},
	[TOKEN_LESS_EQUAL]    = {NULL,     binary,  PREC_COMPARISON},
	[TOKEN_IDENTIFIER]    = {NULL,     NULL,    PREC_NONE},
	[TOKEN_STRING]        = {string,   NULL,    PREC_NONE},
	[TOKEN_NUMBER]        = {number,   NULL,    PREC_NONE},
	[TOKEN_AND]           = {NULL,     NULL,    PREC_NONE},
	[TOKEN_CLASS]         = {NULL,     NULL,    PREC_NONE},
	[TOKEN_ELSE]          = {NULL,     NULL,    PREC_NONE},
	[TOKEN_FALSE]         = {literal,  NULL,    PREC_NONE},
	[TOKEN_FOR]           = {NULL,     NULL,    PREC_NONE},
	[TOKEN_FUN]           = {NULL,     NULL,    PREC_NONE},
	[TOKEN_IF]            = {NULL,     NULL,    PREC_NONE},
	[TOKEN_NIL]           = {literal,  NULL,    PREC_NONE},
	[TOKEN_OR]            = {NULL,     NULL,    PREC_NONE},
	[TOKEN_PRINT]         = {NULL,     NULL,    PREC_NONE},
	[TOKEN_RETURN]        = {NULL,     NULL,    PREC_NONE},
	[TOKEN_SUPER]         = {NULL,     NULL,    PREC_NONE},
	[TOKEN_THIS]          = {NULL,     NULL,    PREC_NONE},
	[TOKEN_TRUE]          = {literal,  NULL,    PREC_NONE},
	[TOKEN_VAR]           = {NULL,     NULL,    PREC_NONE},
	[TOKEN_WHILE]         = {NULL,     NULL,    PREC_NONE},
	[TOKEN_ERROR]         = {NULL,     NULL,    PREC_NONE},
	[TOKEN_EOF]           = {NULL,     NULL,    PREC_NONE},
};

static void parse_precedence(enum Precedence precedence)
{
	advance();
	void (*prefix_rule)(void) = get_rule(parser.previous.type)->prefix;

	if (prefix_rule == NULL) {
		error("Expect expression.");
		return;
	}

	prefix_rule();

	while (precedence <= get_rule(parser.current.type)->precedence) {
		advance();
		void (*infix_rule)(void) = get_rule(parser.previous.type)->infix;

		infix_rule();
	}
}

static struct ParseRule *get_rule(enum TokenType type)
{
	return &rules[type];
}

static void expression(void)
{
	parse_precedence(PREC_ASSIGNMENT);
}

bool compile(const char *source, struct Chunk *chunk)
{
	init_scanner(source);
	compiling_chunk = chunk;

	parser.had_error = false;
	parser.panic_mode = false;

	advance();
	expression();
	consume(TOKEN_EOF, "Expect end of expression.");
	end_compiler();
	return !parser.had_error;
}
