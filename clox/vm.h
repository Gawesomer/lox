#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"

struct VM {
	struct Chunk *chunk;
	uint8_t *ip;
};

enum InterpretResult {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR,
};

void init_vm();
void free_vm();
enum InterpretResult interpret(struct Chunk *chunk);

#endif
