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

// strength of pawn shelter for our king by [distance from edge][rank]
constexpr int shelterStrength[4][8] = {
        { -1, 22, 25,  14,   9,   6,   7},
        {-12, 17,  9, -13,  -8,  -3, -17},
        { -3, 21,  6,  -1,   8,   2, -12},
        {-10, -3, -8, -13, -11, -18, -45},
};

constexpr int unblockedStorm[4][8] = {
        {24, -79, -46, 26, 13, 12, 16},
        {11,  -7,  33, 12,  9, -2,  6},
        {-2,  14,  46,  9, -1, -4, -3},
        {-4,  -3,  28,  1,  9, -4, -8},
};

constexpr int blockedStorm[2][8] = {
        {0, 0, 20, -2, -1, -1, 0},
        {0, 0, 21,  4,  2,  1, 1}
};

constexpr int kingOnFile[2][2][2] = {
        {{-6, 3}, {-2, 1}},
        {{0, -1}, {2, -1}}
};

// stuff for threats
constexpr int threatByMinor[6][2] = {
        {1, 9},
        {15, 11},
        {21, 15},
        {24, 33},
        {22, 45},
        {0, 0}
};

constexpr int threatByRook[6][2] = {
        {1, 12},
        {10, 19},
        {12, 17},
        {0, 11},
        {16, 12},
        {0, 0}
};

constexpr int threatByKing[2] = {7, 25};

constexpr int hanging[2] = {19, 10};

constexpr int weakQueenProtection = 4;

constexpr int restrictedPiece = 2;

constexpr int threatBySafePawn[2] = {48, 26};

constexpr int threatByPawnPush[2] = {13, 11};

constexpr int knightOnQueen[2] = {4, 3};

