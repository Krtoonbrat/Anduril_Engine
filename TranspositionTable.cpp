//
// Created by 80hugkev on 6/9/2022.
//

#include <cstdlib>
#include <cstring>
#include <thread>

#include "Anduril.h"
#include "misc.h"
#include "Thread.h"
#include "TranspositionTable.h"

TranspositionTable table;

extern ThreadPool gondor;

// saves the information passed to the node, possibly overwriting the old position
void Node::save(uint64_t k, int s, int t, int d, uint16_t m, int ev) {
    // keep the move the same for the same position
    if (m != 0 || uint16_t(k) != key) {
        bestMove = m;
    }

    // overwrite less valuable entries
    if (t == 3
        || uint16_t(k) != key
        || int8_t(d) > nodeDepth) {
        key = uint16_t(k);
        nodeScore = int16_t(s);
        nodeEval = int16_t(ev);
        nodeDepth = int8_t(d);
        nodeTypeGenBound = uint8_t(table.generation8 | t);

    }

}

TranspositionTable::~TranspositionTable() {
    aligned_large_pages_free(tPtr);
}

void TranspositionTable::resize(size_t tSize) {

    //std::cout << "info string resizing Transposition Table to " << tSize << "MB" << std::endl;

    sizeMB = tSize;

    aligned_large_pages_free(tPtr);

    clusterCount = (tSize * 1024 * 1024) / sizeof(Cluster);
    size_t bytes = clusterCount * sizeof(Cluster);

    tPtr = static_cast<Cluster*>(aligned_large_pages_alloc(bytes));

    if (!tPtr) {
        std::cerr << "Failed to allocate " << tSize
                  << "MB for transposition table." << std::endl;
        exit(EXIT_FAILURE);
    }

    clear();
}

// based on the stockfish multithreaded implementation
void TranspositionTable::clear() {
    std::vector<std::thread> threadPool;

    for (size_t idx = 0; idx < size_t(gondor.numThreads); ++idx) {
        threadPool.emplace_back([this, idx]() {

            const size_t stride = size_t(clusterCount / gondor.numThreads),
                         start  = size_t(stride * idx),
                         len    = idx != size_t(gondor.numThreads) - 1 ?
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

    for (int i = 0; i < 3; i++) {
        if (entry[i].key == k || entry[i].key == 0) {
            foundNode = entry[i].key == k;
            entry[i].nodeTypeGenBound = uint8_t(generation8 | (entry[i].nodeTypeGenBound & (GEN_DELTA - 1)));
            return &entry[i];
        }
    }

    // find an entry to be replaced
    Node *replace = entry;
    for (int i = 1; i < 3; i++) {
        if (replace->nodeDepth - ((GEN_CYCLE + generation8 - replace->nodeTypeGenBound) & GEN_MASK) > entry[i].nodeDepth - ((GEN_CYCLE + generation8 - entry[i].nodeTypeGenBound) & GEN_MASK)) {
            replace = &entry[i];
        }
    }

    foundNode = false;
    return replace;

}

// returns an approximation of the hash occupancy
int TranspositionTable::hashFull() {
    int count = 0;
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 2; j++) {
            count += tPtr[i].entry[j].nodeDepth && (tPtr[i].entry[j].nodeTypeGenBound & GEN_MASK) == generation8;
        }
    }
    return count / 2;
}