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
    std::vector<int> matScore = getMaterialScore(board);
    scoreMG += matScore[0];
    scoreEG += matScore[1];

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
    if (*board.piece_on(libchess::constants::D1) != libchess::constants::WHITE_QUEEN) {
        if (*board.piece_on(libchess::constants::B1) == libchess::constants::WHITE_KNIGHT) {
            scoreMG -= 2;
            scoreEG -= 2;
        }
        if (*board.piece_on(libchess::constants::G1) == libchess::constants::WHITE_KNIGHT) {
            scoreMG -= 2;
            scoreEG -= 2;
        }
        if (*board.piece_on(libchess::constants::C1) == libchess::constants::WHITE_BISHOP) {
            scoreMG -= 2;
            scoreEG -= 2;
        }
        if (*board.piece_on(libchess::constants::F1) == libchess::constants::WHITE_BISHOP) {
            scoreMG -= 2;
            scoreEG -= 2;
        }
    }
    if (*board.piece_on(libchess::constants::D8) != libchess::constants::BLACK_QUEEN) {
        if (*board.piece_on(libchess::constants::B8) == libchess::constants::BLACK_KNIGHT) {
            scoreMG += 2;
            scoreEG += 2;
        }
        if (*board.piece_on(libchess::constants::G8) == libchess::constants::BLACK_KNIGHT) {
            scoreMG += 2;
            scoreEG += 2;
        }
        if (*board.piece_on(libchess::constants::C8) == libchess::constants::BLACK_BISHOP) {
            scoreMG += 2;
            scoreEG += 2;
        }
        if (*board.piece_on(libchess::constants::F8) == libchess::constants::BLACK_BISHOP) {
            scoreMG += 2;
            scoreEG += 2;
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
    double phase = getPhase(board);

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
std::vector<int> Anduril::getMaterialScore(libchess::Position &board) {
    std::vector<int> score = {0, 0};

    // we first need a bitboard with every piece on the board
    libchess::Bitboard pieces = board.occupancy_bb();

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
                        score[0] += 88 + pawnSquareTableMG[tableCoords];
                        score[1] += 138 + pawnSquareTableEG[tableCoords];
                    }
                    else {
                        int tableCoords = ((7 - (square / 8)) * 8) + square % 8;
                        score[0] -= 88 + pawnSquareTableMG[square];
                        score[1] -= 138 + pawnSquareTableEG[square];
                    }
                    break;
                case libchess::constants::KNIGHT:
                    if (!currPiece->color()) {
                        int tableCoords = ((7 - (square / 8)) * 8) + square % 8;
                        score[0] += kMG + knightSquareTableMG[tableCoords];
                        score[1] += kEG + knightSquareTableEG[tableCoords];

                        score[0] += knightPawnBonus[board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE).popcount()];
                        score[1] += knightPawnBonus[board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE).popcount()];

                        // outposts
                        if (square > libchess::constants::H4
                            && (board.attackers_to(square, libchess::constants::WHITE) & board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE))
                            && !(board.attackers_to(square, libchess::constants::BLACK) & board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK))) {
                            score[0] += out;
                            score[1] += out;
                        }

                        // trapped knights
                        if ((square == libchess::constants::H8 && (board.piece_on(libchess::constants::H7)->to_char() == 'p' || board.piece_on(libchess::constants::F7)->to_char() == 'p'))
                            || (square == libchess::constants::A8 && (board.piece_on(libchess::constants::A7)->to_char() == 'p' || board.piece_on(libchess::constants::C7)->to_char() == 'p'))) {
                            score[0] -= trp;
                            score[1] -= trp;
                        }
                        else if (square == libchess::constants::H7
                                 && (board.piece_on(libchess::constants::G7)->to_char() == 'p'
                                     && (board.piece_on(libchess::constants::H6)->to_char() == 'p' || board.piece_on(libchess::constants::F6)->to_char() == 'p'))) {
                            score[0] -= trp;
                            score[1] -= trp;
                        }
                        else if (square == libchess::constants::A7
                                 && (board.piece_on(libchess::constants::B7)->to_char() == 'p'
                                     && (board.piece_on(libchess::constants::A6)->to_char() == 'p' || board.piece_on(libchess::constants::C6)->to_char() == 'p'))) {
                            score[0] -= trp;
                            score[1] -= trp;
                        }

                        evaluateKnights<true>(board, square);

                    }
                    else {
                        score[0] -= kMG + knightSquareTableMG[square];
                        score[1] -= kEG + knightSquareTableEG[square];

                        score[0] -= knightPawnBonus[board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK).popcount()];
                        score[0] -= knightPawnBonus[board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK).popcount()];

                        // outposts
                        if (square < libchess::constants::H5
                            && (board.attackers_to(square, libchess::constants::BLACK) & board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK))
                            && !(board.attackers_to(square, libchess::constants::WHITE) & board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE))) {
                            score[0] -= out;
                            score[1] -= out;
                        }

                        // trapped knights
                        if ((square == libchess::constants::H1 && (board.piece_on(libchess::constants::H2)->to_char() == 'P' || board.piece_on(libchess::constants::F2)->to_char() == 'P'))
                            || (square == libchess::constants::A1 && (board.piece_on(libchess::constants::A2)->to_char() == 'P' || board.piece_on(libchess::constants::C2)->to_char() == 'P'))) {
                            score[0] += trp;
                            score[1] += trp;
                        }
                        else if (square == libchess::constants::H2
                                 && (board.piece_on(libchess::constants::G2)->to_char() == 'P'
                                     && (board.piece_on(libchess::constants::H3)->to_char() == 'P' || board.piece_on(libchess::constants::F3)->to_char() == 'P'))) {
                            score[0] += trp;
                            score[1] += trp;
                        }
                        else if (square == libchess::constants::A2
                                 && (board.piece_on(libchess::constants::B2)->to_char() == 'P'
                                     && (board.piece_on(libchess::constants::A3)->to_char() == 'P' || board.piece_on(libchess::constants::C3)->to_char() == 'P'))) {
                            score[0] += trp;
                            score[1] += trp;
                        }
                        evaluateKnights<false>(board, square);

                    }
                    break;
                case libchess::constants::BISHOP:
                    if (!currPiece->color()) {
                        int tableCoords = ((7 - (square / 8)) * 8) + square % 8;
                        score[0] += 365 + bishopSquareTableMG[tableCoords];
                        score[1] += 297 + bishopSquareTableEG[tableCoords];

                        // fianchetto
                        if ((square == libchess::constants::G2 && board.king_square(libchess::constants::WHITE) == libchess::constants::G1)
                            || (square == libchess::constants::B2 && board.king_square(libchess::constants::WHITE) == libchess::constants::B1)) {
                            score[0] += 20;
                            score[1] += 20;
                        }
                        evaluateBishops<true>(board, square);

                    }
                    else {
                        score[0] -= 365 + bishopSquareTableMG[square];
                        score[1] -= 297 + bishopSquareTableEG[square];

                        // fianchetto
                        if ((square == libchess::constants::G7 && board.king_square(libchess::constants::BLACK) == libchess::constants::G8)
                            || (square == libchess::constants::B7 && board.king_square(libchess::constants::BLACK) == libchess::constants::B8)) {
                            score[0] -= 20;
                            score[1] -= 20;
                        }
                        evaluateBishops<false>(board, square);

                    }
                    break;
                case libchess::constants::ROOK:
                    if (!currPiece->color()) {
                        int tableCoords = ((7 - (square / 8)) * 8) + square % 8;
                        score[0] += 477 + rookSquareTableMG[tableCoords];
                        score[1] += 512 + rookSquareTableEG[tableCoords];

                        score[0] += rookPawnBonus[board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE).popcount()];
                        score[1] += rookPawnBonus[board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE).popcount()];

                        // trapped rooks
                        libchess::Square kingSquare = board.king_square(libchess::constants::WHITE);
                        switch (square) {
                            case libchess::constants::H1:
                                if (kingSquare == libchess::constants::E1 || kingSquare == libchess::constants::F1 || kingSquare == libchess::constants::G1) {
                                    score[0] -= 40;
                                    score[1] -= 40;
                                }
                                break;
                            case libchess::constants::G1:
                                if (kingSquare == libchess::constants::E1 || kingSquare == libchess::constants::F1) {
                                    score[0] -= 40;
                                    score[1] -= 40;
                                }
                                break;
                            case libchess::constants::F1:
                                if (kingSquare == libchess::constants::E1) {
                                    score[0] -= 40;
                                    score[1] -= 40;
                                }
                                break;
                            case libchess::constants::A1:
                                if (kingSquare == libchess::constants::B1 || kingSquare == libchess::constants::C1 || kingSquare == libchess::constants::D1) {
                                    score[0] -= 40;
                                    score[1] -= 40;
                                }
                                break;
                            case libchess::constants::B1:
                                if (kingSquare == libchess::constants::C1 || kingSquare == libchess::constants::D1) {
                                    score[0] -= 40;
                                    score[1] -= 40;
                                }
                                break;
                            case libchess::constants::C1:
                                if (kingSquare == libchess::constants::D1) {
                                    score[0] -= 40;
                                    score[1] -= 40;
                                }
                                break;
                        }
                        evaluateRooks<true>(board, square);

                    }
                    else {
                        score[0] -= 477 + rookSquareTableMG[square];
                        score[1] -= 512 + rookSquareTableEG[square];

                        score[0] -= rookPawnBonus[board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK).popcount()];
                        score[1] -= rookPawnBonus[board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK).popcount()];

                        // trapped rooks
                        libchess::Square kingSquare = board.king_square(libchess::constants::BLACK);
                        switch (square) {
                            case libchess::constants::H8:
                                if (kingSquare == libchess::constants::E8 || kingSquare == libchess::constants::F8 || kingSquare == libchess::constants::G8) {
                                    score[0] += 40;
                                    score[1] += 40;
                                }
                                break;
                            case libchess::constants::G8:
                                if (kingSquare == libchess::constants::E8 || kingSquare == libchess::constants::F8) {
                                    score[0] += 40;
                                    score[1] += 40;
                                }
                                break;
                            case libchess::constants::F8:
                                if (kingSquare == libchess::constants::E8) {
                                    score[0] += 40;
                                    score[1] += 40;
                                }
                                break;
                            case libchess::constants::A8:
                                if (kingSquare == libchess::constants::B8 || kingSquare == libchess::constants::C8 || kingSquare == libchess::constants::D8) {
                                    score[0] += 40;
                                    score[1] += 40;
                                }
                                break;
                            case libchess::constants::B8:
                                if (kingSquare == libchess::constants::C8 || kingSquare == libchess::constants::D8) {
                                    score[0] += 40;
                                    score[1] += 40;
                                }
                                break;
                            case libchess::constants::C8:
                                if (kingSquare == libchess::constants::D8) {
                                    score[0] += 40;
                                    score[1] += 40;
                                }
                                break;
                        }
                        evaluateRooks<false>(board, square);

                    }
                    break;
                case libchess::constants::QUEEN:
                    if (!currPiece->color()) {
                        int tableCoords = ((7 - (square / 8)) * 8) + square % 8;
                        score[0] += 1025 + queenSquareTableMG[tableCoords];
                        score[1] += 936  + queenSquareTableEG[tableCoords];
                        evaluateQueens<true>(board, square);

                    }
                    else {
                        score[0] -= 1025 + queenSquareTableMG[square];
                        score[1] -= 936  + queenSquareTableEG[square];
                        evaluateQueens<false>(board, square);

                    }
                    break;
                case libchess::constants::KING:
                    if (!currPiece->color()) {
                        int tableCoords = ((7 - (square / 8)) * 8) + square % 8;
                        score[0] += 20000 + kingSquareTableMG[tableCoords];
                        score[1] += 20000 + kingSquareTableEG[tableCoords];
                    }
                    else {
                        score[0] -= 20000 + kingSquareTableMG[square];
                        score[1] -= 20000 + kingSquareTableEG[square];
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
        if (!*board.color_of(currPawnSquare)) {
            switch (currPawnSquare.file()) {
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
                    libchess::File file = currPawnSquare.file();
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
            switch (currPawnSquare.file()) {
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
                    libchess::File file = currPawnSquare.file();
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
            if (*board.piece_on(libchess::constants::A2) != libchess::constants::WHITE_PAWN && *board.piece_on(libchess::constants::A3) != libchess::constants::WHITE_PAWN) {
                score -= 20;
                if (!(whitePawns & libchess::lookups::file_mask(libchess::constants::FILE_A))) {
                    score -= 20;
                }
            }
            // 'B' file
            if (*board.piece_on(libchess::constants::B2) != libchess::constants::WHITE_PAWN && *board.piece_on(libchess::constants::B3) != libchess::constants::WHITE_PAWN) {
                score -= 20;
                if (!(whitePawns & libchess::lookups::file_mask(libchess::constants::FILE_B))) {
                    score -= 20;
                }
            }
            // 'C' file
            if (*board.piece_on(libchess::constants::C2) != libchess::constants::WHITE_PAWN && *board.piece_on(libchess::constants::C3) != libchess::constants::WHITE_PAWN) {
                score -= 20;
                if (!(whitePawns & libchess::lookups::file_mask(libchess::constants::FILE_C))) {
                    score -= 20;
                }
            }
        }
            // king side
        else {
            // 'F' file
            if (*board.piece_on(libchess::constants::F2) != libchess::constants::WHITE_PAWN && *board.piece_on(libchess::constants::F3) != libchess::constants::WHITE_PAWN) {
                score -= 20;
                if (!(whitePawns & libchess::lookups::file_mask(libchess::constants::FILE_F))) {
                    score -= 20;
                }
            }
            // 'G' file
            if (*board.piece_on(libchess::constants::G2) != libchess::constants::WHITE_PAWN && *board.piece_on(libchess::constants::G3) != libchess::constants::WHITE_PAWN) {
                score -= 20;
                if (!(whitePawns & libchess::lookups::file_mask(libchess::constants::FILE_G))) {
                    score -= 20;
                }
            }
            // 'H' file
            if (*board.piece_on(libchess::constants::H2) != libchess::constants::WHITE_PAWN && *board.piece_on(libchess::constants::H3) != libchess::constants::WHITE_PAWN) {
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
            if (*board.piece_on(libchess::constants::A7) != libchess::constants::BLACK_PAWN && *board.piece_on(libchess::constants::A6) != libchess::constants::BLACK_PAWN) {
                score += 20;
                if (!(blackPawns & libchess::lookups::file_mask(libchess::constants::FILE_A))) {
                    score += 20;
                }
            }
            // 'B' file
            if (*board.piece_on(libchess::constants::B7) != libchess::constants::BLACK_PAWN && *board.piece_on(libchess::constants::C6) != libchess::constants::BLACK_PAWN) {
                score += 20;
                if (!(blackPawns & libchess::lookups::file_mask(libchess::constants::FILE_B))) {
                    score += 20;
                }
            }
            // 'C' file
            if (*board.piece_on(libchess::constants::C7) != libchess::constants::BLACK_PAWN && *board.piece_on(libchess::constants::C6) != libchess::constants::BLACK_PAWN) {
                score += 20;
                if (!(blackPawns & libchess::lookups::file_mask(libchess::constants::FILE_C))) {
                    score += 20;
                }
            }
        }
            // king side
        else {
            // 'F' file
            if (*board.piece_on(libchess::constants::F7) != libchess::constants::BLACK_PAWN && *board.piece_on(libchess::constants::F6) != libchess::constants::BLACK_PAWN) {
                score += 20;
                if (!(blackPawns & libchess::lookups::file_mask(libchess::constants::FILE_F))) {
                    score += 20;
                }
            }
            // 'G' file
            if (*board.piece_on(libchess::constants::G7) != libchess::constants::BLACK_PAWN && *board.piece_on(libchess::constants::G6) != libchess::constants::BLACK_PAWN) {
                score += 20;
                if (!(blackPawns & libchess::lookups::file_mask(libchess::constants::FILE_G))) {
                    score += 20;
                }
            }
            // 'H' file
            if (*board.piece_on(libchess::constants::H7) != libchess::constants::BLACK_PAWN && *board.piece_on(libchess::constants::H6) != libchess::constants::BLACK_PAWN) {
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
double Anduril::getPhase(libchess::Position &board) {
    double pawn = 0.125, knight = 1, bishop = 1, rook = 2, queen = 4;
    double totalPhase = pawn*16 + knight*4 + bishop*4 + rook*4 + queen*2;

    double phase = totalPhase;
    phase -= board.piece_type_bb(libchess::constants::PAWN).popcount() * pawn;
    phase -= board.piece_type_bb(libchess::constants::KNIGHT).popcount() * knight;
    phase -= board.piece_type_bb(libchess::constants::BISHOP).popcount() * bishop;
    phase -= board.piece_type_bb(libchess::constants::ROOK).popcount() * rook;
    phase -= board.piece_type_bb(libchess::constants::QUEEN).popcount() * queen;

    return (phase * 256 + (totalPhase / 2)) / totalPhase;
}