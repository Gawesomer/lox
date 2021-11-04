#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLASS(value)    is_obj_type(value, OBJ_CLASS)
#define IS_CLOSURE(value)  is_obj_type(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_INSTANCE(value) is_obj_type(value, OBJ_INSTANCE)
#define IS_NATIVE(value)   is_obj_type(value, OBJ_NATIVE)
#define IS_STRING(value)   is_obj_type(value, OBJ_STRING)

#define AS_CLASS(value)    ((struct ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)  ((struct ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value) ((struct ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value) ((struct ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value)   (((struct ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)   ((struct ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)  ((char*)AS_STRING(value)->chars)

enum ObjType {
	OBJ_CLASS,
	OBJ_CLOSURE,
	OBJ_FUNCTION,
	OBJ_INSTANCE,
	OBJ_NATIVE,
	OBJ_STRING,
	OBJ_UPVALUE,
};

struct Obj {
	enum ObjType type;
	bool is_marked;
	struct Obj *next;
};

struct ObjFunction {
	struct Obj obj;
	int arity;
	int upvalue_count;
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

struct ObjUpvalue {
	struct Obj obj;
	Value *location;
	Value closed;
	struct ObjUpvalue *next;
};

struct ObjClosure {
	struct Obj obj;
	struct ObjFunction *function;
	struct ObjUpvalue **upvalues;
	int upvalue_count;
};

struct ObjClass {
	struct Obj obj;
	struct ObjString *name;
};

struct ObjInstance {
	struct Obj obj;
	struct ObjClass *klass;
	struct Table fields;
};

struct ObjClass *new_class(struct ObjString *name);
struct ObjClosure *new_closure(struct ObjFunction *function);
struct ObjFunction *new_function(void);
struct ObjInstance *new_instance(struct ObjClass *klass);
struct ObjNative *new_native(int arity, bool (*function)(Value*, Value*));
struct ObjString *copy_string(const char *chars, int length);
struct ObjString *concat_strings(struct ObjString *a, struct ObjString *b);
struct ObjUpvalue *new_upvalue(Value *slot);
void print_object(Value value);

static inline bool is_obj_type(Value value, enum ObjType type)
{
	return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif
