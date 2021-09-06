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

static struct Entry *find_entry(struct Entry *entries, int capacity, void *key, uint32_t *hash, size_t size)
{
	uint32_t index;

	index = (hash == NULL) ? hash_bytes(key, size) : *hash;
	index %= capacity;

	struct Entry *tombstone = NULL;

	for (;;) {
		struct Entry *entry = &entries[index];

		if (entry->key == NULL) {
			if (IS_NIL(entry->value)) {
				// Empty entry.
				return tombstone != NULL ? tombstone : entry;
			} else {
				// We found a tombstone.
				if (tombstone == NULL)
					tombstone = entry;
			}
		} else if (entry->key == key) {
			// We found the key.
			return entry;
		}

		index = (index + 1) % capacity;
	}
}

bool table_get(struct Table *table, void *key, uint32_t *hash, size_t size, Value *value)
{
	if (table->count == 0)
		return false;

	struct Entry *entry = find_entry(table->entries, table->capacity, key, hash, size);

	if (entry->key == NULL)
		return false;

	*value = entry->value;
	return true;
}

static void adjust_capacity(struct Table *table, int capacity)
{
	struct Entry *entries = ALLOCATE(struct Entry, capacity);

	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}

	table->count = 0;
	for (int i = 0; i < table->capacity; i++) {
		struct Entry *entry = &table->entries[i];

		if (entry->key == NULL)
			continue;

		struct Entry *dest = find_entry(entries, capacity, entry->key, &entry->hash, 0);

		dest->key = entry->key;
		dest->hash = entry->hash;
		dest->value = entry->value;
		table->count++;
	}

	FREE_ARRAY(struct Entry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

bool table_set(struct Table *table, void *key, uint32_t *hash, size_t size, Value value)
{
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);

		adjust_capacity(table, capacity);
	}

	uint32_t computed_hash = (hash == NULL) ? hash_bytes(key, size) : *hash;
	struct Entry *entry = find_entry(table->entries, table->capacity, key, &computed_hash, 0);
	bool is_new_key = entry->key == NULL;

	if (is_new_key && IS_NIL(entry->value))
		table->count++;

	entry->key = key;
	entry->hash = computed_hash;
	entry->value = value;
	return is_new_key;
}

bool table_delete(struct Table *table, void *key, uint32_t *hash, size_t size)
{
	if (table->count == 0)
		return false;

	// Find the entry.
	struct Entry *entry = find_entry(table->entries, table->capacity, key, hash, size);

	if (entry->key == NULL)
		return false;

	// Place a tombstone in the entry.
	entry->key = NULL;
	entry->value = BOOL_VAL(true);
	return true;
}

void table_add_all(struct Table *from, struct Table *to)
{
	for (int i = 0; i < from->capacity; i++) {
		struct Entry *entry = &from->entries[i];

		if (entry->key != NULL)
			table_set(to, entry->key, &entry->hash, 0, entry->value);
	}
}

struct ObjString *table_find_string(struct Table *table, const char *chars, int length, uint32_t hash)
{
	if (table->count == 0)
		return NULL;

	uint32_t index = hash % table->capacity;

	for (;;) {
		struct Entry *entry = &table->entries[index];
		struct ObjString *key_str = entry->key;

		if (entry->key == NULL) {
			// Stop if we find an empty non-tombstone entry.
			if (IS_NIL(entry->value))
				return NULL;
		} else if (key_str->length == length && entry->hash == hash) {
			const char *key_chars = (key_str->ptr == NULL) ? key_str->chars : key_str->ptr;

			if (memcmp(key_chars, chars, length) == 0)
				return key_str;  // We found it.
		}

		index = (index + 1) % table->capacity;
	}
}
