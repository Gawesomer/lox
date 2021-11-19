#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
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
	bool is_immutable;
	bool is_captured;
};

struct Upvalue {
	uint8_t index;
	bool is_local;
};

struct LocalArray {
	int count;
	int capacity;
	struct Local *locals;
};

enum FunctionType {
	TYPE_FUNCTION,
	TYPE_METHOD,
	TYPE_SCRIPT,
};

struct Compiler {
	struct Compiler *enclosing;
	struct ObjFunction *function;
	enum FunctionType type;

	int scope_depth;
	struct Upvalue upvalues[UINT8_COUNT];
	struct LocalArray local_vars;
	int curr_loop;
	int curr_loop_depth;
	bool in_switch;
	int break_stmts[MAX_BREAK_COUNT];
	int break_count;
	struct Table identifiers;
};

struct ClassCompiler {
	struct ClassCompiler *enclosing;
};

struct Parser parser;
struct Compiler *current = NULL;
struct ClassCompiler *current_class = NULL;

static struct Chunk *current_chunk(void)
{
	return &current->function->chunk;
}

static void init_local_array(struct LocalArray *array)
{
	array->count = 0;
	array->capacity = 0;
	array->locals = NULL;
}

static void free_local_array(struct LocalArray *array)
{
	FREE_ARRAY(struct Local, array->locals, array->capacity);
	init_local_array(array);
}

