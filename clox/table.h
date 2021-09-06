#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

struct Entry {
	void *key;
	uint32_t hash;
	Value value;
};

struct Table {
	int count;
	int capacity;
	struct Entry *entries;
};

void init_table(struct Table *table);
void free_table(struct Table *table);
uint32_t hash_bytes(const uint8_t *bytes, size_t size);
bool table_get(struct Table *table, void *key, uint32_t *hash, size_t size, Value *value);
bool table_set(struct Table *table, void *key, uint32_t *hash, size_t size, Value value);
bool table_delete(struct Table *table, void *key, uint32_t *hash, size_t size);
void table_add_all(struct Table *from, struct Table *to);
struct ObjString *table_find_string(struct Table *table, const char *chars, int length, uint32_t hash);

#endif
