#include "hashmap_u64.h"

#include "core/asserts.h"
#include "darray.h"

// ====================================== Hash functions
// https://gist.github.com/badboy/6267743#64-bit-to-32-bit-hash-functions
u32 Hash6432Shift(u64 key)
{
  key = (~key) + (key << 18); // key = (key << 18) - key - 1;
  key = key ^ (key >> 31);
  key = key * 21; // key = (key + (key << 2)) + (key << 4);
  key = key ^ (key >> 11);
  key = key + (key << 6);
  key = key ^ (key >> 22);
  return (u32)key;
}


// ====================================== Hash map
HashmapU64* MapU64Create(Allocator* allocator, MemTag memtag, u32 backingArrayCapacity, u32 maxCollisions, HashFunctionU64 hashFunction)
{
    HashmapU64* hashmap = Alloc(allocator, sizeof(*hashmap) + backingArrayCapacity * sizeof(MapEntryU64), memtag);
    hashmap->backingArray = (MapEntryU64*)(hashmap + 1);
    hashmap->backingArrayCapacity = backingArrayCapacity;
    hashmap->hashFunction = hashFunction;
    CreatePoolAllocator("Map linked entry pool", allocator, sizeof(MapEntryU64), maxCollisions, &hashmap->linkedEntryPool);
    hashmap->parentAllocator = allocator;

    MemoryZero(hashmap->backingArray, sizeof(MapEntryU64) * backingArrayCapacity);

    return hashmap;
}

void MapU64Destroy(HashmapU64* hashmap)
{
    DestroyPoolAllocator(hashmap->linkedEntryPool);

    Free(hashmap->parentAllocator, hashmap);
}

void MapU64Insert(HashmapU64* hashmap, u64 key, void* value)
{
    u32 hash = hashmap->hashFunction(key) % hashmap->backingArrayCapacity;

    // Checking if the key isn't already in the map
    GRASSERT_DEBUG(MapU64Lookup(hashmap, key) == nullptr);

    MapEntryU64* currentEntry = &hashmap->backingArray[hash];

    while (nullptr != currentEntry->value)
    {
        if (currentEntry->next != nullptr)
        {
            currentEntry = currentEntry->next;
        }
        else
        {
            currentEntry->next = Alloc(hashmap->linkedEntryPool, sizeof(MapEntryU64), MEM_TAG_HASHMAP);
            MemoryZero(currentEntry->next, sizeof(MapEntryU64));
            currentEntry = currentEntry->next;
        }
    }

    currentEntry->key = key;
    currentEntry->value = value;
}

void* MapU64Lookup(HashmapU64* hashmap, u64 key)
{
    u32 hash = hashmap->hashFunction(key) % hashmap->backingArrayCapacity;

    MapEntryU64* currentEntry = &hashmap->backingArray[hash];

    while (true)
    {
        if (currentEntry->key == key)
        {
            return currentEntry->value;
        }
        else if (currentEntry->next == nullptr)
        {
            //GRDEBUG("HashmapU64: Tried to find item that doesn't exist, key: %llu", key);
            return nullptr;
        }
        else
        {
            currentEntry = currentEntry->next;
        }
    }
}

void* MapU64Delete(HashmapU64* hashmap, u64 key)
{
    u32 hash = hashmap->hashFunction(key) % hashmap->backingArrayCapacity;

    MapEntryU64* currentEntry = &hashmap->backingArray[hash];
    MapEntryU64* previousEntry = nullptr;

    while (true)
    {
        if (currentEntry->key == key)
        {
            void* returnValue = currentEntry->value;
            // If there's no previous entry, meaning that current entry is in the backing array
            if (previousEntry == nullptr && currentEntry->next != nullptr)
            {
                MemoryCopy(currentEntry, currentEntry->next, sizeof(*currentEntry));
            }
            else if (previousEntry == nullptr) // First entry and there is no next entry (next is nullptr)
            {
                MemoryZero(currentEntry, sizeof(*currentEntry));
            }
            else // if there is a previous entry, meaning current entry is in a linked list and NOT in the backing array
            {
                previousEntry->next = currentEntry->next;
                Free(hashmap->linkedEntryPool, currentEntry);
            }
            return returnValue;
        }
        else if (currentEntry->next == nullptr)
        {
            _WARN("HashmapU64: Tried to delete item that doesn't exist, key: %llu", key);
            return nullptr;
        }
        else
        {
            previousEntry = currentEntry;
            currentEntry = currentEntry->next;
        }
    }
}

void MapU64Flush(HashmapU64* hashmap)
{
    MemoryZero(hashmap->backingArray, sizeof(MapEntryU64) * hashmap->backingArrayCapacity);
    FlushPoolAllocator(hashmap->linkedEntryPool);
}

void** MapU64GetValueDarray(HashmapU64* hashmap, Allocator* allocator)
{
    // Assume we have at least a few elements in the hashmap, so starting the darray from zero would cause a lot of unnecessary resizes at the start
    #define ARBITRARY_DARRAY_START_CAPACITY 50

    void** valuesDarray = DarrayCreate(sizeof(*valuesDarray), ARBITRARY_DARRAY_START_CAPACITY, allocator, MEM_TAG_HASHMAP);

    for (u32 i = 0; i < hashmap->backingArrayCapacity; ++i)
    {
        MapEntryU64* item = hashmap->backingArray + i;
        
        if (item->value)
        {
            valuesDarray = DarrayPushback(valuesDarray, &item->value);

            while (item->next)
            {
                item = item->next;
                valuesDarray = DarrayPushback(valuesDarray, &item->value);
            }
        }
    }

    return valuesDarray;
}

MapEntryU64** MapU64GetMapEntryDarray(HashmapU64* hashmap, Allocator* allocator)
{
    MapEntryU64** entriesDarray = DarrayCreate(sizeof(*entriesDarray), ARBITRARY_DARRAY_START_CAPACITY, allocator, MEM_TAG_HASHMAP);

    for (u32 i = 0; i < hashmap->backingArrayCapacity; ++i)
    {
        MapEntryU64* item = hashmap->backingArray + i;
        
        if (item->value)
        {
            entriesDarray = DarrayPushback(entriesDarray, &item);

            while (item->next)
            {
                item = item->next;
                entriesDarray = DarrayPushback(entriesDarray, &item);
            }
        }
    }

    return entriesDarray;
}
