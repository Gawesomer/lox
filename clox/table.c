#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"

#define TABLE_MAX_LOAD 0.75

void init_table(struct Table *table)
{
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void free_table(struct Table *table)
{
	FREE_ARRAY(struct Entry, table->entries, table->capacity);
	init_table(table);
}

uint32_t hash_bytes(const uint8_t *bytes, size_t size)
{  // FNV-1a
	uint32_t hash = 2166136261u;

	for (int i = 0; i < size; i++) {
		hash ^= bytes[i];
		hash *= 16777619;
	}
	return hash;
}

static uint32_t hash_value(Value value)
{
#ifdef NAN_BOXING
	if (IS_BOOL(value))
		return AS_BOOL(value) ? 3 : 5;
	else if (IS_NIL(value))
		return hash_bytes(NULL, 0);
	else if (IS_NUMBER(value)) {
		const uint8_t v = AS_NUMBER(value);
		return hash_bytes(&v, sizeof(double));
	} else if (IS_OBJ(value))
		return AS_STRING(value)->hash;
	else
		return 0;
#else
	switch (value.type) {
	case VAL_BOOL:
		return AS_BOOL(value) ? 3 : 5;
	case VAL_NIL:
		return hash_bytes(NULL, 0);
	case VAL_NUMBER:
		return hash_bytes((const uint8_t *)&AS_NUMBER(value), sizeof(double));
	case VAL_OBJ:
		return AS_STRING(value)->hash;
	default:  // Unreachable.
		return 0;
	}
#endif
}

static struct Entry *find_entry(struct Entry *entries, int capacity, Value key)
{
	uint32_t index = hash_value(key) & (capacity - 1); // Fast mod power of two
	struct Entry *tombstone = NULL;

	for (;;) {
		struct Entry *entry = &entries[index];

		if (IS_NIL(entry->key)) {
			if (IS_NIL(entry->value)) {
				// Empty entry.
				return tombstone != NULL ? tombstone : entry;
			} else {
				// We found a tombstone.
				if (tombstone == NULL)
					tombstone = entry;
			}
		} else if (values_equal(entry->key, key)) {
			// We found the key.
			return entry;
		}

		index = (index + 1) & (capacity - 1);
	}
}

bool table_get(struct Table *table, Value key, Value *value)
{
	if (table->count == 0)
		return false;

	struct Entry *entry = find_entry(table->entries, table->capacity, key);

	if (IS_NIL(entry->key))
		return false;

	*value = entry->value;
	return true;
}

static void adjust_capacity(struct Table *table, int capacity)
{
	struct Entry *entries = ALLOCATE(struct Entry, capacity);

	for (int i = 0; i < capacity; i++) {
		entries[i].key = NIL_VAL;
		entries[i].value = NIL_VAL;
	}

	table->count = 0;
	for (int i = 0; i < table->capacity; i++) {
		struct Entry *entry = &table->entries[i];

		if (IS_NIL(entry->key))
			continue;

		struct Entry *dest = find_entry(entries, capacity, entry->key);

		dest->key = entry->key;
		dest->value = entry->value;
		table->count++;
	}

	FREE_ARRAY(struct Entry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

bool table_set(struct Table *table, Value key, Value value)
{
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);

		adjust_capacity(table, capacity);
	}

	struct Entry *entry = find_entry(table->entries, table->capacity, key);
	bool is_new_key = IS_NIL(entry->key);

	if (is_new_key && IS_NIL(entry->value))
		table->count++;

	entry->key = key;
	entry->value = value;
	return is_new_key;
}

bool table_delete(struct Table *table, Value key)
{
	if (table->count == 0)
		return false;

	// Find the entry.
	struct Entry *entry = find_entry(table->entries, table->capacity, key);

	if (IS_NIL(entry->key))
		return false;

	// Place a tombstone in the entry.
	entry->key = NIL_VAL;
	entry->value = BOOL_VAL(true);
	return true;
}

void table_add_all(struct Table *from, struct Table *to)
{
	for (int i = 0; i < from->capacity; i++) {
		struct Entry *entry = &from->entries[i];

		if (!IS_NIL(entry->key))
			table_set(to, entry->key, entry->value);
	}
}

struct ObjString *table_find_string(struct Table *table, const char *chars, int length, uint32_t hash)
{
	if (table->count == 0)
		return NULL;

	uint32_t index = hash & (table->capacity - 1);

	for (;;) {
		struct Entry *entry = &table->entries[index];

		if (IS_NIL(entry->key)) {
			// Stop if we find an empty non-tombstone entry.
			if (IS_NIL(entry->value))
				return NULL;
		} else {
			struct ObjString *key_str = AS_STRING(entry->key);

			if (key_str->length == length && key_str->hash == hash) {
				if (memcmp(key_str->chars, chars, length) == 0)
					return key_str;  // We found it.
			}
		}

		index = (index + 1) & (table->capacity - 1);
	}
}

void mark_table(struct Table *table)
{
	for (int i = 0; i < table->capacity; i++) {
		struct Entry *entry = &table->entries[i];
		mark_value(entry->key);
		mark_value(entry->value);
	}
}

void table_remove_white(struct Table *table)
{
	for (int i = 0; i < table->capacity; i++) {
		struct Entry *entry = &table->entries[i];
		if (IS_OBJ(entry->key) && !(AS_OBJ(entry->key))->is_marked)
			table_delete(table, entry->key);
	}
}
