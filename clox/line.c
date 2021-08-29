#include <stdio.h>

#include "memory.h"
#include "line.h"

void init_line_array(struct LineArray *array)
{
	array->count = 0;
	array->capacity = 0;
	array->lines = NULL;
}

void free_line_array(struct LineArray *array)
{
	FREE_ARRAY(int, array->lines, array->capacity);
	init_line_array(array);
}

void write_line_array(struct LineArray *array, int line)
{  // Using run-length encoding: [line_number, count, ...]
	if (array->count >= 2 && array->lines[array->count - 2] == line) {
		array->lines[array->count - 1]++;
		return;
	}

	if (array->capacity < array->count + 2) {
		int old_capacity = array->capacity;

		array->capacity = GROW_CAPACITY(old_capacity);
		array->lines = GROW_ARRAY(int, array->lines, old_capacity, array->capacity);
	}

	array->lines[array->count] = line;
	array->lines[array->count + 1] = 1;
	array->count += 2;
}

int get_line(struct LineArray *array, int offset)
{
	int curr = 0;

	for (int i = 1; i < array->count; i += 2) {
		curr += array->lines[i];
		if (curr > offset)
			return array->lines[i - 1];
	}
	return 0;
}
