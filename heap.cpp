#include "heap.h"

Heap::Heap(uintptr_t *pool, size_t size)
    : memSize(size), memPool(pool), bitset((uint8_t *)pool, memSize) {
  memPool += memSize / 257 / 8 + 1;
  memSize -= memSize % 257 + memSize / 257;

  for (auto &freeBlock : freeBlocks)
    freeBlock = nullptr;

  splitMemory();
}

void *Heap::heapAlloc(size_t size) {
  roundUpSize(size);
  size_t exponent = log2(size);

  if (!getNextSizeBlock(exponent))
    return nullptr;

  allocatedCnt++;
  return allocate(size);
}

void Heap::heapFree(uintptr_t *block) {
    block--;
    allocatedCnt--;
    bitset.deallocate((block - memPool) >> 2);
    free(block);
    merge(block);
}