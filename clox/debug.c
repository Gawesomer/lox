#include <stdio.h>

#include "debug.h"
#include "line.h"
#include "object.h"
#include "value.h"
#include "vm.h"

void disassemble_chunk(struct Chunk *chunk, const char *name)
{
	printf("== %s ==\n", name);

	for (int offset = 0; offset < chunk->count;)
		offset = disassemble_instruction(chunk, offset);
}

static int constant_instruction(const char *name, struct Chunk *chunk, int offset)
{
	uint8_t constant = chunk->code[offset + 1];

	printf("%-16s %4d '", name, constant);
	print_value(chunk->constants.values[constant]);
	printf("'\n");
	return offset + 2;
}

static int constant_long_instruction(const char *name, struct Chunk *chunk, int offset)
{
	uint32_t constant = 0;

	for (int i = 1; i <= 3; i++) {
		constant <<= 8;
		constant += chunk->code[offset + i];
	}

	printf("%-16s %4d '", name, constant);
	print_value(chunk->constants.values[constant]);
	printf("'\n");
	return offset + 4;
}

static int global_instruction(const char *name, struct Chunk *chunk, int offset)
{
	uint8_t index = chunk->code[offset + 1];

	printf("%-16s %4d '", name, index);
	print_value(vm.global_values.values[index]);
	printf("'\n");
	return offset + 2;
}

static int global_long_instruction(const char *name, struct Chunk *chunk, int offset)
{
	uint32_t index = 0;

	for (int i = 1; i <= 3; i++) {
		index <<= 8;
		index += chunk->code[offset + i];
	}

	printf("%-16s %4d '", name, index);
	print_value(vm.global_values.values[index]);
	printf("'\n");
	return offset + 4;
}

static int byte_instruction(const char *name, struct Chunk *chunk, int offset)
{
	uint8_t slot = chunk->code[offset + 1];

	printf("%-16s %4d\n", name, slot);
	return offset + 2;
}

static int jump_instruction(const char *name, int sign, struct Chunk *chunk, int offset)
{
	uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);

	jump |= chunk->code[offset + 2];
	printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
	return offset + 3;
}

static int byte_long_instruction(const char *name, struct Chunk *chunk, int offset)
{
	uint32_t slot = 0;

	for (int i = 1; i <= 3; i++) {
		slot <<= 8;
		slot += chunk->code[offset + i];
	}

	printf("%-16s %4d\n", name, slot);
	return offset + 4;
}

static int simple_instruction(const char *name, int offset)
{
	printf("%s\n", name);
	return offset + 1;
}

static int invoke_instruction(const char *name, struct Chunk *chunk, int offset)
{
	uint8_t constant = chunk->code[offset + 1];
	uint8_t arg_count = chunk->code[offset + 2];
	printf("%-16s {%d args) %4d '", name, arg_count, constant);
	print_value(chunk->constants.values[constant]);
	printf("'\n");
	return offset + 3;
}

static int invoke_long_instruction(const char *name, struct Chunk *chunk, int offset)
{
	uint32_t constant = 0;

	for (int i = 1; i <= 3; i++) {
		constant <<= 8;
		constant += chunk->code[offset + i];
	}

	uint8_t arg_count = chunk->code[offset + 4];
	printf("%-16s {%d args) %4d '", name, arg_count, constant);
	print_value(chunk->constants.values[constant]);
	printf("'\n");
	return offset + 5;
}

