//
// Created by Hughe on 4/3/2022.
//

#ifndef ANDURIL_ENGINE_NODE_H
#define ANDURIL_ENGINE_NODE_H

#include "libchess/Position.h"

class Node {
public:
    Node() : nodeScore(-32001), nodeType(-1), bestMove(), nodeDepth(-99), key(0), nodeEval(-32001), age(0) {}

    // saves the information passed in to the node
    void save(uint64_t k, int s, int b, int d, libchess::Move m, int a, int ev);

    // the evaluation of the node after a full search
    int16_t nodeScore;

	// the static eval of a node
	int16_t nodeEval;

    // the depth the node has been searched to
    int8_t nodeDepth;

    // type of node
    // 1 if the node is a PV node
    // 2 if the node is a cut node
    // 3 if the node is an all node
    int8_t nodeType;

    // age of the node
    uint16_t age;

    // the key used to look up the node
    uint16_t key;

    // the best move for the node
    libchess::Move bestMove;


};

struct SimpleNode {
    // holds the value of the node
    int16_t score = -32001;

    // the key used to search for the node
    uint64_t key = 0;
};

struct PawnEntry {
    std::pair<int, int> score = {0, 0};

    uint64_t key = 0;
};

#endif //ANDURIL_ENGINE_NODE_H
