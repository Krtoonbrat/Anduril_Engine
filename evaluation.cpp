//
// Created by 80hugkev on 6/10/2022.
//

#include <iostream> // we need this for debugging if we print the board

#include "Anduril.h"

// generates a static evaluation of the board
int Anduril::evaluateBoard(libchess::Position &board) {
    // first check for transpositions
    uint64_t hash = board.hash();
    SimpleNode *eNode = evalTable[hash];
    if (eNode->key == hash) {
        return eNode->score;
    }
    else {
        eNode->key = hash;
    }

    // squares the kings are on
    libchess::Square wKingSquare = board.king_square(libchess::constants::WHITE);
    libchess::Square bKingSquare = board.king_square(libchess::constants::BLACK);

    // reset the mobility scores
    whiteMobility[0] = 0, whiteMobility[1] = 0;
    blackMobility[0] = 0, blackMobility[1] = 0;

    // reset the king tropism variables
    attackCount[0] = 0, attackCount[1] = 0;
    attackWeight[0] = 0, attackWeight[1] = 0;
    kingZoneWBB = libchess::Bitboard(wKingSquare);
    kingZoneBBB = libchess::Bitboard(bKingSquare);

    // reset each colors attack map
    wAttackMap = libchess::Bitboard();
    bAttackMap = libchess::Bitboard();

    // fill the king zone
    kingZoneWBB |= libchess::lookups::king_attacks(board.king_square(libchess::constants::WHITE));
    kingZoneBBB |= libchess::lookups::king_attacks(board.king_square(libchess::constants::BLACK));

    kingZoneWBB |= kingZoneWBB << 16;
    kingZoneBBB |= kingZoneBBB >> 16;

    int scoreMG = 0;
    int scoreEG = 0;

    // get the material score for the board
    std::tuple<int, int> matScore = getMaterialScore(board);
    scoreMG += std::get<0>(matScore);
    scoreEG += std::get<1>(matScore);

    // get the pawn score for the board
    int p = getPawnScore(board);
    scoreMG += p;
    scoreEG += p;

    // get king safety bonus for the board
    // this function only relates to castled kings, therefore its only necessary to add it to the middle game score
    int king = getKingSafety(board, wKingSquare, bKingSquare);
    scoreMG += king;

    // king tropism (black is index 0, white is index 1)
    // white
    if (attackCount[1] >= 2) {
        scoreMG += SafetyTable[attackWeight[1]];
        scoreEG -= SafetyTable[attackWeight[1]];
    }
    // black
    if (attackCount[0] >= 2) {
        scoreMG -= SafetyTable[attackWeight[0]];
        scoreEG -= SafetyTable[attackWeight[0]];
    }

    // mobility
    scoreMG += whiteMobility[0];
    scoreMG -= blackMobility[0];
    scoreEG += whiteMobility[1];
    scoreEG -= blackMobility[1];

    // give the AI a slap for moving the queen too early
    if (!(board.piece_type_bb(libchess::constants::QUEEN, libchess::constants::WHITE)
        & libchess::lookups::square(libchess::constants::D1))) {
        if (board.piece_type_bb(libchess::constants::KNIGHT, libchess::constants::WHITE)
            & libchess::lookups::square(libchess::constants::B1)) {
            scoreMG -= 2;
        }
        if (board.piece_type_bb(libchess::constants::KNIGHT, libchess::constants::WHITE)
            & libchess::lookups::square(libchess::constants::G1)) {
            scoreMG -= 2;
        }
        if (board.piece_type_bb(libchess::constants::BISHOP, libchess::constants::WHITE)
            & libchess::lookups::square(libchess::constants::C1)) {
            scoreMG -= 2;
        }
        if (board.piece_type_bb(libchess::constants::BISHOP, libchess::constants::WHITE)
            & libchess::lookups::square(libchess::constants::F1)) {
            scoreMG -= 2;
        }
    }
    if (!(board.piece_type_bb(libchess::constants::QUEEN, libchess::constants::BLACK)
          & libchess::lookups::square(libchess::constants::D8))) {
        if (board.piece_type_bb(libchess::constants::KNIGHT, libchess::constants::BLACK)
            & libchess::lookups::square(libchess::constants::B8)) {
            scoreMG += 2;
        }
        if (board.piece_type_bb(libchess::constants::KNIGHT, libchess::constants::BLACK)
            & libchess::lookups::square(libchess::constants::G8)) {
            scoreMG += 2;
        }
        if (board.piece_type_bb(libchess::constants::BISHOP, libchess::constants::BLACK)
            & libchess::lookups::square(libchess::constants::C8)) {
            scoreMG += 2;
        }
        if (board.piece_type_bb(libchess::constants::BISHOP, libchess::constants::BLACK)
            & libchess::lookups::square(libchess::constants::F8)) {
            scoreMG += 2;
        }
    }

    // bishop pair
    if (board.piece_type_bb(libchess::constants::BISHOP, libchess::constants::WHITE).popcount() >= 2) {
        scoreMG += 25;
        scoreEG += 40;
    }
    if (board.piece_type_bb(libchess::constants::BISHOP, libchess::constants::BLACK).popcount() >= 2) {
        scoreMG -= 25;
        scoreEG -= 40;
    }

    // rook on (semi)open files
    libchess::Bitboard rooks = board.piece_type_bb(libchess::constants::ROOK);
    while (rooks) {
        libchess::Square currRook = rooks.forward_bitscan();
        libchess::File rookFile = currRook.file();

        // white
        if (!*board.color_of(currRook)) {
            // is the file semi open?
            if (!(board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE) & libchess::lookups::FILE_MASK[rookFile])) {
                scoreMG += 10;
                scoreEG += 10;

                // is the file fully open?
                if (!(board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK) & libchess::lookups::FILE_MASK[rookFile])) {
                    scoreMG += 10;
                    scoreEG += 10;
                }
            }
        }
            // black
        else {
            // is the file semi open?
            if (!(board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK) & libchess::lookups::FILE_MASK[rookFile])) {
                scoreMG -= 10;
                scoreEG -= 10;

                // is the file fully open?
                if (!(board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE) & libchess::lookups::FILE_MASK[rookFile])) {
                    scoreMG -= 10;
                    scoreEG -= 10;
                }
            }
        }

        rooks.forward_popbit();
    }

    // space advantages
    // only relevant if not in an endgame.  3000 seems like a decent number.  It was totally pulled out of my ass
    if (nonPawnMaterial(true, board) + nonPawnMaterial(false, board) > 3000) {
        // white
        libchess::Bitboard wCenterAttacks = centerWhite & wAttackMap;
        scoreMG += 5 * wCenterAttacks.popcount();
        scoreEG += 5 * wCenterAttacks.popcount();

        // black
        libchess::Bitboard bCenterAttacks = centerBlack & bAttackMap;
        scoreMG += 5 * bCenterAttacks.popcount();
        scoreEG += 5 * bCenterAttacks.popcount();
    }

    // get the phase for tapered eval
    int phase = getPhase(board);

    // for negamax to work, we must return a non-adjusted score for white
    // and a negated score for black
    int finalScore = !board.side_to_move() ? ((scoreMG * (256 - phase)) + (scoreEG * phase)) / 256 : -((scoreMG * (256 - phase)) + (scoreEG * phase)) / 256;
    eNode->score = finalScore;

    return finalScore;
}

