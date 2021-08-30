#include <stdio.h>

#include "common.h"
#include "vm.h"

struct VM vm;

void init_vm()
{

}

void free_vm()
{

}

static Value READ_CONSTANT_LONG()
{
	uint32_t constant = 0;

	for (int i = 0; i < 3; i++) {
		constant <<= 8;
		constant += *vm.ip++;  // READ_BYTE()
	}

	return constant;
}

static enum InterpretResult run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

	for (;;) {
		uint8_t instruction;
		Value constant;

		switch (instruction = READ_BYTE()) {
		case OP_CONSTANT:
			constant = READ_CONSTANT();
			print_value(constant);
			printf("\n");
			break;
		case OP_CONSTANT_LONG:
			constant = READ_CONSTANT_LONG();
			print_value(constant);
			printf("\n");
			break;
		case OP_RETURN:
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
