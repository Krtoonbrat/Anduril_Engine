//
// Created by Hughe on 3/5/2023.
//

#include "MovePicker.h"
#include "UCI.h"

int queenOrderVal = 50934;
int rookOrderVal =  24844;
int minorOrderVal = 14039;

// constructor for main search
MovePicker::MovePicker(libchess::Position &b, libchess::Move &ttm, libchess::Move *k, libchess::Move &cm,
                       ButterflyHistory *his, const PieceHistory **contHis, const CaptureHistory *capHis)
                       : board(b), transposition(ttm), refutations{k[0], k[1], cm}, moveHistory(his), continuationHistory(contHis), captureHistory(capHis) {
    stage = (board.in_check() ? EVASION_TT : MAIN_TT) + !(transposition.value() != 0 && board.is_legal_move(transposition));
}

// constructor for qsearch
MovePicker::MovePicker(libchess::Position &b, libchess::Move &ttm, ButterflyHistory *his,
                       const PieceHistory **contHis, const CaptureHistory *capHis, int d)
                      : board(b), transposition(ttm), moveHistory(his), continuationHistory(contHis), captureHistory(capHis), depth(d) {
    stage = (board.in_check() ? EVASION_TT : QSEARCH_TT) + !(transposition.value() != 0 && board.is_legal_move(transposition));
}

// constructor for probcut
MovePicker::MovePicker(libchess::Position &b, libchess::Move &ttm, const CaptureHistory *capHis, int t) : board(b), transposition(ttm), captureHistory(capHis), threshold(t) {
    stage = PROBCUT_TT + !(transposition.value() != 0 && board.is_capture_move(ttm)
                                                      && board.is_legal_move(ttm)
                                                      && board.see_ge(ttm, t));
}

// gets the attacks by a team of a certain piece
template<MovePicker::PieceType pt, bool white>
libchess::Bitboard MovePicker::attackByPiece(libchess::Position &board) {
    constexpr libchess::Color c = white ? libchess::constants::WHITE : libchess::constants::BLACK;
    libchess::Bitboard attacks;
    libchess::Bitboard pieces;
    libchess::PieceType type = libchess::constants::PAWN;

    // grab the correct bitboard
    if constexpr (pt == PAWN) {
        pieces = board.piece_type_bb(type, c);
    }
    else if constexpr (pt == KNIGHT) {
        type = libchess::constants::KNIGHT;
        pieces = board.piece_type_bb(type, c);
    }
    else if constexpr (pt == BISHOP) {
        type = libchess::constants::BISHOP;
        pieces = board.piece_type_bb(type, c);
    }
    else if constexpr (pt == ROOK) {
        type = libchess::constants::ROOK;
        pieces = board.piece_type_bb(type, c);
    }
    else if constexpr (pt == QUEEN) {
        type = libchess::constants::QUEEN;
        pieces = board.piece_type_bb(type, c);
    }
    else if constexpr (pt == KING) {
        type = libchess::constants::KING;
        pieces = board.piece_type_bb(type, c);
    }

    // loop through the pieces
    // special case for pawns because it uses a different function
    if constexpr (pt == PAWN) {
        while (pieces) {
            libchess::Square square = pieces.forward_bitscan();
            attacks |= libchess::lookups::pawn_attacks(square, c);
            pieces.forward_popbit();
        }
    }
    else {
        while (pieces) {
            libchess::Square square = pieces.forward_bitscan();
            attacks |= libchess::lookups::non_pawn_piece_type_attacks(type, square, board.occupancy_bb());
            pieces.forward_popbit();
        }
    }

    return attacks;
}

template<MovePicker::ScoreType type>
void MovePicker::score() {

    [[maybe_unused]] libchess::Bitboard pawnThreat, minorThreat, rookThreat, threatenedPieces;
    if constexpr (type == QUIETS) {
        if (!board.side_to_move()) {
            pawnThreat = attackByPiece<PAWN, false>(board);
            minorThreat = attackByPiece<KNIGHT, false>(board) | attackByPiece<BISHOP, false>(board) | pawnThreat;
            rookThreat = attackByPiece<ROOK, false>(board) | minorThreat;
        }
        else {
            pawnThreat = attackByPiece<PAWN, true>(board);
            minorThreat = attackByPiece<KNIGHT, true>(board) | attackByPiece<BISHOP, true>(board) | pawnThreat;
            rookThreat = attackByPiece<ROOK, true>(board) | minorThreat;
        }

        // pieces threatened by material of lesser value
        threatenedPieces =  (board.piece_type_bb(libchess::constants::QUEEN, board.side_to_move()) & rookThreat)
                            | (board.piece_type_bb(libchess::constants::ROOK, board.side_to_move()) & minorThreat)
                            | ((board.piece_type_bb(libchess::constants::KNIGHT, board.side_to_move()) | board.piece_type_bb(libchess::constants::BISHOP, board.side_to_move())) & pawnThreat);
    }

    for (auto& m : *this) {
        if constexpr (type == CAPTURES) {
            m.score = 8 * libchess::Position::pieceValuesMG[(board.piece_type_on(m.to_square()) ? board.piece_type_on(m.to_square())->value() : 0)] // the condition is for enpassant, as there would not be a piece on the destination
                        + (*captureHistory)[board.piece_on(m.from_square())->to_nnue()][m.to_square()][board.piece_type_on(m.to_square()) ? board.piece_type_on(m.to_square())->value() : 0];
        }
        else if constexpr(type == QUIETS) {
            m.score =  (threatenedPieces & libchess::lookups::square(m.from_square()) ?
                       (*board.piece_type_on(m.from_square()) == libchess::constants::QUEEN && !(libchess::lookups::square(m.to_square()) & rookThreat ) ? queenOrderVal
                      : *board.piece_type_on(m.from_square()) == libchess::constants::ROOK  && !(libchess::lookups::square(m.to_square()) & minorThreat) ? rookOrderVal
                      :                                                                              !(libchess::lookups::square(m.to_square()) & pawnThreat)  ? minorOrderVal
                      :                                                                                                                                    0)
                      :                                                                                                                                    0)
                      +  moveHistory->at(board.side_to_move()).at(m.from_square()).at(m.to_square())
                      + (2 * (*continuationHistory[0])[board.piece_on(m.from_square())->value()][m.to_square()])
                      + (*continuationHistory[1])[board.piece_on(m.from_square())->value()][m.to_square()]
                      + (*continuationHistory[3])[board.piece_on(m.from_square())->value()][m.to_square()]
                      + (*continuationHistory[5])[board.piece_on(m.from_square())->value()][m.to_square()];
        }
        else { // evasions
            if (board.is_capture_move(m)) {
                // the condition is for enpassant, as there would not be a piece on the destination
                m.score = 8 * libchess::Position::pieceValuesMG[(board.piece_type_on(m.to_square()) ? board.piece_type_on(m.to_square())->value() : 0)]
                          + (*captureHistory)[board.piece_on(m.from_square())->to_nnue()][m.to_square()][board.piece_type_on(m.to_square()) ? board.piece_type_on(m.to_square())->value() : 0] + (500000); // we add a large number to make sure captures are always searched first
            }
            else {
                m.score = moveHistory->at(board.side_to_move()).at(m.from_square()).at(m.to_square())
                        + (*continuationHistory[0])[board.piece_on(m.from_square())->value()][m.to_square()];
            }
        }


    }

}

