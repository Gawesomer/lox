#include <stdlib.h>

#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

void *reallocate(void *pointer, size_t old_size, size_t new_size)
{
	if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
		collect_garbage();
#endif
	}
	if (new_size == 0) {
		free(pointer);
		return NULL;
	}

	void *result = realloc(pointer, new_size);

	if (result == NULL)
		exit(1);
	return result;

}

void mark_object(struct Obj *object)
{
	if (object == NULL)
		return;
#ifdef DEBUG_LOG_GC
	printf("%p mark ", (void*)object);
	print_value(OBJ_VAL(object));
	printf("\n");
#endif
	object->is_marked = true;
}

void mark_value(Value value)
{
	if (IS_OBJ(value))
		mark_object(AS_OBJ(value));
}

void free_object(struct Obj *object)
{
#ifdef DEBUG_LOG_GC
	printf("%p free type %d\n", (void*)object, object->type);
#endif
	switch (object->type) {
	case OBJ_CLOSURE: {
		struct ObjClosure *closure = (struct ObjClosure *)object;
		FREE_ARRAY(struct ObjValue*, closure->upvalues, closure->upvalue_count);
		FREE(struct ObjClosure, object);
		break;
	}
	case OBJ_FUNCTION: {
		struct ObjFunction *function = (struct ObjFunction *)object;
		free_chunk(&function->chunk);
		FREE(struct ObjFunction, object);
		break;
	}
	case OBJ_NATIVE:
		FREE(struct ObjNative, object);
		break;
	case OBJ_STRING: {
		struct ObjString *string = (struct ObjString *)object;

		FREE_SIZE(string, FLEX_ARR_STRUCT_SIZE(struct ObjString, char, string->length + 1));
		break;
	}
	case OBJ_UPVALUE: {
		FREE(struct ObjUpvalue, object);
		break;
	}
	}
}

static void mark_globals(void)
{
	for (int i = 0; i < vm.global_names.count; i++) {
		struct Entry entry = vm.global_names.entries[i];
		int global_index = AS_NUMBER(entry.value);
		mark_object(AS_OBJ(vm.global_values.values[global_index]));
		mark_value(entry.key);
	}
}

static void mark_roots(void)
{
	for (Value *slot = vm.stack; slot < vm.stack_top; slot++) {
		mark_value(*slot);
	}

	mark_globals();
}

void collect_garbage(void)
{
#ifdef DEBUG_LOG_GC
	printf("-- gc begin\n");
#endif

	mark_roots();

#ifdef DEBUG_LOG_GC
	printf("-- gc end\n");
#endif
}

void free_objects(void)
{
	struct Obj *object = vm.objects;

	while (object != NULL) {
		struct Obj *next = object->next;

		free_object(object);
		object = next;
	}
}
