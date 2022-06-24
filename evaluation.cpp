//
// Created by 80hugkev on 6/10/2022.
//

#include <iostream> // we need this for debugging if we print the board
#include <unordered_set>

#include "Anduril.h"
#include "thc.h"
#include "ZobristHasher.h"

// these were totally stolen from the thc.cpp file directly
// Macro to convert chess notation to Square convention,
//  eg SQ('c','5') -> c5
//  (We didn't always have such a sensible Square convention. SQ() remains
//  useful for cases like SQ(file,rank), but you may actually see examples
//  like the hardwired SQ('c','5') which can safely be changed to simply
//  c5).

#define SQ(f,r)  ( (thc::Square) ( ('8'-(r))*8 + ((f)-'a') )   )

// More Square macros
#define FILE(sq)    ( (char) (  ((sq)&0x07) + 'a' ) )           // eg c5->'c'
#define RANK(sq)    ( (char) (  '8' - (((sq)>>3) & 0x07) ) )    // eg c5->'5'
#define IFILE(sq)   (  (int)(sq) & 0x07 )                       // eg c5->2
#define IRANK(sq)   (  7 - ((((int)(sq)) >>3) & 0x07) )         // eg c5->4
#define SOUTH(sq)   (  (thc::Square)((sq) + 8) )                     // eg c5->c4
#define NORTH(sq)   (  (thc::Square)((sq) - 8) )                     // eg c5->c6
#define SW(sq)      (  (thc::Square)((sq) + 7) )                     // eg c5->b4
#define SE(sq)      (  (thc::Square)((sq) + 9) )                     // eg c5->d4
#define NW(sq)      (  (thc::Square)((sq) - 9) )                     // eg c5->b6
#define NE(sq)      (  (thc::Square)((sq) - 7) )                     // eg c5->d6


