#ifndef clox_line_h
#define clox_line_h

#include "common.h"

struct LineArray {
	int count;
	int capacity;
	int *lines;
};

void init_line_array(struct LineArray *array);
void free_line_array(struct LineArray *array);
void write_line_array(struct LineArray *array, int line);
int get_line(struct LineArray *array, int offset);

#endif
