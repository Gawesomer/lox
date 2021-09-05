#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
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

struct ObjString *copy_string(const char *chars, int length)
{
	struct ObjString *string = (struct ObjString *)allocate_object(
					FLEX_ARR_STRUCT_SIZE(struct ObjString, char, length + 1), OBJ_STRING);

	memcpy(string->chars, chars, length);
	string->chars[length] = '\0';
	string->length = length;
	return string;
}

struct ObjString *copy_strings(const char *s1, int l1, const char *s2, int l2)
{
	int length = l1 + l2;
	struct ObjString *string = (struct ObjString *)allocate_object(
					FLEX_ARR_STRUCT_SIZE(struct ObjString, char, length + 1), OBJ_STRING);

	memcpy(string->chars, s1, l1);
	memcpy(string->chars + l1, s2, l2);
	string->chars[length] = '\0';
	string->length = length;
	return string;
}

void print_object(Value value)
{
	switch (OBJ_TYPE(value)) {
	case OBJ_STRING:
		printf("%s", AS_CSTRING(value));
		break;
	}
}
