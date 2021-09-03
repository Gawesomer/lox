#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_STRING(value) ((struct ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) ((char *)AS_STRING(value)->chars)

enum ObjType {
	OBJ_STRING,
};

struct Obj {
	enum ObjType type;
};

struct ObjString {
	struct Obj obj;
	int length;
	char *chars;
};

struct ObjString *take_string(char *chars, int length);
struct ObjString *copy_string(const char *chars, int length);
void print_object(Value value);

static inline bool is_obj_type(Value value, enum ObjType type)
{
	return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif
