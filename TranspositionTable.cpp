//
// Created by 80hugkev on 6/9/2022.
//

#include "TranspositionTable.h"

TranspositionTable::TranspositionTable(size_t tSize) {
    resize(tSize);
    clear();
}

TranspositionTable::~TranspositionTable() {
    delete[] tPtr;
}

void TranspositionTable::resize(size_t tSize) {
    std::cout << "info string resizing Transposition Table to " << tSize << "MB" << std::endl;
    int nodeCount = (tSize * 1024 * 1024) / sizeof(Cluster);
    delete[] tPtr;
    tPtr = new Cluster[nodeCount];
    tableSize = nodeCount - 1;
}

void TranspositionTable::clear() {
    delete[] tPtr;
    tPtr = new Cluster[tableSize + 1];
}

Node* TranspositionTable::probe(uint64_t key, bool &foundNode) {
    Node *entry = &tPtr[key & tableSize].entry[0];

    // look for a matching node or one that needs to be filled
    if (entry[0].key == key || entry[0].key  == 0) {
        entry[0].key == key ? foundNode = true : foundNode = false;
        return &entry[0];
    }
    else if (entry[1].key == key || entry[1].key  == 0) {
        entry[1].key == key ? foundNode = true : foundNode = false;
        return &entry[1];
    }

    // no nodes matched, determine which gets replaced
    Node *replace = entry;
    if (entry[0].nodeDepth > entry[1].nodeDepth) {
        replace = &entry[1];
    }

    return replace;

}