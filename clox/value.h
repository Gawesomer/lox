#ifndef clox_value_h
#define clox_value_h

#include "common.h"

enum ValueType {
	VAL_BOOL,
	VAL_NIL,
	VAL_NUMBER,
};

typedef struct {
	enum ValueType type;
	union {
		bool boolean;
		double number;
	} as;
} Value;

#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NIL(value)    ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define BOOL_VAL(value)   ((Value) {VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value) {VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value) {VAL_NUMBER, {.number = value}})

struct ValueArray {
	int count;
	int capacity;
	Value *values;
};

bool values_equal(Value a, Value b);
void init_value_array(struct ValueArray *array);
void free_value_array(struct ValueArray *array);
void write_value_array(struct ValueArray *array, Value value);
void print_value(Value value);

#endif
