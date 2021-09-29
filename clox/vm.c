#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
	vm.frame_count = 0;
	vm.open_upvalues = NULL;
}

static void runtime_error(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	for (int i = vm.frame_count - 1; i >= 0; i--) {
		struct CallFrame *frame = &vm.frames[i];
		struct ObjFunction *function = frame->closure->function;
		size_t instruction = frame->ip - function->chunk.code - 1;

		fprintf(stderr, "[line %d] in ", get_line(&function->chunk.lines, instruction));
		if (function->name == NULL)
			fprintf(stderr, "script\n");
		else
			fprintf(stderr, "%s()\n", function->name->chars);
	}

	reset_stack();
}

static bool clock_native(Value *args, Value *res)
{
	*res = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
	return true;
}

static bool chr_native(Value *args, Value *res)
{
	Value n = *args;

	if (!IS_NUMBER(n)) {
		runtime_error("chr: Argument must be a number.");
		return false;
	}

	char c = AS_NUMBER(n);
	*res = OBJ_VAL(copy_string(&c, 1));
	return true;
}

static bool int_native(Value *args, Value *res)
{
	Value n = *args;

	if (IS_NUMBER(n)) {
		*res = NUMBER_VAL((int)AS_NUMBER(n));
		return true;
	}

	if (IS_OBJ(n) && IS_STRING(n)) {
		struct ObjString *s = AS_STRING(n);
		if (s->length == 1) {
			*res = NUMBER_VAL((int)s->chars[0]);
			return true;
		}
	}

	runtime_error("int: Argument must be a number or character.");
	return false;
}

static bool readfile_native(Value *args, Value *res)
{
	Value n = *args;

	if (!(IS_OBJ(n) && IS_STRING(n))) {
		runtime_error("readfile: Argument must be a string.");
		return false;
	}

	char *path = AS_CSTRING(n);
	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		runtime_error("readfile: Could not open file \"%s\".", path);
		return false;
	}

	fseek(file, 0L, SEEK_END);
	size_t file_size = ftell(file);

	rewind(file);

	char *buffer = (char *)malloc(file_size);

	if (buffer == NULL) {
		runtime_error("readfile: Not enough memory to read \"%s\".\n", path);
		return false;
	}

	size_t bytes_read = fread(buffer, sizeof(char), file_size, file);

	if (bytes_read < file_size) {
		free(buffer);
		runtime_error("readfile: Could not read file \"%s\".\n", path);
		return false;
	}

	fclose(file);

	struct ObjString *s = copy_string(buffer, file_size);
	free(buffer);

	*res = OBJ_VAL(s);
	return true;
}

static bool writefile_native(Value *args, Value *res)
{
	if (!(IS_OBJ(args[0]) && IS_STRING(args[0]))) {
		runtime_error("writefile: First argument must be a string.");
		return false;
	} else if (!(IS_OBJ(args[1]) && IS_STRING(args[1]))) {
		runtime_error("writefile: Second argument must be a string.");
		return false;
	}

	char *path = AS_CSTRING(args[0]);
	FILE *file = fopen(path, "wb");

	if (file == NULL) {
		runtime_error("writefile: Could not open file \"%s\".", path);
		return false;
	}

	struct ObjString *s = AS_STRING(args[1]);
	size_t bytes_written = fwrite(s->chars, sizeof(char), s->length, file);

	if (bytes_written < s->length) {
		runtime_error("writefile: Could not write file \"%s\".\n", path);
		return false;
	}

	fclose(file);

	*res = NIL_VAL;
	return true;
}

static void define_native(const char *name, int arity, bool (*function)(Value*, Value*))
{
	push(OBJ_VAL(copy_string(name, (int)strlen(name))));
	push(OBJ_VAL(new_native(arity, function)));

	Value index = NUMBER_VAL(vm.global_values.count);
	write_value_array(&vm.global_values, vm.stack[1]);
	table_set(&vm.global_names, vm.stack[0], index);

	pop();
	pop();
}

void init_vm(void)
{
	reset_stack();
	vm.objects = NULL;
	init_table(&vm.strings);
	init_table(&vm.global_immutables);
	init_table(&vm.global_names);
	init_value_array(&vm.global_values);

	define_native("clock", 0, clock_native);
	define_native("chr", 1, chr_native);
	define_native("int", 1, int_native);
	define_native("readfile", 1, readfile_native);
	define_native("writefile", 2, writefile_native);
}