// generates a static evaluation of the board
int Anduril::evaluateBoard(thc::ChessRules &board) {
    // first check for transpositions
    uint64_t hash = Zobrist::zobristHash(board);
    SimpleNode *eNode = evalTable[hash];
    if (eNode->key == hash) {
        return eNode->score;
    }
    else {
        eNode->key = hash;
    }

    // check for mate first
    thc::TERMINAL mate = thc::NOT_TERMINAL;
    board.Evaluate(mate);
    if (mate == thc::TERMINAL_WCHECKMATE || mate == thc::TERMINAL_BCHECKMATE) {
        return board.WhiteToPlay() ? 999999999 : -999999999;
    }

    // reset the king tropism variables
    attackCount[0] = 0, attackCount[1] = 0;
    attackWeight[0] = 0, attackWeight[1] = 0;
    kingZoneW.clear(), kingZoneB.clear();

    // fill the king zone
    // white
    int startSquare = static_cast<int>(board.wking_square) - 1;
    int endSquare = static_cast<int>(board.wking_square) + 1;
    int startRankModifier = -3;
    int endRankModifier = 1;
    // grab the modifier for the startSquare if needed
    switch (thc::get_file(board.wking_square)) {
        case 'a':
            startSquare++;
            break;
        case 'h':
            endSquare--;
            break;
    }
    // grab the modifier for the startRankModifier if needed
    switch (thc::get_rank(board.wking_square)) {
        case '1':
            endRankModifier--;
            break;
        case '6':
            startRankModifier++;
            break;
        case '7':
            startRankModifier += 2;
            break;
        case '8':
            startRankModifier += 3;
            break;
    }
    for (int modifier = startRankModifier; modifier <= endRankModifier; modifier++) {
        for (int square = startSquare; square <= endSquare; square++) {
            kingZoneW.insert(static_cast<thc::Square>(square + (modifier * 8)));
        }
    }
    // black
    startSquare = static_cast<int>(board.bking_square) - 1;
    endSquare = static_cast<int>(board.bking_square) + 1;
    startRankModifier = -1;
    endRankModifier = 3;
    // grab the modifier for the startSquare if needed
    switch (thc::get_file(board.bking_square)) {
        case 'a':
            startSquare++;
            break;
        case 'h':
            endSquare--;
            break;
    }
    // grab the modifier for the startRankModifier if needed
    switch (thc::get_rank(board.bking_square)) {
        case '1':
            endRankModifier -= 3;
            break;
        case '2':
            endRankModifier -= 2;
            break;
        case '3':
            startRankModifier--;
            break;
        case '8':
            startRankModifier++;
            break;
    }
    for (int modifier = startRankModifier; modifier <= endRankModifier; modifier++) {
        for (int square = startSquare; square <= endSquare; square++) {
            kingZoneB.insert(static_cast<thc::Square>(square + (modifier * 8)));
        }
    }

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
    int king = getKingSafety(board, board.wking_square, board.bking_square);
    scoreMG += king;
    scoreEG += king;

    // give the AI a slap for moving the queen too early
    if (board.squares[59] != 'Q') {
        if (board.squares[57] == 'N') {
            scoreMG -= 2;
            scoreEG -= 2;
        }
        if (board.squares[62] == 'N') {
            scoreMG -= 2;
            scoreEG -= 2;
        }
        if (board.squares[58] == 'B') {
            scoreMG -= 2;
            scoreEG -= 2;
        }
        if (board.squares[61] == 'B') {
            scoreMG -= 2;
            scoreEG -= 2;
        }
    }
    if (board.squares[3] != 'q') {
        if (board.squares[1] == 'n') {
            scoreMG -= 2;
            scoreEG -= 2;
        }
        if (board.squares[6] == 'n') {
            scoreMG -= 2;
            scoreEG -= 2;
        }
        if (board.squares[2] == 'b') {
            scoreMG -= 2;
            scoreEG -= 2;
        }
        if (board.squares[5] == 'b') {
            scoreMG -= 2;
            scoreEG -= 2;
        }
    }

    // bishop pair
    if (whiteBishops.size() >= 2) {
        scoreMG += 25;
        scoreEG += 40;
    }
    if (blackBishops.size() >= 2) {
        scoreMG -= 25;
        scoreEG -= 40;
    }

    // rook on (semi)open files
    bool half = false;
    bool open = true;
    // white
    for (auto rook : whiteRooks) {
        char rookFile = thc::get_file(rook);
        // if we find a pawn on the same file, its not open
        for (auto pawn : whitePawns) {
            if (rookFile == thc::get_file(pawn)) {
                open = false;
                break;
            }
        }

        // if the file is open, find out if its half or fully open, then apply bonus
        if (open) {
            for (auto enemyPawn : blackPawns) {
                if (rookFile == thc::get_file(enemyPawn)) {
                    half = true;
                    break;
                }
            }

            scoreMG += half ? 10 : 20;
            scoreEG += half ? 10 : 20;
        }
    }

    // reset the variable for black
    half = false;
    open = true;
    // black
    for (auto rook : blackRooks) {
        char rookFile = thc::get_file(rook);
        // if we find a pawn on the same file, its not open
        for (auto pawn : blackPawns) {
            if (rookFile == thc::get_file(pawn)) {
                open = false;
                break;
            }
        }

        // if the file is open, find out if its half or fully open, then apply bonus
        if (open) {
            for (auto enemyPawn : whitePawns) {
                if (rookFile == thc::get_file(enemyPawn)) {
                    half = true;
                    break;
                }
            }

            scoreMG -= half ? 10 : 20;
            scoreEG -= half ? 10 : 20;
        }
    }

    // space advantages
    // white
    int whiteSafe = 0;
    for (int i = 2; i <= 5; i++) {
        for (int j = 48; j < 32; j -= 8) {
            thc::Square square = thc::make_square(thc::get_file(static_cast<thc::Square>(i)), thc::get_rank(
                    static_cast<thc::Square>(j)));
            if (!board.AttackedSquare(square, false)) {
                whiteSafe++;
            }
        }
    }
    scoreMG += whiteSafe * 5;
    scoreEG += whiteSafe * 5;

    // black
    int blackSafe = 0;
    for (int i = 2; i <= 5; i++) {
        for (int j = 8; j < 24; j += 8) {
            thc::Square square = thc::make_square(thc::get_file(static_cast<thc::Square>(i)), thc::get_rank(
                    static_cast<thc::Square>(j)));
            if (!board.AttackedSquare(square, true)) {
                blackSafe++;
            }
        }
    }
    scoreMG -= blackSafe * 5;
    scoreEG -= blackSafe * 5;

    // get the phase for tapered eval
    double phase = getPhase(board);

    // for negamax to work, we must return a non-adjusted scoreMG for white
    // and a negated scoreMG for black
    int finalScore = board.WhiteToPlay() ? ((scoreMG * (256 - phase)) + (scoreEG * phase)) / 256 : -((scoreMG * (256 - phase)) + (scoreEG * phase)) / 256;
    eNode->score = finalScore;

    return finalScore;
}

