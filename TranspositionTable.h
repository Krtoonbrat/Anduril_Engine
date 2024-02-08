//
// Created by 80hugkev on 6/9/2022.
//

#ifndef ANDURIL_ENGINE_TRANSPOSITIONTABLE_H
#define ANDURIL_ENGINE_TRANSPOSITIONTABLE_H

#include "Node.h"

// one cluster holds two nodes
// 24 bytes
struct Cluster {
    Node entry[2];
};

// this was the stockfish way to find the index for a cluster.  If it works for them, it works for me
// this function returns the high 64 bits of a 128 bit product of two values we pass in.
inline uint64_t mul_hi64(uint64_t a, uint64_t b) {
#if defined(__GNUC__) && defined(IS_64BIT)
    __extension__ typedef unsigned __int128 uint128;
    return ((uint128)a * (uint128)b) >> 64;
#else
    uint64_t aL = (uint32_t)a, aH = a >> 32;
    uint64_t bL = (uint32_t)b, bH = b >> 32;
    uint64_t c1 = (aL * bL) >> 32;
    uint64_t c2 = aH * bL + c1;
    uint64_t c3 = aL * bH + (uint32_t)c2;
    return aH * bH + (c2 >> 32) + (c3 >> 32);
#endif
}

// mostly based on the stockfish implementation
class TranspositionTable {

    // constants used to refresh the hash table
    static constexpr unsigned GEN_BITS = 3;
    static constexpr int GEN_DELTA = (1 << GEN_BITS); // increment for generation field
    static constexpr int GEN_CYCLE = 255 + (1 << GEN_BITS); // cycle length
    static constexpr int GEN_MASK = (0xFF << GEN_BITS) & 0xFF; // mask to pull out generation number

public:
    ~TranspositionTable();

    void newSearch() { generation8 += GEN_DELTA; }

    void resize(size_t tSize);

    void clear();

    Node* probe(uint64_t key, bool &foundNode);

    Node* firstEntry(uint64_t key) {
        return &tPtr[mul_hi64(key, clusterCount)].entry[0];
    }

    int hashFull();

    size_t sizeMB = 0;

    uint8_t generation8;

private:
    Cluster *tPtr = nullptr;

    size_t clusterCount = 0;

};

// I totally stole this from stockfish
template<class nodeType, int Size>
struct HashTable {
    nodeType* operator[](uint64_t key) { return &table[(uint32_t)key & (Size - 1)]; }

private:
    std::vector<nodeType> table = std::vector<nodeType>((Size * 1024 * 1024) / sizeof(nodeType));
};

extern TranspositionTable table;

#endif //ANDURIL_ENGINE_TRANSPOSITIONTABLE_H
