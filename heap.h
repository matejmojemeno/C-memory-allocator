#include "bitset.h"

const size_t MAX_BLOCK_SIZE = 64;

class Heap {
public:
    Heap() = default;

    Heap(uintptr_t *pool, size_t size);

private:
    int allocatedCnt = 0;

    Bitset bitset;

    size_t memSize;
    uintptr_t *memPool;
    uintptr_t *freeBlocks[MAX_BLOCK_SIZE];
};