// evaluates knights for tropism
template<bool white>
void Anduril::evaluateKnights(libchess::Position &board, libchess::Square square) {
    // grab the attacked squares for the piece
    libchess::Bitboard attackedSquares = libchess::lookups::knight_attacks(square);
    libchess::Bitboard kingZoneAttacks;

    // white
    if constexpr (white) {
        // get the squares we can actually move to
        attackedSquares &= ~(board.color_bb(libchess::constants::WHITE));

        // mobility
        whiteMobility[0] += 4 * (attackedSquares.popcount() - 4);
        whiteMobility[1] += 4 * (attackedSquares.popcount() - 4);

        // king zone attacks
        wAttackMap |= attackedSquares;
        kingZoneAttacks = attackedSquares & kingZoneBBB;
    }
        // black
    else {
        // get the squares we can actually move to
        attackedSquares &= ~(board.color_bb(libchess::constants::BLACK));

        // mobility
        blackMobility[0] += 4 * (attackedSquares.popcount() - 4);
        blackMobility[1] += 4 * (attackedSquares.popcount() - 4);

        // king zone attacks
        bAttackMap |= attackedSquares;
        kingZoneAttacks = attackedSquares & kingZoneWBB;
    }

    if (kingZoneAttacks) {
        attackCount[white]++;
        attackWeight[white] += 2 * kingZoneAttacks.popcount();
    }
}

// evaluates bishops for tropism
template<bool white>
void Anduril::evaluateBishops(libchess::Position &board, libchess::Square square) {
    // grab the attacked squares for the piece
    libchess::Bitboard attackedSquares = libchess::lookups::bishop_attacks(square, board.occupancy_bb());
    libchess::Bitboard kingZoneAttacks;

    // white
    if constexpr (white) {
        attackedSquares &= ~(board.color_bb(libchess::constants::WHITE));

        // mobility
        whiteMobility[0] += 3 * (attackedSquares.popcount() - 7);
        whiteMobility[1] += 3 * (attackedSquares.popcount() - 7);

        // king zone attacks
        wAttackMap |= attackedSquares;
        kingZoneAttacks = attackedSquares & kingZoneBBB;
    }
        // black
    else {
        attackedSquares &= ~(board.color_bb(libchess::constants::BLACK));

        // mobility
        blackMobility[0] += 3 * (attackedSquares.popcount() - 7);
        blackMobility[1] += 3 * (attackedSquares.popcount() - 7);

        // king zone attacks
        bAttackMap |= attackedSquares;
        kingZoneAttacks = attackedSquares & kingZoneWBB;
    }

    if (kingZoneAttacks) {
        attackCount[white]++;
        attackWeight[white] += 2 * kingZoneAttacks.popcount();
    }
}

// evaluates rooks for tropism
template<bool white>
void Anduril::evaluateRooks(libchess::Position &board, libchess::Square square) {
    // grab the attacked squares for the piece
    libchess::Bitboard attackedSquares = libchess::lookups::rook_attacks(square, board.occupancy_bb());
    libchess::Bitboard kingZoneAttacks;

    // white
    if constexpr (white) {
        attackedSquares &= ~(board.color_bb(libchess::constants::WHITE));

        // mobility
        whiteMobility[0] += 2 * (attackedSquares.popcount() - 7);
        whiteMobility[1] += 4 * (attackedSquares.popcount() - 7);

        // king zone attacks
        wAttackMap |= attackedSquares;
        kingZoneAttacks = attackedSquares & kingZoneBBB;
    }
        // black
    else {
        attackedSquares &= ~(board.color_bb(libchess::constants::BLACK));

        // mobility
        blackMobility[0] += 2 * (attackedSquares.popcount() - 7);
        blackMobility[1] += 4 * (attackedSquares.popcount() - 7);

        // king zone attacks
        bAttackMap |= attackedSquares;
        bAttackMap |= attackedSquares;
        kingZoneAttacks = attackedSquares & kingZoneWBB;
    }

    if (kingZoneAttacks) {
        attackCount[white]++;
        attackWeight[white] += 3 * kingZoneAttacks.popcount();
    }
}

