#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"

enum OpCode {
	OP_RETURN,
};

struct Chunk {
	int count;
	int capacity;
	uint8_t *code;
};

void init_chunk(struct Chunk *chunk);
void free_chunk(struct Chunk *chunk);
void write_chunk(struct Chunk *chunk, uint8_t byte);

#endif