constexpr int sliderOnQueen[2] = {17, 5};

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
    for (auto & i : attackMap) {
        for (auto & j : i)
            j = libchess::Bitboard();
    }

    libchess::Bitboard doublePawnAttacks = libchess::lookups::pawn_double_attacks<true>(board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE));
    attackMap[0][5] = libchess::lookups::king_attacks(wKingSquare);
    attackMap[0][0] = libchess::lookups::pawn_attacks<true>(board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE));
    attackMap[0][6] |= attackMap[0][5] | attackMap[0][0];
    attackByTwo[0] = doublePawnAttacks | (attackMap[0][5] & attackMap[0][0]);

    doublePawnAttacks = libchess::lookups::pawn_double_attacks<false>(board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK));
    attackMap[1][5] = libchess::lookups::king_attacks(bKingSquare);
    attackMap[1][0] = libchess::lookups::pawn_attacks<false>(board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK));
    attackMap[1][6] |= attackMap[1][5] | attackMap[1][0];
    attackByTwo[1] = doublePawnAttacks | (attackMap[1][5] & attackMap[1][0]);

    // setup mobility area
    libchess::Bitboard lowRanks = libchess::lookups::RANK_3_MASK | libchess::lookups::RANK_2_MASK;
    // find pawns blocked on first two ranks
    libchess::Bitboard b = board.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE) & (libchess::lookups::pawn_shift(board.occupancy_bb(), libchess::constants::BLACK) | lowRanks);
    mobilityArea[0] = ~(b | (board.piece_type_bb(libchess::constants::QUEEN, libchess::constants::WHITE) | board.piece_type_bb(libchess::constants::KING, libchess::constants::WHITE)) | board.pinned_pieces_of(libchess::constants::WHITE) | attackMap[1][0]);
    lowRanks = libchess::lookups::RANK_7_MASK | libchess::lookups::RANK_6_MASK;
    // find pawns blocked on first two ranks
    b = board.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK) & (libchess::lookups::pawn_shift(board.occupancy_bb(), libchess::constants::WHITE) | lowRanks);
    mobilityArea[1] = ~(b | (board.piece_type_bb(libchess::constants::QUEEN, libchess::constants::BLACK) | board.piece_type_bb(libchess::constants::KING, libchess::constants::BLACK)) | board.pinned_pieces_of(libchess::constants::BLACK) | attackMap[0][0]);

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
        std::pair<int16_t, int16_t> wp = getPawnScore<true>(board);
        std::pair<int16_t, int16_t> bp = getPawnScore<false>(board);

        scoreMG += wp.first - bp.first;
        scoreEG += wp.second - bp.second;

        // save the node
        node->score.first = wp.first - bp.first;
        node->score.second = wp.second - bp.second;
    }

    // get king safety bonus for the board
    // this function only relates to castled kings, therefore its only necessary to add it to the middle game score
    std::pair<int, int> Wking = getKingSafety<true>(board);
    std::pair<int, int> Bking = getKingSafety<false>(board);
    scoreMG += Wking.first - Bking.first;
    scoreEG += Wking.second - Bking.second;

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

    // bishop pair
    if (board.piece_type_bb(libchess::constants::BISHOP, libchess::constants::WHITE).popcount() >= 2) {
        scoreMG += bpM;
        scoreEG += bpE;
    }
    if (board.piece_type_bb(libchess::constants::BISHOP, libchess::constants::BLACK).popcount() >= 2) {
        scoreMG -= bpM;
        scoreEG -= bpE;
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

    // threats
    std::pair<int, int> wThreats = threats<true>(board);
    std::pair<int, int> bThreats = threats<false>(board);
    scoreMG += wThreats.first  - bThreats.first;
    scoreEG += wThreats.second - bThreats.second;

    // space advantages
    // white
    b = attackMap[0][1] | attackMap[0][2] | attackMap[0][3] | attackMap[0][4];
    libchess::Bitboard wCenterAttacks = centerWhite & b & (~attackMap[1][6]);
    scoreMG += spc * wCenterAttacks.popcount();

    // black
    b = attackMap[1][1] | attackMap[1][2] | attackMap[1][3] | attackMap[1][4];
    libchess::Bitboard bCenterAttacks = centerBlack & b & (~attackMap[0][6]);
    scoreMG -= spc * bCenterAttacks.popcount();

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
        attackByTwo[0] |= attackMap[0][6] & attackedSquares;
        attackMap[0][1] |= attackedSquares;
        attackMap[0][6] |= attackedSquares;
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
        attackByTwo[1] |= attackMap[1][6] & attackedSquares;
        attackMap[1][1] |= attackedSquares;
        attackMap[1][6] |= attackedSquares;
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
        attackByTwo[0] |= attackMap[0][6] & attackedSquares;
        attackMap[0][2] |= attackedSquares;
        attackMap[0][6] |= attackedSquares;
        kingZoneAttacks = attackedSquares & kingZoneBBB;
    }
        // black
    else {
        attackedSquares &= ~(board.color_bb(libchess::constants::BLACK));

        // mobility
        blackMobility[0] += 3 * (attackedSquares.popcount() - 7);
        blackMobility[1] += 3 * (attackedSquares.popcount() - 7);

        // king zone attacks
        attackByTwo[1] |= attackMap[1][6] & attackedSquares;
        attackMap[1][2] |= attackedSquares;
        attackMap[1][6] |= attackedSquares;
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
        attackByTwo[0] |= attackMap[0][6] & attackedSquares;
        attackMap[0][3] |= attackedSquares;
        attackMap[0][6] |= attackedSquares;
        kingZoneAttacks = attackedSquares & kingZoneBBB;
    }
        // black
    else {
        attackedSquares &= ~(board.color_bb(libchess::constants::BLACK));

        // mobility
        blackMobility[0] += 2 * (attackedSquares.popcount() - 7);
        blackMobility[1] += 4 * (attackedSquares.popcount() - 7);

        // king zone attacks
        attackByTwo[1] |= attackMap[1][6] & attackedSquares;
        attackMap[1][3] |= attackedSquares;
        attackMap[1][6] |= attackedSquares;
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
        attackByTwo[0] |= attackMap[0][6] & attackedSquares;
        attackMap[0][4] |= attackedSquares;
        attackMap[0][6] |= attackedSquares;
        kingZoneAttacks = attackedSquares & kingZoneBBB;
    }
        // black
    else {
        attackedSquares &= ~(board.color_bb(libchess::constants::BLACK));

        // mobility
        blackMobility[0] += 1 * (attackedSquares.popcount() - 14);
        blackMobility[1] += 2 * (attackedSquares.popcount() - 14);

        // king zone attacks
        attackByTwo[1] |= attackMap[1][6] & attackedSquares;
        attackMap[1][4] |= attackedSquares;
        attackMap[1][6] |= attackedSquares;
        kingZoneAttacks = attackedSquares & kingZoneWBB;
    }

    if (kingZoneAttacks) {
        attackCount[white]++;
        attackWeight[white] += 3 * kingZoneAttacks.popcount();
    }
}

