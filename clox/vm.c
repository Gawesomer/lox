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
	vm.objects = NULL;
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
}

void free_vm(void)
{
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

static Value READ_CONSTANT_LONG(void)
{
	uint32_t offset = 0;

	for (int i = 0; i < 3; i++) {
		offset <<= 8;
		offset += *vm.ip++;  // READ_BYTE()
	}

	return vm.chunk->constants.values[offset];
}

static enum InterpretResult run(void)
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
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

		switch (instruction = READ_BYTE()) {
		case OP_CONSTANT:
			constant = READ_CONSTANT();
			push(constant);
			break;
		case OP_CONSTANT_LONG:
			constant = READ_CONSTANT_LONG();
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
