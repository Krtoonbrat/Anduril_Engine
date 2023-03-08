//
// Created by 80hugkev on 6/9/2022.
//

#include "TranspositionTable.h"

// saves the information passed to the node, possibly overwrting the old position
void Node::save(uint64_t k, int s, int t, int d, libchess::Move m, int a, int ev) {
    // keep the move the same for the same position
    if (m.value() != 0 || (uint16_t)k != key) {
        bestMove = m;
    }

    // overwrite less valuable entries (cheapest check first)
    if (t == 1
        || (uint16_t)k != key
        || (int8_t)d > nodeDepth) {
        key = (uint16_t)k;
        nodeScore = (int16_t)s;
        nodeEval = (int16_t)ev;
        age = (int16_t)a;
        nodeDepth = (int8_t)d;
        nodeType = (int8_t)t;

    }

}

TranspositionTable::TranspositionTable(size_t tSize) {
    resize(tSize);
    clear();
}

TranspositionTable::~TranspositionTable() {
    delete[] tPtr;
}

void TranspositionTable::resize(size_t tSize) {
    std::cout << "info string resizing Transposition Table to " << tSize << "MB" << std::endl;
    clusterCount = (tSize * 1024 * 1024) / sizeof(Cluster);
    delete[] tPtr;
    tPtr = new Cluster[clusterCount];
}

void TranspositionTable::clear() {
    delete[] tPtr;
    tPtr = new Cluster[clusterCount];
}

Node* TranspositionTable::probe(uint64_t key, bool &foundNode) {
    Node *entry = firstEntry(key);

    uint16_t k = (uint16_t)key;

    // look for a matching node or one that needs to be filled
    if (entry[0].key == k || entry[0].key  == 0) {
        entry[0].key == k ? foundNode = true : foundNode = false;
        return &entry[0];
    }
    else if (entry[1].key == k || entry[1].key  == 0) {
        entry[1].key == k ? foundNode = true : foundNode = false;
        return &entry[1];
    }

    // no nodes matched, determine which gets replaced
    Node *replace = entry;
    if (entry[0].age > entry[1].age || entry[0].nodeDepth > entry[1].nodeDepth) {
        replace = &entry[1];
    }

    return replace;

}