template<bool color>
std::pair<int16_t, int16_t> Anduril::getPawnScore(libchess::Position &board) {
    constexpr libchess::Color us = color ? libchess::constants::WHITE : libchess::constants::BLACK;
    constexpr libchess::Color them = color ? libchess::constants::BLACK : libchess::constants::WHITE;

    libchess::Bitboard neighbors, stoppers, support, phalanx, opposed;
    libchess::Bitboard lever, leverPush, blocked;
    libchess::Square s(0);
    bool backward, passed, doubled;
    std::pair<int16_t, int16_t> score = {0, 0};
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
                     && (libchess::lookups::pawn_shift(support, us) & ~(theirPawns | libchess::lookups::pawn_double_attacks<!color>(theirPawns))));

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
// based on the stockfish 14 code
template<bool color>
std::pair<int, int> Anduril::getKingSafety(libchess::Position &board) {
    constexpr libchess::Color us = color ? libchess::constants::WHITE : libchess::constants::BLACK;

    libchess::Square kingSquare = board.king_square(us);
    std::pair<int, int> shelter = evaluateShelter<color>(board, kingSquare);
    auto compare = [](std::pair<int, int> a, std::pair<int, int> b) { return a.first < b.first; };

    // use bonus after castling if its bigger
    if (board.castling_rights().is_allowed(libchess::CastlingRight(1 + (!color * 3)))) {
        shelter = std::max(shelter, evaluateShelter<color>(board, libchess::lookups::relative_square(us, libchess::constants::G1)), compare);
    }

    if (board.castling_rights().is_allowed(libchess::CastlingRight(2 + (!color * 6)))) {
        shelter = std::max(shelter, evaluateShelter<color>(board, libchess::lookups::relative_square(us, libchess::constants::C1)), compare);
    }

    // in endgames we like to bring our king near our closest pawn
    libchess::Bitboard pawns = board.piece_type_bb(libchess::constants::PAWN, us);
    int minPawnDistance = 6;

    if (pawns & libchess::lookups::king_attacks(kingSquare)) {
        minPawnDistance = 1;
    }
    else while (pawns) {
        libchess::Square square = pawns.forward_bitscan();
        pawns.forward_popbit();
        minPawnDistance = std::min(minPawnDistance, libchess::lookups::distance(kingSquare, square));
    }
    shelter.second -= 4 * minPawnDistance;

    return shelter;
}

// calculates shelter bonus and storm penalty for a king
template<bool color>
std::pair<int, int> Anduril::evaluateShelter(libchess::Position &board, libchess::Square ksq) {
    constexpr libchess::Color us = color ? libchess::constants::WHITE : libchess::constants::BLACK;
    constexpr libchess::Color them = color ? libchess::constants::BLACK : libchess::constants::WHITE;

    std::pair<int, int> score = {1, 1};

    libchess::Bitboard b = board.piece_type_bb(libchess::constants::PAWN) & ~libchess::lookups::forward_ranks_mask(ksq, them);
    libchess::Bitboard ourPawns = b & board.color_bb(us) & ~attackMap[them][0];
    libchess::Bitboard theirPawns = b & board.color_bb(them);

    libchess::File center = std::clamp(ksq.file(), libchess::constants::FILE_B, libchess::constants::FILE_G);
    for (auto f = libchess::File(center - 1); f <= center + 1; f++) {
        b = ourPawns & libchess::lookups::file_mask(f);
        int ourRank = b ? libchess::lookups::relative_rank(libchess::lookups::frontmost_square(them, b).rank(), us).value() : 0;

        b = theirPawns & libchess::lookups::file_mask(f);
        int theirRank = b ? libchess::lookups::relative_rank(libchess::lookups::frontmost_square(them, b).rank(), us).value() : 0;

        int d = libchess::lookups::edge_distance(f);
        score.first += shelterStrength[d][ourRank];

        if (ourRank && (ourRank == theirRank - 1)) {
            score.first -= blockedStorm[0][theirRank];
            score.second-= blockedStorm[1][theirRank];
        }
        else {
            score.first -= unblockedStorm[d][theirRank];
        }
    }

    score.first -= kingOnFile[board.is_on_semiopen_file(us, ksq)][board.is_on_semiopen_file(them, ksq)][0];
    score.second -= kingOnFile[board.is_on_semiopen_file(us, ksq)][board.is_on_semiopen_file(them, ksq)][1];

    return score;
}

