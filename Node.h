//
// Created by Hughe on 4/3/2022.
//

#ifndef ANDURIL_ENGINE_NODE_H
#define ANDURIL_ENGINE_NODE_H

#include "libchess/Position.h"

class Node {
public:
    Node() : nodeScore(-999999999), nodeType(-1), bestMove(), nodeDepth(-99), key(0), nodeEval(-999999999) {}

    // the evaluation of the node after a full search
    int nodeScore;

	// the static eval of a node
	int nodeEval;

    // the depth (ply) the node has been searched to
    int nodeDepth;

    // type of node
    // 1 if the node is a PV node
    // 2 if the node is a cut node
    // 3 if the node is an all node
    int nodeType;

    // the key used to look up the node
    uint64_t key;

    // the best move for the node
    libchess::Move bestMove;


};

struct SimpleNode {
    // holds the value of the node
    int score = -999999999;

    // the key used to search for the node
    uint64_t key = 0;
};

#endif //ANDURIL_ENGINE_NODE_H
