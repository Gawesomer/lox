#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_MIN 256

struct VM {
	struct Chunk *chunk;
	uint8_t *ip;
	Value *stack;
	Value *stack_top;
	int stack_capacity;
	struct Table global_immutables;
	struct Table global_names;
	struct ValueArray global_values;
	struct Table strings;
	struct Obj *objects;
};

enum InterpretResult {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR,
};

extern struct VM vm;

void init_vm(void);
void free_vm(void);
enum InterpretResult interpret(const char *source);
void push(Value value);
Value pop(void);

#endif
