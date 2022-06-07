//
// Created by Hughe on 4/1/2022.
//

#include <algorithm>
#include <cmath>
#include <iostream>

#include "Anduril.h"
#include "ConsoleGame.h"

// finds the raw material score for the position
std::vector<int> Anduril::getMaterialScore(thc::ChessRules &board) {
    std::vector<int> score = {0, 0};
    // pawns
    // white
    for (auto square : whitePawns){
        int i = static_cast<int>(square);
        score[0] += 100 + pawnSquareTableMG[i];
        score[1] += 100 + pawnSquareTableEG[i];
    }
    // black
    for (auto square : blackPawns) {
        int i = static_cast<int>(square);
        // calculate the correct value to use for the piece square table
        int tableCoords = ((7 - (i / 8)) * 8) + i % 8;
        score[0] += -100 - pawnSquareTableMG[tableCoords];
        score[1] += -100 - pawnSquareTableEG[tableCoords];
    }

    // knights
    // white
    for (auto square : whiteKnights){
        int i = static_cast<int>(square);
        score[0] += 300 + knightSquareTableMG[i];
        score[1] += 300 + knightSquareTableEG[i];
    }
    // black
    for (auto square : blackKnights) {
        int i = static_cast<int>(square);
        // calculate the correct value to use for the piece square table
        int tableCoords = ((7 - (i / 8)) * 8) + i % 8;
        score[0] += -300 - knightSquareTableMG[tableCoords];
        score[1] += -300 - knightSquareTableEG[tableCoords];
    }

    // bishops
    // white
    for (auto square : whiteBishops){
        int i = static_cast<int>(square);
        score[0] += 320 + bishopSquareTableMG[i];
        score[1] += 350 + bishopSquareTableEG[i];
    }
    // black
    for (auto square : blackBishops) {
        int i = static_cast<int>(square);
        // calculate the correct value to use for the piece square table
        int tableCoords = ((7 - (i / 8)) * 8) + i % 8;
        score[0] += -320 - bishopSquareTableMG[tableCoords];
        score[1] += -350 - bishopSquareTableEG[tableCoords];
    }

    // rooks
    // white
    for (auto square : whiteRooks){
        int i = static_cast<int>(square);
        score[0] += 500 + rookSquareTableMG[i];
        score[1] += 500 + rookSquareTableEG[i];
    }
    // black
    for (auto square : blackRooks) {
        int i = static_cast<int>(square);
        // calculate the correct value to use for the piece square table
        int tableCoords = ((7 - (i / 8)) * 8) + i % 8;
        score[0] += -500 - rookSquareTableMG[tableCoords];
        score[1] += -500 - rookSquareTableEG[tableCoords];
    }

    // queens
    // white
    for (auto square : whiteQueens){
        int i = static_cast<int>(square);
        score[0] += 900 + queenSquareTableMG[i];
        score[1] += 900 + queenSquareTableEG[i];
    }
    // black
    for (auto square : blackQueens) {
        int i = static_cast<int>(square);
        // calculate the correct value to use for the piece square table
        int tableCoords = ((7 - (i / 8)) * 8) + i % 8;
        score[0] += -900 - queenSquareTableMG[tableCoords];
        score[1] += -900 - queenSquareTableEG[tableCoords];
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

    // first check for a transposition
    uint64_t hash = hashPawns(board);
    if (pawnTranspoTable.count(hash) == 1) {
        return pawnTranspoTable[hash];
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
    pawnTranspoTable[hash] = score;

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

    return score;
}

// generates a static evaluation of the board
int Anduril::evaluateBoard(thc::ChessRules &board) {
    // check for mate first
    thc::TERMINAL mate = thc::NOT_TERMINAL;
    board.Evaluate(mate);
    if (mate == thc::TERMINAL_WCHECKMATE || mate == thc::TERMINAL_BCHECKMATE) {
        return board.WhiteToPlay() ? 999999999 : -999999999;
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
    if (rootPly < 18) {
        // white
        for (int i = 1; i < 16; i += 2) {
            if (board.getMoveFromStack(i).src == thc::d1) {
                scoreMG -= 15;
                break;
            }
        }

        // black
        for (int i = 2; i < 16; i += 2) {
            if (board.getMoveFromStack(i).src == thc::d8) {
                scoreMG += 15;
                break;
            }
        }
    }

    // bishop pair
    if (whiteBishops.size() >= 2) {
        scoreMG += 25;
        scoreEG += 40;
    }
    if (blackBishops.size() >= 2) {
        scoreMG -= 25;
        scoreEG += 40;
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
    return board.WhiteToPlay() ? ((scoreMG * (256 - phase)) + (scoreEG * phase)) / 256 : -((scoreMG * (256 - phase)) + (scoreEG * phase)) / 256;
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

// Most Valuable Victim, Least Valuable Attacker
int Anduril::MVVLVA(thc::ChessRules &board, int src, int dst) {
    return std::abs(pieceValues[board.squares[dst]]) - std::abs(pieceValues[board.squares[src]]);
}

// generates a move list in the order we want to search them
std::vector<std::tuple<int, thc::Move>> Anduril::getMoveList(thc::ChessRules &board, Node *node) {
    // current ply for finding killer moves
    int ply = board.WhiteToPlay() ? (board.full_move_count * 2) - 1 : board.full_move_count * 2;

    // this is the list of moves with their assigned scores
    std::vector<std::tuple<int, thc::Move>> movesWithScores;

    // get a list of possible moves
    thc::MOVELIST moves;
    board.GenLegalMoveList(&moves);

    // loop through the board, give each move a score, and add it to the list
    for (int i = 0; i < moves.count; i++) {
        // first check if the move is a hash move
        if (node != nullptr && moves.moves[i] == node->bestMove) {
            movesWithScores.emplace_back(10000, moves.moves[i]);
            continue;
        }

        // next check if the move is a capture
        if (moves.moves[i].capture != ' ') {
            movesWithScores.emplace_back(MVVLVA(board, moves.moves[i].src, moves.moves[i].dst), moves.moves[i]);
            continue;
        }

        // next check for killer moves
        if (std::count(killers[ply - rootPly].begin(), killers[ply - rootPly].end(), moves.moves[i]) != 0) {
            // the value of -30 makes sure that killers are searched after equal captures.
            // equal captures are going to have a value between 20 and -20 because the value of knights and bishops were
            // marked with an offset of 20 from each other.  I don't know why I did this either but it's not too big of a deal
            movesWithScores.emplace_back(-30, moves.moves[i]);
            continue;
        }

        // the move isn't special, mark it to be searched with every other non-capture
        // the value of -40 was chosen because it makes sure the moves are searched after killers, but before losing captures
        movesWithScores.emplace_back(-40, moves.moves[i]);
    }

    return movesWithScores;
}

std::vector<std::tuple<int, thc::Move>> Anduril::getQMoveList(thc::ChessRules &board, Node *node) {
    // this is the list of moves with their assigned scores
    std::vector<std::tuple<int, thc::Move>> movesWithScores;

    // get a list of possible moves
    thc::MOVELIST moves;
    board.GenLegalMoveList(&moves);

    for (int i = 0; i < moves.count; i++) {
        // first check if the move is a capture
        if (moves.moves[i].capture != ' ') {
            // next check if it's the hash move
            if (node != nullptr && moves.moves[i] == node->bestMove) {
                movesWithScores.emplace_back(10000, moves.moves[i]);
                continue;
            }
            // if it isn't a hash move, add it with its MVVLVA score
            movesWithScores.emplace_back(MVVLVA(board, moves.moves[i].src, moves.moves[i].dst), moves.moves[i]);
        }
    }

    return movesWithScores;
}

// the quiesce search
// searches possible captures to make sure we aren't mis-evaluating certain positions
template <Anduril::NodeType nodeType>
int Anduril::quiesce(thc::ChessRules &board, int alpha, int beta, int Qdepth) {
    constexpr bool PvNode = nodeType == PV;
    if (Qdepth <= 0){
        return evaluateBoard(board);
    }


    // represents the best score we have found so far
    int bestScore = -999999999;

    // transposition lookup
    uint64_t hash = zobrist(board);
    Node *node;
    if (transpoTable.count(hash) == 1) {
        node = transpoTable[hash];
        movesTransposed++;
        if (!PvNode && node->nodeDepth >= -Qdepth) {
            switch (node->nodeType) {
                case 1:
                    return node->nodeScore;
                    break;
                case 2:
                    if (node->nodeScore >= beta) {
                        return node->nodeScore;
                    }
                    break;
                case 3:
                    if (node->nodeScore <= alpha && node->nodeScore != -999999999) {
                        return node->nodeScore;
                    }
                    break;
            }
        }
        node->nodeDepth = -Qdepth;
    }
    else {
        node = new Node();
        transpoTable[hash] = node;
        node->nodeDepth = -Qdepth;
    }

    // stand pat score to see if we can exit early
    int standPat = evaluateBoard(board);
    bestScore = standPat;
    node->nodeScore = standPat;

    // adjust alpha and beta based on the stand pat
    if (standPat >= beta) {
        node->nodeType = 2;
        return standPat;
    }
    if (PvNode && standPat > alpha) {
        alpha = standPat;
    }

    // get a move list
    std::vector<std::tuple<int, thc::Move>> moveList;
    if (node->nodeScore != -999999999) {
        moveList = getQMoveList(board, node);
    }
    else {
        moveList = getQMoveList(board, nullptr);
    }

    // if there are no moves, we can just return our stand pat
    if (moveList.empty()){
        return standPat;
    }

    // arbitrarily low score to make sure its replaced
    int score = -999999999;

    // holds the move we want to search
    thc::Move move;

    // loop through the moves and score them
    for (int i = 0; i < moveList.size(); i++) {
        move = pickNextMove(moveList, i);
        makeMove(board, move);
        //board.PushMove(move);
        movesExplored++;
        quiesceExplored++;
        score = -quiesce<nodeType>(board, -beta, -alpha, Qdepth - 1);
        undoMove(board, move);
        //board.PopMove(move);

        if (score > bestScore) {
            bestScore = score;
            node->nodeScore = score;
            node->bestMove = move;
            if (score > alpha) {
                if (score >= beta) {
                    cutNodes++;
                    node->nodeType = 2;
                    return bestScore;
                }
                node->nodeType = 1;
                alpha = score;
            }
        }
    }
    return bestScore;

}

// the negamax function.  Does the heavy lifting for the search
template <Anduril::NodeType nodeType>
int Anduril::negamax(thc::ChessRules &board, int depth, int alpha, int beta) {
    depthNodes++;
    constexpr bool PvNode = nodeType == PV;

    // check for 50 move rule
    if (board.half_move_clock >= 100) {
        return 0;
    }

    // if we are at max depth, start a quiescence search
    if (depth <= 0){
        return quiesce<PvNode ? PV : NonPV>(board, alpha, beta, 4);
    }

    // represents the best score we have found so far
    int bestScore = -999999999;

    // holds the score of the move we just searched
    int score = -999999999;

    // current ply (white moves + black moves from the starting position)
    int ply = board.WhiteToPlay() ? (board.full_move_count * 2) - 1 : (board.full_move_count * 2);

    // transposition lookup
    uint64_t hash = zobrist(board);
    Node *node = nullptr;
    if (transpoTable.count(hash) == 1) {
        node = transpoTable[hash];
        movesTransposed++;
        if (!PvNode && node->nodeDepth >= depth) {
            switch (node->nodeType) {
                case 1:
                    return node->nodeScore;
                    break;
                case 2:
                    if (node->nodeScore >= beta) {
                        return node->nodeScore;
                    }
                    break;
                case 3:
                    if (node->nodeScore <= alpha && node->nodeScore != -999999999) {
                        return node->nodeScore;
                    }
                    break;
            }
        }
        node->nodeDepth = depth;
    }
    else {
        node = new Node();
        transpoTable[hash] = node;
        node->nodeDepth = depth;
    }

    // get the static evaluation
    int staticEval = evaluateBoard(board);

    // razoring
    // weird magic values stolen straight from stockfish
    if (!PvNode && depth <= 3 && staticEval < alpha - 348 - 258 * depth * depth) {
        score = quiesce<NonPV>(board, alpha - 1, alpha, 4);
        if (score < alpha) {
            return score;
        }
    }

    // null move pruning
    if (!PvNode && board.getMoveFromStack(ply - 1).Valid()
        && (board.WhiteToPlay() ? board.AttackedPiece(board.wking_square) : board.AttackedPiece(board.bking_square))
        && staticEval >= beta
        && nonPawnMaterial(board.WhiteToPlay())
        && depth >= 4
        && nmpColorCurrent != board.WhiteToPlay()) {

        // we pass an invalid move to make move to represent a passing turn
        thc::Move nullMove;
        nullMove.Invalid();

        nmpColorCurrent = static_cast<nmpColor>(board.WhiteToPlay());

        makeMove(board, nullMove);
        int nullScore = -negamax<NonPV>(board, depth - 2, -beta, -beta + 1);
        undoMove(board, nullMove);

        if (nullScore >= beta) {
            return nullScore;
        }
    }

    // generate a move list
    std::vector<std::tuple<int, thc::Move>> moveList;
    if (node->nodeScore != -999999999) {
        moveList = getMoveList(board, node);
    }
    else {
        moveList = getMoveList(board, nullptr);
    }

    // if the move list is empty, we found mate or stalemate and the current player lost
    if (moveList.empty()){
        // find out how the game ended
        thc::TERMINAL endType = thc::NOT_TERMINAL;
        board.Evaluate(endType);

        // if it's mate, return "-infinity"
        if (endType == thc::TERMINAL_WCHECKMATE || endType == thc::TERMINAL_BCHECKMATE) {
            return -999999999;
        }

        // if it's not checkmate, then it's stalemate
        return 0;
    }

    // represents our next move to search
    thc::Move move;

    // do we need to search a move with the full depth?
    bool doFull = false;

    // loop through the possible moves and score each
    for (int i = 0; i < moveList.size(); i++) {
        move = pickNextMove(moveList, i);

        //board.PushMove(move);
        movesExplored++;

        // search with zero window
        // first check if we can reduce
        doFull = false;
        if (depth > 1 && i > 2 && isLateReduction(board, move)) {
            makeMove(board, move);
            score = -negamax<NonPV>(board, depth - 2, -(alpha + 1), -alpha);
            doFull = score > alpha && score < beta;
            undoMove(board, move);
        }
        else {
            doFull = !PvNode || i > 0;
        }
        if (doFull) {
            makeMove(board, move);
            score = -negamax<NonPV>(board, depth - 1, -(alpha + 1), -alpha);
            undoMove(board, move);
        }

        // full PV search for the first move and for moves that fail in the zero window
        if (PvNode && (i == 0 || (score > alpha && score < beta))) {
            makeMove(board, move);
            score = -negamax<PV>(board, depth - 1, -beta, -alpha);
            undoMove(board, move);
        }

        //board.PopMove(move);

        if (score > bestScore) {
            bestScore = score;
            node->nodeScore = score;
            node->bestMove = move;
            if (score > alpha) {
                if (score >= beta) {
                    cutNodes++;
                    node->nodeType = 2;
                    if (move.capture == ' ') {
                        insertKiller(ply - rootPly, move);
                    }
                    return bestScore;
                }
                node->nodeType = 1;
                alpha = score;
            }
        }
    }

    return bestScore;

}

// zero window search will search a node with tight bounds to see if it needs a full search or if we can skip the node
int Anduril::zwSearch(thc::ChessRules &board, int beta, int depth) {
    depthNodes++;
    if (depth <= 0) {
        return quiesce<NonPV>(board, beta - 1, beta, 4);
    }
    // represents the best score we have found so far
    int bestScore = -999999999;

    // holds the score of the move we just searched
    int score = -999999999;

    // transposition lookup
    // NOTE: the transposition table is only being used for move ordering here
    uint64_t hash = zobrist(board);
    Node *node = nullptr;
    if (transpoTable.count(hash) == 1) {
        node = transpoTable[hash];
        movesTransposed++;
    }
    else {
        node = new Node();
        transpoTable[hash] = node;
        node->nodeDepth = depth;
    }

    // generate a move list
    std::vector<std::tuple<int, thc::Move>> moveList;
    if (node->nodeScore != -999999999) {
        moveList = getMoveList(board, node);
    }
    else {
        moveList = getMoveList(board, nullptr);
    }

    // if the move list is empty, we found mate and the current player lost
    if (moveList.empty()){
        return score;
    }

    // holds the move we want to search
    thc::Move move;

    // loop through the possible moves and score each
    for (int i = 0; i < moveList.size(); i++) {
        move = pickNextMove(moveList, i);
        makeMove(board, move);
        //board.PushMove(move);
        movesExplored++;
        score = -zwSearch(board, 1 - beta, depth - 1);
        undoMove(board, move);

        if (score > bestScore) {
            bestScore = score;
            node->nodeScore = score;
            node->bestMove = move;
            if (score >= beta) {
                cutNodes++;
                node->nodeType = 2;
                return bestScore;
            }
        }
    }

    return bestScore;

}


// calls negamax and keeps track of the best move
void Anduril::go(thc::ChessRules &board, int depth) {
    int alpha = -999999999;
    int beta = 999999999;
    int bestScore = -999999999;

    // set up the piece lists
    fillPieceLists(board);

    // set the killer vector to have the correct number of slots
    // and the root node's ply
    killers = std::vector<std::vector<thc::Move>>(depth + 1);
    for (auto & killer : killers) {
        killer = std::vector<thc::Move>(2);
    }

    rootPly = board.WhiteToPlay() ? (board.full_move_count * 2) - 1 : board.full_move_count * 2;

    // these variables are for debugging
    int aspMissesL = 0, aspMissesH = 0;
    int n1 = 0;
    double branchingFactor = 0;
    double avgBranchingFactor = 0;
    bool research = false;
    std::vector<int> misses;
    // this makes sure the score is reported correctly
    // if the AI is controlling black, we need to
    // multiply the score by -1 to report it correctly to the player
    int flipped = 1;
    thc::Move bestMove;

    // we need to values for alpha because we need to change alpha based on
    // our results, but we also need a copy of the original alpha for the
    // aspiration window
    int alphaTheSecond = alpha;

    // get the move list
    std::vector<std::tuple<int, thc::Move>> moveListWithScores = getMoveList(board, nullptr);

    // this starts our move to search at the first move in the list
    thc::Move move = std::get<1>(moveListWithScores[0]);

    if (!board.white){
        flipped = -1;
    }

    int score = -999999999;
    int deep = 1;
    bool finalDepth = false;
    // iterative deepening loop
    while (!finalDepth) {
        if (deep == depth){
            finalDepth = true;
        }

        alphaTheSecond = alpha;

        for (int i = 0; i < moveListWithScores.size(); i++) {
            // find the next best move to search
            // skips this step if we are on the first depth because all scores will be zero
            if (deep != 1) {
                move = pickNextMove(moveListWithScores, i);
            }
            else {
                move = std::get<1>(moveListWithScores[i]);
            }

            makeMove(board, move);
            //board.PushMove(move);
            deep--;
            movesExplored++;
            depthNodes++;
            score = -negamax<PV>(board, deep, -beta, -alphaTheSecond);
            deep++;
            undoMove(board, move);
            //board.PopMove(move);

            if (score > bestScore || (score == -999999999 && bestScore == -999999999)) {
                bestScore = score;
                alphaTheSecond = score;
                bestMove = move;
            }

            // see if we need to reset the aspiration window
            if ((bestScore <= alpha || bestScore >= beta) && !(bestScore == 999999999 || bestScore == -999999999)) {
                break;
            }

            // update the score within the tuple
            std::get<0>(moveListWithScores[i]) = score;

        }

        // check if we found mate
        if (bestScore == 999999999 || bestScore == -999999999) {
            break;
        }

        /*
        // move the previous search's best move to the front of the list for the next search
        if (bestMove != moveList[0]){
            for (auto i = moveList.begin(); i != moveList.end();){
                if (*i == bestMove){
                    moveList.erase(i);
                    moveList.insert(moveList.begin(), bestMove);
                    break;
                }
                i++;
            }
        }
        */

        // set the aspiration window
        // we only actually use the aspiration window at depths greater than 2.
        // this is because we don't have a decent picture of the position until after each player has moved.
        if (deep >= 2) {
            // search was outside the window, need to redo the search
            // fail low
            if (bestScore <= alpha) {
                finalDepth = false;
                misses.push_back(deep);
                aspMissesL++;
                alpha = bestScore - 50 * (std::pow(3, aspMissesL));
                beta = bestScore + 50 * (std::pow(3, aspMissesH));
                research = true;
            }
            // fail high
            else if (bestScore >= beta) {
                finalDepth = false;
                misses.push_back(deep);
                aspMissesH++;
                alpha = bestScore - 50 * (std::pow(3, aspMissesL));
                beta = bestScore + 50 * (std::pow(3, aspMissesH));
                research = true;
            }
            // the search didn't fall outside the window, we can move to the next depth
            else {
                alpha = bestScore - 50 * (std::pow(3, aspMissesL));
                beta = bestScore + 50 * (std::pow(3, aspMissesH));
                deep++;
                research = false;
            }
        }
        // for depths less than 2
        else {
            deep++;
            research = false;
        }

        // add the current depth to the branching factor
        // if we searched to depth 1, then we need to do another search
        // to get a branching factor
        if (n1 == 0){
            n1 = movesExplored;
        }
        // we can't calculate a branching factor if we researched because of
        // a window miss, so we first check if the last entry to misses is our current depth
        else if (!research) {
            branchingFactor = (double) depthNodes / n1;
            avgBranchingFactor = ((avgBranchingFactor * (deep - 2)) + branchingFactor) / (deep - 1);
            n1 = depthNodes;
            depthNodes = 0;
        }

        // reset the variables to prepare for the next loop
        if (!finalDepth) {
            bestScore = -999999999;
            alphaTheSecond = -999999999;
            score = -999999999;
        }
    }

    std::vector<thc::Move> PV = getPV(board, depth, bestMove);

    // output information about the search to the console
    // mostly info for debug, but still interesting to look at
    // for a player
    std::cout << "Total moves explored: " << movesExplored << std::endl;
    std::cout << "Total Quiescence Moves Searched: " << quiesceExplored << std::endl;
    std::cout << "Branching factor: " << avgBranchingFactor << std::endl;
    std::cout << "Moves transposed: " << movesTransposed << std::endl;
    std::cout << "Cut Nodes: " << cutNodes << std::endl;
    std::cout << "Aspiration Window Misses: " << aspMissesL + aspMissesH << ", at depth(s): ";
    std::string missesStr = "[";
    for (int i = 0; i < misses.size(); i++) {
        missesStr += std::to_string(misses[i]);
        if (i != misses.size() - 1) {
            missesStr += ", ";
        }
    }
    missesStr += "]";
    std::cout << missesStr + "\n" << "PV: ";
    std::string PVout = "[";
    for (int i = 0; i < PV.size(); i++) {
        PVout += PV[i].TerseOut();
        if (i != PV.size() - 1) {
            PVout += ", ";
        }
    }
    PVout += "]";
    std::cout << PVout + "\n";
    std::cout << "Moving: " << bestMove.TerseOut() << " with a score of " << (bestScore/ 100.0) * flipped << std::endl;
    cutNodes = 0;
    movesTransposed = 0;
    quiesceExplored = 0;
    transpoTable.clear();
    clearPieceLists();
    nmpColorCurrent = NONE;

    board.PlayMove(bestMove);
}

int Anduril::nonPawnMaterial(bool whiteToPlay) {
    if (whiteToPlay) {
        return whiteKnights.size() + whiteBishops.size() + whiteRooks.size() + whiteQueens.size();
    }
    else {
        return blackKnights.size() + blackBishops.size() + blackRooks.size() + blackQueens.size();
    }
}

// can we reduce the depth of our search for this move?
bool Anduril::isLateReduction(thc::ChessRules &board, thc::Move &move) {
    // don't reduce if the move is a capture
    if (move.capture != ' ') {
        return false;
    }

    // don't reduce if the move is a promotion to a queen
    if (static_cast<int>(move.special) == 6) {
        return false;
    }

    // don't reduce if the side to move is in check, or on moves that give check
    if (board.WhiteToPlay()) {
        if (board.AttackedPiece(board.wking_square)) {
            return false;
        }
        board.PushMove(move);
        if (board.AttackedPiece(board.bking_square)) {
            board.PopMove(move);
            return false;
        }
        board.PopMove(move);

    }
    else {
        if (board.AttackedPiece(board.bking_square)) {
            return false;
        }
        board.PushMove(move);
        if (board.AttackedPiece(board.wking_square)) {
            board.PopMove(move);
            return false;
        }
        board.PopMove(move);
    }

    // if we passed everything else, we can reduce
    return true;
}

// returns the next move to search
thc::Move Anduril::pickNextMove(std::vector<std::tuple<int, thc::Move>> &moveListWithScores, int currentIndex) {
    for (int j = currentIndex + 1; j < moveListWithScores.size(); j++) {
        if (std::get<0>(moveListWithScores[currentIndex]) < std::get<0>(moveListWithScores[j])) {
            moveListWithScores[currentIndex].swap(moveListWithScores[j]);
        }
    }

    return std::get<1>(moveListWithScores[currentIndex]);
}

// clears the piece lists
void Anduril::clearPieceLists() {
    whitePawns.clear();
    blackPawns.clear();
    whiteBishops.clear();
    blackBishops.clear();
    whiteKnights.clear();
    blackKnights.clear();
    whiteRooks.clear();
    blackRooks.clear();
    whiteQueens.clear();
    blackQueens.clear();
}

// creates the piece lists for the board
void Anduril::fillPieceLists(thc::ChessRules &board){
    for (int i = 0; i < 64; i++) {
        switch (board.squares[i]) {
            case 'p':
                blackPawns.push_back(static_cast<const thc::Square>(i));
                break;
            case 'P':
                whitePawns.push_back(static_cast<const thc::Square>(i));
                break;
            case 'n':
                blackKnights.push_back(static_cast<const thc::Square>(i));
                break;
            case 'N':
                whiteKnights.push_back(static_cast<const thc::Square>(i));
                break;
            case 'b':
                blackBishops.push_back(static_cast<const thc::Square>(i));
                break;
            case 'B':
                whiteBishops.push_back(static_cast<const thc::Square>(i));
                break;
            case 'r':
                blackRooks.push_back(static_cast<const thc::Square>(i));
                break;
            case 'R':
                whiteRooks.push_back(static_cast<const thc::Square>(i));
                break;
            case 'q':
                blackQueens.push_back(static_cast<const thc::Square>(i));
                break;
            case 'Q':
                whiteQueens.push_back(static_cast<const thc::Square>(i));
                break;
        }
    }
}

void Anduril::makeMove(thc::ChessRules &board, thc::Move move) {
    int src = static_cast<int>(move.src);
    int dst = static_cast<int>(move.dst);

    // adjust the piece lists to reflect the move
    switch (board.squares[src]) {
        // pawns require special attention in the case of a promotion
        case 'p':
            switch (move.special) {
                case thc::SPECIAL_PROMOTION_QUEEN:
                    for (int i = 0; i < blackPawns.size(); i++) {
                        if (blackPawns[i] == move.src) {
                            blackPawns.erase(blackPawns.begin() + i);
                            blackQueens.push_back(move.dst);
                            break;
                        }
                    }
                    break;

                case thc::SPECIAL_PROMOTION_ROOK:
                    for (int i = 0; i < blackPawns.size(); i++) {
                        if (blackPawns[i] == move.src) {
                            blackPawns.erase(blackPawns.begin() + i);
                            blackRooks.push_back(move.dst);
                            break;
                        }
                    }
                    break;

                case thc::SPECIAL_PROMOTION_BISHOP:
                    for (int i = 0; i < blackPawns.size(); i++) {
                        if (blackPawns[i] == move.src) {
                            blackPawns.erase(blackPawns.begin() + i);
                            blackBishops.push_back(move.dst);
                            break;
                        }
                    }
                    break;
                case thc::SPECIAL_PROMOTION_KNIGHT:
                    for (int i = 0; i < blackPawns.size(); i++) {
                        if (blackPawns[i] == move.src) {
                            blackPawns.erase(blackPawns.begin() + i);
                            blackKnights.push_back(move.dst);
                            break;
                        }
                    }
                    break;

                default:
                    for (int i = 0; i < blackPawns.size(); i++) {
                        if (blackPawns[i] == move.src) {
                            blackPawns[i] = move.dst;
                            break;
                        }
                    }
                    break;
            }
            break;
        case 'P':
            switch (move.special) {
                case thc::SPECIAL_PROMOTION_QUEEN:
                    for (int i = 0; i < whitePawns.size(); i++) {
                        if (whitePawns[i] == move.src) {
                            whitePawns.erase(whitePawns.begin() + i);
                            whiteQueens.push_back(move.dst);
                            break;
                        }
                    }
                    break;

                case thc::SPECIAL_PROMOTION_ROOK:
                    for (int i = 0; i < whitePawns.size(); i++) {
                        if (whitePawns[i] == move.src) {
                            whitePawns.erase(whitePawns.begin() + i);
                            whiteRooks.push_back(move.dst);
                            break;
                        }
                    }
                    break;

                case thc::SPECIAL_PROMOTION_BISHOP:
                    for (int i = 0; i < whitePawns.size(); i++) {
                        if (whitePawns[i] == move.src) {
                            whitePawns.erase(whitePawns.begin() + i);
                            whiteBishops.push_back(move.dst);
                            break;
                        }
                    }
                    break;
                case thc::SPECIAL_PROMOTION_KNIGHT:
                    for (int i = 0; i < whitePawns.size(); i++) {
                        if (whitePawns[i] == move.src) {
                            whitePawns.erase(whitePawns.begin() + i);
                            whiteKnights.push_back(move.dst);
                            break;
                        }
                    }
                    break;

                default:
                    for (int i = 0; i < whitePawns.size(); i++) {
                        if (whitePawns[i] == move.src) {
                            whitePawns[i] = move.dst;
                            break;
                        }
                    }
                    break;
            }
            break;

        case 'n':
            for (int i = 0; i < blackKnights.size(); i++) {
                if (blackKnights[i] == move.src) {
                    blackKnights[i] = move.dst;
                    break;
                }
            }
            break;
        case 'N':
            for (int i = 0; i < whiteKnights.size(); i++) {
                if (whiteKnights[i] == move.src) {
                    whiteKnights[i] = move.dst;
                    break;
                }
            }
            break;
        case 'b':
            for (int i = 0; i < blackBishops.size(); i++) {
                if (blackBishops[i] == move.src) {
                    blackBishops[i] = move.dst;
                    break;
                }
            }
            break;
        case 'B':
            for (int i = 0; i < whiteBishops.size(); i++) {
                if (whiteBishops[i] == move.src) {
                    whiteBishops[i] = move.dst;
                    break;
                }
            }
            break;
        case 'r':
            for (int i = 0; i < blackRooks.size(); i++) {
                if (blackRooks[i] == move.src) {
                    blackRooks[i] = move.dst;
                    break;
                }
            }
            break;
        case 'R':
            for (int i = 0; i < whiteRooks.size(); i++) {
                if (whiteRooks[i] == move.src) {
                    whiteRooks[i] = move.dst;
                    break;
                }
            }
            break;
        case 'q':
            for (int i = 0; i < blackQueens.size(); i++) {
                if (blackQueens[i] == move.src) {
                    blackQueens[i] = move.dst;
                    break;
                }
            }
            break;
        case 'Q':
            for (int i = 0; i < whiteQueens.size(); i++) {
                if (whiteQueens[i] == move.src) {
                    whiteQueens[i] = move.dst;
                    break;
                }
            }
            break;
    }

    // captures
    if (move.capture != ' ' && (move.special != thc::SPECIAL_BEN_PASSANT && move.special != thc::SPECIAL_WEN_PASSANT)) {
        switch (board.squares[dst]) {
            case 'p':
                for (int i = 0; i < blackPawns.size(); i++) {
                    if (blackPawns[i] == move.dst) {
                        blackPawns.erase(blackPawns.begin() + i);
                        break;
                    }
                }
                break;
            case 'P':
                for (int i = 0; i < whitePawns.size(); i++) {
                    if (whitePawns[i] == move.dst) {
                        whitePawns.erase(whitePawns.begin() + i);
                        break;
                    }
                }
                break;
            case 'n':
                for (int i = 0; i < blackKnights.size(); i++) {
                    if (blackKnights[i] == move.dst) {
                        blackKnights.erase(blackKnights.begin() + i);
                        break;
                    }
                }
                break;
            case 'N':
                for (int i = 0; i < whiteKnights.size(); i++) {
                    if (whiteKnights[i] == move.dst) {
                        whiteKnights.erase(whiteKnights.begin() + i);
                        break;
                    }
                }
                break;
            case 'b':
                for (int i = 0; i < blackBishops.size(); i++) {
                    if (blackBishops[i] == move.dst) {
                        blackBishops.erase(blackBishops.begin() + i);
                        break;
                    }
                }
                break;
            case 'B':
                for (int i = 0; i < whiteBishops.size(); i++) {
                    if (whiteBishops[i] == move.dst) {
                        whiteBishops.erase(whiteBishops.begin() + i);
                        break;
                    }
                }
                break;
            case 'r':
                for (int i = 0; i < blackRooks.size(); i++) {
                    if (blackRooks[i] == move.dst) {
                        blackRooks.erase(blackRooks.begin() + i);
                        break;
                    }
                }
                break;
            case 'R':
                for (int i = 0; i < whiteRooks.size(); i++) {
                    if (whiteRooks[i] == move.dst) {
                        whiteRooks.erase(whiteRooks.begin() + i);
                        break;
                    }
                }
                break;
            case 'q':
                for (int i = 0; i < blackQueens.size(); i++) {
                    if (blackQueens[i] == move.dst) {
                        blackQueens.erase(blackQueens.begin() + i);
                        break;
                    }
                }
                break;
            case 'Q':
                for (int i = 0; i < whiteQueens.size(); i++) {
                    if (whiteQueens[i] == move.dst) {
                        whiteQueens.erase(whiteQueens.begin() + i);
                        break;
                    }
                }
                break;
        }
    }
    else if (move.special == thc::SPECIAL_BEN_PASSANT) {
        for (int i = 0; i < whitePawns.size(); i++) {
            if (whitePawns[i] == thc::make_square(thc::get_file(board.enpassant_target), '4')) {
                whitePawns.erase(whitePawns.begin() + i);
            }
        }
    }
    else if (move.special == thc::SPECIAL_WEN_PASSANT) {
        for (int i = 0; i < blackPawns.size(); i++) {
            if (blackPawns[i] == thc::make_square(thc::get_file(board.enpassant_target), '5')) {
                blackPawns.erase(blackPawns.begin() + i);
            }
        }
    }

    board.PushMove(move);
}

void Anduril::undoMove(thc::ChessRules &board, thc::Move move) {
    int src = static_cast<int>(move.src);
    int dst = static_cast<int>(move.dst);

    board.PopMove(move);

    // adjust the piece lists to reflect the move
    switch (board.squares[src]) {
        // pawns require special attention in the case of a promotion
        case 'p':
            switch (move.special) {
                case thc::SPECIAL_PROMOTION_QUEEN:
                    for (int i = 0; i < blackQueens.size(); i++) {
                        if (blackQueens[i] == move.dst) {
                            blackQueens.erase(blackQueens.begin() + i);
                            blackPawns.push_back(move.src);
                            break;
                        }
                    }
                    break;

                case thc::SPECIAL_PROMOTION_ROOK:
                    for (int i = 0; i < blackRooks.size(); i++) {
                        if (blackRooks[i] == move.dst) {
                            blackRooks.erase(blackRooks.begin() + i);
                            blackPawns.push_back(move.src);
                            break;
                        }
                    }
                    break;

                case thc::SPECIAL_PROMOTION_BISHOP:
                    for (int i = 0; i < blackBishops.size(); i++) {
                        if (blackBishops[i] == move.dst) {
                            blackBishops.erase(blackBishops.begin() + i);
                            blackPawns.push_back(move.src);
                            break;
                        }
                    }
                    break;
                case thc::SPECIAL_PROMOTION_KNIGHT:
                    for (int i = 0; i < blackKnights.size(); i++) {
                        if (blackKnights[i] == move.dst) {
                            blackKnights.erase(blackKnights.begin() + i);
                            blackPawns.push_back(move.src);
                            break;
                        }
                    }
                    break;

                default:
                    for (int i = 0; i < blackPawns.size(); i++) {
                        if (blackPawns[i] == move.dst) {
                            blackPawns[i] = move.src;
                            break;
                        }
                    }
                    break;
            }
            break;
        case 'P':
            switch (move.special) {
                case thc::SPECIAL_PROMOTION_QUEEN:
                    for (int i = 0; i < whiteQueens.size(); i++) {
                        if (whiteQueens[i] == move.dst) {
                            whiteQueens.erase(whiteQueens.begin() + i);
                            whitePawns.push_back(move.src);
                            break;
                        }
                    }
                    break;

                case thc::SPECIAL_PROMOTION_ROOK:
                    for (int i = 0; i < whiteRooks.size(); i++) {
                        if (whiteRooks[i] == move.dst) {
                            whiteRooks.erase(whiteRooks.begin() + i);
                            whitePawns.push_back(move.src);
                            break;
                        }
                    }
                    break;

                case thc::SPECIAL_PROMOTION_BISHOP:
                    for (int i = 0; i < whiteBishops.size(); i++) {
                        if (whiteBishops[i] == move.dst) {
                            whiteBishops.erase(whiteBishops.begin() + i);
                            whitePawns.push_back(move.src);
                            break;
                        }
                    }
                    break;
                case thc::SPECIAL_PROMOTION_KNIGHT:
                    for (int i = 0; i < whiteKnights.size(); i++) {
                        if (whiteKnights[i] == move.dst) {
                            whiteKnights.erase(whiteKnights.begin() + i);
                            whitePawns.push_back(move.src);
                            break;
                        }
                    }
                    break;

                default:
                    for (int i = 0; i < whitePawns.size(); i++) {
                        if (whitePawns[i] == move.dst) {
                            whitePawns[i] = move.src;
                            break;
                        }
                    }
                    break;
            }
            break;

        case 'n':
            for (int i = 0; i < blackKnights.size(); i++) {
                if (blackKnights[i] == move.dst) {
                    blackKnights[i] = move.src;
                    break;
                }
            }
            break;
        case 'N':
            for (int i = 0; i < whiteKnights.size(); i++) {
                if (whiteKnights[i] == move.dst) {
                    whiteKnights[i] = move.src;
                    break;
                }
            }
            break;
        case 'b':
            for (int i = 0; i < blackBishops.size(); i++) {
                if (blackBishops[i] == move.dst) {
                    blackBishops[i] = move.src;
                    break;
                }
            }
            break;
        case 'B':
            for (int i = 0; i < whiteBishops.size(); i++) {
                if (whiteBishops[i] == move.dst) {
                    whiteBishops[i] = move.src;
                    break;
                }
            }
            break;
        case 'r':
            for (int i = 0; i < blackRooks.size(); i++) {
                if (blackRooks[i] == move.dst) {
                    blackRooks[i] = move.src;
                    break;
                }
            }
            break;
        case 'R':
            for (int i = 0; i < whiteRooks.size(); i++) {
                if (whiteRooks[i] == move.dst) {
                    whiteRooks[i] = move.src;
                    break;
                }
            }
            break;
        case 'q':
            for (int i = 0; i < blackQueens.size(); i++) {
                if (blackQueens[i] == move.dst) {
                    blackQueens[i] = move.src;
                    break;
                }
            }
            break;
        case 'Q':
            for (int i = 0; i < whiteQueens.size(); i++) {
                if (whiteQueens[i] == move.dst) {
                    whiteQueens[i] = move.src;
                    break;
                }
            }
            break;
    }

    // captures
    if (move.capture != ' ' && (move.special != thc::SPECIAL_BEN_PASSANT && move.special != thc::SPECIAL_WEN_PASSANT)) {
        switch (move.capture) {
            case 'p':
                blackPawns.push_back(move.dst);
                break;
            case 'P':
                whitePawns.push_back(move.dst);
                break;
            case 'n':
                blackKnights.push_back(move.dst);
                break;
            case 'N':
                whiteKnights.push_back(move.dst);
                break;
            case 'b':
                blackBishops.push_back(move.dst);
                break;
            case 'B':
                whiteBishops.push_back(move.dst);
                break;
            case 'r':
                blackRooks.push_back(move.dst);
                break;
            case 'R':
                whiteRooks.push_back(move.dst);
                break;
            case 'q':
                blackQueens.push_back(move.dst);
                break;
            case 'Q':
                whiteQueens.push_back(move.dst);
                break;
        }
    }
    else if (move.special == thc::SPECIAL_BEN_PASSANT) {
        whitePawns.push_back(thc::make_square(thc::get_file(board.enpassant_target), '4'));
    }
    else if (move.special == thc::SPECIAL_WEN_PASSANT) {
        blackPawns.push_back(thc::make_square(thc::get_file(board.enpassant_target), '5'));
    }
}

// finds and returns the principal variation
std::vector<thc::Move> Anduril::getPV(thc::ChessRules &board, int depth, thc::Move bestMove) {
    std::vector<thc::Move> PV;
    uint64_t hash = 0;
    Node *node;

    // push the best move and add to the PV
    PV.push_back(bestMove);
    board.PushMove(bestMove);

    // This loop hashes the board, finds the node associated with the current board
    // adds the best move we found to the PV, then pushes it to the board
    hash = zobrist(board);
    node = transpoTable[hash];
    while (node != nullptr && transpoTable.count(hash) != 0 && node->bestMove.src != node->bestMove.dst) {
        PV.push_back(node->bestMove);
        board.PushMove(node->bestMove);
        hash = zobrist(board);
        node = transpoTable[hash];
        // in case of infinite loops of repeating moves
        // I just capped it at 15 because I honestly don't care about lines
        // calculated that long anyways
        if (PV.size() > 7) {
            break;
        }
    }

    // undoes each move in the PV so that we don't have a messed up board
    for (int i = PV.size() - 1; i >= 0; i--){
        board.PopMove(PV[i]);
    }

    return PV;
}

// calculates a hash for just pawns
// this is used for the pawn transposition table
// since the pawn score only depends on the position of the pawns
// we can just calculate the hash of the pawns position
uint64_t Anduril::hashPawns(thc::ChessRules &board){
    int pieceIndex = 0;
    uint64_t hash = 0;

    for (int i = 0; i < 64; i++) {
        if (board.squares[i] == 'p' || board.squares[i] == 'P') {
            // grabs the correct index for the piece
            pieceIndex = pieceTypes[board.squares[i]] * 2;

            // if the piece is white, add one to the piece index
            if (board.squares[i] < 90){
                pieceIndex++;
            }

            // xor the pseudorandom value from the array with the hash
            hash ^= zobristArray[64 * pieceIndex + i];
        }
    }

    return hash;
}

// calculates a zobrist hash of the position
uint64_t Anduril::zobrist(thc::ChessRules &board) {
    return hashBoard(board) ^ hashCastling(board) ^ hashCastling(board) ^ hashEP(board) ^ hashTurn(board);
}

// calculates the board part of a zobrist hash
uint64_t Anduril::hashBoard(thc::ChessRules &board) {
    int pieceIndex = 0;
    uint64_t hash = 0;

    for (int i = 0; i < 64; i++) {
        if (board.squares[i] != ' ') {
            // grabs the correct index for the piece
            pieceIndex = pieceTypes[board.squares[i]] * 2;

            // if the piece is white, add one to the piece index
            if (board.squares[i] < 90){
                pieceIndex++;
            }

            // xor the pseudorandom value from the array with the hash
            hash ^= zobristArray[64 * pieceIndex + i];
        }
    }

    return hash;
}

// calculates hashing for castling rights
uint64_t Anduril::hashCastling(thc::ChessRules &board) {
    uint64_t hash = 0;

    // each type of castling right has its own pseudorandom value
    if (board.wking_allowed()){
        hash ^= zobristArray[768];
    }
    if (board.wqueen_allowed()){
        hash ^= zobristArray[769];
    }
    if (board.bking_allowed()){
        hash ^= zobristArray[770];
    }
    if (board.bqueen_allowed()){
        hash ^= zobristArray[771];
    }

    return hash;
}

// calculates hashing for en passant
uint64_t Anduril::hashEP(thc::ChessRules &board) {
    uint64_t hash = 0;
    int fileMask = 0;

    // this finds the square where en passant is possible
    // then it applies an offset based on the file en passant
    // is possible in
    if (board.enpassant_target != thc::SQUARE_INVALID){
        thc::Square target = board.groomed_enpassant_target();
        if (target != thc::SQUARE_INVALID){
            char file = thc::get_file(target);
            switch (file){
                case 'a':
                    fileMask = 0;
                    break;
                case 'b':
                    fileMask = 1;
                    break;
                case 'c':
                    fileMask = 2;
                    break;
                case 'd':
                    fileMask = 3;
                    break;
                case 'e':
                    fileMask = 4;
                    break;
                case 'f':
                    fileMask = 5;
                    break;
                case 'g':
                    fileMask = 6;
                    break;
                case 'h':
                    fileMask = 7;
                    break;
            }

            return zobristArray[772 + fileMask];
        }
    }

    return 0;
}

// calculates the hash for who's turn it is
uint64_t Anduril::hashTurn(thc::ChessRules &board) {
    return board.WhiteToPlay() ? zobristArray[780] : 0;
}