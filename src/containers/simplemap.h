#pragma once
#include "defines.h"
#include "core/meminc.h"


/// @brief Struct with all the data the simple map holds. The collision strategy this hashmap uses is storing it in the first free element of the backing array from the hash index.
typedef struct SimpleMap
{
    Allocator* allocator;		// Allocator used to allocate this map.
    Allocator* keyPool;			// Pool for allocating memory to store key strings.
    char** keys;				// Array of pointers to key strings, the strings are owned by the key pool.
    void** values;				// Array of pointers to values, this struct does not own the pointers to values in this array.
    u32 backingArraySize;		// Size of the backing arrays and the key pool.
} SimpleMap;

/// @brief Creates a SimpleMap which is a map that uses strings as keys and pointers as values.
/// @param allocator Allocator used to create the map and backing arrays.
/// @param maxEntries Entries in the backing array, while it is called max entries this is a map and so filling all the entries is unlikely and shouldn't be expected.
/// @return A handle to the map. (SimpleMap*)
SimpleMap* SimpleMapCreate(Allocator* allocator, u32 maxEntries);

/// @brief Destroys the given map and it's backing arrays, the items in the map are not destroyed as they are owned by the client.
/// @param map Pointer to a SimpleMap to destroy.
void SimpleMapDestroy(SimpleMap* map);

/// @brief Inserts an item into the given simple map.
/// @param map Pointer to a SimpleMap to insert the item into.
/// @param key Key in the form of a string.
/// @param value The pointer to the item.
void SimpleMapInsert(SimpleMap* map, const char* key, void* value);

/// @brief Looks up the item corresponding to the given key in the given map. Returns nullptr if the element doesn't exist.
/// @param map Pointer to a SimpleMap to search.
/// @param key Key in the form of a string.
/// @return Pointer to the item corresponding to the given key. Or nullptr if the element doesn't exist
void* SimpleMapLookup(SimpleMap* map, const char* key);

/// @brief Removes the item corresponding to the given key from the given map, DOES NOT delete the actual item. Expects that the item existed in the first place.
/// @param map Pointer to a SimpleMap to remove the item from.
/// @param key Key in the form of a string.
/// @return Pointer to the deleted item.
void* SimpleMapDelete(SimpleMap* map, const char* key);

/// @brief Gets the backing array that stores the items from a SimpleMap. Can be used to iterate over all elements of the map, the map has nullptr's where there is no item.
/// The backing array that is returned is owned by the SimpleMap and destroyed when the simple map is.
/// @param map Pointer to a SimpleMap to get the backing array from.
/// @param elementCount Pointer to an integer that gets filled in with the size of the array.
/// @return void** to the array.
void** SimpleMapGetBackingArrayRef(SimpleMap* map, u32* elementCount);

