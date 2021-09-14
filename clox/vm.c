#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
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

static void runtime_error(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	size_t instruction = vm.ip - vm.chunk->code - 1;
	int line = get_line(&vm.chunk->lines, instruction);

	fprintf(stderr, "[line %d] in script\n", line);
	reset_stack();
}

void init_vm(void)
{
	reset_stack();
	vm.objects = NULL;
	init_table(&vm.strings);
	init_table(&vm.global_names);
	init_value_array(&vm.global_values);
}

void free_vm(void)
{
	free_table(&vm.global_names);
	free_value_array(&vm.global_values);
	free_table(&vm.strings);
	FREE_ARRAY(Value, vm.stack, vm.stack_capacity);
	free_objects();
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

static Value peek(int distance)
{
	return vm.stack_top[-1 - distance];
}

static bool is_falsey(Value value)
{
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(void)
{
	struct ObjString *b = AS_STRING(pop());
	struct ObjString *a = AS_STRING(pop());

	struct ObjString *result = concat_strings(a, b);

	push(OBJ_VAL(result));
}

static uint8_t read_byte(void)
{
	return *vm.ip++;
}

static uint32_t read_long(void)
{
	uint32_t offset = 0;

	for (int i = 0; i < 3; i++) {
		offset <<= 8;
		offset += read_byte();
	}

	return offset;
}

static Value read_constant(void)
{
	return vm.chunk->constants.values[read_byte()];
}

static Value read_constant_long(void)
{
	return vm.chunk->constants.values[read_long()];
}

static enum InterpretResult run(void)
{
#define BINARY_OP(value_type,  op) \
	do { \
		if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
			runtime_error("Operands must be numbers."); \
			return INTERPRET_RUNTIME_ERROR; \
		} \
		double b = AS_NUMBER(pop()); \
		double a = AS_NUMBER(pop()); \
		push(value_type(a op b)); \
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

		switch (instruction = read_byte()) {
		case OP_CONSTANT:
			constant = read_constant();
			push(constant);
			break;
		case OP_CONSTANT_LONG:
			constant = read_constant_long();
			push(constant);
			break;
		case OP_NIL:
			push(NIL_VAL);
			break;
		case OP_TRUE:
			push(BOOL_VAL(true));
			break;
		case OP_FALSE:
			push(BOOL_VAL(false));
			break;
		case OP_POP:
			pop();
			break;
		case OP_GET_LOCAL: {
			uint8_t slot = read_byte();

			push(vm.stack[slot]);
			break;
		}
		case OP_SET_LOCAL: {
			uint8_t slot = read_byte();

			vm.stack[slot] = peek(0);
			break;
		}
		case OP_GET_GLOBAL:
		case OP_GET_GLOBAL_LONG: {
			uint32_t index = (instruction == OP_GET_GLOBAL) ? read_byte() : read_long();
			Value value = vm.global_values.values[index];

			if (IS_UNDEFINED(value)) {
				runtime_error("Undefined variable.");
				return INTERPRET_RUNTIME_ERROR;
			}
			push(value);
			break;
		}
		case OP_DEFINE_GLOBAL:
		case OP_DEFINE_GLOBAL_LONG: {
			uint32_t index = (instruction == OP_DEFINE_GLOBAL) ? read_byte() : read_long();

			vm.global_values.values[index] = pop();
			break;
		}
		case OP_SET_GLOBAL:
		case OP_SET_GLOBAL_LONG: {
			uint32_t index = (instruction == OP_SET_GLOBAL) ? read_byte() : read_long();

			if (IS_UNDEFINED(vm.global_values.values[index])) {
				runtime_error("Undefined variable.");
				return INTERPRET_RUNTIME_ERROR;
			}
			vm.global_values.values[index] = peek(0);
			break;
		}
		case OP_EQUAL: {
			Value b = pop();
			Value a = pop();

			push(BOOL_VAL(values_equal(a, b)));
			break;
		}
		case OP_GREATER:
			BINARY_OP(BOOL_VAL, >);
			break;
		case OP_LESS:
			BINARY_OP(BOOL_VAL, <);
			break;
		case OP_ADD: {
			if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
				concatenate();
			} else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
				double b = AS_NUMBER(pop());
				double a = AS_NUMBER(pop());

				push(NUMBER_VAL(a + b));
			} else {
				runtime_error("Operands must be two numbers or two strings.");
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_SUBTRACT:
			BINARY_OP(NUMBER_VAL, -);
			break;
		case OP_MULTIPLY:
			BINARY_OP(NUMBER_VAL, *);
			break;
		case OP_DIVIDE:
			BINARY_OP(NUMBER_VAL, /);
			break;
		case OP_NOT:
			push(BOOL_VAL(is_falsey(pop())));
			break;
		case OP_NEGATE:
			if (!IS_NUMBER(peek(0))) {
				runtime_error("Operand must be a number.");
				return INTERPRET_RUNTIME_ERROR;
			}
			push(NUMBER_VAL(-AS_NUMBER(pop())));
			break;
		case OP_PRINT:
			print_value(pop());
			printf("\n");
			break;
		case OP_RETURN:
			// Exit interpreter.
			return INTERPRET_OK;
		}
	}

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
