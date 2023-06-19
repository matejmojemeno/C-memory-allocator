#include <cstring>
#include <cstdint>

class Bitset {
public:
    Bitset() = default;

    Bitset(uint8_t *pool, size_t size);

    void allocate(size_t block);

    void deallocate(size_t block);

    bool operator[](size_t block) const;
private:
    uint8_t *bitset;
};