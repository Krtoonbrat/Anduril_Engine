//
// Created by 80hugkev on 7/7/2022.
//

#ifndef ANDURIL_ENGINE_LIMIT_H
#define ANDURIL_ENGINE_LIMIT_H

struct limit {
    // time left on the clock for white and black (milliseconds)
    int time = -1;

    // increment received per move for white and black (milliseconds)
    int increment = -1;

    // depth to search to
    int depth = -1;

    // is there a time set at all?
    bool timeSet = false;

    // maximum number of nodes to search
    int nodes = -1;
};

#endif //ANDURIL_ENGINE_LIMIT_H
