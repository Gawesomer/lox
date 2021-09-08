#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void init_chunk(struct Chunk *chunk)
{
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
	init_line_array(&chunk->lines);
	init_value_array(&chunk->constants);
}

void free_chunk(struct Chunk *chunk)
{
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	free_line_array(&chunk->lines);
	free_value_array(&chunk->constants);
	init_chunk(chunk);
}

void write_chunk(struct Chunk *chunk, uint8_t byte, int line)
{
	if (chunk->capacity < chunk->count + 1) {
		int old_capacity = chunk->capacity;

		chunk->capacity = GROW_CAPACITY(old_capacity);
		chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
	}

	chunk->code[chunk->count] = byte;
	write_line_array(&chunk->lines, line);
	chunk->count++;
}

int add_constant(struct Chunk *chunk, Value value)
{
	write_value_array(&chunk->constants, value);
	return chunk->constants.count - 1;
}

int write_constant_op(struct Chunk *chunk, enum OpCode op, enum OpCode op_long, Value value, int line)
{
	int constant = add_constant(chunk, value);

	if (constant < 256) {
		write_chunk(chunk, op, line);
		write_chunk(chunk, constant, line);
	} else {
		int constant_copy = constant;
		uint8_t constant_arr[3];

		for (int i = 2; i >= 0; i--) {
			constant_arr[i] = constant_copy;
			constant_copy >>= 8;
		}
		write_chunk(chunk, op_long, line);
		for (int i = 0; i < 3; i++)
			write_chunk(chunk, constant_arr[i], line);
	}
	return constant;
}