// evaluates queens for tropism
template<bool white>
void Anduril::evaluateQueens(libchess::Position &board, libchess::Square square) {
    // grab the attacked squares for the piece
    libchess::Bitboard attackedSquares = libchess::lookups::queen_attacks(square, board.occupancy_bb());
    libchess::Bitboard kingZoneAttacks;

    // white
    if constexpr (white) {
        attackedSquares &= ~(board.color_bb(libchess::constants::WHITE));

        // mobility
        whiteMobility[0] += 1 * (attackedSquares.popcount() - 14);
        whiteMobility[1] += 2 * (attackedSquares.popcount() - 14);

        // king zone attacks
        wAttackMap |= attackedSquares;
        kingZoneAttacks = attackedSquares & kingZoneBBB;
    }
        // black
    else {
        attackedSquares &= ~(board.color_bb(libchess::constants::BLACK));

        // mobility
        blackMobility[0] += 1 * (attackedSquares.popcount() - 14);
        blackMobility[1] += 2 * (attackedSquares.popcount() - 14);

        // king zone attacks
        bAttackMap |= attackedSquares;
        kingZoneAttacks = attackedSquares & kingZoneWBB;
    }

    if (kingZoneAttacks) {
        attackCount[white]++;
        attackWeight[white] += 3 * kingZoneAttacks.popcount();
    }
}

