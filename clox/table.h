#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

struct Entry {
	struct ObjString *key;
	Value value;
};

struct Table {
	int count;
	int capacity;
	struct Entry *entries;
};

void init_table(struct Table *table);
void free_table(struct Table *table);
bool table_get(struct Table *table, struct ObjString *key, Value *value);
bool table_set(struct Table *table, struct ObjString *key, Value value);
bool table_delete(struct Table *table, struct ObjString *key);
void table_add_all(struct Table *from, struct Table *to);

#endif
