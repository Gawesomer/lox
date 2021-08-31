#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "vm.h"

struct VM vm;

static void reset_stack(void)
{
	vm.stack_top = vm.stack;
}

void init_vm(void)
{
	reset_stack();
}

void free_vm(void)
{

}

void push(Value value)
{
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
		case OP_RETURN:
			print_value(pop());
			printf("\n");
			return INTERPRET_OK;
		}
	}

#undef READ_BYTE
#undef READ_CONSTANT
}

enum InterpretResult interpret(struct Chunk *chunk)
{
	vm.chunk = chunk;
	vm.ip = vm.chunk->code;
	return run();
}
