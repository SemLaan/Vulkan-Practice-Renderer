#include "simplemap.h"

#include "core/asserts.h"
#include <string.h>

#define SIMPLEMAP_MAX_KEY_LEN 32


// The hash function used is djb2 by Dan Bernstein: http://www.cse.yorku.ca/~oz/hash.html
static inline u32 HashString_djb2(const char* str, u32 moduloValue)
{
    u32 hash = 5381;
    i32 c;

	// The bit shift trick works because bit shifting by 5 multiplies the number by 32, then adding the hash is the 33d time, bit shifting can be a fast way of multiplying by a power of two.
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash % moduloValue;
}

SimpleMap* SimpleMapCreate(Allocator* allocator, u32 maxEntries)
{
	// Allocating the struct and it's arrays.
    SimpleMap* map = Alloc(allocator, sizeof(*map));
    map->allocator = allocator;
    map->backingArraySize = maxEntries;

    map->keys = Alloc(allocator, sizeof(*map->keys) * maxEntries);
    map->values = Alloc(allocator, sizeof(*map->values) * maxEntries);
	MemoryZero(map->keys, sizeof(*map->keys) * maxEntries);
	MemoryZero(map->values, sizeof(*map->values) * maxEntries);

	// Creating a pool allocator for the key strings.
    CreatePoolAllocator("Simple Map keyPool", allocator, SIMPLEMAP_MAX_KEY_LEN, maxEntries, &map->keyPool, true);

	return map;
}

void SimpleMapDestroy(SimpleMap* map)
{
	GRASSERT_DEBUG(map && map->keyPool && map->keys && map->values);

	// Freeing all the memory associated with the struct.
	DestroyPoolAllocator(map->keyPool);
	Free(map->allocator, map->keys);
	Free(map->allocator, map->values);
	Free(map->allocator, map);
}

void SimpleMapInsert(SimpleMap* map, const char* key, void* value)
{
	GRASSERT_DEBUG(map && key && value && map->keyPool && map->keys && map->values);

	u32 hash = HashString_djb2(key, map->backingArraySize);

	// Finding the first empty spot in the array from the hash position
	u32 i = 0;
	while (map->values[hash] != nullptr)
	{
		GRASSERT_MSG(i <= map->backingArraySize, "Simple map backing array ran out of space or key not found");
		GRASSERT_MSG(0 != strncmp(key, map->keys[hash], SIMPLEMAP_MAX_KEY_LEN), "Key with name: check it in the debugger, already exists.");
		hash = (hash + 1) % map->backingArraySize;
		i++;
	}

	u32 keyStringLength = strlen(key);
	GRASSERT_DEBUG(keyStringLength < SIMPLEMAP_MAX_KEY_LEN);

	// Allocating memory for the key and storing it
	map->keys[hash] = Alloc(map->keyPool, SIMPLEMAP_MAX_KEY_LEN);
	MemoryZero(map->keys[hash], SIMPLEMAP_MAX_KEY_LEN);
	MemoryCopy(map->keys[hash], key, keyStringLength);

	// Storing the given item
	map->values[hash] = value;
}

void* SimpleMapLookup(SimpleMap* map, const char* key)
{
	GRASSERT_DEBUG(map && key && map->keyPool && map->keys && map->values);

	u32 hash = HashString_djb2(key, map->backingArraySize);

	u32 i = 0;
	while (map->keys[hash] && 0 != strncmp(key, map->keys[hash], SIMPLEMAP_MAX_KEY_LEN))
	{
		hash = (hash + 1) % map->backingArraySize;
		if (i > map->backingArraySize) // If the entire array was checked and the element wasn't found return nullptr
			return nullptr;
		i++;
	}

	// If the item doesn't exists nullptr should be stored in it's place already so we can 
	// just return the value because the function is supposed to return nullptr if the item doesn't exist.
	return map->values[hash];
}

void* SimpleMapDelete(SimpleMap* map, const char* key)
{
	GRASSERT_DEBUG(map && key && map->keyPool && map->keys && map->values);

	u32 hash = HashString_djb2(key, map->backingArraySize);

	// Find the item
	u32 i = 0;
	while (map->values[hash] != nullptr && 0 != strncmp(key, map->keys[hash], SIMPLEMAP_MAX_KEY_LEN))
	{
		GRASSERT_MSG(i <= map->backingArraySize, "Key not found");
		hash = (hash + 1) % map->backingArraySize;
		i++;
	}

	// Making sure the key exists in the map.
	GRASSERT(map->values[hash]);

	void* temp = map->values[hash];

	Free(map->keyPool, map->keys[hash]);
	map->keys[hash] = 0;
	map->values[hash] = 0;

	return temp;
}

void** SimpleMapGetBackingArrayRef(SimpleMap* map, u32* elementCount)
{
	GRASSERT_DEBUG(map && elementCount && map->keyPool && map->keys && map->values);
	*elementCount = map->backingArraySize;
	return map->values;	
}
