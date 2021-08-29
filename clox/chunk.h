#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

enum OpCode {
	OP_CONSTANT,
	OP_RETURN,
};

struct Chunk {
	int count;
	int capacity;
	uint8_t *code;
	int *lines;
	struct ValueArray constants;
};

void init_chunk(struct Chunk *chunk);
void free_chunk(struct Chunk *chunk);
void write_chunk(struct Chunk *chunk, uint8_t byte, int line);
int add_constant(struct Chunk *chunk, Value value);

#endif
