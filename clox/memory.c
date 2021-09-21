#include <stdlib.h>

#include "memory.h"
#include "vm.h"

void *reallocate(void *pointer, size_t old_size, size_t new_size)
{
	if (new_size == 0) {
		free(pointer);
		return NULL;
	}

	void *result = realloc(pointer, new_size);

	if (result == NULL)
		exit(1);
	return result;

}

void free_object(struct Obj *object)
{
	switch (object->type) {
	case OBJ_FUNCTION: {
		struct ObjFunction *function = (struct ObjFunction *)object;
		free_chunk(&function->chunk);
		FREE(struct ObjFunction, object);
		break;
	}
	case OBJ_STRING: {
		struct ObjString *string = (struct ObjString *)object;

		FREE_SIZE(string, FLEX_ARR_STRUCT_SIZE(struct ObjString, char, string->length + 1));
		break;
	}
	}
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
