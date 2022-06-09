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
    TranspositionTable(size_t tSize);

    void resize(size_t tSize);

    void clear();

    Node* probe(uint64_t key, bool &foundNode);

private:
    std::vector<Cluster> table;

    int tableSize = 0;

};

#endif //ANDURIL_ENGINE_TRANSPOSITIONTABLE_H
