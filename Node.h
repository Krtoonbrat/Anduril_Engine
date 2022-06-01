//
// Created by Hughe on 4/3/2022.
//

#ifndef ANDURIL_ENGINE_NODE_H
#define ANDURIL_ENGINE_NODE_H

#include "thc.h"

class Node {
public:
    Node() {nodeScore = -999999999; nodeType = 3; bestMove.Invalid(); nodeDepth = -99;};

    // the evaluation of the node
    int nodeScore;

    // the depth (ply) the node has been searched to
    int nodeDepth;

    // type of node
    // 1 if the node is a PV node
    // 2 if the node is a cut node
    int nodeType;

    // the best move for the node
    thc::Move bestMove;


};

#endif //ANDURIL_ENGINE_NODE_H
