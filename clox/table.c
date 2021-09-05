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

static struct Entry *find_entry(struct Entry *entries, int capacity, struct ObjString *key)
{
	uint32_t index = key->hash % capacity;
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

bool table_get(struct Table *table, struct ObjString *key, Value *value)
{
	if (table->count == 0)
		return false;

	struct Entry *entry = find_entry(table->entries, table->capacity, key);

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

		struct Entry *dest = find_entry(entries, capacity, entry->key);

		dest->key = entry->key;
		dest->value = entry->value;
		table->count++;
	}

	FREE_ARRAY(struct Entry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

bool table_set(struct Table *table, struct ObjString *key, Value value)
{
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);

		adjust_capacity(table, capacity);
	}

	struct Entry *entry = find_entry(table->entries, table->capacity, key);
	bool is_new_key = entry->key == NULL;

	if (is_new_key && IS_NIL(entry->value))
		table->count++;

	entry->key = key;
	entry->value = value;
	return is_new_key;
}

bool table_delete(struct Table *table, struct ObjString *key)
{
	if (table->count == 0)
		return false;

	// Find the entry.
	struct Entry *entry = find_entry(table->entries, table->capacity, key);

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
			table_set(to, entry->key, entry->value);
	}
}

struct ObjString *table_find_string(struct Table *table, const char *chars, int length, uint32_t hash)
{
	if (table->count == 0)
		return NULL;

	uint32_t index = hash % table->capacity;

	for (;;) {
		struct Entry *entry = &table->entries[index];

		if (entry->key == NULL) {
			// Stop if we find an empty non-tombstone entry.
			if (IS_NIL(entry->value))
				return NULL;
		} else if (entry->key->length == length && entry->key->hash == hash) {
			const char *key_chars = (entry->key->ptr == NULL) ? entry->key->chars : entry->key->ptr;

			if (memcmp(key_chars, chars, length) == 0)
				return entry->key;  // We found it.
		}

		index = (index + 1) % table->capacity;
	}
}
