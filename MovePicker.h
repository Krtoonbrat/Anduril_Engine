//
// Created by Hughe on 3/5/2023.
//

#ifndef ANDURIL_ENGINE_MOVEPICKER_H
#define ANDURIL_ENGINE_MOVEPICKER_H

#include "libchess/Position.h"
#include "Anduril.h"

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

    MovePicker(libchess::Position &b, libchess::Move &ttm, libchess::Move *k, libchess::Move& cm, std::array<std::array<std::array<int, 64>, 64>, 2>* his, std::array<int, 6> *see);
    MovePicker(libchess::Position &b, libchess::Move &ttm, std::array<std::array<std::array<int, 64>, 64>, 2>* his, std::array<int, 6> *see);
    MovePicker(libchess::Position &b, libchess::Move &ttm, int t, std::array<int, 6> *see);

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
    std::array<int, 6> *seeValues;
};

// shamelessly stolen from stockfish again
// partial_insertion_sort() sorts moves in descending order up to and including
// a given limit. The order of moves smaller than the limit is left unspecified.
inline void partial_insertion_sort(libchess::Move* begin, libchess::Move* end, int limit) {

    // I'm going to comment the shit out of this so that I understand it
    // sortedEnd points to the last element that is sorted, p is the current move we are putting in place
    for (libchess::Move *sortedEnd = begin, *p = begin + 1; p < end; ++p)
        if (p->score >= limit)
        {
            // tmp get the value of p, q is a pointer to a move
            libchess::Move tmp = *p, *q;
            // the value of p is now equal to our last move that is in its proper place, sortedEnd is advanced
            *p = *++sortedEnd;
            // q points to the end of the sorted section, we iterate until we get to the front of the list, or until we
            // find the proper position for the move we are putting in place (that move is saved with tmp)
            // basically, if the move that q points to is less than tmp (the move we are putting in place), we replace the
            // value at q with the value at q - 1.  This is ok because the only move that is "deleted" from the list
            // right now is the move we are sorting, which is saved in tmp
            for (q = sortedEnd; q != begin && *(q - 1) < tmp; --q)
                *q = *(q - 1);
            // once we find the proper position for the move we are putting in place, we throw it in that spot.  This is
            // ok because q + 1 is the same move as q, so we aren't losing any information.
            *q = tmp;
        }
}

#endif //ANDURIL_ENGINE_MOVEPICKER_H