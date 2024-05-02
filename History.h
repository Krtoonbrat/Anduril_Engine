//
// Created by Hughe on 5/3/2023.
//

#ifndef ANDURIL_ENGINE_HISTORY_H
#define ANDURIL_ENGINE_HISTORY_H

#include <array>

// this is the type for our main history table
using ButterflyHistory = std::array<std::array<std::array<int16_t, 64>, 64>, 2>;

// same as butterfly, but indexed by [piece][to square] instead of [from square][to square]
using PieceHistory = std::array<std::array<int16_t, 64>, 16>;

// this is the type for continuation history, which is basically butterfly history
// except it considers previous moves
using ContinuationHistory = std::array<std::array<PieceHistory, 64>, 16>;

// this is the type for the capture history table
using CaptureHistory = std::array<std::array<std::array<int16_t, 5>, 64>, 13>;


#endif //ANDURIL_ENGINE_HISTORY_H
