#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>

using namespace std;

class Bitset {
public:
    Bitset() = default;

    Bitset(uint8_t *pool, size_t size) : bitset(pool) {
        for (size_t i = 0; i < size / 257; i++)
            bitset[i] = 0;
    }

    void allocate(size_t block) {
        bitset[block / 8] = bitset[block / 8] | (1 << (block % 8));
    }

    void deallocate(size_t block) {
        bitset[block / 8] = bitset[block / 8] ^ (1 << (block % 8));
    }

    bool operator[](size_t block) const {
        return (bool) (bitset[block / 8] & (1 << (block % 8)));
    }

private:
    uint8_t *bitset;
};


class Heap {
public:
    Heap() = default;

    Heap(uintptr_t *pool, size_t size) : memSize(size), memPool(pool), bitset((uint8_t *) pool, memSize) {
        memPool += memSize / 257 / 8 + 1;
        memSize -= memSize % 257 + memSize / 257;

        for (auto &freeBlock: freeBlocks)
            freeBlock = nullptr;

        splitMemory();
    }

    void *heapAlloc(size_t size) {
        roundUpSize(size);
        size_t exponent = log2(size);

        if (!getNextSizeBlock(exponent))
            return nullptr;

        allocatedCnt++;
        return allocate(size);
    }

    void heapFree(uintptr_t *block) {
        block--;
        allocatedCnt--;
        bitset.deallocate((block - memPool) >> 2);
        free(block);
        merge(block);
    }

    [[nodiscard]]
    int heapDone() const {
        return allocatedCnt;
    }

    bool isAllocated(const uint8_t *ptr) const {
        size_t distanceFromStart = ptr - (uint8_t *) memPool - 8;
        if (distanceFromStart % 32 || distanceFromStart > memSize)
            return false;
        return bitset[distanceFromStart >> 5];
    }

private:
    void splitMemory() {
        size_t remaining = memSize, blockExponent, blockSize;
        uintptr_t *currStart = memPool;

        while (remaining > 32) {
            blockExponent = log2(remaining);
            blockSize = 1 << blockExponent;

            remaining -= blockSize;
            createFreeBlock(blockSize, currStart);
            pushList(currStart, blockExponent);

            currStart += blockSize >> 3;
        }

        memSize -= remaining;
    }

    static void createFreeBlock(size_t size, uintptr_t *startPtr) {
        startPtr[0] = size;
        startPtr[1] = (uintptr_t) nullptr;
        startPtr[2] = (uintptr_t) nullptr;
        startPtr[(size >> 3) - 1] = size;
    }

    void pushList(uintptr_t *startPtr, size_t exponent) {
        startPtr[2] = (uintptr_t) freeBlocks[exponent];
        if (freeBlocks[exponent])
            freeBlocks[exponent][1] = (uintptr_t) startPtr;
        freeBlocks[exponent] = startPtr;
    }

    void removeList(uintptr_t *block) {
        auto *prev = (uintptr_t *) block[1];
        auto *next = (uintptr_t *) block[2];

        if (prev && next) {
            prev[2] = (uintptr_t) next;
            next[1] = (uintptr_t) prev;
        } else if (prev)
            prev[2] = (uintptr_t) nullptr;
        else if (next)
            next[1] = (uintptr_t) nullptr;

        size_t exponent = log2(*block);
        if (block == freeBlocks[exponent])
            freeBlocks[exponent] = next;
    }

    void splitBlock(uintptr_t *block) {
        removeList(block);
        size_t newSize = *block / 2;
        createFreeBlock(newSize, block);
        createFreeBlock(newSize, block + newSize / sizeof(uintptr_t));

        //TODO: maybe better to switch, but this looks better
        pushList(block, log2(newSize));
        pushList(block + newSize / sizeof(uintptr_t), log2(newSize));
    }

    static void roundUpSize(size_t &size) {
        size += 2 * sizeof(size_t);
        size_t exponent = log2(size);
        if ((size_t) 1 << exponent != size)
            size = 1 << (exponent + 1);
    }

    [[nodiscard]]
    static size_t log2(size_t size) {
        size_t exponent = 0;
        while (size > 1) {
            size /= 2;
            exponent++;
        }

        return exponent;
    }

    void *allocate(size_t size) {
        uintptr_t *block = freeBlocks[log2(size)];
        removeList(block);
        block[((*block) >> 3) - 1] |= 1;
        block[0] |= 1;
        bitset.allocate((block - memPool) >> 2);

        return (void *) (block + 1);
    }

    void free(uintptr_t *startPtr) {
        size_t size = *startPtr & ~1;
        createFreeBlock(size, startPtr);
        pushList(startPtr, log2(size));
    }

