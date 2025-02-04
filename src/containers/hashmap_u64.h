#pragma once

#include "defines.h"
#include "core/meminc.h"
#include "containers/darray.h"

// ============================================= Hashmap explaination ===================================================
// The hashmap has a backing array that elements are put in, collisions are handled via linked lists.
// The elements that are in the backing array are stored in an array and the elements that are stored in linked list nodes are kept in a pool allocator
// that is managed by the hashmap internally




typedef u32 (*HashFunctionU64)(u64 key);

// https://gist.github.com/badboy/6267743#64-bit-to-32-bit-hash-functions
u32 Hash6432Shift(u64 key);


typedef struct MapEntryU64
{
    u64 key;                    // Key
    void* value;                // Value
    struct MapEntryU64* next;   // Next map entry in the linked list (see hashmap explaination)
} MapEntryU64;

DEFINE_DARRAY_TYPE_REF(MapEntryU64);

// Hashmap struct, client shouldn't touch internals
typedef struct HashmapU64
{
    HashFunctionU64 hashFunction;   // Function used to determine an elements hash
    Allocator* linkedEntryPool;     // Pool allocator for storing linked list elements
    Allocator* parentAllocator;     // Allocator used to allocate everything used by this hashmap
    MapEntryU64* backingArray;      // Array that map elements are stored in
    u32 backingArrayCapacity;       // Amount of elements that can be stored in the backing array
} HashmapU64;

// Creates a map, objects are to be kept track of outside of the hashmap
// note that the hashmap stores pointers to objects, so those pointers can not be invalidated
// or the hashmap will have outdated pointers
// backingArrayCapacity: amount of elements in the backing array
// maxCollisions: determines the size of linkedEntryPool, also determines how many collisions can happen before the map runs out of space to store collisions
HashmapU64* MapU64Create(Allocator* allocator, MemTag memtag, u32 backingArrayCapacity, u32 maxCollisions, HashFunctionU64 hashFunction);

// Destroys everything about the map, except the objects
void MapU64Destroy(HashmapU64* hashmap);

// Inserts item into map, asserts if the key is already in the map
void MapU64Insert(HashmapU64* hashmap, u64 key, void* value);

// Returns a void pointer to the found object or nullptr if the object wasn't found
void* MapU64Lookup(HashmapU64* hashmap, u64 key);

// Returns the deleted element, returns nullptr when the object isn't found
void* MapU64Delete(HashmapU64* hashmap, u64 key);

// Deletes every entry from the map, the client still owns all the value's in the map
void MapU64Flush(HashmapU64* hashmap);

// Returns a Darray made with the given allocator, this darray needs to be destroyed by the client of this function
Darray* MapU64GetValueRefDarray(HashmapU64* hashmap, Allocator* allocator);

// Returns a Darray made with the given allocator, this darray needs to be destroyed by the client of this function
MapEntryU64RefDarray* MapU64GetMapEntryRefDarray(HashmapU64* hashmap, Allocator* allocator);
