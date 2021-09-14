#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "table.h"

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
	void (*prefix)(bool);
	void (*infix)(bool);
	enum Precedence precedence;
};

struct Local {
	struct Token name;
	int depth;
};

struct Compiler {
	struct Local locals[UINT8_COUNT];
	int local_count;
	int scope_depth;
};

struct Parser parser;
struct Compiler *current = NULL;
struct Chunk *compiling_chunk;
struct Table identifiers;

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

static bool check(enum TokenType type)
{
	return parser.current.type == type;
}

static bool match(enum TokenType type)
{
	if (!check(type))
		return false;
	advance();
	return true;
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

static int make_constant(enum OpCode op, enum OpCode op_long, Value value)
{
	int constant;
	Value get_res;

	if (table_get(&identifiers, value, &get_res)) {
		constant = AS_NUMBER(get_res);
	} else {
		constant = add_constant(current_chunk(), value);
		table_set(&identifiers, value, NUMBER_VAL(constant));
	}
	write_constant_op(current_chunk(), op, op_long, constant, parser.previous.line);

	if (constant > 0xFFFFFF) {
		error("Too many constants in one chunk.");
		return 0;
	}

	return constant;
}

static void emit_constant(enum OpCode op, enum OpCode op_long, Value value)
{
	make_constant(op, op_long, value);
}

static void make_global(enum OpCode op, enum OpCode op_long, Value name)
{
	Value index;

	if (!table_get(&vm.global_names, name, &index)) {
		index = NUMBER_VAL(vm.global_values.count);
		write_value_array(&vm.global_values, UNDEFINED_VAL);
		table_set(&vm.global_names, name, index);
	}
	write_constant_op(current_chunk(), op, op_long, AS_NUMBER(index), parser.previous.line);

	if (AS_NUMBER(index) > 0xFFFFFF)
		error("Too many globals in one chunk.");
}

static void emit_global(enum OpCode op, enum OpCode op_long, Value name)
{
	make_global(op, op_long, name);
}

static void init_compiler(struct Compiler *compiler)
{
	compiler->local_count = 0;
	compiler->scope_depth = 0;
	current = compiler;
}

static void end_compiler(void)
{
	emit_return();
#ifdef DEBUG_PRINT_CODE
	if (!parser.had_error)
		disassemble_chunk(current_chunk(), "code");
#endif
}

static void begin_scope(void)
{
	current->scope_depth++;
}

static void end_scope(void)
{
	current->scope_depth--;

	while (current->local_count > 0 && current->locals[current->local_count - 1].depth > current->scope_depth) {
		emit_byte(OP_POP);
		current->local_count--;
	}
}

static void expression(void);
static void statement(void);
static void declaration(void);
static struct ParseRule *get_rule(enum TokenType type);
static void parse_precedence(enum Precedence precedence);

static Value identifier_constant(struct Token *name)
{
	return OBJ_VAL(copy_string(name->start, name->length));
}

static bool identifiers_equal(struct Token *a, struct Token *b)
{
	if (a->length != b->length)
		return false;

	return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(struct Compiler *compiler, struct Token *name)
{
	for (int i = compiler->local_count - 1; i >= 0; i--) {
		struct Local *local = &compiler->locals[i];

		if (identifiers_equal(name, &local->name))
			return i;
	}

	return -1;
}

static void add_local(struct Token name)
{
	if (current->local_count == UINT8_COUNT) {
		error("Too many local variables.");
		return;
	}

	struct Local *local = &current->locals[current->local_count++];

	local->name = name;
	local->depth = current->scope_depth;
}

static void declare_variable(void)
{
	if (current->scope_depth == 0)
		return;

	struct Token *name = &parser.previous;

	for (int i = current->local_count - 1; i >= 0; i--) {
		struct Local *local = &current->locals[i];

		if (local->depth != -1 && local->depth < current->scope_depth)
			break;

		if (identifiers_equal(name, &local->name))
			error("Already a variable with this name in this scope.");
	}

	add_local(*name);
}

static Value parse_variable(const char *error_message)
{
	consume(TOKEN_IDENTIFIER, error_message);

	declare_variable();

	return identifier_constant(&parser.previous);
}

static void define_variable(Value global)
{
	if (current->scope_depth > 0)
		return;

	emit_global(OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG, global);
}

static void binary(bool can_assign)
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

static void literal(bool can_assign)
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

static void ternary(bool can_assign)
{
	struct ParseRule *rule = get_rule(parser.previous.type);

	parse_precedence((enum Precedence)(rule->precedence));
	consume(TOKEN_COLON, "Expect ':' after '?' operator.");
	parse_precedence((enum Precedence)(rule->precedence));
	// TODO: Compile ternary
}

static void grouping(bool can_assign)
{
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool can_assign)
{
	double value = strtod(parser.previous.start, NULL);

	emit_constant(OP_CONSTANT, OP_CONSTANT_LONG, NUMBER_VAL(value));
}

static void string(bool can_assign)
{
	emit_constant(OP_CONSTANT, OP_CONSTANT_LONG, OBJ_VAL(copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

static void named_variable(struct Token name, bool can_assign)
{
	int local_index = resolve_local(current, &name);
	Value global;

	if (local_index == -1)
		global = identifier_constant(&name);

	if (can_assign && match(TOKEN_EQUAL)) {
		expression();
		if (local_index == -1)
			emit_global(OP_SET_GLOBAL, OP_SET_GLOBAL_LONG, global);
		else
			emit_bytes(OP_SET_LOCAL, local_index);
	} else {
		if (local_index == -1)
			emit_global(OP_GET_GLOBAL, OP_GET_GLOBAL_LONG, global);
		else
			emit_bytes(OP_GET_LOCAL, local_index);
	}
}

static void variable(bool can_assign)
{
	named_variable(parser.previous, can_assign);
}

static void unary(bool can_assign)
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
	[TOKEN_IDENTIFIER]    = {variable, NULL,    PREC_NONE},
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
	void (*prefix_rule)(bool) = get_rule(parser.previous.type)->prefix;

	if (prefix_rule == NULL) {
		error("Expect expression.");
		return;
	}

	bool can_assign = precedence <= PREC_ASSIGNMENT;

	prefix_rule(can_assign);

	while (precedence <= get_rule(parser.current.type)->precedence) {
		advance();
		void (*infix_rule)(bool) = get_rule(parser.previous.type)->infix;

		infix_rule(can_assign);
	}

	if (can_assign && match(TOKEN_EQUAL))
		error("Invalid assignment target.");
}

static struct ParseRule *get_rule(enum TokenType type)
{
	return &rules[type];
}

static void expression(void)
{
	parse_precedence(PREC_ASSIGNMENT);
}

static void block(void)
{
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
	       declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void var_declaration(void)
{
	Value name  = parse_variable("Expect variable name.");

	if (match(TOKEN_EQUAL))
		expression();
	else
		emit_byte(OP_NIL);

	consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

	define_variable(name);
}

static void expression_statement(void)
{
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
	emit_byte(OP_POP);
}

static void print_statement(void)
{
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emit_byte(OP_PRINT);
}

static void synchronize(void)
{
	parser.panic_mode = false;

	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON)
			return;

		switch (parser.current.type) {
		case TOKEN_CLASS:
		case TOKEN_FUN:
		case TOKEN_VAR:
		case TOKEN_FOR:
		case TOKEN_IF:
		case TOKEN_WHILE:
		case TOKEN_PRINT:
		case TOKEN_RETURN:
			return;
		default:
			;  // Do nothing.
		}

		advance();
	}
}

static void declaration(void)
{
	if (match(TOKEN_VAR))
		var_declaration();
	else
		statement();

	if (parser.panic_mode)
		synchronize();
}

static void statement(void)
{
	if (match(TOKEN_PRINT)) {
		print_statement();
	} else if (match(TOKEN_LEFT_BRACE)) {
		begin_scope();
		block();
		end_scope();
	} else {
		expression_statement();
	}
}

bool compile(const char *source, struct Chunk *chunk)
{
	init_scanner(source);
	init_table(&identifiers);

	struct Compiler compiler;

	init_compiler(&compiler);
	compiling_chunk = chunk;

	parser.had_error = false;
	parser.panic_mode = false;

	advance();

	while (!match(TOKEN_EOF))
		declaration();

	end_compiler();
	free_table(&identifiers);
	return !parser.had_error;
}