int disassemble_instruction(struct Chunk *chunk, int offset)
{  // offset line_num name constant value
	int line_num = get_line(&chunk->lines, offset);

	printf("%04d ", offset);

	if (offset > 0 && line_num == get_line(&chunk->lines, offset - 1))
		printf("   | ");
	else
		printf("%4d ", line_num);

	uint8_t instruction = chunk->code[offset];

	switch (instruction) {
	case OP_CONSTANT:
		return constant_instruction("OP_CONSTANT", chunk, offset);
	case OP_CONSTANT_LONG:
		return constant_long_instruction("OP_CONSTANT_LONG", chunk, offset);
	case OP_NIL:
		return simple_instruction("OP_NIL", offset);
	case OP_TRUE:
		return simple_instruction("OP_TRUE", offset);
	case OP_FALSE:
		return simple_instruction("OP_FALSE", offset);
	case OP_POP:
		return simple_instruction("OP_POP", offset);
	case OP_GET_LOCAL:
		return byte_instruction("OP_GET_LOCAL", chunk, offset);
	case OP_GET_LOCAL_LONG:
		return byte_long_instruction("OP_GET_LOCAL_LONG", chunk, offset);
	case OP_SET_LOCAL:
		return byte_instruction("OP_SET_LOCAL", chunk, offset);
	case OP_SET_LOCAL_LONG:
		return byte_long_instruction("OP_SET_LOCAL_LONG", chunk, offset);
	case OP_GET_GLOBAL:
		return global_instruction("OP_GET_GLOBAL", chunk, offset);
	case OP_GET_GLOBAL_LONG:
		return global_instruction("OP_GET_GLOBAL_LONG", chunk, offset);
	case OP_DEFINE_GLOBAL:
		return global_instruction("OP_DEFINE_GLOBAL", chunk, offset);
	case OP_DEFINE_GLOBAL_LONG:
		return global_long_instruction("OP_DEFINE_GLOBAL_LONG", chunk, offset);
	case OP_SET_GLOBAL:
		return global_instruction("OP_SET_GLOBAL", chunk, offset);
	case OP_SET_GLOBAL_LONG:
		return global_long_instruction("OP_SET_GLOBAL_LONG", chunk, offset);
	case OP_GET_PROPERTY:
		return constant_instruction("OP_GET_PROPERTY", chunk, offset);
	case OP_GET_PROPERTY_LONG:
		return constant_long_instruction("OP_GET_PROPERTY_LONG", chunk, offset);
	case OP_SET_PROPERTY:
		return constant_instruction("OP_SET_PROPERTY", chunk, offset);
	case OP_SET_PROPERTY_LONG:
		return constant_long_instruction("OP_SET_PROPERTY_LONG", chunk, offset);
	case OP_GET_SUPER:
		return constant_instruction("OP_GET_SUPER", chunk, offset);
	case OP_GET_SUPER_LONG:
		return constant_long_instruction("OP_GET_SUPER_LONG", chunk, offset);
	case OP_CASE_EQUAL:
		return simple_instruction("OP_CASE_EQUAL", offset);
	case OP_EQUAL:
		return simple_instruction("OP_EQUAL", offset);
	case OP_GET_UPVALUE:
		return byte_instruction("OP_GET_UPVALUE", chunk, offset);
	case OP_SET_UPVALUE:
		return byte_instruction("OP_SET_UPVALUE", chunk, offset);
	case OP_GREATER:
		return simple_instruction("OP_GREATER", offset);
	case OP_LESS:
		return simple_instruction("OP_LESS", offset);
	case OP_ADD:
		return simple_instruction("OP_ADD", offset);
	case OP_SUBTRACT:
		return simple_instruction("OP_SUBTRACT", offset);
	case OP_MULTIPLY:
		return simple_instruction("OP_MULTIPLY", offset);
	case OP_DIVIDE:
		return simple_instruction("OP_DIVIDE", offset);
	case OP_NOT:
		return simple_instruction("OP_NOT", offset);
	case OP_NEGATE:
		return simple_instruction("OP_NEGATE", offset);
	case OP_PRINT:
		return simple_instruction("OP_PRINT", offset);
	case OP_JUMP:
		return jump_instruction("OP_JUMP", 1, chunk, offset);
	case OP_JUMP_IF_FALSE:
		return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
	case OP_LOOP:
		return jump_instruction("OP_LOOP", -1, chunk, offset);
	case OP_CLOSURE:
	case OP_CLOSURE_LONG: {
		uint32_t constant = 0;

		offset++;
		if (instruction == OP_CLOSURE) {
			constant = chunk->code[offset++];
			printf("%-16s %4d ", "OP_CLOSURE", constant);
		} else {
			for (int i = 1; i < 3; i++) {
				constant <<= 8;
				constant += chunk->code[offset++];
			}
			printf("%-16s %4d ", "OP_CLOSURE_LONG", constant);
		}
		print_value(chunk->constants.values[constant]);
		printf("\n");

		struct ObjFunction *function = AS_FUNCTION(chunk->constants.values[constant]);
		for (int j = 0; j < function->upvalue_count; j++) {
			int is_local = chunk->code[offset++];
			int index = chunk->code[offset++];
			printf("%04d      |                     %s %d\n", offset - 2, is_local ? "local" : "upvalue", index);
		}

		return offset;
	}
	case OP_CALL:
		return byte_instruction("OP_CALL", chunk, offset);
	case OP_INVOKE:
		return invoke_instruction("OP_INVOKE", chunk, offset);
	case OP_INVOKE_LONG:
		return invoke_long_instruction("OP_INVOKE_LONG", chunk, offset);
	case OP_CLOSE_UPVALUE:
		return simple_instruction("OP_CLOSE_UPVALUE", offset);
	case OP_RETURN:
		return simple_instruction("OP_RETURN", offset);
	case OP_CLASS:
		return constant_instruction("OP_CLASS", chunk, offset);
	case OP_CLASS_LONG:
		return constant_long_instruction("OP_CLASS_LONG", chunk, offset);
	case OP_INHERIT:
		return simple_instruction("OP_INHERIT", offset);
	case OP_METHOD:
		return constant_instruction("OP_METHOD", chunk, offset);
	case OP_METHOD_LONG:
		return constant_long_instruction("OP_METHOD_LONG", chunk, offset);
	default:
		printf("Unknown opcode %d\n", instruction);
		return offset + 1;
	}
}
