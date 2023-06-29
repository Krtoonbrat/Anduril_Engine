//
// Created by 80hugkev on 6/10/2022.
//

#include <iostream> // we need this for debugging if we print the board

#include "Anduril.h"

// table holding the values for king tropism
constexpr int SafetyTable[100] = {
        0,  0,   1,   2,   3,   5,   7,   9,  12,  15,
        18,  22,  26,  30,  35,  39,  44,  50,  56,  62,
        68,  75,  82,  85,  89,  97, 105, 113, 122, 131,
        140, 150, 169, 180, 191, 202, 213, 225, 237, 248,
        260, 272, 283, 295, 307, 319, 330, 342, 354, 366,
        377, 389, 401, 412, 424, 436, 448, 459, 471, 483,
        494, 500, 500, 500, 500, 500, 500, 500, 500, 500,
        500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
        500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
        500, 500, 500, 500, 500, 500, 500, 500, 500, 500
};

// Connected pawn bonus
constexpr int Connected[7] = { 0, 1, 3, 3, 6, 21, 34 };

// blocked pawns at 5th or 6th rank
constexpr int BlockedPawnMG[2] = { 6, 2 };
constexpr int BlockedPawnEG[2] = { 2, 1 };

// mask for the central squares we are looking for in space evaluations
libchess::Bitboard centerWhite = (libchess::lookups::FILE_C_MASK | libchess::lookups::FILE_D_MASK | libchess::lookups::FILE_E_MASK | libchess::lookups::FILE_F_MASK)
                                 & (libchess::lookups::RANK_2_MASK | libchess::lookups::RANK_3_MASK | libchess::lookups::RANK_4_MASK);
libchess::Bitboard centerBlack = (libchess::lookups::FILE_C_MASK | libchess::lookups::FILE_D_MASK | libchess::lookups::FILE_E_MASK | libchess::lookups::FILE_F_MASK)
                                 & (libchess::lookups::RANK_7_MASK | libchess::lookups::RANK_6_MASK | libchess::lookups::RANK_5_MASK);

// point bonus that increases the value of knights with more pawns on the board
int knightPawnBonus[9] = { -20, -16, -12, -8, -4,  0,  4,  8, 12 };

// point bonus that increases the value of rooks with less pawns
int rookPawnBonus[9] = { 15,  12,  9,  6,  3,  0, -3, -6, -9 };

// passed pawn bonuses by rank
// values stolen from stockfish (their values are reported to the GUI using v * 100 / 361, which is already applied)
int passedBonusMG[7] = {0, 1, 4, 6, 18, 46, 78};
int passedBonusEG[7] = {0, 10, 10, 14, 22, 51, 74};

