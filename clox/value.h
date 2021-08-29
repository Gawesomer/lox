#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef double Value;

struct ValueArray {
	int count;
	int capacity;
	Value *values;
};

void init_value_array(struct ValueArray *array);
void free_value_array(struct ValueArray *array);
void write_value_array(struct ValueArray *array, Value value);
void print_value(Value value);

#endif