libchess::Move MovePicker::nextMove(bool skipQuiet) {

top:
    switch (stage) {
        case MAIN_TT:
        case EVASION_TT:
        case QSEARCH_TT:
        case PROBCUT_TT:
            stage++;
            return transposition;

        case CAPTURE_INIT:
        case PROBCUT_INIT:
        case QCAPTURE_INIT:
            cur = endBadCaptures = moves.begin();
            board.generate_capture_moves(moves, board.side_to_move());
            endMoves = moves.end();

            score<CAPTURES>();
            partial_insertion_sort(cur, endMoves, std::numeric_limits<int>::min());
            stage++;
            goto top;

        case GOOD_CAPTURE:
            while (cur < endMoves) {
                if (*cur != transposition) {
                    if (board.see_ge(*cur, -cur->score)) {
                        return *cur++;
                    }
                    *endBadCaptures++ = *cur++;
                }
                else {
                    cur++;
                }
            }

            cur = std::begin(refutations);
            endMoves = std::end(refutations);

            // if countermove is the same as one of the killer moves, we can skip it
            if (refutations[0] == refutations[2]
                || refutations[1] == refutations[2]) {
                endMoves--;
            }

            stage++;

        case REFUTATION:
            while (cur < endMoves) {
                if (cur->value() != 0 && *cur != transposition) {
                    return *cur++;
                }
                cur++;
            }
            stage++;

        case QUIET_INIT:
            if (!skipQuiet) {
                cur = endBadCaptures;
                board.generate_quiet_moves(moves, board.side_to_move());
                endMoves = moves.end();

                score<QUIETS>();
                partial_insertion_sort(cur, endMoves, std::numeric_limits<int>::min());
            }

            stage++;

        case QUIET:
            while (!skipQuiet && cur < endMoves) {
                if (*cur != transposition
                    && *cur != refutations[0]
                    && *cur != refutations[1]
                    && *cur != refutations[2]) {
                    return *cur++;
                }
                else {
                    cur++;
                }
            }

            // prepare to loop through bad captures
            cur = moves.begin();
            endMoves = endBadCaptures;
            stage++;

        case BAD_CAPTURE:
            while (cur < endMoves) {
                if (*cur != transposition) {
                    return *cur++;
                }
                cur++;
            }
            return libchess::Move(0);

        case EVASION_INIT:
            moves = board.check_evasion_move_list();
            cur = moves.begin();
            endMoves = moves.end();

            score<EVASIONS>();
            partial_insertion_sort(cur, endMoves, std::numeric_limits<int>::min());
            stage++;

        case EVASION:
            while (cur < endMoves) {
                if (*cur != transposition) {
                    return *cur++;
                }
                cur++;
            }
            return libchess::Move(0);

        case PROBCUT:
            while (cur < endMoves) {
                if (board.see_ge(*cur, threshold) && *cur != transposition) {
                    return *cur++;
                }
                cur++;
            }
            return libchess::Move(0);

        case QCAPTURE:
            while (cur < endMoves) {
                if (*cur != transposition) {
                    return *cur++;
                }
                cur++;
            }
            if (depth < 0) {
                return libchess::Move(0);
            }

            stage++;

        case QTACTICAL_INIT:
            board.generate_quiet_checks(moves, board.side_to_move());
            board.generate_quiet_promotions(moves, board.side_to_move());
            endMoves = moves.end();
            
            score<QUIETS>();
            partial_insertion_sort(cur, endMoves, std::numeric_limits<int>::min());
            stage++;
        case QTACTICAL:
            while (cur < endMoves) {
                if (*cur != transposition) {
                    return *cur++;
                }
                cur++;
            }
            return libchess::Move(0);
    }

    return libchess::Move(0);

}