// generates a static evaluation of the board
int Anduril::evaluateBoard(libchess::Position &board) {
    // first check for transpositions
    uint64_t hash = board.hash();
    SimpleNode *eNode = evalTable[hash];
    if (eNode->key == hash) {
        return eNode->score;
    }

    // prefetch the pawn evaluation node
    prefetch(pTable[board.pawn_hash()]);

    // squares the kings are on
    libchess::Square wKingSquare = board.king_square(libchess::constants::WHITE);
    libchess::Square bKingSquare = board.king_square(libchess::constants::BLACK);

    // reset the mobility scores
    whiteMobility[0] = 0, whiteMobility[1] = 0;
    blackMobility[0] = 0, blackMobility[1] = 0;

    // reset the king tropism variables
    attackCount[0] = 0, attackCount[1] = 0;
    attackWeight[0] = 0, attackWeight[1] = 0;
    kingZoneWBB = libchess::Bitboard(*libchess::Square::from(std::clamp(wKingSquare.file(), libchess::constants::FILE_B, libchess::constants::FILE_G), std::clamp(wKingSquare.rank(), libchess::constants::RANK_2, libchess::constants::RANK_7)));
    kingZoneBBB = libchess::Bitboard(*libchess::Square::from(std::clamp(bKingSquare.file(), libchess::constants::FILE_B, libchess::constants::FILE_G), std::clamp(bKingSquare.rank(), libchess::constants::RANK_2, libchess::constants::RANK_7)));

    // reset each colors attack map
    wAttackMap = libchess::Bitboard();
    bAttackMap = libchess::Bitboard();

    // fill the king zone
    kingZoneWBB |= libchess::lookups::king_attacks(*libchess::Square::from(std::clamp(wKingSquare.file(), libchess::constants::FILE_B, libchess::constants::FILE_G), std::clamp(wKingSquare.rank(), libchess::constants::RANK_2, libchess::constants::RANK_7)));
    kingZoneBBB |= libchess::lookups::king_attacks(*libchess::Square::from(std::clamp(bKingSquare.file(), libchess::constants::FILE_B, libchess::constants::FILE_G), std::clamp(bKingSquare.rank(), libchess::constants::RANK_2, libchess::constants::RANK_7)));

    kingZoneWBB |= kingZoneWBB << 8;
    kingZoneBBB |= kingZoneBBB >> 8;

    int scoreMG = 0;
    int scoreEG = 0;

    // get the material score for the board
    //std::pair<int, int> matScore = getMaterialScore(board);
    scoreMG += board.getPSQTMG();
    scoreEG += board.getPSQTEG();

    // first check for a transposition
    uint64_t phash = board.pawn_hash();
    PawnEntry *node = pTable[phash];
    if (node->key == hash) {
        scoreMG += node->score.first;
        scoreEG += node->score.second;
    }
    else {
        // reset the node
        node->key = hash;

        // get the pawn score for the board
        std::pair<int, int> wp = getPawnScore<true>(board);
        std::pair<int, int> bp = getPawnScore<false>(board);

        scoreMG += wp.first - bp.first;
        scoreEG += wp.second - bp.second;

        // save the node
        node->score.first = wp.first - bp.first;
        node->score.second = wp.second - bp.second;
    }

    // get king safety bonus for the board
    // this function only relates to castled kings, therefore its only necessary to add it to the middle game score
    int king = getKingSafety(board, wKingSquare, bKingSquare);
    scoreMG += king;

    // evaluate positional themes and gather mobility and tropism data
    std::pair<int, int> positional = positionalMobilityTropism(board);
    scoreMG += positional.first;
    scoreEG += positional.second;

    // king tropism (black is index 0, white is index 1)
    // white
    if (attackCount[1] >= 2) {
        scoreMG += SafetyTable[attackWeight[1]];
        scoreEG += SafetyTable[attackWeight[1]];
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
                // is the file fully open?
                if (!(board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK) & libchess::lookups::FILE_MASK[rookFile])) {
                    scoreMG += 13;
                    scoreEG += 7;
                }
                else {
                    scoreMG += 5;
                    scoreEG += 2;
                }
            }
        }
            // black
        else {
            // is the file semi open?
            if (!(board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK) & libchess::lookups::FILE_MASK[rookFile])) {
                // is the file fully open?
                if (!(board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE) & libchess::lookups::FILE_MASK[rookFile])) {
                    scoreMG -= 13;
                    scoreEG -= 7;
                }
                else {
                    scoreMG -= 5;
                    scoreEG -= 2;
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

        // black
        libchess::Bitboard bCenterAttacks = centerBlack & bAttackMap;
        scoreMG -= 5 * bCenterAttacks.popcount();
    }

    // get the phase for tapered eval
    int phase = getPhase(board);

    // for negamax to work, we must return a non-adjusted score for white
    // and a negated score for black
    int finalScore = !board.side_to_move() ? ((scoreMG * (256 - phase)) + (scoreEG * phase)) / 256 : -((scoreMG * (256 - phase)) + (scoreEG * phase)) / 256;

    // save information to the node
    eNode->key = hash;
    eNode->score = finalScore;

    return finalScore;
}

