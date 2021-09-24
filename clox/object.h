#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_FUNCTION(value) ((struct ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value) (((struct ObjNative *)AS_OBJ(value))->function)
#define AS_STRING(value)  ((struct ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) ((char *)AS_STRING(value)->chars)

enum ObjType {
	OBJ_FUNCTION,
	OBJ_NATIVE,
	OBJ_STRING,
};

struct Obj {
	enum ObjType type;
	struct Obj *next;
};

struct ObjFunction {
	struct Obj obj;
	int arity;
	struct Chunk chunk;
	struct ObjString *name;
};

struct ObjNative {
	struct Obj obj;
	int arity;
	bool (*function)(Value *args, Value *result);
};

struct ObjString {
	struct Obj obj;
	int length;
	uint32_t hash;
	char chars[];
};

struct ObjFunction *new_function(void);
struct ObjNative *new_native(int arity, bool (*function)(Value*, Value*));
struct ObjString *copy_string(const char *chars, int length);
struct ObjString *concat_strings(struct ObjString *a, struct ObjString *b);
void print_object(Value value);

static inline bool is_obj_type(Value value, enum ObjType type)
{
	return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif
