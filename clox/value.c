#include <stdio.h>

#include "memory.h"
#include "value.h"

void init_value_array(struct ValueArray *array)
{
	array->count = 0;
	array->capacity = 0;
	array->values = NULL;
}

void free_value_array(struct ValueArray *array)
{
	FREE_ARRAY(Value, array->values, array->capacity);
	init_value_array(array);
}

void write_value_array(struct ValueArray *array, Value value)
{
	if (array->capacity < array->count + 1) {
		int old_capacity = array->capacity;

		array->capacity = GROW_CAPACITY(old_capacity);
		array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
	}

	array->values[array->count] = value;
	array->count++;
}

void print_value(Value value)
{
	printf("%g", value);
}