// based on implementation in stockfish 14
template<bool color>
std::pair<int, int> Anduril::threats(libchess::Position &board) {
    constexpr libchess::Color us = color ? libchess::constants::WHITE : libchess::constants::BLACK;
    constexpr libchess::Color them = color ? libchess::constants::BLACK : libchess::constants::WHITE;
    constexpr libchess::Bitboard relativeRank3bb = (color ? libchess::lookups::RANK_3_MASK : libchess::lookups::RANK_6_MASK);

    std::pair<int, int> score = {0, 0};
    libchess::Bitboard b, weak, defended, nonPawnEnemies, stronglyProtected, safe;

    // non pawn enemies
    nonPawnEnemies = board.color_bb(them) & ~board.piece_type_bb(libchess::constants::PAWN);

    // squares strongly protected by the enemy
    stronglyProtected = attackMap[them][0] | (attackByTwo[them] & ~attackByTwo[us]);

    // non-pawn enemies, strongly protected
    defended = nonPawnEnemies & stronglyProtected;

    // enemies not strongly protected and under our attack
    weak = board.color_bb(them) & ~stronglyProtected & attackMap[us][6];

    // bonus according to the kind of attacking pieces
    if (defended | weak) {
        b = (defended | weak) & (attackMap[us][1] | attackMap[us][2]);
        while (b) {
            libchess::Square square = b.forward_bitscan();
            b.forward_popbit();
            score.first +=  threatByMinor[board.piece_type_on(square)->value()][0];
            score.second += threatByMinor[board.piece_type_on(square)->value()][1];
        }

        b = weak & attackMap[us][3];
        while (b) {
            libchess::Square square = b.forward_bitscan();
            b.forward_popbit();
            score.first +=  threatByRook[board.piece_type_on(square)->value()][0];
            score.second += threatByRook[board.piece_type_on(square)->value()][1];
        }

        if (weak & attackMap[us][5]) {
            score.first +=  threatByKing[0];
            score.second += threatByKing[1];
        }

        b = ~attackMap[them][6] | (nonPawnEnemies & attackByTwo[us]);
        score.first +=  hanging[0] * (weak & b).popcount();
        score.second += hanging[1] * (weak & b).popcount();

        // additional bonus if weak pieces are only protected by the queen
        score.first +=  weakQueenProtection * (weak & attackMap[them][4]).popcount();
    }

    // bonus for restricting piece movement
    b = attackMap[them][6] & ~stronglyProtected & attackMap[us][6];
    score.first  += restrictedPiece * b.popcount();
    score.second += restrictedPiece * b.popcount();

    // protected or unattacked squares
    safe = ~attackMap[them][6] | attackMap[us][6];

    // bonus for attacking enemy piece with our safe pawns
    b = board.piece_type_bb(libchess::constants::PAWN, us) & safe;
    b = libchess::lookups::pawn_attacks<color>(b) & nonPawnEnemies;
    score.first +=  threatBySafePawn[0] * b.popcount();
    score.second += threatBySafePawn[1] * b.popcount();

    // find squares where our pawns can push next move
    b  = libchess::lookups::pawn_shift(board.piece_type_bb(libchess::constants::PAWN, us), us) & ~board.occupancy_bb();
    b |= libchess::lookups::pawn_shift(b & relativeRank3bb, us) & ~board.occupancy_bb();

    // keep only squares that are relatively safe
    b &= ~attackMap[them][0] & safe;

    // bonus for safe pawn threats on next move
    b = libchess::lookups::pawn_attacks<color>(b) & nonPawnEnemies;
    score.first +=  threatByPawnPush[0] * b.popcount();
    score.second += threatByPawnPush[1] * b.popcount();

    // bonus for threats on the next moves against enemy queen
    if (board.piece_type_bb(libchess::constants::QUEEN, them).popcount() == 1) {
        bool queenImbalance = board.piece_type_bb(libchess::constants::QUEEN).popcount() == 1;

        libchess::Square s = board.piece_type_bb(libchess::constants::QUEEN, them).forward_bitscan();
        safe = mobilityArea[us] & ~board.piece_type_bb(libchess::constants::PAWN, us) & ~stronglyProtected;

        b = attackMap[us][1] & libchess::lookups::knight_attacks(s);

        score.first +=  knightOnQueen[0] * (b & safe).popcount() * (1 + queenImbalance);
        score.second += knightOnQueen[1] * (b & safe).popcount() * (1 + queenImbalance);

        b = (attackMap[us][2] & libchess::lookups::bishop_attacks(s, board.occupancy_bb()))
          | (attackMap[us][3] & libchess::lookups::rook_attacks(s, board.occupancy_bb()));

        score.first += sliderOnQueen[0] * (b & safe & attackByTwo[us]).popcount() * (1 + queenImbalance);
        score.second+= sliderOnQueen[1] * (b & safe & attackByTwo[us]).popcount() * (1 + queenImbalance);
    }

    return score;
}

// gets the phase of the game for evalutation
int Anduril::getPhase(libchess::Position &board) {
    int knight = 1, bishop = 1, rook = 2, queen = 4;
    int totalPhase = knight*4 + bishop*4 + rook*4 + queen*2;

    int phase = totalPhase;
    phase -= board.piece_type_bb(libchess::constants::KNIGHT).popcount() * knight;
    phase -= board.piece_type_bb(libchess::constants::BISHOP).popcount() * bishop;
    phase -= board.piece_type_bb(libchess::constants::ROOK).popcount() * rook;
    phase -= board.piece_type_bb(libchess::constants::QUEEN).popcount() * queen;

    return ((phase * 256 + (totalPhase / 2)) / totalPhase);
}