void free_vm(void)
{
	free_table(&vm.global_names);
	free_value_array(&vm.global_values);
	free_table(&vm.global_immutables);
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

static bool call(struct ObjClosure *closure, int arg_count)
{
	if (arg_count != closure->function->arity) {
		runtime_error("Expected %d arguments but got %d.", closure->function->arity, arg_count);
		return false;
	}

	if (vm.frame_count == FRAMES_MAX) {
		runtime_error("Stack overflow.");
		return false;
	}

	struct CallFrame *frame = &vm.frames[vm.frame_count++];

	frame->closure = closure;
	frame->ip = closure->function->chunk.code;
	frame->slots = vm.stack_top - arg_count - 1;
	return true;
}

static bool call_value(Value callee, int arg_count)
{
	if (IS_OBJ(callee)) {
		switch (OBJ_TYPE(callee)) {
		case OBJ_CLOSURE:
			return call(AS_CLOSURE(callee), arg_count);
		case OBJ_NATIVE: {
			int arity = ((struct ObjNative *)AS_OBJ(callee))->arity;
			if (arg_count != arity) {
				runtime_error("Expected %d arguments but got %d.", arity, arg_count);
				return false;
			}
			bool (*native)(Value*, Value*) = AS_NATIVE(callee);
			Value result;
			bool ret = native(vm.stack_top - arg_count, &result);
			if (ret == false)
				return false;
			vm.stack_top -= arg_count + 1;
			push(result);
			return true;
		}
		default:
			break;  // Non-callable object type.
		}
	}
	runtime_error("Can only call functions and classes.");
	return false;
}

static struct ObjUpvalue *capture_upvalue(Value *local)
{
	struct ObjUpvalue *prev_upvalue = NULL;
	struct ObjUpvalue *upvalue = vm.open_upvalues;

	while (upvalue != NULL && upvalue->location > local) {
		prev_upvalue = upvalue;
		upvalue = upvalue->next;
	}

	if (upvalue != NULL && upvalue->location == local)
		return upvalue;

	struct ObjUpvalue *created_upvalue = new_upvalue(local);
	created_upvalue->next = upvalue;

	if (prev_upvalue == NULL)
		vm.open_upvalues = created_upvalue;
	else
		prev_upvalue->next = created_upvalue;

	return created_upvalue;
}

static void close_upvalues(Value *last)
{
	while (vm.open_upvalues != NULL && vm.open_upvalues->location >= last) {
		struct ObjUpvalue *upvalue = vm.open_upvalues;
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		vm.open_upvalues = upvalue->next;
	}
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
	return *vm.frames[vm.frame_count - 1].ip++;
}

static uint16_t read_short(void)
{
	struct CallFrame *frame = &vm.frames[vm.frame_count - 1];

	frame->ip += 2;
	return ((frame->ip[-2] << 8) | frame->ip[-1]);
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
	struct CallFrame *frame = &vm.frames[vm.frame_count - 1];

	return frame->closure->function->chunk.constants.values[read_byte()];
}

static Value read_constant_long(void)
{
	struct CallFrame *frame = &vm.frames[vm.frame_count - 1];

	return frame->closure->function->chunk.constants.values[read_long()];
}

static enum InterpretResult run(void)
{
		struct CallFrame *frame = &vm.frames[vm.frame_count - 1];

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
		disassemble_instruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
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
		case OP_GET_LOCAL:
		case OP_GET_LOCAL_LONG: {
			uint32_t slot = (instruction == OP_GET_LOCAL) ? read_byte() : read_long();

			push(frame->slots[slot]);
			break;
		}
		case OP_SET_LOCAL:
		case OP_SET_LOCAL_LONG: {
			uint32_t slot = (instruction == OP_SET_LOCAL) ? read_byte() : read_long();

			frame->slots[slot] = peek(0);
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
		case OP_CASE_EQUAL: {
			Value case_val = pop();
			Value switch_val = peek(0);

			push(BOOL_VAL(values_equal(switch_val, case_val)));
			break;
		}
		case OP_GET_UPVALUE: {
			uint8_t slot = read_byte();
			push(*(frame->closure->upvalues[slot]->location));
			break;
		}
		case OP_SET_UPVALUE: {
			uint8_t slot = read_byte();
			*frame->closure->upvalues[slot]->location = peek(0);
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
		case OP_JUMP: {
			uint16_t offset = read_short();

			frame->ip += offset;
			break;
		}
		case OP_JUMP_IF_FALSE: {
			uint16_t offset = read_short();

			if (is_falsey(peek(0)))
				frame->ip += offset;
			break;
		}
		case OP_LOOP: {
			uint16_t offset = read_short();

			frame->ip -= offset;
			break;
		}
		case OP_CALL: {
			int arg_count = read_byte();

			if (!call_value(peek(arg_count), arg_count))
				return INTERPRET_RUNTIME_ERROR;
			frame = &vm.frames[vm.frame_count - 1];
			break;
		}
		case OP_CLOSURE:
		case OP_CLOSURE_LONG: {
			constant = (instruction == OP_CLOSURE) ? read_constant() : read_constant_long();
			struct ObjFunction *function = AS_FUNCTION(constant);
			struct ObjClosure *closure = new_closure(function);

			push(OBJ_VAL(closure));
			for (int i = 0; i < closure->upvalue_count; i++) {
				uint8_t is_local = read_byte();
				uint8_t index = read_byte();
				if (is_local)
					closure->upvalues[i] = capture_upvalue(frame->slots + index);
				else
					closure->upvalues[i] = frame->closure->upvalues[index];
			}
			break;
		}
		case OP_CLOSE_UPVALUE:
			close_upvalues(vm.stack_top - 1);
			pop();
			break;
		case OP_RETURN: {
			Value result = pop();
			close_upvalues(frame->slots);
			vm.frame_count--;
			if (vm.frame_count == 0) {
				pop();
				return INTERPRET_OK;
			}

			vm.stack_top = frame->slots;
			push(result);
			frame = &vm.frames[vm.frame_count - 1];
			break;
		}
		}
	}

#undef BINARY_OP
}

enum InterpretResult interpret(const char *source)
{
	struct ObjFunction *function = compile(source);

	if (function == NULL)
		return INTERPRET_COMPILE_ERROR;

	push(OBJ_VAL(function));
	struct ObjClosure *closure = new_closure(function);
	pop();
	push(OBJ_VAL(closure));
	call(closure, 0);

	return run();
}
