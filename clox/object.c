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

	object->next = vm.objects;
	vm.objects = object;
	return object;
}

struct ObjFunction *new_function(void)
{
	struct ObjFunction *function = ALLOCATE_OBJ(struct ObjFunction, OBJ_FUNCTION);

	function->arity = 0;
	function->name = NULL;
	init_chunk(&function->chunk);
	return function;
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
	table_set(&vm.strings, OBJ_VAL(string), NIL_VAL);
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
	printf("<fn %s>", function->name->chars);
}

void print_object(Value value)
{
	switch (OBJ_TYPE(value)) {
	case OBJ_FUNCTION:
		print_function(AS_FUNCTION(value));
		break;
	case OBJ_STRING:
		printf("%s", AS_CSTRING(value));
		break;
	}
}
