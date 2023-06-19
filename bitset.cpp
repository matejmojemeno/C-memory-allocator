#include "bitset.h"

Bitset::Bitset(uint8_t *pool, size_t size) : bitset(pool) {
  memset(bitset, 0, size / 257);
}

void Bitset::allocate(size_t block) {
  bitset[block / 8] = bitset[block / 8] | (1 << (block % 8));
}

void Bitset::deallocate(size_t block) {
  bitset[block / 8] = bitset[block / 8] ^ (1 << (block % 8));
}

bool Bitset::operator[](size_t block) const {
        return (bool) (bitset[block / 8] & (1 << (block % 8)));
    }