// finds the raw material score for the position
std::tuple<int, int> Anduril::getMaterialScore(libchess::Position &board) {
    std::tuple<int, int> score = {0, 0};

    // we first need a bitboard with every piece on the board
    libchess::Bitboard pieces = board.occupancy_bb();

    // a few of the features use the pawn bitboards, we grab them here so that we only have to do it once
    libchess::Bitboard whitePawns = board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE);
    libchess::Bitboard blackPawns = board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK);

    // loop goes through all the pieces
    while (pieces) {
        // grab the next piece
        libchess::Square square = pieces.forward_bitscan();
        std::optional<libchess::Piece> currPiece = board.piece_on(square);

        if (currPiece) {
            switch (currPiece->type()) {
                case libchess::constants::PAWN:
                    if (!currPiece->color()) {
                        int tableCoords = ((7 - (square / 8)) * 8) + square % 8;
                        std::get<0>(score) += pMG + pawnSquareTableMG[tableCoords];
                        std::get<1>(score) += pEG + pawnSquareTableEG[tableCoords];
                    }
                    else {
                        std::get<0>(score) -= pMG + pawnSquareTableMG[square];
                        std::get<1>(score) -= pEG + pawnSquareTableEG[square];
                    }
                    break;
                case libchess::constants::KNIGHT:
                    if (!currPiece->color()) {
                        int tableCoords = ((7 - (square / 8)) * 8) + square % 8;
                        std::get<0>(score) += kMG + knightSquareTableMG[tableCoords];
                        std::get<1>(score) += kEG + knightSquareTableEG[tableCoords];

                        std::get<0>(score) += knightPawnBonus[whitePawns.popcount()];
                        std::get<1>(score) += knightPawnBonus[whitePawns.popcount()];

                        // outposts
                        if (square > libchess::constants::H3
                            && (board.attackers_to(square, libchess::constants::WHITE) & whitePawns)
                            && !(board.attackers_to(square, libchess::constants::BLACK) & blackPawns)) {
                            std::get<0>(score) += oMG;
                            std::get<1>(score) += oEG;
                        }

                        // trapped knights
                        // Knight on H8 with black pawns on either F7 or H7
                        if (square == libchess::constants::H8) {
                            if ((libchess::lookups::square(libchess::constants::H7) & blackPawns)
                                 || (libchess::lookups::square(libchess::constants::F7) & blackPawns)) {
                                std::get<0>(score) -= tMG;
                                std::get<1>(score) -= tEG;
                            }
                        }
                        // Knight on A8 with black pawns on either A7 or C7
                        else if (square == libchess::constants::A8) {
                            if ((libchess::lookups::square(libchess::constants::A7) & blackPawns)
                                 || (libchess::lookups::square(libchess::constants::C7) & blackPawns)) {
                                std::get<0>(score) -= tMG;
                                std::get<1>(score) -= tEG;
                            }
                        }
                        // Knight on H7 with black pawns on G7 and either H6 or F6
                        else if (square == libchess::constants::H7) {
                            if ((libchess::lookups::square(libchess::constants::G7) & blackPawns)
                                && ((libchess::lookups::square(libchess::constants::H6) & blackPawns)
                                    || (libchess::lookups::square(libchess::constants::F6) & blackPawns))) {
                                std::get<0>(score) -= tMG;
                                std::get<1>(score) -= tEG;
                            }
                        }
                        // Knight on A7 with black pawns on B7 and either A6 or C6
                        else if (square == libchess::constants::A7) {
                            if ((libchess::lookups::square(libchess::constants::B7) & blackPawns)
                                && ((libchess::lookups::square(libchess::constants::A6) & blackPawns)
                                    || (libchess::lookups::square(libchess::constants::C6) & blackPawns))) {
                                std::get<0>(score) -= tMG;
                                std::get<1>(score) -= tEG;
                            }
                        }

                        evaluateKnights<true>(board, square);

                    }
                    else {
                        std::get<0>(score) -= kMG + knightSquareTableMG[square];
                        std::get<1>(score) -= kEG + knightSquareTableEG[square];

                        std::get<0>(score) -= knightPawnBonus[blackPawns.popcount()];
                        std::get<1>(score) -= knightPawnBonus[blackPawns.popcount()];

                        // outposts
                        if (square < libchess::constants::H6
                            && (board.attackers_to(square, libchess::constants::BLACK) & blackPawns)
                            && !(board.attackers_to(square, libchess::constants::WHITE) & whitePawns)) {
                            std::get<0>(score) -= oMG;
                            std::get<1>(score) -= oEG;
                        }

                        // trapped knights
                        // Knight on H1 with white pawns on either H2 or F2
                        if (square == libchess::constants::H1) {
                            if ((libchess::lookups::square(libchess::constants::H2) & whitePawns)
                                || (libchess::lookups::square(libchess::constants::F2) & whitePawns)) {
                                std::get<0>(score) += tMG;
                                std::get<1>(score) += tEG;
                            }
                        }
                        // Knight on A1 with white pawns on either A2 or C2
                        else if (square == libchess::constants::A1) {
                            if ((libchess::lookups::square(libchess::constants::A2) & whitePawns)
                                || (libchess::lookups::square(libchess::constants::C2) & whitePawns)) {
                                std::get<0>(score) += tMG;
                                std::get<1>(score) += tEG;
                            }
                        }
                        // Knight on H2 with white pawns on G2 and either H3 or F3
                        else if (square == libchess::constants::H2) {
                            if ((libchess::lookups::square(libchess::constants::G2) & whitePawns)
                                && ((libchess::lookups::square(libchess::constants::H3) & whitePawns)
                                    || (libchess::lookups::square(libchess::constants::F3) & whitePawns))) {
                                std::get<0>(score) += tMG;
                                std::get<1>(score) += tEG;
                            }
                        }
                        // Knight on A2 with white pawns on B2 and either A3 or C3
                        else if (square == libchess::constants::A2) {
                            if ((libchess::lookups::square(libchess::constants::B2) & whitePawns)
                                && ((libchess::lookups::square(libchess::constants::A3) & whitePawns)
                                    || (libchess::lookups::square(libchess::constants::C3) & whitePawns))) {
                                std::get<0>(score) += tMG;
                                std::get<1>(score) += tEG;
                            }
                        }
                        evaluateKnights<false>(board, square);

                    }
                    break;
                case libchess::constants::BISHOP:
                    if (!currPiece->color()) {
                        int tableCoords = ((7 - (square / 8)) * 8) + square % 8;
                        std::get<0>(score) += 365 + bishopSquareTableMG[tableCoords];
                        std::get<1>(score) += 297 + bishopSquareTableEG[tableCoords];

                        // fianchetto
                        if ((square == libchess::constants::G2 && board.king_square(libchess::constants::WHITE) == libchess::constants::G1)
                            || (square == libchess::constants::B2 && board.king_square(libchess::constants::WHITE) == libchess::constants::B1)) {
                            std::get<0>(score) += 20;
                            std::get<1>(score) += 20;
                        }
                        evaluateBishops<true>(board, square);

                    }
                    else {
                        std::get<0>(score) -= 365 + bishopSquareTableMG[square];
                        std::get<1>(score) -= 297 + bishopSquareTableEG[square];

                        // fianchetto
                        if ((square == libchess::constants::G7 && board.king_square(libchess::constants::BLACK) == libchess::constants::G8)
                            || (square == libchess::constants::B7 && board.king_square(libchess::constants::BLACK) == libchess::constants::B8)) {
                            std::get<0>(score) -= 20;
                            std::get<1>(score) -= 20;
                        }
                        evaluateBishops<false>(board, square);

                    }
                    break;
                case libchess::constants::ROOK:
                    if (!currPiece->color()) {
                        int tableCoords = ((7 - (square / 8)) * 8) + square % 8;
                        std::get<0>(score) += 477 + rookSquareTableMG[tableCoords];
                        std::get<1>(score) += 512 + rookSquareTableEG[tableCoords];

                        std::get<0>(score) += rookPawnBonus[whitePawns.popcount()];
                        std::get<1>(score) += rookPawnBonus[whitePawns.popcount()];

                        // trapped rooks
                        libchess::Square kingSquare = board.king_square(libchess::constants::WHITE);
                        switch (square) {
                            case libchess::constants::H1:
                                if (kingSquare == libchess::constants::E1 || kingSquare == libchess::constants::F1 || kingSquare == libchess::constants::G1) {
                                    std::get<0>(score) -= 40;
                                    std::get<1>(score) -= 40;
                                }
                                break;
                            case libchess::constants::G1:
                                if (kingSquare == libchess::constants::E1 || kingSquare == libchess::constants::F1) {
                                    std::get<0>(score) -= 40;
                                    std::get<1>(score) -= 40;
                                }
                                break;
                            case libchess::constants::F1:
                                if (kingSquare == libchess::constants::E1) {
                                    std::get<0>(score) -= 40;
                                    std::get<1>(score) -= 40;
                                }
                                break;
                            case libchess::constants::A1:
                                if (kingSquare == libchess::constants::B1 || kingSquare == libchess::constants::C1 || kingSquare == libchess::constants::D1) {
                                    std::get<0>(score) -= 40;
                                    std::get<1>(score) -= 40;
                                }
                                break;
                            case libchess::constants::B1:
                                if (kingSquare == libchess::constants::C1 || kingSquare == libchess::constants::D1) {
                                    std::get<0>(score) -= 40;
                                    std::get<1>(score) -= 40;
                                }
                                break;
                            case libchess::constants::C1:
                                if (kingSquare == libchess::constants::D1) {
                                    std::get<0>(score) -= 40;
                                    std::get<1>(score) -= 40;
                                }
                                break;
                        }
                        evaluateRooks<true>(board, square);

                    }
                    else {
                        std::get<0>(score) -= 477 + rookSquareTableMG[square];
                        std::get<1>(score) -= 512 + rookSquareTableEG[square];

                        std::get<0>(score) -= rookPawnBonus[blackPawns.popcount()];
                        std::get<1>(score) -= rookPawnBonus[blackPawns.popcount()];

                        // trapped rooks
                        libchess::Square kingSquare = board.king_square(libchess::constants::BLACK);
                        switch (square) {
                            case libchess::constants::H8:
                                if (kingSquare == libchess::constants::E8 || kingSquare == libchess::constants::F8 || kingSquare == libchess::constants::G8) {
                                    std::get<0>(score) += 40;
                                    std::get<1>(score) += 40;
                                }
                                break;
                            case libchess::constants::G8:
                                if (kingSquare == libchess::constants::E8 || kingSquare == libchess::constants::F8) {
                                    std::get<0>(score) += 40;
                                    std::get<1>(score) += 40;
                                }
                                break;
                            case libchess::constants::F8:
                                if (kingSquare == libchess::constants::E8) {
                                    std::get<0>(score) += 40;
                                    std::get<1>(score) += 40;
                                }
                                break;
                            case libchess::constants::A8:
                                if (kingSquare == libchess::constants::B8 || kingSquare == libchess::constants::C8 || kingSquare == libchess::constants::D8) {
                                    std::get<0>(score) += 40;
                                    std::get<1>(score) += 40;
                                }
                                break;
                            case libchess::constants::B8:
                                if (kingSquare == libchess::constants::C8 || kingSquare == libchess::constants::D8) {
                                    std::get<0>(score) += 40;
                                    std::get<1>(score) += 40;
                                }
                                break;
                            case libchess::constants::C8:
                                if (kingSquare == libchess::constants::D8) {
                                    std::get<0>(score) += 40;
                                    std::get<1>(score) += 40;
                                }
                                break;
                        }
                        evaluateRooks<false>(board, square);

                    }
                    break;
                case libchess::constants::QUEEN:
                    if (!currPiece->color()) {
                        int tableCoords = ((7 - (square / 8)) * 8) + square % 8;
                        std::get<0>(score) += 1025 + queenSquareTableMG[tableCoords];
                        std::get<1>(score) += 936  + queenSquareTableEG[tableCoords];
                        evaluateQueens<true>(board, square);

                    }
                    else {
                        std::get<0>(score) -= 1025 + queenSquareTableMG[square];
                        std::get<1>(score) -= 936  + queenSquareTableEG[square];
                        evaluateQueens<false>(board, square);

                    }
                    break;
                case libchess::constants::KING:
                    if (!currPiece->color()) {
                        int tableCoords = ((7 - (square / 8)) * 8) + square % 8;
                        std::get<0>(score) += kingSquareTableMG[tableCoords];
                        std::get<1>(score) += kingSquareTableEG[tableCoords];
                    }
                    else {
                        std::get<0>(score) -= kingSquareTableMG[square];
                        std::get<1>(score) -= kingSquareTableEG[square];
                    }
                    break;


            }
        }

        // get rid of the last piece on the board to prepare for scoring the next
        pieces.forward_popbit();
    }

    return score;
}