    void mergeBlocks(uintptr_t *startPtr1, uintptr_t *startPtr2) {
        removeList(startPtr1);
        removeList(startPtr2);
        createFreeBlock(*startPtr1 * 2, startPtr1);
        pushList(startPtr1, log2(*startPtr1));
    }

    void merge(uintptr_t *startPtr) {
        size_t blockOrder = ((startPtr - memPool) * 8) / *startPtr;
        if (blockOrder % 2 && startPtr[0] == startPtr[-1]) {
            auto *newStart = startPtr - (*startPtr >> 3);
            mergeBlocks(newStart, startPtr);
            merge(newStart);
        } else if (!(blockOrder % 2)
                   && ((size_t) (startPtr - memPool) << 3) + *startPtr < memSize
                   && startPtr[*startPtr >> 3] == *startPtr) {
            auto *newStart = startPtr + (*startPtr >> 3);
            mergeBlocks(startPtr, newStart);
            merge(startPtr);
        }
    }

    [[nodiscard]]
    size_t findNextIndex(size_t exponent) const {
        for (size_t i = exponent; i < 64; i++)
            if (freeBlocks[i])
                return i;
        return 0;
    }

    bool getNextSizeBlock(size_t exponent) {
        size_t nextIndex = findNextIndex(exponent);
        if (! nextIndex)
            return false;

        for (size_t i = nextIndex; i > exponent; i--)
            splitBlock(freeBlocks[i]);

        return true;
    }

    int allocatedCnt = 0;
    size_t memSize;
    uintptr_t *memPool;
    Bitset bitset;
    uintptr_t *freeBlocks[64];
};


Heap heap;

void HeapInit(void *memPool, int memSize) {
    heap = Heap((uintptr_t *) memPool, (size_t) memSize);
}

void *HeapAlloc(int size) {
    if (!size)
        return nullptr;
    return heap.heapAlloc(size);
}

bool HeapFree(void *blk) {
    if (!blk || !heap.isAllocated((uint8_t *) blk))
        return false;
    heap.heapFree((uintptr_t *) blk);
    return true;
}

void HeapDone(int *pendingBlk) {
    *pendingBlk = heap.heapDone();
}


int main(void) {
    uint8_t *p0, *p1, *p2, *p3, *p4;
    int pendingBlk;
    static uint8_t memPool[3 * 1048576];

    HeapInit(memPool, 2097152);
    assert((p0 = (uint8_t *) HeapAlloc(512000)) != NULL);
    memset(p0, 0, 512000);
    assert((p1 = (uint8_t *) HeapAlloc(511000)) != NULL);
    memset(p1, 0, 511000);
    assert((p2 = (uint8_t *) HeapAlloc(26000)) != NULL);
    memset(p2, 0, 26000);
    HeapDone(&pendingBlk);
    assert(pendingBlk == 3);


    HeapInit(memPool, 2097152);
    assert((p0 = (uint8_t *) HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);
    assert((p1 = (uint8_t *) HeapAlloc(250000)) != NULL);
    memset(p1, 0, 250000);
    assert((p2 = (uint8_t *) HeapAlloc(250000)) != NULL);
    memset(p2, 0, 250000);
    assert((p3 = (uint8_t *) HeapAlloc(250000)) != NULL);
    memset(p3, 0, 250000);
    assert((p4 = (uint8_t *) HeapAlloc(50000)) != NULL);
    memset(p4, 0, 50000);
    assert(HeapFree(p2));
    assert(HeapFree(p4));
    assert(HeapFree(p3));
    assert(HeapFree(p1));
    assert((p1 = (uint8_t *) HeapAlloc(500000)) != NULL);
    memset(p1, 0, 500000);
    assert(HeapFree(p0));
    assert(HeapFree(p1));
    HeapDone(&pendingBlk);
    assert(pendingBlk == 0);


    HeapInit(memPool, 2359296);
    assert((p0 = (uint8_t *) HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);
    assert((p1 = (uint8_t *) HeapAlloc(500000)) != NULL);
    memset(p1, 0, 500000);
    assert((p2 = (uint8_t *) HeapAlloc(500000)) != NULL);
    memset(p2, 0, 500000);
    assert((p3 = (uint8_t *) HeapAlloc(500000)) == NULL);
    assert(HeapFree(p2));
    assert((p2 = (uint8_t *) HeapAlloc(300000)) != NULL);
    memset(p2, 0, 300000);
    assert(HeapFree(p0));
    assert(HeapFree(p1));
    HeapDone(&pendingBlk);
    assert(pendingBlk == 1);


    HeapInit(memPool, 2359296);
    assert((p0 = (uint8_t *) HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);
    assert(!HeapFree(p0 + 1000));
    HeapDone(&pendingBlk);
    assert(pendingBlk == 1);

    return 0;
}

