#pragma once


// TODO: WARNING this whole thing isn't finished

#include "defines.h"
#include "core/meminc.h"

typedef u32 (*HashFunctionStr)(const char* key, u32 keyLength);


typedef struct MapEntryStr
{
    char* key;
    void* value;
    struct MapEntryStr* next;
    u32 keyLength;
} MapEntryStr;

typedef struct HashmapStr
{
    HashFunctionStr hashFunction;
    Allocator* linkedEntryPool;
    Allocator* keyPool;
    Allocator* parentAllocator;
    MapEntryStr* backingArray;
    u32 backingArrayElementCount;
    u32 maxKeyLength;
} HashmapStr;

// Creates a map, objects are to be kept track of outside of the hashmap
// note that the hashmap stores pointers to objects, so those pointers can not be invalidated
// or the hashmap has outdated pointers
// backingArrayElementCount: amount of elements in the backing array
HashmapStr* MapStrCreate(Allocator* allocator, MemTag memtag, u32 backingArrayElementCount, u32 maxCollisions, u32 maxKeyLength, HashFunctionStr hashFunction);

// Destroys everything about the map, except the objects
void MapStrDestroy(HashmapStr* hashmap);

// Inserts item into map, asserts if the key is already in the map
void MapStrInsert(HashmapStr* hashmap, const char* key, u32 keyLength, void* value);

// Returns a void pointer to the found object or nullptr if the object wasn't found
void* MapStrLookup(HashmapStr* hashmap, const char* key, u32 keyLength);

// Returns the deleted element
void* MapStrDelete(HashmapStr* hashmap, const char* key, u32 keyLength);