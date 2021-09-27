#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "line.h"
#include "value.h"

enum OpCode {
	OP_CONSTANT,
	OP_CONSTANT_LONG,
	OP_NIL,
	OP_TRUE,
	OP_FALSE,
	OP_POP,
	OP_GET_LOCAL,
	OP_GET_LOCAL_LONG,
	OP_SET_LOCAL,
	OP_SET_LOCAL_LONG,
	OP_GET_GLOBAL,
	OP_GET_GLOBAL_LONG,
	OP_DEFINE_GLOBAL,
	OP_DEFINE_GLOBAL_LONG,
	OP_SET_GLOBAL,
	OP_SET_GLOBAL_LONG,
	OP_CASE_EQUAL,
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_NOT,
	OP_NEGATE,
	OP_PRINT,
	OP_JUMP,
	OP_JUMP_IF_FALSE,
	OP_LOOP,
	OP_CLOSURE,
	OP_CLOSURE_LONG,
	OP_CALL,
	OP_RETURN,
};

struct Chunk {
	int count;
	int capacity;
	uint8_t *code;
	struct LineArray lines;
	struct ValueArray constants;
};

void init_chunk(struct Chunk *chunk);
void free_chunk(struct Chunk *chunk);
void write_chunk(struct Chunk *chunk, uint8_t byte, int line);
int add_constant(struct Chunk *chunk, Value value);
int write_constant_op(struct Chunk *chunk, enum OpCode op, enum OpCode op_long, int constant, int line);

#endif