void Anduril::evaluateKnights(thc::ChessRules &board, thc::Square square, bool white) {
    // list of moves for the knight
    thc::MOVELIST list;
    list.count = 0;

    // lookup table for the piece
    const thc::lte *ptr = thc::knight_lookup[static_cast<int>(square)];

    // how many squares does it attack in the zone?
    int attack = 0;

    // generate the move list
    board.ShortMoves(&list, square, ptr, thc::NOT_SPECIAL);

    // white
    if (white) {
        for (int i = 0; i < list.count; i++) {
            if (kingZoneB.contains(list.moves[i].dst)) {
                attack++;
            }
        }
    }
    else {
        for (int i = 0; i < list.count; i++) {
            if (kingZoneW.contains(list.moves[i].dst)) {
                attack++;
            }
        }
    }

    if (attack) {
        attackCount[white]++;
        attackWeight[white] += 2 * attack;
    }

}

void Anduril::evaluateBishops(thc::ChessRules &board, thc::Square square, bool white) {
    // list of moves for the knight
    thc::MOVELIST list;
    list.count = 0;

    // lookup table for the piece
    const thc::lte *ptr = thc::bishop_lookup[static_cast<int>(square)];

    // how many squares does it attack in the zone?
    int attack = 0;

    // generate the move list
    board.LongMoves(&list, square, ptr);

    // white
    if (white) {
        for (int i = 0; i < list.count; i++) {
            if (kingZoneB.contains(list.moves[i].dst)) {
                attack++;
            }
        }
    }
    else {
        for (int i = 0; i < list.count; i++) {
            if (kingZoneW.contains(list.moves[i].dst)) {
                attack++;
            }
        }
    }

    if (attack) {
        attackCount[white]++;
        attackWeight[white] += 2 * attack;
    }
}

void Anduril::evaluateRooks(thc::ChessRules &board, thc::Square square, bool white) {
    // list of moves for the knight
    thc::MOVELIST list;
    list.count = 0;

    // lookup table for the piece
    const thc::lte *ptr = thc::rook_lookup[static_cast<int>(square)];

    // how many squares does it attack in the zone?
    int attack = 0;

    // generate the move list
    board.LongMoves(&list, square, ptr);

    // white
    if (white) {
        for (int i = 0; i < list.count; i++) {
            if (kingZoneB.contains(list.moves[i].dst)) {
                attack++;
            }
        }
    }
    else {
        for (int i = 0; i < list.count; i++) {
            if (kingZoneW.contains(list.moves[i].dst)) {
                attack++;
            }
        }
    }

    if (attack) {
        attackCount[white]++;
        attackWeight[white] += 3 * attack;
    }
}

void Anduril::evaluateQueens(thc::ChessRules &board, thc::Square square, bool white) {
    // list of moves for the knight
    thc::MOVELIST list;
    list.count = 0;

    // lookup table for the piece
    const thc::lte *ptr = thc::queen_lookup[static_cast<int>(square)];

    // how many squares does it attack in the zone?
    int attack = 0;

    // generate the move list
    board.LongMoves(&list, square, ptr);

    // white
    if (white) {
        for (int i = 0; i < list.count; i++) {
            if (kingZoneB.contains(list.moves[i].dst)) {
                attack++;
            }
        }
    }
    else {
        for (int i = 0; i < list.count; i++) {
            if (kingZoneW.contains(list.moves[i].dst)) {
                attack++;
            }
        }
    }

    if (attack) {
        attackCount[white]++;
        attackWeight[white] += 4 * attack;
    }
}