// finds the pawn structure bonus for the position
int Anduril::getPawnScore(libchess::Position &board) {
    // first check for a transposition
    uint64_t hash = board.pawn_hash();
    SimpleNode *node = pTable[hash];
    if (node->key == hash) {
        return node->score;
    }
    else {
        node->key = hash;
    }

    int score = 0;

    // boards with white, black, and both color pawns
    libchess::Bitboard whitePawns = board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE);
    libchess::Bitboard blackPawns = board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK);
    libchess::Bitboard pawns = board.piece_type_bb(libchess::constants::PAWN);

    while (pawns) {
        libchess::Square currPawnSquare = pawns.forward_bitscan();
        libchess::File file = currPawnSquare.file();
        if (!*board.color_of(currPawnSquare)) {
            switch (file) {
                case libchess::constants::FILE_A:
                    // isolated pawn
                    if (!(whitePawns & libchess::lookups::FILE_B_MASK)) {
                        score -= 13;
                    }

                    // passed pawn
                    if (!((blackPawns & libchess::lookups::FILE_A_MASK) | (blackPawns & libchess::lookups::FILE_B_MASK))) {
                        score += 24;
                    }

                    break;
                case libchess::constants::FILE_H:
                    // isolated pawn
                    if (!(whitePawns & libchess::lookups::FILE_G_MASK)) {
                        score -= 13;
                    }

                    // passed pawn
                    if (!((blackPawns & libchess::lookups::FILE_H_MASK) | (blackPawns & libchess::lookups::FILE_G_MASK))) {
                        score += 24;
                    }

                    break;
                default:
                    // isolated pawn
                    if (!((whitePawns & libchess::lookups::FILE_MASK[file - 1]) | (whitePawns & libchess::lookups::FILE_MASK[file + 1]))) {
                        score -= 13;
                    }

                    // passed pawn
                    if (!((blackPawns & libchess::lookups::FILE_MASK[file]) | (blackPawns & libchess::lookups::FILE_MASK[file - 1]) | (blackPawns & libchess::lookups::FILE_MASK[file + 1]))) {
                        score += 24;
                    }

                    break;
            }

            // defended pawns
            libchess::Bitboard defenders = board.attackers_to(currPawnSquare, libchess::constants::WHITE) & whitePawns;
            if (defenders) {
                score += 11 * defenders.popcount();
            }

        }
        else {
            switch (file) {
                case libchess::constants::FILE_A:
                    // isolated pawn
                    if (!(blackPawns & libchess::lookups::FILE_B_MASK)) {
                        score += 13;
                    }

                    // passed pawn
                    if (!((whitePawns & libchess::lookups::FILE_A_MASK) | (whitePawns & libchess::lookups::FILE_B_MASK))) {\
                        score -= 24;
                    }

                    break;
                case libchess::constants::FILE_H:
                    // isolated pawn
                    if (!(blackPawns & libchess::lookups::FILE_G_MASK)) {
                        score += 13;
                    }

                    // passed pawn
                    if (!((whitePawns & libchess::lookups::FILE_H_MASK) | (whitePawns & libchess::lookups::FILE_G_MASK))) {
                        score -= 24;
                    }

                    break;
                default:
                    // isolated pawn
                    if (!((blackPawns & libchess::lookups::FILE_MASK[file - 1]) | (blackPawns & libchess::lookups::FILE_MASK[file + 1]))) {
                        score += 13;
                    }

                    // passed pawn
                    if (!((whitePawns & libchess::lookups::FILE_MASK[file]) | (whitePawns & libchess::lookups::FILE_MASK[file - 1]) | (whitePawns & libchess::lookups::FILE_MASK[file + 1]))) {
                        score -= 24;
                    }

                    break;
            }

            // defended pawns
            libchess::Bitboard defenders = board.attackers_to(currPawnSquare, libchess::constants::BLACK) & blackPawns;
            if (defenders) {
                score -= 11 * defenders.popcount();
            }
        }

        pawns.forward_popbit();
    }

    // doubled pawns
    for (auto file : libchess::lookups::FILE_MASK) {
        int pawnsOnFileWhite = (file & whitePawns).popcount();
        int pawnsOnFileBlack = (file & blackPawns).popcount();
        if (pawnsOnFileWhite > 1) {
            score -= 10 * (pawnsOnFileWhite - 1);
        }
        if (pawnsOnFileBlack > 1) {
            score += 10 * (pawnsOnFileBlack - 1);
        }
    }

    node->score = score;

    return score;

}

