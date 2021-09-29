#ifndef clox_vm_h
#define clox_vm_h

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MIN 256

struct CallFrame {
	struct ObjClosure *closure;
	uint8_t *ip;
	Value *slots;
};

struct VM {
	struct CallFrame frames[FRAMES_MAX];
	int frame_count;

	Value *stack;
	Value *stack_top;
	int stack_capacity;
	struct Table global_immutables;
	struct Table global_names;
	struct ValueArray global_values;
	struct Table strings;
	struct ObjUpvalue *open_upvalues;
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
