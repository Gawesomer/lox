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

static uint32_t hash_string(const char *key, int length)
{  // FNV-1a
	uint32_t hash = 2166136261u;

	for (int i = 0; i < length; i++) {
		hash ^= (uint8_t)key[i];
		hash *= 16777619;
	}
	return hash;
}

struct ObjString *const_string(const char *chars, int length)
{
	uint32_t hash = hash_string(chars, length);
	struct ObjString *interned = table_find_string(&vm.strings, chars, length, hash);

	if (interned != NULL)
		return interned;

	struct ObjString *string = ALLOCATE_OBJ(struct ObjString, OBJ_STRING);

	string->ptr = chars;
	string->length = length;
	string->hash = hash;
	table_set(&vm.strings, string, NIL_VAL);
	return string;
}

struct ObjString *concat_strings(struct ObjString *a, struct ObjString *b)
{
	int length = a->length + b->length;
	struct ObjString *string = (struct ObjString *)allocate_object(
					FLEX_ARR_STRUCT_SIZE(struct ObjString, char, length), OBJ_STRING);
	const char *a_chars = (a->ptr == NULL) ? a->chars : a->ptr;
	const char *b_chars = (b->ptr == NULL) ? b->chars : b->ptr;

	memcpy(string->chars, a_chars, a->length);
	memcpy(string->chars + a->length, b_chars, b->length);
	string->ptr = NULL;
	string->length = length;

	uint32_t hash = hash_string(string->chars, string->length);
	struct ObjString *interned = table_find_string(&vm.strings, string->chars, string->length, hash);

	if (interned != NULL) {
		vm.objects = string->obj.next;  // Make sure to remove from VM's object list since unused.
		FREE_SIZE(string, FLEX_ARR_STRUCT_SIZE(struct ObjString, char, string->length));
		return interned;
	}

	string->hash = hash_string(string->chars, string->length);
	table_set(&vm.strings, string, NIL_VAL);
	return string;
}

void print_object(Value value)
{
	switch (OBJ_TYPE(value)) {
	case OBJ_STRING: {
		struct ObjString *string = AS_STRING(value);
		const char *chars = (string->ptr == NULL) ? string->chars : string->ptr;

		printf("%.*s", string->length, chars);
		break;
	}
	}
}
