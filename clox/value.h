#ifndef clox_value_h
#define clox_value_h

#include <string.h>

#include "common.h"

struct Obj;
struct ObjString;

#ifdef NAN_BOXING

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN ((uint64_t)0x7ffc000000000000)

#define TAG_UNDEFINED 0 // 00
#define TAG_NIL       1 // 01
#define TAG_FALSE     2 // 10
#define TAG_TRUE      3 // 11

typedef uint64_t Value;

#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#define IS_NIL(value)       ((value) == NIL_VAL)
#define IS_UNDEFINED(value) ((value) == UNDEFINED_VAL)
#define IS_NUMBER(value)    (((value & QNAN) != QNAN))
#define IS_OBJ(value)							\
	(((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)   ((value) == TRUE_VAL)
#define AS_NUMBER(value) value_to_num(value)
#define AS_OBJ(value)							\
	((struct Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))
#define UNDEFINED_VAL   ((Value)(uint64_t)(QNAN | TAG_UNDEFINED))
#define NUMBER_VAL(num) num_to_value(num)
#define OBJ_VAL(obj)							\
	(Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)obj)

static inline double value_to_num(Value value)
{
	double num;
	memcpy(&num, &value, sizeof(Value));
	return num;
}

static inline Value num_to_value(double num)
{
	Value value;
	memcpy(&value, &num, sizeof(double));
	return value;
}

#else

enum ValueType {
	VAL_BOOL,
	VAL_NIL,
	VAL_NUMBER,
	VAL_OBJ,
	VAL_UNDEFINED,
};

typedef struct {
	enum ValueType type;
	union {
		bool boolean;
		double number;
		struct Obj *obj;
	} as;
} Value;

#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#define IS_OBJ(value)       ((value).type == VAL_OBJ)
#define IS_UNDEFINED(value) ((value).type == VAL_UNDEFINED)

#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value)    ((value).as.obj)

#define BOOL_VAL(value)   ((Value) {VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value) {VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value) {VAL_NUMBER, {.number = value}})
#define OBJ_VAL(value)    ((Value) {VAL_OBJ, {.obj = (struct Obj *)value}})
#define UNDEFINED_VAL     ((Value) {VAL_UNDEFINED, {.number = 0}})

#endif

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
