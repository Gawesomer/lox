#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

void disassemble_chunk(struct Chunk *chunk, const char *name);
int disassemble_instruction(struct Chunk *chunk, int offset);

#endif