static void write_local_array(struct LocalArray *array, struct Local local)
{
	if (array->capacity < array->count + 1) {
		int old_capacity = array->capacity;

		array->capacity = GROW_CAPACITY(old_capacity);
		array->locals = GROW_ARRAY(struct Local, array->locals, old_capacity, array->capacity);
	}

	array->locals[array->count] = local;
	array->count++;
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

static void emit_loop(int loop_start)
{
	emit_byte(OP_LOOP);

	int offset = current_chunk()->count - loop_start + 2;

	if (offset > UINT16_MAX)
		error("Loop body too large.");

	emit_byte((offset >> 8) & 0xFF);
	emit_byte(offset & 0xFF);
}

static int emit_jump(uint8_t instruction)
{
	emit_byte(instruction);
	emit_byte(0xFF);
	emit_byte(0xFF);
	return current_chunk()->count - 2;
}

static void emit_return(void)
{
	emit_byte(OP_NIL);
	emit_byte(OP_RETURN);
}

static int make_constant(enum OpCode op, enum OpCode op_long, Value value)
{
	int constant;
	Value get_res;

	if (table_get(&current->identifiers, value, &get_res)) {
		constant = AS_NUMBER(get_res);
	} else {
		constant = add_constant(current_chunk(), value);
		table_set(&current->identifiers, value, NUMBER_VAL(constant));
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

static void patch_jump(int offset)
{
	// -2 to adjust for the bytecode for the jump offset itself.
	int jump = current_chunk()->count - offset - 2;

	if (jump > UINT16_MAX)
		error("Too much code to jump over.");

	current_chunk()->code[offset] = (jump >> 8) & 0xFF;
	current_chunk()->code[offset + 1] = jump & 0xFF;
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

static void init_compiler(struct Compiler *compiler, enum FunctionType type)
{
	compiler->enclosing = current;
	compiler->function = NULL;
	compiler->type = type;
	init_local_array(&compiler->local_vars);
	compiler->scope_depth = 0;
	compiler->curr_loop = -1;
	compiler->curr_loop_depth = 0;
	compiler->in_switch = false;
	compiler->break_count = 0;
	init_table(&compiler->identifiers);
	compiler->function = new_function();
	current = compiler;
	if (type != TYPE_SCRIPT)
		current->function->name = copy_string(parser.previous.start, parser.previous.length);

	struct Local local = {.name.start = "", .name.length = 0, .depth = 0, .is_immutable = false, .is_captured = false};
	if (type != TYPE_FUNCTION) {
		local.name.start = "this";
		local.name.length = 4;
	}
	write_local_array(&current->local_vars, local);
}

static struct ObjFunction *end_compiler(void)
{
	emit_return();
	free_local_array(&current->local_vars);
	free_table(&current->identifiers);
	struct ObjFunction *function = current->function;

#ifdef DEBUG_PRINT_CODE
	if (!parser.had_error)
		disassemble_chunk(current_chunk(), function->name != NULL
				  ? function->name->chars : "<script>");
#endif
	current = current->enclosing;
	return function;
}

static void begin_scope(void)
{
	current->scope_depth++;
}

static void end_scope(void)
{
	current->scope_depth--;

	while (current->local_vars.count > 0 && \
	       current->local_vars.locals[current->local_vars.count - 1].depth \
			> current->scope_depth) {
		if (current->local_vars.locals[current->local_vars.count - 1].is_captured)
			emit_byte(OP_CLOSE_UPVALUE);
		else
			emit_byte(OP_POP);
		current->local_vars.count--;
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
	for (int i = compiler->local_vars.count - 1; i >= 0; i--) {
		struct Local *local = &compiler->local_vars.locals[i];

		if (identifiers_equal(name, &local->name)) {
			if (local->depth == -1)
				error("Can't read local variable in its own initializer.");
			return i;
		}
	}

	return -1;
}

static int add_upvalue(struct Compiler *compiler, uint8_t index, bool is_local)
{
	int upvalue_count = compiler->function->upvalue_count;

	for (int i = 0; i < upvalue_count; i++) {
		struct Upvalue *upvalue = &compiler->upvalues[i];
		if (upvalue->index == index && upvalue->is_local == is_local)
			return i;
	}

	if (upvalue_count == UINT8_COUNT) {
		error("Too many closure variables in function.");
		return 0;
	}

	compiler->upvalues[upvalue_count].is_local = is_local;
	compiler->upvalues[upvalue_count].index = index;
	return compiler->function->upvalue_count++;
}

static int resolve_upvalue(struct Compiler *compiler, struct Token *name)
{
	if (compiler->enclosing == NULL)
		return -1;

	int local = resolve_local(compiler->enclosing, name);
	if (local != -1) {
		compiler->enclosing->local_vars.locals[local].is_captured = true;
		return add_upvalue(compiler, (uint8_t)local, true);
	}

	int upvalue = resolve_upvalue(compiler->enclosing, name);
	if (upvalue != -1)
		return add_upvalue(compiler, (uint8_t)upvalue, false);

	return -1;
}

static void add_local(struct Token name, bool is_immutable)
{
	if (current->local_vars.count == UINT24_COUNT) {
		error("Too many local variables.");
		return;
	}

	write_local_array(&current->local_vars, \
			  (struct Local){.name = name, .depth = -1, .is_immutable = is_immutable, .is_captured = false});
}

static void declare_variable(bool is_immutable)
{
	if (current->scope_depth == 0)
		return;

	struct Token *name = &parser.previous;

	for (int i = current->local_vars.count - 1; i >= 0; i--) {
		struct Local *local = &current->local_vars.locals[i];

		if (local->depth != -1 && local->depth < current->scope_depth)
			break;

		if (identifiers_equal(name, &local->name))
			error("Already a variable with this name in this scope.");
	}

	add_local(*name, is_immutable);
}

static Value parse_variable(bool is_immutable, const char *error_message)
{
	consume(TOKEN_IDENTIFIER, error_message);

	declare_variable(is_immutable);

	return identifier_constant(&parser.previous);
}

static void mark_initialized(void)
{
	if (current->scope_depth == 0)
		return;
	current->local_vars.locals[current->local_vars.count - 1].depth = current->scope_depth;
}

static void define_variable(Value global, bool is_immutable)
{
	if (current->scope_depth > 0) {
		mark_initialized();
		return;
	}

	if (is_immutable)
		table_set(&vm.global_immutables, global, NIL_VAL);

	emit_global(OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG, global);
}

static uint8_t argument_list(void)
{
	uint8_t arg_count = 0;

	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			expression();
			if (arg_count == 255)
				error("Can't have more than 255 arguments.");
			arg_count++;
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
	return arg_count;
}

static void and_(bool can_assign)
{
	int end_jump = emit_jump(OP_JUMP_IF_FALSE);

	emit_byte(OP_POP);
	parse_precedence(PREC_AND);

	patch_jump(end_jump);
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

static void call(bool can_assign)
{
	uint8_t arg_count = argument_list();

	emit_bytes(OP_CALL, arg_count);
}

static void dot(bool can_assign)
{
	consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
	Value name = identifier_constant(&parser.previous);

	if (can_assign && match(TOKEN_EQUAL)) {
		expression();
		emit_constant(OP_SET_PROPERTY, OP_SET_PROPERTY_LONG, name);
	} else {
		emit_constant(OP_GET_PROPERTY, OP_GET_PROPERTY_LONG, name);
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
{  // Condition has already been compiled.
	int then_jump = emit_jump(OP_JUMP_IF_FALSE);
	struct ParseRule *rule = get_rule(parser.previous.type);

	emit_byte(OP_POP);  // Remove condition.
	parse_precedence((enum Precedence)(rule->precedence));

	int else_jump = emit_jump(OP_JUMP);

	patch_jump(then_jump);
	emit_byte(OP_POP);  // Remove condition.

	consume(TOKEN_COLON, "Expect ':' after '?' operator.");
	parse_precedence((enum Precedence)(rule->precedence));

	patch_jump(else_jump);
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

static void or_(bool can_assign)
{
	int else_jump = emit_jump(OP_JUMP_IF_FALSE);
	int end_jump = emit_jump(OP_JUMP);

	patch_jump(else_jump);
	emit_byte(OP_POP);

	parse_precedence(PREC_OR);
	patch_jump(end_jump);
}

static void string(bool can_assign)
{
	emit_constant(OP_CONSTANT, OP_CONSTANT_LONG, OBJ_VAL(copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

static void named_variable(struct Token name, bool can_assign)
{
	int local_index = resolve_local(current, &name);
	int upvalue_index;
	Value global;

	if (local_index == -1) {
		upvalue_index = resolve_upvalue(current, &name);
		if (upvalue_index == -1)
			global = identifier_constant(&name);
	}

	if (can_assign && match(TOKEN_EQUAL)) {
		expression();
		if (local_index != -1) {
			if (current->local_vars.locals[local_index].is_immutable)
				error("Can't assign to immutable variable.");
			write_constant_op(current_chunk(), OP_SET_LOCAL, OP_SET_LOCAL_LONG, \
					  local_index, parser.previous.line);
		} else if (upvalue_index != -1) {
			emit_bytes(OP_SET_UPVALUE, upvalue_index);
		} else {
			Value get_res;

			if (!table_get(&vm.global_immutables, global, &get_res))
				emit_global(OP_SET_GLOBAL, OP_SET_GLOBAL_LONG, global);
			else
				error("Can't assign to immutable variable.");
		}
	} else {
		if (local_index != -1)
			write_constant_op(current_chunk(), OP_GET_LOCAL, OP_GET_LOCAL_LONG, \
					  local_index, parser.previous.line);
		else if (upvalue_index != -1)
			emit_bytes(OP_GET_UPVALUE, upvalue_index);
		else
			emit_global(OP_GET_GLOBAL, OP_GET_GLOBAL_LONG, global);
	}
}

static void variable(bool can_assign)
{
	named_variable(parser.previous, can_assign);
}

static void this_(bool can_assign)
{
	if (current_class == NULL) {
		error("Can't use 'this' outside of a class.");
		return;
	}

	variable(false);
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
	[TOKEN_LEFT_PAREN]    = {grouping, call,    PREC_CALL},
	[TOKEN_RIGHT_PAREN]   = {NULL,     NULL,    PREC_NONE},
	[TOKEN_LEFT_BRACE]    = {NULL,     NULL,    PREC_NONE},
	[TOKEN_RIGHT_BRACE]   = {NULL,     NULL,    PREC_NONE},
	[TOKEN_PLUS]          = {NULL,     binary,  PREC_TERM},
	[TOKEN_MINUS]         = {unary,    binary,  PREC_TERM},
	[TOKEN_STAR]          = {NULL,     binary,  PREC_FACTOR},
	[TOKEN_SLASH]         = {NULL,     binary,  PREC_FACTOR},
	[TOKEN_COMMA]         = {NULL,     NULL,    PREC_NONE},
	[TOKEN_DOT]           = {NULL,     dot,     PREC_CALL},
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
	[TOKEN_AND]           = {NULL,     and_,    PREC_AND},
	[TOKEN_CLASS]         = {NULL,     NULL,    PREC_NONE},
	[TOKEN_ELSE]          = {NULL,     NULL,    PREC_NONE},
	[TOKEN_FALSE]         = {literal,  NULL,    PREC_NONE},
	[TOKEN_FOR]           = {NULL,     NULL,    PREC_NONE},
	[TOKEN_FUN]           = {NULL,     NULL,    PREC_NONE},
	[TOKEN_IF]            = {NULL,     NULL,    PREC_NONE},
	[TOKEN_NIL]           = {literal,  NULL,    PREC_NONE},
	[TOKEN_OR]            = {NULL,     or_,     PREC_OR},
	[TOKEN_PRINT]         = {NULL,     NULL,    PREC_NONE},
	[TOKEN_RETURN]        = {NULL,     NULL,    PREC_NONE},
	[TOKEN_SUPER]         = {NULL,     NULL,    PREC_NONE},
	[TOKEN_THIS]          = {this_,     NULL,    PREC_NONE},
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

static void function(enum FunctionType type)
{
	struct Compiler compiler;
	init_compiler(&compiler, type);
	begin_scope();

	consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			bool is_immutable = match(TOKEN_IMMUT) == true;
			current->function->arity++;
			if (current->function->arity > 255)
				error_at_current("Can't have more than 255 parameters.");
			Value param = parse_variable(is_immutable, "Expect parameter name.");
			define_variable(param, is_immutable);

		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
	consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	block();

	struct ObjFunction *function = end_compiler();
	int constant = add_constant(current_chunk(), OBJ_VAL(function));
	write_constant_op(current_chunk(), OP_CLOSURE, OP_CLOSURE_LONG, constant, parser.previous.line);

	for (int i = 0; i < function->upvalue_count; i++) {
		emit_byte(compiler.upvalues[i].is_local ? 1 : 0);
		emit_byte(compiler.upvalues[i].index);
	}
}

static void method(void)
{
	consume(TOKEN_IDENTIFIER, "Expect method name.");
	Value name = identifier_constant(&parser.previous);

	enum FunctionType type = TYPE_METHOD;
	function(type);
	emit_constant(OP_METHOD, OP_METHOD_LONG, name);
}

static void class_declaration(void)
{
	Value name = parse_variable(false, "Expect class name.");
	struct Token class_name = parser.previous;

	emit_constant(OP_CLASS, OP_CLASS_LONG, name);
	define_variable(name, false);

	struct ClassCompiler class_compiler;
	class_compiler.enclosing = current_class;
	current_class = &class_compiler;

	named_variable(class_name, false);
	consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
		method();
	consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
	emit_byte(OP_POP); // Pop off the class

	current_class = current_class->enclosing;
}

static void fun_declaration(void)
{
	Value global = parse_variable(false, "Expect function name.");

	mark_initialized();
	function(TYPE_FUNCTION);
	define_variable(global, false);
}

static void var_declaration(bool is_immutable)
{
	Value name  = parse_variable(is_immutable, "Expect variable name.");
	Value get_res;

	if (current->scope_depth == 0 && table_get(&vm.global_immutables, name, &get_res))
		error("Cannot redefine immutable variable.");

	if (match(TOKEN_EQUAL))
		expression();
	else
		emit_byte(OP_NIL);

	consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

	define_variable(name, is_immutable);
}

static void expression_statement(void)
{
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
	emit_byte(OP_POP);
}

static void for_statement(void)
{
	int loop_var_index = -1;  // Used for closures capturing the loop variable

	begin_scope();
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
	if (match(TOKEN_SEMICOLON)) {
		// No initializer.
	} else if (match(TOKEN_VAR)) {
		loop_var_index = current->local_vars.count;
		var_declaration(false);
	} else {
		expression_statement();
	}

	int loop_start = current_chunk()->count;
	int exit_jump = -1;

	if (!match(TOKEN_SEMICOLON)) {
		expression();
		consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

		// Jump out of the loop if the condition is false.
		exit_jump = emit_jump(OP_JUMP_IF_FALSE);
		emit_byte(OP_POP);  // Condition.
	}

	if (!match(TOKEN_RIGHT_PAREN)) {
		int body_jump = emit_jump(OP_JUMP);
		int increment_start = current_chunk()->count;

		expression();
		emit_byte(OP_POP);  // Discard increment expression's value.
		consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

		emit_loop(loop_start);

		loop_start = increment_start;
		patch_jump(body_jump);
	}
	int curr_break_count = current->break_count;
	int prev_loop = current->curr_loop;
	int prev_loop_depth = current->curr_loop_depth;

	current->curr_loop = loop_start;
	current->curr_loop_depth = current->scope_depth;

	if (loop_var_index != -1) {
		// Declare hidden variable shadowing the loop variable
		// initialized to the same value
		begin_scope();
		add_local(current->local_vars.locals[loop_var_index].name, \
			  current->local_vars.locals[loop_var_index].is_immutable);
		mark_initialized();
		write_constant_op(current_chunk(), OP_GET_LOCAL, OP_GET_LOCAL_LONG, \
				  loop_var_index, parser.previous.line);
	}

	// loop body
	statement();

	if (loop_var_index != -1) {
		// Set loop variable to value of hidden loop variable
		// The hidden loop variable will be at the top of the stack already
		// because the only way new variables might be declared above it,
		// would be within a block, whose scope would have ended by now.
		write_constant_op(current_chunk(), OP_SET_LOCAL, OP_SET_LOCAL_LONG, \
				  loop_var_index, parser.previous.line);
		end_scope();
	}

	emit_loop(loop_start);

	if (exit_jump != -1) {
		patch_jump(exit_jump);
		emit_byte(OP_POP);  // Condition.
	}
	for (int i = curr_break_count; i < current->break_count; i++)
		patch_jump(current->break_stmts[i]);
	current->break_count = curr_break_count;

	current->curr_loop = prev_loop;
	current->curr_loop_depth = prev_loop_depth;

	end_scope();
}

static void if_statement(void)
{
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int then_jump = emit_jump(OP_JUMP_IF_FALSE);

	emit_byte(OP_POP);
	statement();

	int else_jump = emit_jump(OP_JUMP);

	patch_jump(then_jump);
	emit_byte(OP_POP);

	if (match(TOKEN_ELSE))
		statement();
	patch_jump(else_jump);
}

static void print_statement(void)
{
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emit_byte(OP_PRINT);
}

static void return_statement(void)
{
	if (current->type == TYPE_SCRIPT)
		error("Can't return from top-level code.");

	if (match(TOKEN_SEMICOLON)) {
		emit_return();
	} else {
		expression();
		consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
		emit_byte(OP_RETURN);
	}
}

static void while_statement(void)
{
	int curr_break_count = current->break_count;
	int prev_loop = current->curr_loop;
	int prev_loop_depth = current->curr_loop_depth;
	int loop_start = current_chunk()->count;

	current->curr_loop = loop_start;
	current->curr_loop_depth = current->scope_depth;

	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int exit_jump = emit_jump(OP_JUMP_IF_FALSE);

	emit_byte(OP_POP);
	statement();
	emit_loop(loop_start);

	patch_jump(exit_jump);
	emit_byte(OP_POP);
	for (int i = curr_break_count; i < current->break_count; i++)
		patch_jump(current->break_stmts[i]);
	current->break_count = curr_break_count;

	current->curr_loop = prev_loop;
	current->curr_loop_depth = prev_loop_depth;
}

static void break_statement(void)
{
	consume(TOKEN_SEMICOLON, "Expect ';' after 'break'.");
	if (current->curr_loop < 0 && !current->in_switch)
		error("'break' statement outside of loop or switch.");
	if (current->break_count >= MAX_BREAK_COUNT) {
		error("Too many 'break' statements.");
		return;
	}

	for (int i = current->local_vars.count - 1;
	     i >= 0 && current->local_vars.locals[i].depth > current->curr_loop_depth;
	     i--)
		emit_byte(OP_POP);

	int break_jump = emit_jump(OP_JUMP);

	current->break_stmts[current->break_count++] = break_jump;
}

static void continue_statement(void)
{
	consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");
	if (current->curr_loop < 0)
		error("'continue' statement outside of loop.");

	for (int i = current->local_vars.count - 1;
	     i >= 0 && current->local_vars.locals[i].depth > current->curr_loop_depth;
	     i--)
		emit_byte(OP_POP);

	emit_loop(current->curr_loop);
}

static void switch_statement(void)
{
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int prev_switch = current->in_switch;
	int curr_break_count = current->break_count;

	current->in_switch = true;

	int fallthrough_jump = -1;

	consume(TOKEN_LEFT_BRACE, "Expect '{' at beginning of switch body.");
	while (match(TOKEN_CASE)) {
		expression();
		consume(TOKEN_COLON, "Expect ':' after switch-case.");

		emit_byte(OP_CASE_EQUAL);
		int case_jump = emit_jump(OP_JUMP_IF_FALSE);

		emit_byte(OP_POP);  // Remove case comparison.
		if (fallthrough_jump != -1)
			patch_jump(fallthrough_jump);

		statement();

		fallthrough_jump = emit_jump(OP_JUMP);

		patch_jump(case_jump);
		emit_byte(OP_POP);  // Remove case comparison.
	}
	if (fallthrough_jump != -1)
		patch_jump(fallthrough_jump);
	if (match(TOKEN_DEFAULT)) {
		consume(TOKEN_COLON, "Expect ':' after default switch-case.");

		statement();
	}
	consume(TOKEN_RIGHT_BRACE, "Expect '}' after switch body.");

	for (int i = curr_break_count; i < current->break_count; i++)
		patch_jump(current->break_stmts[i]);
	current->break_count = curr_break_count;

	emit_byte(OP_POP);  // Remove switch value.

	current->in_switch = prev_switch;
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
	if (match(TOKEN_CLASS))
		class_declaration();
	else if (match(TOKEN_FUN))
		fun_declaration();
	else if (match(TOKEN_VAR))
		var_declaration(false);
	else if (match(TOKEN_IMMUT))
		var_declaration(true);
	else
		statement();

	if (parser.panic_mode)
		synchronize();
}

static void statement(void)
{
	if (match(TOKEN_PRINT)) {
		print_statement();
	} else if (match(TOKEN_FOR)) {
		for_statement();
	} else if (match(TOKEN_IF)) {
		if_statement();
	} else if (match(TOKEN_RETURN)) {
		return_statement();
	} else if (match(TOKEN_WHILE)) {
		while_statement();
	} else if (match(TOKEN_SWITCH)) {
		switch_statement();
	} else if (match(TOKEN_BREAK)) {
		break_statement();
	} else if (match(TOKEN_CONTINUE)) {
		continue_statement();
	} else if (match(TOKEN_LEFT_BRACE)) {
		begin_scope();
		block();
		end_scope();
	} else {
		expression_statement();
	}
}

struct ObjFunction *compile(const char *source)
{
	init_scanner(source);

	struct Compiler compiler;

	init_compiler(&compiler, TYPE_SCRIPT);

	parser.had_error = false;
	parser.panic_mode = false;

	advance();

	while (!match(TOKEN_EOF))
		declaration();

	struct ObjFunction *function = end_compiler();
	return parser.had_error ? NULL : function;
}

void mark_compiler_roots(void)
{
	struct Compiler *compiler = current;

	while (compiler != NULL) {
		mark_object((struct Obj*)compiler->function);
		compiler = compiler->enclosing;
	}
}
