//
// Created by Hughe on 4/3/2022.
//

#ifndef ANDURIL_ENGINE_NODE_H
#define ANDURIL_ENGINE_NODE_H

#include "libchess/Position.h"

// 12 bytes
struct Node {
    // saves the information passed in to the node
    void save(uint64_t k, int s, int t, int d, uint32_t m, int ev);

    // the evaluation of the node after a full search
    int16_t nodeScore;

	// the static eval of a node
	int16_t nodeEval;

    // the depth the node has been searched to
    int8_t nodeDepth;

    // type of node and age
    // 3 if the node is a PV node
    // 2 if the node is a cut node
    // 1 if the node is an all node
    uint8_t nodeTypeGenBound;

    // the key used to look up the node
    uint16_t key;

    // the best move for the node
    uint32_t bestMove;


};

struct SimpleNode {
    // holds the value of the node
    int16_t score = -32001;

    // the key used to search for the node
    uint64_t key = 0;
};

struct PawnEntry {
    std::pair<int16_t, int16_t> score = {0, 0};

    uint64_t key = 0;
};

#endif //ANDURIL_ENGINE_NODE_H
