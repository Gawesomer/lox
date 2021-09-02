#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

struct VM vm;

static void reset_stack(void)
{
	vm.stack_capacity = STACK_MIN;
	vm.stack = realloc(vm.stack, sizeof(Value) * vm.stack_capacity);
	if (vm.stack == NULL)
		exit(1);
	vm.stack_top = vm.stack;
}

void init_vm(void)
{
	reset_stack();
}

void free_vm(void)
{
	free(vm.stack);
}

void push(Value value)
{
	if (vm.stack_capacity <= (int)(vm.stack_top - vm.stack)) {
		int old_capacity = vm.stack_capacity;
		int top_offset = (int)(vm.stack_top - vm.stack);

		vm.stack_capacity = GROW_CAPACITY(old_capacity);
		vm.stack = GROW_ARRAY(Value, vm.stack, old_capacity, vm.stack_capacity);
		vm.stack_top = vm.stack + top_offset;
	}

	*vm.stack_top = value;
	vm.stack_top++;
}

Value pop(void)
{
	vm.stack_top--;
	return *vm.stack_top;
}

static Value READ_CONSTANT_LONG(void)
{
	uint32_t constant = 0;

	for (int i = 0; i < 3; i++) {
		constant <<= 8;
		constant += *vm.ip++;  // READ_BYTE()
	}

	return constant;
}

static enum InterpretResult run(void)
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op) \
	do { \
		double b = pop(); \
		double a = pop(); \
		push(a op b); \
	} while (false)

	for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
		printf("          ");
		for (Value *slot = vm.stack; slot < vm.stack_top; slot++) {
			printf("[ ");
			print_value(*slot);
			printf(" ]");
		}
		printf("\n");
		disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
		uint8_t instruction;
		Value constant;

		switch (instruction = READ_BYTE()) {
		case OP_CONSTANT:
			constant = READ_CONSTANT();
			push(constant);
			break;
		case OP_CONSTANT_LONG:
			constant = READ_CONSTANT_LONG();
			push(constant);
			break;
		case OP_ADD:
			BINARY_OP(+);
			break;
		case OP_SUBTRACT:
			BINARY_OP(-);
			break;
		case OP_MULTIPLY:
			BINARY_OP(*);
			break;
		case OP_DIVIDE:
			BINARY_OP(/);
			break;
		case OP_NEGATE:
			push(-pop());
			break;
		case OP_RETURN:
			print_value(pop());
			printf("\n");
			return INTERPRET_OK;
		}
	}

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

enum InterpretResult interpret(const char *source)
{
	struct Chunk chunk;

	init_chunk(&chunk);

	if (!compile(source, &chunk)) {
		free_chunk(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	vm.chunk = &chunk;
	vm.ip = vm.chunk->code;

	enum InterpretResult result = run();

	free_chunk(&chunk);
	return result;
}