// calculates all mobility scores and tropism
std::pair<int, int> Anduril::positionalMobilityTropism(libchess::Position &board) {
    // grab the pieces
    libchess::Bitboard knights = board.piece_type_bb(libchess::constants::KNIGHT);
    libchess::Bitboard bishops = board.piece_type_bb(libchess::constants::BISHOP);
    libchess::Bitboard rooks = board.piece_type_bb(libchess::constants::ROOK);
    libchess::Bitboard queens = board.piece_type_bb(libchess::constants::QUEEN);

    // a few of the features use the pawn bitboards, we grab them here so that we only have to do it once
    libchess::Bitboard whitePawns = board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE);
    libchess::Bitboard blackPawns = board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK);

    std::pair<int, int> score = {0, 0};

    // knights
    while (knights) {
        libchess::Square square = knights.forward_bitscan();
        std::optional<libchess::Color> color = board.color_of(square);

        if (!*color) {
            // knight pawn bonus
            score.first  += knightPawnBonus[whitePawns.popcount()];
            score.second += knightPawnBonus[whitePawns.popcount()];

            // outposts
            if (square > libchess::constants::H3
                && (board.attackers_to(square, libchess::constants::WHITE) & whitePawns)
                && !(board.attackers_to(square, libchess::constants::BLACK) & blackPawns)) {
                score.first  += oMG;
                score.second += oEG;
            }

            // trapped knights
            // Knight on H8 with black pawns on either F7 or H7
            if (square == libchess::constants::H8) {
                if ((libchess::lookups::square(libchess::constants::H7) & blackPawns)
                    || (libchess::lookups::square(libchess::constants::F7) & blackPawns)) {
                    score.first  -= tMG;
                    score.second -= tEG;
                }
            }
                // Knight on A8 with black pawns on either A7 or C7
            else if (square == libchess::constants::A8) {
                if ((libchess::lookups::square(libchess::constants::A7) & blackPawns)
                    || (libchess::lookups::square(libchess::constants::C7) & blackPawns)) {
                    score.first  -= tMG;
                    score.second -= tEG;
                }
            }
                // Knight on H7 with black pawns on G7 and either H6 or F6
            else if (square == libchess::constants::H7) {
                if ((libchess::lookups::square(libchess::constants::G7) & blackPawns)
                    && ((libchess::lookups::square(libchess::constants::H6) & blackPawns)
                        || (libchess::lookups::square(libchess::constants::F6) & blackPawns))) {
                    score.first  -= tMG;
                    score.second -= tEG;
                }
            }
                // Knight on A7 with black pawns on B7 and either A6 or C6
            else if (square == libchess::constants::A7) {
                if ((libchess::lookups::square(libchess::constants::B7) & blackPawns)
                    && ((libchess::lookups::square(libchess::constants::A6) & blackPawns)
                        || (libchess::lookups::square(libchess::constants::C6) & blackPawns))) {
                    score.first  -= tMG;
                    score.second -= tEG;
                }
            }

            evaluateKnights<true>(board, square);
        }
        else {
            // knight pawn bonus
            score.first  -= knightPawnBonus[blackPawns.popcount()];
            score.second -= knightPawnBonus[blackPawns.popcount()];

            // outposts
            if (square < libchess::constants::H6
                && (board.attackers_to(square, libchess::constants::BLACK) & blackPawns)
                && !(board.attackers_to(square, libchess::constants::WHITE) & whitePawns)) {
                score.first  -= oMG;
                score.second -= oEG;
            }

            // trapped knights
            // Knight on H1 with white pawns on either H2 or F2
            if (square == libchess::constants::H1) {
                if ((libchess::lookups::square(libchess::constants::H2) & whitePawns)
                    || (libchess::lookups::square(libchess::constants::F2) & whitePawns)) {
                    score.first  += tMG;
                    score.second += tEG;
                }
            }
                // Knight on A1 with white pawns on either A2 or C2
            else if (square == libchess::constants::A1) {
                if ((libchess::lookups::square(libchess::constants::A2) & whitePawns)
                    || (libchess::lookups::square(libchess::constants::C2) & whitePawns)) {
                    score.first  += tMG;
                    score.second += tEG;
                }
            }
                // Knight on H2 with white pawns on G2 and either H3 or F3
            else if (square == libchess::constants::H2) {
                if ((libchess::lookups::square(libchess::constants::G2) & whitePawns)
                    && ((libchess::lookups::square(libchess::constants::H3) & whitePawns)
                        || (libchess::lookups::square(libchess::constants::F3) & whitePawns))) {
                    score.first  += tMG;
                    score.second += tEG;
                }
            }
                // Knight on A2 with white pawns on B2 and either A3 or C3
            else if (square == libchess::constants::A2) {
                if ((libchess::lookups::square(libchess::constants::B2) & whitePawns)
                    && ((libchess::lookups::square(libchess::constants::A3) & whitePawns)
                        || (libchess::lookups::square(libchess::constants::C3) & whitePawns))) {
                    score.first  += tMG;
                    score.second += tEG;
                }
            }

            evaluateKnights<false>(board, square);
        }

        knights.forward_popbit();
    }

    // bishops
    while (bishops) {
        libchess::Square square = bishops.forward_bitscan();
        std::optional<libchess::Color> color = board.color_of(square);

        if (!*color) {
            // fianchetto
            if ((square == libchess::constants::G2 && board.king_square(libchess::constants::WHITE) == libchess::constants::G1)
                || (square == libchess::constants::B2 && board.king_square(libchess::constants::WHITE) == libchess::constants::B1)) {
                score.first  += 10;
                score.second += 1;
            }

            evaluateBishops<true>(board, square);
        }
        else {
            // fianchetto
            if ((square == libchess::constants::G7 && board.king_square(libchess::constants::BLACK) == libchess::constants::G8)
                || (square == libchess::constants::B7 && board.king_square(libchess::constants::BLACK) == libchess::constants::B8)) {
                score.first  -= 10;
                score.second -= 1;
            }

            evaluateBishops<false>(board, square);
        }

        bishops.forward_popbit();
    }

    // rooks
    while (rooks) {
        libchess::Square square = rooks.forward_bitscan();
        std::optional<libchess::Color> color = board.color_of(square);

        if (!*color) {
            // rook pawn bonus
            score.first  += rookPawnBonus[whitePawns.popcount()];
            score.second += rookPawnBonus[whitePawns.popcount()];

            // trapped rooks
            libchess::Square kingSquare = board.king_square(libchess::constants::WHITE);
            switch (square) {
                case libchess::constants::H1:
                    if (kingSquare == libchess::constants::E1 || kingSquare == libchess::constants::F1 || kingSquare == libchess::constants::G1) {
                        score.first  -= 15;
                        score.second -= 3;
                    }
                    break;
                case libchess::constants::G1:
                    if (kingSquare == libchess::constants::E1 || kingSquare == libchess::constants::F1) {
                        score.first  -= 15;
                        score.second -= 3;
                    }
                    break;
                case libchess::constants::F1:
                    if (kingSquare == libchess::constants::E1) {
                        score.first  -= 15;
                        score.second -= 3;
                    }
                    break;
                case libchess::constants::A1:
                    if (kingSquare == libchess::constants::B1 || kingSquare == libchess::constants::C1 || kingSquare == libchess::constants::D1) {
                        score.first  -= 15;
                        score.second -= 3;
                    }
                    break;
                case libchess::constants::B1:
                    if (kingSquare == libchess::constants::C1 || kingSquare == libchess::constants::D1) {
                        score.first  -= 15;
                        score.second -= 3;
                    }
                    break;
                case libchess::constants::C1:
                    if (kingSquare == libchess::constants::D1) {
                        score.first  -= 15;
                        score.second -= 3;
                    }
                    break;
            }

            evaluateRooks<true>(board, square);
        }
        else {
            // rook pawn bonus
            score.first  -= rookPawnBonus[blackPawns.popcount()];
            score.second -= rookPawnBonus[blackPawns.popcount()];

            // trapped rooks
            libchess::Square kingSquare = board.king_square(libchess::constants::BLACK);
            switch (square) {
                case libchess::constants::H8:
                    if (kingSquare == libchess::constants::E8 || kingSquare == libchess::constants::F8 || kingSquare == libchess::constants::G8) {
                        score.first  += 15;
                        score.second += 3;
                    }
                    break;
                case libchess::constants::G8:
                    if (kingSquare == libchess::constants::E8 || kingSquare == libchess::constants::F8) {
                        score.first  += 15;
                        score.second += 3;
                    }
                    break;
                case libchess::constants::F8:
                    if (kingSquare == libchess::constants::E8) {
                        score.first  += 15;
                        score.second += 3;
                    }
                    break;
                case libchess::constants::A8:
                    if (kingSquare == libchess::constants::B8 || kingSquare == libchess::constants::C8 || kingSquare == libchess::constants::D8) {
                        score.first  += 15;
                        score.second += 3;
                    }
                    break;
                case libchess::constants::B8:
                    if (kingSquare == libchess::constants::C8 || kingSquare == libchess::constants::D8) {
                        score.first  += 15;
                        score.second += 3;
                    }
                    break;
                case libchess::constants::C8:
                    if (kingSquare == libchess::constants::D8) {
                        score.first  += 15;
                        score.second += 3;
                    }
                    break;
            }

            evaluateRooks<false>(board, square);
        }

        rooks.forward_popbit();
    }

    // queens
    while (queens) {
        libchess::Square square = queens.forward_bitscan();
        std::optional<libchess::Color> color = board.color_of(square);

        if (!*color) {
            evaluateQueens<true>(board, square);
        }
        else {
            evaluateQueens<false>(board, square);
        }

        queens.forward_popbit();
    }

    return score;
}

