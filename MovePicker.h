//
// Created by Hughe on 3/5/2023.
//

#ifndef ANDURIL_ENGINE_MOVEPICKER_H
#define ANDURIL_ENGINE_MOVEPICKER_H

#include "libchess/Position.h"

class MovePicker {
    enum Stages {
        MAIN_TT, CAPTURE_INIT, GOOD_CAPTURE, REFUTATION, QUIET_INIT, QUIET, BAD_CAPTURE,
        EVASION_TT, EVASION_INIT, EVASION,
        PROBCUT_TT, PROBCUT_INIT, PROBCUT,
        QSEARCH_TT, QCAPTURE_INIT, QCAPTURE,
    };

    enum ScoreType {
        CAPTURES, QUIETS, EVASIONS
    };

    // enumeration type for the pieces
    enum PieceType { PAWN,
        KNIGHT,
        BISHOP,
        ROOK,
        QUEEN,
        KING };

public:
    // delete the copy and copy assignment constructors so that we don't accidentally do that
    MovePicker(const MovePicker&) = delete;
    MovePicker& operator=(const MovePicker&) = delete;

    MovePicker(libchess::Position &b, libchess::Move &ttm, libchess::Move *k, libchess::Move& cm, std::array<std::array<std::array<int, 64>, 64>, 2>* his);
    MovePicker(libchess::Position &b, libchess::Move &ttm, std::array<std::array<std::array<int, 64>, 64>, 2>* his);
    MovePicker(libchess::Position &b, libchess::Move &ttm, int t);

    libchess::Move nextMove();

    libchess::MoveList getMoves() { return moves; }

private:
    template<ScoreType>
    void score();

    libchess::Move* begin() { return cur; }
    libchess::Move* end() { return endMoves; }

    // gets the attacks by a team of a certain piece
    template<PieceType pt, bool white>
    static libchess::Bitboard attackByPiece(libchess::Position &board);

    libchess::Move *cur, *endMoves, *endBadCaptures;
    libchess::Position& board;
    std::array<std::array<std::array<int, 64>, 64>, 2>* moveHistory;
    libchess::Move transposition;
    libchess::Move refutations[3];
    int stage;
    int threshold;
    libchess::MoveList moves;



    // these values allow me to use see inside the move picker class.  I should find a better solution.
    int kMG = 337;
    int kEG = 281;

    int pMG = 88;
    int pEG = 138;

    int bMG = 365;
    int bEG = 297;

    int rMG = 477;
    int rEG = 512;

    int qMG = 1025;
    int qEG = 936;
    std::array<int, 6> seeValues = {pMG, kMG, bMG, rMG, qMG, 0};
};

#endif //ANDURIL_ENGINE_MOVEPICKER_H
