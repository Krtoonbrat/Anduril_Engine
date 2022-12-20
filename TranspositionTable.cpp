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
    for (int i = 0; i < 2; i++) {
        if (entry[i].key == key || entry[i].key == 0) {
            entry[i].key == key ? foundNode = true : foundNode = false;
            return &entry[i];
        }
    }

    // no nodes matched, determine which gets replaced
    Node *replace = entry;
    if (entry[0].nodeDepth > entry[1].nodeDepth) {
        replace = &entry[1];
    }

    return replace;

}