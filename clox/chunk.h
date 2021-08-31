#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "line.h"
#include "value.h"

enum OpCode {
	OP_CONSTANT,
	OP_CONSTANT_LONG,
	OP_NEGATE,
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
int write_constant(struct Chunk *chunk, Value value, int line);

#endif
