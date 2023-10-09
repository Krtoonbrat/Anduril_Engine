//
// Created by 80hugkev on 6/9/2022.
//

#include <cstdlib>
#include <cstring>
#include <thread>

#include "Anduril.h"
#include "TranspositionTable.h"

TranspositionTable table;

// saves the information passed to the node, possibly overwrting the old position
void Node::save(uint64_t k, int s, int t, int d, uint32_t m, int ev) {
    // keep the move the same for the same position
    if (m != 0 || (uint16_t)k != key) {
        bestMove = m;
    }

    // overwrite less valuable entries
    if (t == 1
        || (uint16_t)k != key
        || (int8_t)d > nodeDepth) {
        key = (uint16_t)k;
        nodeScore = (int16_t)s;
        nodeEval = (int16_t)ev;
        age = (int16_t)table.age;
        nodeDepth = (int8_t)d;
        nodeType = (int8_t)t;

    }

}

TranspositionTable::~TranspositionTable() {
    _aligned_free(tPtr);
}

void TranspositionTable::resize(size_t tSize) {
    constexpr size_t alignment = 4096;

    std::cout << "info string resizing Transposition Table to " << tSize << "MB" << std::endl;

    sizeMB = tSize;

    _aligned_free(tPtr);

    clusterCount = (tSize * 1024 * 1024) / sizeof(Cluster);
    size_t bytes = clusterCount * sizeof(Cluster);

    size_t size = ((bytes + alignment - 1) / alignment) * alignment;
    tPtr = static_cast<Cluster*>(_aligned_malloc(size, alignment));

    if (!tPtr) {
        std::cerr << "Failed to allocate " << tSize
                  << "MB for transposition table." << std::endl;
        exit(EXIT_FAILURE);
    }

    clear();
}

// based on the stockfish multi-threaded implementation
void TranspositionTable::clear() {
    std::vector<std::thread> threadPool;

    for (size_t idx = 0; idx < size_t(threads); ++idx) {
        threadPool.emplace_back([this, idx]() {

            const size_t stride = size_t(clusterCount / threads),
                    start  = size_t(stride * idx),
                    len    = idx != size_t(threads) - 1 ?
                             stride : clusterCount - start;

            std::memset(&tPtr[start], 0, len * sizeof(Cluster));
        });
    }

    for (auto & t : threadPool) {
        t.join();
    }
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
    if (replace->age > entry[1].age || (replace->age == entry[1].age && replace->nodeDepth > entry[1].nodeDepth)) {
        replace = &entry[1];
    }

    return replace;

}

// returns an approximation of the hash occupancy
int TranspositionTable::hashFull() {
    int count = 0;
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 2; j++) {
            count += tPtr[i].entry[j].nodeDepth && tPtr[i].entry[j].age == age;
        }
    }
    return count / 2;
}