// finds the raw material score for the position
std::vector<int> Anduril::getMaterialScore(thc::ChessRules &board) {
    std::vector<int> score = {0, 0};
    // pawns
    // white
    for (auto square : whitePawns){
        int i = static_cast<int>(square);
        score[0] += 100 + pawnSquareTableMG[i];
        score[1] += 110 + pawnSquareTableEG[i];
    }
    // black
    for (auto square : blackPawns) {
        int i = static_cast<int>(square);
        // calculate the correct value to use for the piece square table
        int tableCoords = ((7 - (i / 8)) * 8) + i % 8;
        score[0] += -100 - pawnSquareTableMG[tableCoords];
        score[1] += -110 - pawnSquareTableEG[tableCoords];
    }

    // knights
    // white
    for (auto square : whiteKnights){
        int i = static_cast<int>(square);
        score[0] += 337 + knightSquareTableMG[i];
        score[1] += 281 + knightSquareTableEG[i];

        // evaluate the square for king tropism
        evaluateKnights(board, square, true);

        // knight pawn bonus
        score[0] += knightPawnBonus[whitePawns.size()];
        score[1] += knightPawnBonus[whitePawns.size()];

        // outposts
        if (square < thc::a3
            && (board.squares[i + 7] == 'P' || board.squares[i + 9] == 'P')
            && board.squares[i - 7] != 'p'
            && board.squares[i - 9] != 'p') {
            score[0] += 10;
            score[1] += 10;
        }

        // trapped knights
        if ((square == thc::h8 && (board.squares[13] == 'p' || board.squares[15] == 'p'))
            || (square == thc::a8) && (board.squares[8] == 'p' || board.squares[10] == 'p')) {
            score[0] -= 150;
            score[1] -= 150;
        }
        else if (square == thc::h7
                 && (board.squares[14] == 'p'
                 && (board.squares[21] == 'p' || board.squares[23] == 'p'))) {
            score[0] -= 150;
            score[1] -= 150;
        }
        else if (square == thc::a7
                 && (board.squares[9] == 'p'
                 && (board.squares[16] == 'p' || board.squares[18] == 'p'))) {
            score[0] -= 150;
            score[1] -= 150;
        }
    }
    // black
    for (auto square : blackKnights) {
        int i = static_cast<int>(square);
        // calculate the correct value to use for the piece square table
        int tableCoords = ((7 - (i / 8)) * 8) + i % 8;
        score[0] += -337 - knightSquareTableMG[tableCoords];
        score[1] += -281 - knightSquareTableEG[tableCoords];

        // evaluate the square for king tropism
        evaluateKnights(board, square, false);

        // knight pawn bonus
        score[0] -= knightPawnBonus[blackPawns.size()];
        score[1] -= knightPawnBonus[blackPawns.size()];

        // outposts
        if (square > thc::h6
            && (board.squares[i - 7] == 'p' || board.squares[i - 9] == 'p')
            && board.squares[i + 7] != 'P'
            && board.squares[i + 9] != 'P') {
            score[0] -= 10;
            score[1] -= 10;
        }

        // trapped knights
        if ((square == thc::h1 && (board.squares[53] == 'P' || board.squares[55] == 'P'))
            || (square == thc::a1) && (board.squares[48] == 'P' || board.squares[50] == 'P')) {
            score[0] += 150;
            score[1] += 150;
        }
        else if (square == thc::h2
                 && (board.squares[54] == 'P'
                 && (board.squares[45] == 'P' || board.squares[47] == 'P'))) {
            score[0] += 150;
            score[1] += 150;
        }
        else if (square == thc::a2
                 && (board.squares[49] == 'P'
                 && (board.squares[40] == 'P' || board.squares[42] == 'P'))) {
            score[0] += 150;
            score[1] += 150;
        }
    }

    // bishops
    // white
    for (auto square : whiteBishops){
        int i = static_cast<int>(square);
        score[0] += 365 + bishopSquareTableMG[i];
        score[1] += 297 + bishopSquareTableEG[i];

        // evaluate for king tropism
        evaluateBishops(board, square, true);

        // fianchetto
        if ((square == thc::g2 && board.wking_square == thc::g1)
            || (square == thc::b2 && board.wking_square == thc::b1)) {
            score[0] += 20;
            score[1] += 20;
        }
    }
    // black
    for (auto square : blackBishops) {
        int i = static_cast<int>(square);
        // calculate the correct value to use for the piece square table
        int tableCoords = ((7 - (i / 8)) * 8) + i % 8;
        score[0] += -365 - bishopSquareTableMG[tableCoords];
        score[1] += -297 - bishopSquareTableEG[tableCoords];

        // evaluate for king tropism
        evaluateBishops(board, square, false);

        // fianchetto
        if ((square == thc::g7 && board.wking_square == thc::g8)
            || (square == thc::b7 && board.wking_square == thc::b8)) {
            score[0] -= 20;
            score[1] -= 20;
        }
    }

    // rooks
    // white
    for (auto square : whiteRooks){
        int i = static_cast<int>(square);
        score[0] += 477 + rookSquareTableMG[i];
        score[1] += 512 + rookSquareTableEG[i];

        score[0] += rookPawnBonus[whitePawns.size()];
        score[1] += rookPawnBonus[whitePawns.size()];

        // evaluate for king tropism
        evaluateRooks(board, square, true);

        // trapped rooks
        switch (square) {
            case thc::h1:
                if (board.wking_square == thc::e1 || board.wking_square == thc::f1 || board.wking_square == thc::g1) {
                    score[0] -= 40;
                    score[1] -= 40;
                }
                break;
            case thc::g1:
                if (board.wking_square == thc::e1 || board.wking_square == thc::f1) {
                    score[0] -= 40;
                    score[1] -= 40;
                }
                break;
            case thc::f1:
                if (board.wking_square == thc::e1) {
                    score[0] -= 40;
                    score[1] -= 40;
                }
                break;
            case thc::a1:
                if (board.wking_square == thc::b1 || board.wking_square == thc::c1 || board.wking_square == thc::d1) {
                    score[0] -= 40;
                    score[1] -= 40;
                }
                break;
            case thc::b1:
                if (board.wking_square == thc::c1 || board.wking_square == thc::d1) {
                    score[0] -= 40;
                    score[1] -= 40;
                }
                break;
            case thc::c1:
                if (board.bking_square == thc::d1) {
                    score[0] -= 40;
                    score[1] -= 40;
                }
                break;
        }
    }
    // black
    for (auto square : blackRooks) {
        int i = static_cast<int>(square);
        // calculate the correct value to use for the piece square table
        int tableCoords = ((7 - (i / 8)) * 8) + i % 8;
        score[0] += -477 - rookSquareTableMG[tableCoords];
        score[1] += -512 - rookSquareTableEG[tableCoords];

        score[0] -= rookPawnBonus[blackPawns.size()];
        score[1] -= rookPawnBonus[blackPawns.size()];

        // evaluate for king tropism
        evaluateRooks(board, square, false);

        // trapped rooks
        switch (square) {
            case thc::h8:
                if (board.bking_square == thc::e8 || board.bking_square == thc::f8 || board.bking_square == thc::g8) {
                    score[0] += 40;
                    score[1] += 40;
                }
                break;
            case thc::g8:
                if (board.bking_square == thc::e8 || board.bking_square == thc::f8) {
                    score[0] += 40;
                    score[1] += 40;
                }
                break;
            case thc::f8:
                if (board.bking_square == thc::e8) {
                    score[0] += 40;
                    score[1] += 40;
                }
                break;
            case thc::a8:
                if (board.bking_square == thc::b8 || board.bking_square == thc::c8 || board.bking_square == thc::d8) {
                    score[0] += 40;
                    score[1] += 40;
                }
                break;
            case thc::b8:
                if (board.bking_square == thc::c8 || board.bking_square == thc::d8) {
                    score[0] += 40;
                    score[1] += 40;
                }
                break;
            case thc::c8:
                if (board.bking_square == thc::d8) {
                    score[0] += 40;
                    score[1] += 40;
                }
                break;
        }
    }

    // queens
    // white
    for (auto square : whiteQueens){
        int i = static_cast<int>(square);
        score[0] += 1025 + queenSquareTableMG[i];
        score[1] += 936  + queenSquareTableEG[i];

        // evaluate for king tropism
        evaluateQueens(board, square, true);
    }
    // black
    for (auto square : blackQueens) {
        int i = static_cast<int>(square);
        // calculate the correct value to use for the piece square table
        int tableCoords = ((7 - (i / 8)) * 8) + i % 8;
        score[0] += -1025 - queenSquareTableMG[tableCoords];
        score[1] += -936  - queenSquareTableEG[tableCoords];

        // evaluate for king tropism
        evaluateQueens(board, square, false);
    }

    // kings
    // white
    int i = static_cast<int>(board.wking_square);
    score[0] += 20000 + kingSquareTableMG[i];
    score[1] += 20000 + kingSquareTableEG[i];
    // black
    i = static_cast<int>(board.bking_square);
    // calculate the correct value to use for the piece square table
    int tableCoords = ((7 - (i / 8)) * 8) + i % 8;
    score[0] += -20000 - kingSquareTableMG[tableCoords];
    score[1] += -20000 - kingSquareTableEG[tableCoords];


    return score;
}

