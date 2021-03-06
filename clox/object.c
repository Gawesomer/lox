#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, object_type) \
	((type *)allocate_object(sizeof(type), object_type))

static struct Obj *allocate_object(size_t size, enum ObjType type)
{
	struct Obj *object = (struct Obj *)reallocate(NULL, 0, size);

	object->type = type;
	object->is_marked = false;

	object->next = vm.objects;
	vm.objects = object;

#ifdef DEBUG_LOG_GC
	printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

	return object;
}

struct ObjBoundMethod *new_bound_method(Value receiver, struct ObjClosure *method)
{
	struct ObjBoundMethod *bound = ALLOCATE_OBJ(struct ObjBoundMethod, OBJ_BOUND_METHOD);

	bound->receiver = receiver;
	bound->method = method;
	return bound;
}

struct ObjClass *new_class(struct ObjString *name)
{
	struct ObjClass *klass = ALLOCATE_OBJ(struct ObjClass, OBJ_CLASS);
	init_table(&klass->methods);
	klass->name = name;
	return klass;
}

struct ObjClosure *new_closure(struct ObjFunction *function)
{
	struct ObjUpvalue **upvalues = ALLOCATE(struct ObjUpvalue*, function->upvalue_count);

	for (int i = 0; i < function->upvalue_count; i++)
		upvalues[i] = NULL;

	struct ObjClosure *closure = ALLOCATE_OBJ(struct ObjClosure, OBJ_CLOSURE);

	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalue_count = function->upvalue_count;
	return closure;
}

struct ObjFunction *new_function(void)
{
	struct ObjFunction *function = ALLOCATE_OBJ(struct ObjFunction, OBJ_FUNCTION);

	function->arity = 0;
	function->upvalue_count = 0;
	function->name = NULL;
	init_chunk(&function->chunk);
	return function;
}

struct ObjInstance *new_instance(struct ObjClass *klass)
{
	struct ObjInstance *instance = ALLOCATE_OBJ(struct ObjInstance, OBJ_INSTANCE);
	instance->klass = klass;
	init_table(&instance->fields);
	return instance;
}

struct ObjNative *new_native(int arity, bool (*function)(Value*, Value*))
{
	struct ObjNative *native = ALLOCATE_OBJ(struct ObjNative, OBJ_NATIVE);
	native->arity = arity;
	native->function = function;
	return native;
}

struct ObjString *copy_string(const char *chars, int length)
{
	uint32_t hash = hash_bytes((const uint8_t *)chars, length);
	struct ObjString *interned = table_find_string(&vm.strings, chars, length, hash);

	if (interned != NULL)
		return interned;

	struct ObjString *string = (struct ObjString *)allocate_object(
					FLEX_ARR_STRUCT_SIZE(struct ObjString, char, length + 1), OBJ_STRING);

	memcpy(string->chars, chars, length);
	string->chars[length] = '\0';
	string->length = length;
	string->hash = hash;

	Value string_value = OBJ_VAL(string);
	push(string_value);
	table_set(&vm.strings, string_value, NIL_VAL);
	pop();
	return string;
}

struct ObjString *concat_strings(struct ObjString *a, struct ObjString *b)
{
	int length = a->length + b->length;
	struct ObjString *string = (struct ObjString *)allocate_object(
					FLEX_ARR_STRUCT_SIZE(struct ObjString, char, length + 1), OBJ_STRING);

	memcpy(string->chars, a->chars, a->length);
	memcpy(string->chars + a->length, b->chars, b->length);
	string->chars[length] = '\0';
	string->length = length;

	uint32_t hash = hash_bytes((const uint8_t *)string->chars, string->length);
	struct ObjString *interned = table_find_string(&vm.strings, string->chars, string->length, hash);

	if (interned != NULL) {
		vm.objects = string->obj.next;  // Make sure to remove from VM's object list since unused.
		FREE_SIZE(string, FLEX_ARR_STRUCT_SIZE(struct ObjString, char, string->length + 1));
		return interned;
	}

	string->hash = hash;
	table_set(&vm.strings, OBJ_VAL(string), NIL_VAL);
	return string;
}

static void print_function(struct ObjFunction *function)
{
	if (function->name == NULL) {
		printf("<script>");
		return;
	}
	printf("<fn %s>", function->name->chars);
}

struct ObjUpvalue *new_upvalue(Value *slot)
{
	struct ObjUpvalue *upvalue = ALLOCATE_OBJ(struct ObjUpvalue, OBJ_UPVALUE);
	upvalue->closed = NIL_VAL;
	upvalue->location = slot;
	upvalue->next = NULL;
	return upvalue;
}

void print_object(Value value)
{
	switch (OBJ_TYPE(value)) {
	case OBJ_BOUND_METHOD:
		print_function(AS_BOUND_METHOD(value)->method->function);
		break;
	case OBJ_CLASS:
		printf("%s", AS_CLASS(value)->name->chars);
		break;
	case OBJ_CLOSURE:
		print_function(AS_CLOSURE(value)->function);
		break;
	case OBJ_FUNCTION:
		print_function(AS_FUNCTION(value));
		break;
	case OBJ_INSTANCE:
		printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
		break;
	case OBJ_NATIVE:
		printf("<native fn>");
		break;
	case OBJ_STRING:
		printf("%s", AS_CSTRING(value));
		break;
	case OBJ_UPVALUE:
		printf("upvalue");
		break;
	}
}
