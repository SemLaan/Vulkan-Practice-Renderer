#include "simplemap.h"

#include "core/asserts.h"
#include <string.h>

#define SIMPLEMAP_MAX_KEY_LEN 32

// http://www.cse.yorku.ca/~oz/hash.html
u32 HashString_djb2(const char* str, u32 moduloValue)
{
    u32 hash = 5381;
    i32 c;

	// The bit shift trick works because bit shifting by 5 multiplies the number by 32, then adding the hash is the 33d time, bit shifting can be a fast way of multiplying by a power of two.
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash % moduloValue;
}

typedef struct SimpleMap
{
    Allocator* allocator;		// Allocator used to allocate this map.
    Allocator* keyPool;			// Pool for allocating memory to store key strings.
    char** keys;				// Array of pointers to key strings, the strings are owned by the key pool.
    void** values;				// Array of pointers to values, this struct does not own the pointers to values in this array.
    u32 backingArraySize;		// Size of the backing arrays and the key pool.
} SimpleMap;

SimpleMap* SimpleMapCreate(Allocator* allocator, u32 maxEntries)
{
	// Allocating the struct and it's arrays.
    SimpleMap* map = Alloc(allocator, sizeof(*map), MEM_TAG_HASHMAP);
    map->allocator = allocator;
    map->backingArraySize = maxEntries;

    map->keys = Alloc(allocator, sizeof(*map->keys) * maxEntries, MEM_TAG_HASHMAP);
    map->values = Alloc(allocator, sizeof(*map->values) * maxEntries, MEM_TAG_HASHMAP);
	MemoryZero(map->keys, sizeof(*map->keys) * maxEntries);
	MemoryZero(map->values, sizeof(*map->values) * maxEntries);

	// Creating a pool allocator for the key strings.
    CreatePoolAllocator("Simple Map keyPool", allocator, SIMPLEMAP_MAX_KEY_LEN, maxEntries, &map->keyPool);

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

	// Making sure that there are no collisions
	if (map->values[hash] != nullptr)
	{
		if (0 == strncmp(key, map->keys[hash], SIMPLEMAP_MAX_KEY_LEN))
			_ERROR("Key with name: \"%s\" already exists.", key);
		else
			_ERROR("Key with name: \"%s\" collides with key: \"%s\", simple map can't have collisions so change one of their names.", key, map->keys[hash]);

		GRASSERT(false);
	}

	u32 keyStringLength = strlen(key);
	GRASSERT_DEBUG(keyStringLength < SIMPLEMAP_MAX_KEY_LEN);

	// Allocating memory for the key and storing it
	map->keys[hash] = Alloc(map->keyPool, SIMPLEMAP_MAX_KEY_LEN, MEM_TAG_HASHMAP);
	MemoryZero(map->keys[hash], SIMPLEMAP_MAX_KEY_LEN);
	MemoryCopy(map->keys[hash], key, keyStringLength);

	// Storing the given item
	map->values[hash] = value;
}

void* SimpleMapLookup(SimpleMap* map, const char* key)
{
	GRASSERT_DEBUG(map && key && map->keyPool && map->keys && map->values);

	u32 hash = HashString_djb2(key, map->backingArraySize);

	// If the item doesn't exists nullptr should be stored in it's place already so we can 
	// just return the value because the function is supposed to return nullptr if the item doesn't exist.
	return map->values[hash];
}

void* SimpleMapDelete(SimpleMap* map, const char* key)
{
	GRASSERT_DEBUG(map && key && map->keyPool && map->keys && map->values);

	u32 hash = HashString_djb2(key, map->backingArraySize);

	// Making sure the key exists in the map.
	GRASSERT(map->values[hash] && 0 == strncmp(key, map->keys[hash], SIMPLEMAP_MAX_KEY_LEN));

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