// finds the pawn structure bonus for the position
int Anduril::getPawnScore(thc::ChessRules &board) {
    // these were totally stolen from the thc.cpp file directly
    // Macro to convert chess notation to Square convention,
//  eg SQ('c','5') -> c5
//  (We didn't always have such a sensible Square convention. SQ() remains
//  useful for cases like SQ(file,rank), but you may actually see examples
//  like the hardwired SQ('c','5') which can safely be changed to simply
//  c5).

    // first check for a transposition
    uint64_t hash = Zobrist::hashPawns(board);
    SimpleNode *node = pTable[hash];
    if (node->key == hash) {
        return node->score;
    }
    else {
        node->key = hash;
    }

    int score = 0;

    // white pawns
    for (auto square : whitePawns) {
        bool isolated = true;
        int i = static_cast<int>(square);
        // isolated pawns and defended pawns
        switch (FILE(square)) {
            case 'a':
                // isolated pawns
                // north
                for (int j = i + 1; j >= 0; j -= 8) {
                    if (board.squares[j] == 'P') {
                        isolated = false;
                        break;
                    }
                }

                // south
                for (int j = i + 1; j <= 63; j += 8) {
                    if (board.squares[j] == 'P') {
                        isolated = false;
                        break;
                    }
                }

                if (isolated) {
                    score -= 10;
                }

                // defended pawns
                if (board.squares[i - 7] == 'P'){
                    score += 10;
                }

                break;

            case 'h':
                // isolated pawns
                // north
                for (int j = i - 1; j >= 0; j -= 8) {
                    if (board.squares[j] == 'P') {
                        isolated = false;
                        break;
                    }
                }

                // south
                for (int j = i - 1; j <= 63; j += 8) {
                    if (board.squares[j] == 'P') {
                        isolated = false;
                        break;
                    }
                }

                if (isolated) {
                    score -= 10;
                }

                // defended pawns
                if (board.squares[i - 9] == 'P'){
                    score += 10;
                }

                break;
            default:
                // isolated pawns
                // left
                // north
                for (int j = i - 1; j >= 0; j -= 8) {
                    if (board.squares[j] == 'P') {
                        isolated = false;
                        break;
                    }
                }

                // south
                for (int j = i - 1; j <= 63; j += 8) {
                    if (board.squares[j] == 'P') {
                        isolated = false;
                        break;
                    }
                }

                // right
                // north
                for (int j = i + 1; j >= 0; j -= 8) {
                    if (board.squares[j] == 'P') {
                        isolated = false;
                        break;
                    }
                }

                // south
                for (int j = i + 1; j <= 63; j += 8) {
                    if (board.squares[j] == 'P') {
                        isolated = false;
                        break;
                    }
                }

                if (isolated) {
                    score -= 10;
                }

                // defended pawns
                if (board.squares[i - 7] == 'P'){
                    score += 15;
                }
                if (board.squares[i - 9] == 'P'){
                    score += 15;
                }
        }

        // doubled pawns
        // north
        for (int j = i; j > 0; j -= 8) {
            if (board.squares[j - 8] == 'P') {
                score -= 20;
            }
        }
        // south
        for (int j = i; j < 63; j += 8) {
            if (board.squares[j + 8] == 'P') {
                score -= 20;
            }
        }

        // passed pawns
        bool passed = true;
        for (int j = i; j >= 0; j -= 8) {
            if (board.squares[j] == 'p') {
                passed = false;
                break;
            }
            if (IFILE(square) != 0 && board.squares[j - 1] == 'p') {
                passed = false;
                break;
            }
            if (IFILE(square) != 7 && board.squares[j + 1] == 'p') {
                passed = false;
                break;
            }
        }
        if (passed){
            score += 15;
        }
    }

    // black pawns
    for (auto square : blackPawns) {
        bool isolated = true;
        int i = static_cast<int>(square);

        // isolated pawns and defended pawns
        switch (FILE(square)) {
            case 'a':
                // north
                for (int j = i + 1; j >= 0; j -= 8) {
                    if (board.squares[j] == 'p') {
                        isolated = false;
                        break;
                    }
                }
                // south
                for (int j = i + 1; j <= 63; j += 8) {
                    if (board.squares[j] == 'p') {
                        isolated = false;
                        break;
                    }
                }

                if (isolated) {
                    score += 10;
                }

                // defended pawns
                if (board.squares[i + 9] == 'p'){
                    score += 10;
                }

                break;

            case 'h':
                // north
                for (int j = i - 1; j >= 0; j -= 8) {
                    if (board.squares[j] == 'p') {
                        isolated = false;
                        break;
                    }
                }
                // south
                for (int j = i - 1; j <= 63; j += 8) {
                    if (board.squares[j] == 'p') {
                        isolated = false;
                        break;
                    }
                }

                if (isolated) {
                    score += 10;
                }

                // defended pawns
                if (board.squares[i + 7] == 'p'){
                    score += 10;
                }

                break;

            default:
                // isolated pawns
                // left
                // north
                for (int j = i - 1; j >= 0; j -= 8) {
                    if (board.squares[j] == 'p') {
                        isolated = false;
                        break;
                    }
                }
                // south
                for (int j = i - 1; j <= 63; j += 8) {
                    if (board.squares[j] == 'p') {
                        isolated = false;
                        break;
                    }
                }

                // right
                // north
                for (int j = i + 1; j >= 0; j -= 8) {
                    if (board.squares[j] == 'p') {
                        isolated = false;
                        break;
                    }
                }
                // south
                for (int j = i + 1; j <= 63; j += 8) {
                    if (board.squares[j] == 'p') {
                        isolated = false;
                        break;
                    }
                }

                if (isolated) {
                    score += 10;
                }

                // defended pawns
                if (board.squares[i + 7] == 'p'){
                    score += 10;
                }
                if (board.squares[i + 9] == 'p'){
                    score += 10;
                }
        }

        // doubled pawns
        // north
        for (int j = i; j > 0; j -= 8) {
            if (board.squares[j - 8] == 'p') {
                score += 20;
            }
        }
        // south
        for (int j = i; j < 63; j += 8) {
            if (board.squares[j + 8] == 'p') {
                score += 20;
            }
        }

        // passed pawns
        bool passed = true;
        for (int j = i; j <= 63; j += 8) {
            if (board.squares[j] == 'P') {
                passed = false;
                break;
            }
            if (IFILE(square) != 0 && board.squares[j - 1] == 'P') {
                passed = false;
                break;
            }
            if (IFILE(square) != 7 && board.squares[j + 1] == 'P') {
                passed = false;
                break;
            }
        }
        if (passed){
            score -= 15;
        }
    }

    // add the score to the transposition table
    node->score = score;

    return score;
}