// finds the king safety bonus for the position
int Anduril::getKingSafety(libchess::Position &board, libchess::Square whiteKing, libchess::Square blackKing) {
    libchess::Bitboard whitePawns = board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE);
    libchess::Bitboard blackPawns = board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK);
    int score = 0;

    // white shield
    if (whiteKing.rank() == libchess::constants::RANK_1 && whiteKing != libchess::constants::D1 && whiteKing != libchess::constants::E1) {
        // queen side
        if (whiteKing == libchess::constants::A1 || whiteKing == libchess::constants::B1 || whiteKing == libchess::constants::C1) {
            // 'A' file
            if (!(libchess::lookups::square(libchess::constants::A2) & whitePawns)
                && !(libchess::lookups::square(libchess::constants::A3) & whitePawns)) {
                score -= 20;
                if (!(whitePawns & libchess::lookups::file_mask(libchess::constants::FILE_A))) {
                    score -= 20;
                }
            }
            // 'B' file
            if (!(libchess::lookups::square(libchess::constants::B2) & whitePawns)
                && !(libchess::lookups::square(libchess::constants::B3) & whitePawns)) {
                score -= 20;
                if (!(whitePawns & libchess::lookups::file_mask(libchess::constants::FILE_B))) {
                    score -= 20;
                }
            }
            // 'C' file
            if (!(libchess::lookups::square(libchess::constants::C2) & whitePawns)
                && !(libchess::lookups::square(libchess::constants::C3) & whitePawns)) {
                score -= 20;
                if (!(whitePawns & libchess::lookups::file_mask(libchess::constants::FILE_C))) {
                    score -= 20;
                }
            }
        }
            // king side
        else {
            // 'F' file
            if (!(libchess::lookups::square(libchess::constants::F2) & whitePawns)
                && !(libchess::lookups::square(libchess::constants::F3) & whitePawns)) {
                score -= 20;
                if (!(whitePawns & libchess::lookups::file_mask(libchess::constants::FILE_F))) {
                    score -= 20;
                }
            }
            // 'G' file
            if (!(libchess::lookups::square(libchess::constants::G2) & whitePawns)
                && !(libchess::lookups::square(libchess::constants::G3) & whitePawns)) {
                score -= 20;
                if (!(whitePawns & libchess::lookups::file_mask(libchess::constants::FILE_G))) {
                    score -= 20;
                }
            }
            // 'H' file
            if (!(libchess::lookups::square(libchess::constants::H2) & whitePawns)
                && !(libchess::lookups::square(libchess::constants::H3) & whitePawns)) {
                score -= 20;
                if (!(whitePawns & libchess::lookups::file_mask(libchess::constants::FILE_H))) {
                    score -= 20;
                }
            }
        }
    }

    // black shield
    if (blackKing.rank() == libchess::constants::RANK_8 && blackKing != libchess::constants::D8 && blackKing != libchess::constants::E8) {
        // queen side
        if (blackKing == libchess::constants::A8 || blackKing == libchess::constants::B8 || blackKing == libchess::constants::C8) {
            // 'A' file
            if (!(libchess::lookups::square(libchess::constants::A7) & blackPawns)
                && !(libchess::lookups::square(libchess::constants::A6) & blackPawns)) {
                score += 20;
                if (!(blackPawns & libchess::lookups::file_mask(libchess::constants::FILE_A))) {
                    score += 20;
                }
            }
            // 'B' file
            if (!(libchess::lookups::square(libchess::constants::B7) & blackPawns)
                && !(libchess::lookups::square(libchess::constants::B6) & blackPawns)) {
                score += 20;
                if (!(blackPawns & libchess::lookups::file_mask(libchess::constants::FILE_B))) {
                    score += 20;
                }
            }
            // 'C' file
            if (!(libchess::lookups::square(libchess::constants::C7) & blackPawns)
                && !(libchess::lookups::square(libchess::constants::C6) & blackPawns)) {
                score += 20;
                if (!(blackPawns & libchess::lookups::file_mask(libchess::constants::FILE_C))) {
                    score += 20;
                }
            }
        }
            // king side
        else {
            // 'F' file
            if (!(libchess::lookups::square(libchess::constants::F7) & blackPawns)
                && !(libchess::lookups::square(libchess::constants::F6) & blackPawns)) {
                score += 20;
                if (!(blackPawns & libchess::lookups::file_mask(libchess::constants::FILE_F))) {
                    score += 20;
                }
            }
            // 'G' file
            if (!(libchess::lookups::square(libchess::constants::G7) & blackPawns)
                && !(libchess::lookups::square(libchess::constants::G6) & blackPawns)) {
                score += 20;
                if (!(blackPawns & libchess::lookups::file_mask(libchess::constants::FILE_G))) {
                    score += 20;
                }
            }
            // 'H' file
            if (!(libchess::lookups::square(libchess::constants::H7) & blackPawns)
                && !(libchess::lookups::square(libchess::constants::H6) & blackPawns)) {
                score += 20;
                if (!(blackPawns & libchess::lookups::file_mask(libchess::constants::FILE_H))) {
                    score += 20;
                }
            }
        }
    }

    // pawn storms
    // white
    libchess::Bitboard stormZone = libchess::Bitboard();
    // first make the storm zone files
    libchess::Bitboard stormZoneFiles = libchess::Bitboard();
    switch (whiteKing.file()) {
        case libchess::constants::FILE_A:
            stormZoneFiles |= libchess::lookups::FILE_A_MASK | libchess::lookups::FILE_B_MASK | libchess::lookups::FILE_C_MASK;
            break;
        case libchess::constants::FILE_B:
            stormZoneFiles |= libchess::lookups::FILE_A_MASK | libchess::lookups::FILE_B_MASK | libchess::lookups::FILE_C_MASK | libchess::lookups::FILE_D_MASK;
            break;
        case libchess::constants::FILE_G:
            stormZoneFiles |= libchess::lookups::FILE_E_MASK | libchess::lookups::FILE_F_MASK | libchess::lookups::FILE_G_MASK | libchess::lookups::FILE_H_MASK;
            break;
        case libchess::constants::FILE_H:
            stormZoneFiles |= libchess::lookups::FILE_F_MASK | libchess::lookups::FILE_G_MASK | libchess::lookups::FILE_H_MASK;
            break;
        default:
            stormZoneFiles |= libchess::lookups::FILE_MASK[whiteKing.file().value() - 2];
            stormZoneFiles |= libchess::lookups::FILE_MASK[whiteKing.file().value() - 1];
            stormZoneFiles |= libchess::lookups::FILE_MASK[whiteKing.file().value()];
            stormZoneFiles |= libchess::lookups::FILE_MASK[whiteKing.file().value() + 1];
            stormZoneFiles |= libchess::lookups::FILE_MASK[whiteKing.file().value() + 2];
            break;
    }

    // we now look for opposing pawns
    switch (whiteKing.rank()) {
        case libchess::constants::RANK_1:
            stormZone = (stormZoneFiles & libchess::lookups::RANK_1_MASK) | (stormZoneFiles & libchess::lookups::RANK_2_MASK) | (stormZoneFiles & libchess::lookups::RANK_3_MASK);
            if (stormZone & blackPawns) {
                score -= 5 * (stormZone & blackPawns).popcount();
            }
            break;
        case libchess::constants::RANK_2:
            stormZone = (stormZoneFiles & libchess::lookups::RANK_1_MASK) | (stormZoneFiles & libchess::lookups::RANK_2_MASK) | (stormZoneFiles & libchess::lookups::RANK_3_MASK) | (stormZoneFiles & libchess::lookups::RANK_4_MASK);
            if (stormZone & blackPawns) {
                score -= 5 * (stormZone & blackPawns).popcount();
            }
            break;
        case libchess::constants::RANK_7:
            stormZone = (stormZoneFiles & libchess::lookups::RANK_8_MASK) | (stormZoneFiles & libchess::lookups::RANK_7_MASK) | (stormZoneFiles & libchess::lookups::RANK_6_MASK) | (stormZoneFiles & libchess::lookups::RANK_5_MASK);
            if (stormZone & blackPawns) {
                score -= 5 * (stormZone & blackPawns).popcount();
            }
            break;
        case libchess::constants::RANK_8:
            stormZone = (stormZoneFiles & libchess::lookups::RANK_8_MASK) | (stormZoneFiles & libchess::lookups::RANK_7_MASK) | (stormZoneFiles & libchess::lookups::RANK_6_MASK);
            if (stormZone & blackPawns) {
                score -= 5 * (stormZone & blackPawns).popcount();
            }
            break;
        default:
            stormZone |= stormZoneFiles & libchess::lookups::RANK_MASK[whiteKing.rank().value() - 2];
            stormZone |= stormZoneFiles & libchess::lookups::RANK_MASK[whiteKing.rank().value() - 1];
            stormZone |= stormZoneFiles & libchess::lookups::RANK_MASK[whiteKing.rank().value()];
            stormZone |= stormZoneFiles & libchess::lookups::RANK_MASK[whiteKing.rank().value() + 1];
            stormZone |= stormZoneFiles & libchess::lookups::RANK_MASK[whiteKing.rank().value() + 2];
            if (stormZone & blackPawns) {
                score -= 5 * (stormZone & blackPawns).popcount();
            }
            break;
    }

    // black
    stormZone = libchess::Bitboard();
    // first make the storm zone files
    stormZoneFiles = libchess::Bitboard();
    switch (blackKing.file()) {
        case libchess::constants::FILE_A:
            stormZoneFiles |= libchess::lookups::FILE_A_MASK | libchess::lookups::FILE_B_MASK | libchess::lookups::FILE_C_MASK;
            break;
        case libchess::constants::FILE_B:
            stormZoneFiles |= libchess::lookups::FILE_A_MASK | libchess::lookups::FILE_B_MASK | libchess::lookups::FILE_C_MASK | libchess::lookups::FILE_D_MASK;
            break;
        case libchess::constants::FILE_G:
            stormZoneFiles |= libchess::lookups::FILE_E_MASK | libchess::lookups::FILE_F_MASK | libchess::lookups::FILE_G_MASK | libchess::lookups::FILE_H_MASK;
            break;
        case libchess::constants::FILE_H:
            stormZoneFiles |= libchess::lookups::FILE_F_MASK | libchess::lookups::FILE_G_MASK | libchess::lookups::FILE_H_MASK;
            break;
        default:
            stormZoneFiles |= libchess::lookups::FILE_MASK[blackKing.file().value() - 2];
            stormZoneFiles |= libchess::lookups::FILE_MASK[blackKing.file().value() - 1];
            stormZoneFiles |= libchess::lookups::FILE_MASK[blackKing.file().value()];
            stormZoneFiles |= libchess::lookups::FILE_MASK[blackKing.file().value() + 1];
            stormZoneFiles |= libchess::lookups::FILE_MASK[blackKing.file().value() + 2];
            break;
    }

    // we now look for opposing pawns
    switch (blackKing.rank()) {
        case libchess::constants::RANK_1:
            stormZone = (stormZoneFiles & libchess::lookups::RANK_1_MASK) | (stormZoneFiles & libchess::lookups::RANK_2_MASK) | (stormZoneFiles & libchess::lookups::RANK_3_MASK);
            if (stormZone & whitePawns) {
                score += 5 * (stormZone & whitePawns).popcount();
            }
            break;
        case libchess::constants::RANK_2:
            stormZone = (stormZoneFiles & libchess::lookups::RANK_1_MASK) | (stormZoneFiles & libchess::lookups::RANK_2_MASK) | (stormZoneFiles & libchess::lookups::RANK_3_MASK) | (stormZoneFiles & libchess::lookups::RANK_4_MASK);
            if (stormZone & whitePawns) {
                score += 5 * (stormZone & whitePawns).popcount();
            }
            break;
        case libchess::constants::RANK_7:
            stormZone = (stormZoneFiles & libchess::lookups::RANK_8_MASK) | (stormZoneFiles & libchess::lookups::RANK_7_MASK) | (stormZoneFiles & libchess::lookups::RANK_6_MASK) | (stormZoneFiles & libchess::lookups::RANK_5_MASK);
            if (stormZone & whitePawns) {
                score += 5 * (stormZone & whitePawns).popcount();
            }
            break;
        case libchess::constants::RANK_8:
            stormZone = (stormZoneFiles & libchess::lookups::RANK_8_MASK) | (stormZoneFiles & libchess::lookups::RANK_7_MASK) | (stormZoneFiles & libchess::lookups::RANK_6_MASK);
            if (stormZone & whitePawns) {
                score += 5 * (stormZone & whitePawns).popcount();
            }
            break;
        default:
            stormZone |= stormZoneFiles & libchess::lookups::RANK_MASK[blackKing.rank().value() - 2];
            stormZone |= stormZoneFiles & libchess::lookups::RANK_MASK[blackKing.rank().value() - 1];
            stormZone |= stormZoneFiles & libchess::lookups::RANK_MASK[blackKing.rank().value()];
            stormZone |= stormZoneFiles & libchess::lookups::RANK_MASK[blackKing.rank().value() + 1];
            stormZone |= stormZoneFiles & libchess::lookups::RANK_MASK[blackKing.rank().value() + 2];
            if (stormZone & whitePawns) {
                score += 5 * (stormZone & whitePawns).popcount();
            }
            break;
    }

    return score;
}

// gets the phase of the game for evalutation
int Anduril::getPhase(libchess::Position &board) {
    double pawn = Pph, knight = Kph, bishop = Bph, rook = Rph, queen = Qph;
    double totalPhase = pawn*16 + knight*4 + bishop*4 + rook*4 + queen*2;

    double phase = totalPhase;
    phase -= board.piece_type_bb(libchess::constants::PAWN).popcount() * pawn;
    phase -= board.piece_type_bb(libchess::constants::KNIGHT).popcount() * knight;
    phase -= board.piece_type_bb(libchess::constants::BISHOP).popcount() * bishop;
    phase -= board.piece_type_bb(libchess::constants::ROOK).popcount() * rook;
    phase -= board.piece_type_bb(libchess::constants::QUEEN).popcount() * queen;

    return (int)((phase * 256 + (totalPhase / 2)) / totalPhase);
}