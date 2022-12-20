//
// Created by 80hugkev on 6/9/2022.
//

#ifndef ANDURIL_ENGINE_TRANSPOSITIONTABLE_H
#define ANDURIL_ENGINE_TRANSPOSITIONTABLE_H

#include "Node.h"

// one cluster holds two nodes
struct Cluster {
    Node entry[2] = {Node(), Node()};
};

class TranspositionTable {
public:
    explicit TranspositionTable(size_t tSize);

    ~TranspositionTable();

    void resize(size_t tSize);

    void clear();

    Node* probe(uint64_t key, bool &foundNode);

private:
    Cluster *tPtr = nullptr;

    int tableSize = 0;

};

// I totally stole this from stockfish
template<class nodeType, int Size>
struct HashTable {
    nodeType* operator[](uint64_t key) { return &table[key & Size]; }

private:
    std::vector<nodeType> table = std::vector<nodeType>((Size * 1024 * 1024) / sizeof(nodeType));
};

#endif //ANDURIL_ENGINE_TRANSPOSITIONTABLE_H