// finds the king safety bonus for the position
int Anduril::getKingSafety(thc::ChessRules &board, thc::Square whiteKing, thc::Square blackKing) {
    int score = 0;
    // white shield
    if (thc::get_rank(whiteKing) == '1' && whiteKing != thc::d1 && whiteKing != thc::e1) {
        // queen side
        if (whiteKing == thc::a1 || whiteKing == thc::b1 || whiteKing == thc::c1) {
            if (board.squares[48] != 'P' && board.squares[40] != 'P') {
                score -= 20;
                bool open = true;
                for (int i = 32; i > 0; i -= 8) {
                    if (board.squares[i] == 'P' || board.squares[i] == 'p') {
                        open = false;
                        break;
                    }
                }
                if (open) {
                    score -= 20;
                }
            }
            if (board.squares[49] != 'P' && board.squares[41] != 'P') {
                score -= 20;
                bool open = true;
                for (int i = 33; i > 0; i -= 8) {
                    if (board.squares[i] == 'P' || board.squares[i] == 'p') {
                        open = false;
                        break;
                    }
                }
                if (open) {
                    score -= 20;
                }
            }
            if (board.squares[50] != 'P' && board.squares[42] != 'P') {
                score -= 20;
                bool open = true;
                for (int i = 34; i > 0; i -= 8) {
                    if (board.squares[i] == 'P' || board.squares[i] == 'p') {
                        open = false;
                        break;
                    }
                }
                if (open) {
                    score -= 20;
                }
            }
        }
            // king side
        else {
            if (board.squares[53] != 'P' && board.squares[45] != 'P') {
                score -= 20;
                bool open = true;
                for (int i = 37; i > 0; i -= 8) {
                    if (board.squares[i] == 'P' || board.squares[i] == 'p') {
                        open = false;
                        break;
                    }
                }
                if (open) {
                    score -= 20;
                }
            }
            if (board.squares[54] != 'P' && board.squares[46] != 'P') {
                score -= 20;
                bool open = true;
                for (int i = 38; i > 0; i -= 8) {
                    if (board.squares[i] == 'P' || board.squares[i] == 'p') {
                        open = false;
                        break;
                    }
                }
                if (open) {
                    score -= 20;
                }
            }
            if (board.squares[55] != 'P' && board.squares[47] != 'P') {
                score -= 20;
                bool open = true;
                for (int i = 39; i > 0; i -= 8) {
                    if (board.squares[i] == 'P' || board.squares[i] == 'p') {
                        open = false;
                        break;
                    }
                }
                if (open) {
                    score -= 20;
                }
            }
        }
    }
    // black shield
    if (thc::get_rank(blackKing) == '8' && blackKing != thc::d8 && blackKing != thc::e8) {
        // queen side
        if (blackKing == thc::a8 || blackKing == thc::b8 || blackKing == thc::c8) {
            if (board.squares[8] != 'p' && board.squares[16] != 'p') {
                score += 20;
                bool open = true;
                for (int i = 24; i < 63; i += 8) {
                    if (board.squares[i] == 'P' || board.squares[i] == 'p') {
                        open = false;
                        break;
                    }
                }
                if (open) {
                    score += 20;
                }
            }
            if (board.squares[9] != 'p' && board.squares[17] != 'p') {
                score += 20;
                bool open = true;
                for (int i = 25; i < 63; i += 8) {
                    if (board.squares[i] == 'P' || board.squares[i] == 'p') {
                        open = false;
                        break;
                    }
                }
                if (open) {
                    score += 20;
                }
            }
            if (board.squares[10] != 'p' && board.squares[18] != 'p') {
                score += 20;
                bool open = true;
                for (int i = 26; i < 63; i += 8) {
                    if (board.squares[i] == 'P' || board.squares[i] == 'p') {
                        open = false;
                        break;
                    }
                }
                if (open) {
                    score += 20;
                }
            }
        }
            // king side
        else {
            if (board.squares[13] != 'p' && board.squares[21] != 'p') {
                score += 20;
                bool open = true;
                for (int i = 29; i < 63; i += 8) {
                    if (board.squares[i] == 'P' || board.squares[i] == 'p') {
                        open = false;
                        break;
                    }
                }
                if (open) {
                    score += 20;
                }
            }
            if (board.squares[14] != 'p' && board.squares[22] != 'p') {
                score += 20;
                bool open = true;
                for (int i = 30; i < 63; i += 8) {
                    if (board.squares[i] == 'P' || board.squares[i] == 'p') {
                        open = false;
                        break;
                    }
                }
                if (open) {
                    score += 20;
                }
            }
            if (board.squares[15] != 'p' && board.squares[23] != 'p') {
                score += 20;
                bool open = true;
                for (int i = 31; i < 63; i += 8) {
                    if (board.squares[i] == 'P' || board.squares[i] == 'p') {
                        open = false;
                        break;
                    }
                }
                if (open) {
                    score += 20;
                }
            }
        }
    }

    // pawn storms
    // white
    int startSquare = static_cast<int>(whiteKing) - 2;
    int endSquare = static_cast<int>(whiteKing) + 2;
    int startRankModifier = -2;
    int endRankModifier = 2;
    // grab the modifier for the startSquare if needed
    switch (thc::get_file(whiteKing)) {
        case 'a':
            startSquare += 2;
            break;
        case 'b':
            startSquare++;
            break;
        case 'g':
            endSquare--;
            break;
        case 'h':
            endSquare -= 2;
            break;
    }
    // grab the modifier for the startRankModifier if needed
    switch (thc::get_rank(whiteKing)) {
        case '1':
            endRankModifier -= 2;
            break;
        case '2':
            endRankModifier--;
            break;
        case '7':
            startRankModifier++;
            break;
        case '8':
            startRankModifier += 2;
            break;
    }


    // do the actual loop
    // searches the kings location +2 in every direction
    // or until an edge is hit (determined by start and end of square and rankModifier being set above)
    for (int modifier = startRankModifier; modifier <= endRankModifier; modifier++) {
        for (int square = startSquare; square <= endSquare; square++) {
            if (board.squares[square + (modifier * 8)] == 'p') {
                score -= 5;
            }
        }
    }

    // black
    startSquare = static_cast<int>(blackKing) - 2;
    endSquare = static_cast<int>(blackKing) + 2;
    startRankModifier = -2;
    endRankModifier = 2;
    // grab the modifier for the startSquare if needed
    switch (thc::get_file(blackKing)) {
        case 'a':
            startSquare += 2;
            break;
        case 'b':
            startSquare++;
            break;
        case 'g':
            endSquare--;
            break;
        case 'h':
            endSquare -= 2;
            break;
    }
    // grab the modifier for the startRankModifier if needed
    switch (thc::get_rank(blackKing)) {
        case '1':
            endRankModifier -= 2;
            break;
        case '2':
            endRankModifier--;
            break;
        case '7':
            startRankModifier++;
            break;
        case '8':
            startRankModifier += 2;
            break;
    }


    // do the actual loop
    // searches the kings location +2 in every direction
    // or until an edge is hit (determined by start and end of square and rankModifier being set above)
    for (int modifier = startRankModifier; modifier <= endRankModifier; modifier++) {
        for (int square = startSquare; square <= endSquare; square++) {
            if (board.squares[square + (modifier * 8)] == 'P') {
                score += 5;
            }
        }
    }

    // king tropism (black is index 0, white is index 1)
    // white
    if (attackCount[1] >= 2) {
        score += SafetyTable[attackWeight[1]];
    }
    // black
    if (attackCount[0] >= 2) {
        score -= SafetyTable[attackWeight[0]];
    }


    return score;
}

// gets the phase of the game for evaluation
double Anduril::getPhase(thc::ChessRules &board) {
    double pawn = 0.125, knight = 1, bishop = 1, rook = 2, queen = 4;
    double totalPhase = pawn*8 + knight*4 + bishop*4 + rook*4 + queen*2;

    double phase = totalPhase;
    phase -= (blackPawns.size() + whitePawns.size()) * pawn;
    phase -= (blackKnights.size() + whiteKnights.size()) * knight;
    phase -= (blackBishops.size() + whiteBishops.size()) * bishop;
    phase -= (blackRooks.size() + whiteRooks.size()) * rook;
    phase -= (blackQueens.size() + whiteQueens.size()) * queen;

    return (phase * 256 + (totalPhase / 2)) / totalPhase;
}