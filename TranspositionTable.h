//
// Created by 80hugkev on 6/9/2022.
//

#ifndef ANDURIL_ENGINE_TRANSPOSITIONTABLE_H
#define ANDURIL_ENGINE_TRANSPOSITIONTABLE_H

#include "Node.h"

// one cluster holds two nodes
// 32 bytes
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

class TranspositionTable {
public:
    explicit TranspositionTable(size_t tSize);

    ~TranspositionTable();

    void resize(size_t tSize);

    void clear();

    Node* probe(uint64_t key, bool &foundNode);

    Node* firstEntry(uint64_t key) {
        return &tPtr[mul_hi64(key, clusterCount)].entry[0];
    }

private:
    Cluster *tPtr = nullptr;

    size_t clusterCount = 0;

};

// I totally stole this from stockfish
template<class nodeType, int Size>
struct HashTable {
    nodeType* operator[](uint64_t key) { return &table[key & Size]; }

private:
    std::vector<nodeType> table = std::vector<nodeType>((Size * 1024 * 1024) / sizeof(nodeType));
};

#endif //ANDURIL_ENGINE_TRANSPOSITIONTABLE_H