// evaluates knights for tropism and calculates mobility
template<bool white>
inline void Anduril::evaluateKnights(libchess::Position &board, libchess::Square square) {
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

// evaluates bishops for tropism and calculates mobility
template<bool white>
inline void Anduril::evaluateBishops(libchess::Position &board, libchess::Square square) {
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

// evaluates rooks for tropism and calculates mobility
template<bool white>
inline void Anduril::evaluateRooks(libchess::Position &board, libchess::Square square) {
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
        kingZoneAttacks = attackedSquares & kingZoneWBB;
    }

    if (kingZoneAttacks) {
        attackCount[white]++;
        attackWeight[white] += 3 * kingZoneAttacks.popcount();
    }
}

// evaluates queens for tropism and calculates mobility
template<bool white>
inline void Anduril::evaluateQueens(libchess::Position &board, libchess::Square square) {
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

template<bool color>
std::pair<int, int> Anduril::getPawnScore(libchess::Position &board) {
    constexpr libchess::Color us = color ? libchess::constants::WHITE : libchess::constants::BLACK;
    constexpr libchess::Color them = color ? libchess::constants::BLACK : libchess::constants::WHITE;

    libchess::Bitboard neighbors, stoppers, support, phalanx, opposed;
    libchess::Bitboard lever, leverPush, blocked;
    libchess::Square s(0);
    bool backward, passed, doubled;
    std::pair<int, int> score = {0, 0};
    libchess::Bitboard pawns = board.piece_type_bb(libchess::constants::PAWN, us);

    libchess::Bitboard ourPawns = board.piece_type_bb(libchess::constants::PAWN, us);
    libchess::Bitboard theirPawns = board.piece_type_bb(libchess::constants::PAWN, them);

    while (pawns) {
        s = pawns.forward_bitscan();
        pawns.forward_popbit();

        libchess::Rank r = libchess::lookups::relative_rank(s.rank(), us);

        // flag the pawn
        opposed = theirPawns & libchess::lookups::forward_file_mask(s, us);
        blocked = theirPawns & libchess::lookups::pawn_shift(libchess::Bitboard(s), us);
        stoppers = theirPawns & libchess::lookups::passed_pawn_span(us, s);
        lever = theirPawns & libchess::lookups::pawn_attacks(s, us);
        leverPush = theirPawns & libchess::lookups::pawn_attacks(libchess::lookups::pawn_shift(s, us), us);
        doubled = ourPawns & libchess::Bitboard(libchess::lookups::pawn_shift(s, them));
        neighbors = ourPawns & libchess::lookups::adjacent_files_mask(s);
        phalanx = neighbors & libchess::lookups::RANK_MASK[s.rank()];
        support = neighbors & libchess::lookups::RANK_MASK[(libchess::lookups::pawn_shift(s, them)).rank()];

        // A pawn is backward when it is behind all pawns of the same color on adjacent files and cannot advance
        backward = !(neighbors & libchess::lookups::forward_ranks_mask(libchess::lookups::pawn_shift(s, us), them))
                    && (leverPush | blocked);

        // a pawn is passed if there are no stoppers except some levers, the only stoppers are the leverPush, but we outnumber them,
        // and there is only one front stopper that can be levered
        passed = !(stoppers ^ lever)
                 || (!(stoppers ^ leverPush)
                     && phalanx.popcount() >= leverPush.popcount())
                 || (stoppers == blocked && r.value() >= 5
                     && (libchess::lookups::pawn_shift(support, libchess::constants::WHITE) & ~(theirPawns | libchess::lookups::pawn_double_attacks(them, theirPawns))));

        passed &= !(libchess::lookups::forward_file_mask(s, us) & ourPawns);

        if (passed) {
            score.first += passedBonusMG[libchess::lookups::relative_rank(s.rank(), us).value()];
            score.second += passedBonusEG[libchess::lookups::relative_rank(s.rank(), us).value()];
        }

        if (support | phalanx) {
            int bonus = Connected[r] * (2 + bool(phalanx) - bool(opposed)) + 6 * support.popcount();
            score.first += bonus;
            score.second += bonus * (r.value() - 2) / 4;
        }
        else if (!neighbors) {
            if (opposed
                && (ourPawns & libchess::lookups::forward_file_mask(s, them))
                && !(theirPawns & libchess::lookups::adjacent_files_mask(s))) {
                // doubled
                score.first -= 3;
                score.second -= 15;
            }
            else {
                // isolated
                score.first -= 1;
                score.second -= 5;
                // weak unopposed
                score.first -= 4 * !opposed;
                score.second -= 5 * !opposed;
            }
        }
        else if (backward) {
            // backward
            score.first -= 2;
            score.second -= 6;
            // weak unopposed
            score.first -= 4 * !opposed * bool(~(libchess::lookups::FILE_A_MASK | libchess::lookups::FILE_H_MASK) & libchess::Bitboard(s));
            score.second -= 5 * !opposed * bool(~(libchess::lookups::FILE_A_MASK | libchess::lookups::FILE_H_MASK) & libchess::Bitboard(s));
        }

        if (!support) {
            // doubled
            score.first -= 3 * doubled;
            score.second -= 15 * doubled;
            // weak lever
            score.first -= 1 * bool(lever & (lever - 1));
            score.second -= 17 * bool(lever & (lever - 1));
        }

        if (blocked && r >= 5) {
            score.first -= BlockedPawnMG[r.value() - libchess::constants::RANK_5.value()];
            score.second -= BlockedPawnEG[r.value() - libchess::constants::RANK_5.value()];
        }
    }

    return score;

}

// finds the king safety bonus for the position
int Anduril::getKingSafety(libchess::Position &board, libchess::Square whiteKing, libchess::Square blackKing) {
    libchess::Bitboard whitePawns = board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE);
    libchess::Bitboard blackPawns = board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK);

    libchess::Bitboard wKBB = libchess::lookups::square(whiteKing);
    libchess::Bitboard bKBB = libchess::lookups::square(blackKing);

    libchess::Bitboard wCastleQueen = (libchess::lookups::FILE_A_MASK | libchess::lookups::FILE_B_MASK | libchess::lookups::FILE_C_MASK)
                                      & (libchess::lookups::RANK_1_MASK | libchess::lookups::RANK_2_MASK);
    libchess::Bitboard wCastleKing = (libchess::lookups::FILE_F_MASK | libchess::lookups::FILE_G_MASK | libchess::lookups::FILE_H_MASK)
                                      & (libchess::lookups::RANK_1_MASK | libchess::lookups::RANK_2_MASK);

    libchess::Bitboard bCastleQueen = (libchess::lookups::FILE_A_MASK | libchess::lookups::FILE_B_MASK |  libchess::lookups::FILE_C_MASK)
                                      & (libchess::lookups::RANK_7_MASK | libchess::lookups::RANK_8_MASK);
    libchess::Bitboard bCastleKing = (libchess::lookups::FILE_F_MASK | libchess::lookups::FILE_G_MASK | libchess::lookups::FILE_H_MASK)
                                     & (libchess::lookups::RANK_7_MASK | libchess::lookups::RANK_8_MASK);

    int score = 0;

    // white shield
    if (wKBB & wCastleKing || wKBB & wCastleQueen) {
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
        }
    }

    // black shield
    if (bKBB & bCastleKing || bKBB & bCastleQueen) {
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

        // black
        libchess::Bitboard stormZone = libchess::Bitboard();
        // first make the storm zone files
        libchess::Bitboard stormZoneFiles = libchess::Bitboard();
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
            case libchess::constants::RANK_7:
                stormZone = (stormZoneFiles & libchess::lookups::RANK_8_MASK) |
                            (stormZoneFiles & libchess::lookups::RANK_7_MASK) |
                            (stormZoneFiles & libchess::lookups::RANK_6_MASK) |
                            (stormZoneFiles & libchess::lookups::RANK_5_MASK);
                if (stormZone & whitePawns) {
                    score += 5 * (stormZone & whitePawns).popcount();
                }
                break;
            case libchess::constants::RANK_8:
                stormZone = (stormZoneFiles & libchess::lookups::RANK_8_MASK) |
                            (stormZoneFiles & libchess::lookups::RANK_7_MASK) |
                            (stormZoneFiles & libchess::lookups::RANK_6_MASK);
                if (stormZone & whitePawns) {
                    score += 5 * (stormZone & whitePawns).popcount();
                }
                break;
        }
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