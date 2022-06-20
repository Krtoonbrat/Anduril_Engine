//
// Created by Hughe on 4/1/2022.
//

#ifndef ANDURIL_ENGINE_ANDURIL_H
#define ANDURIL_ENGINE_ANDURIL_H

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "Node.h"
#include "thc.h"
#include "TranspositionTable.h"
#include "PolyglotBook.h"

class Anduril {
public:

    // the type of node we are searching
    enum NodeType {PV, NonPV};

    // the negamax function.  Does the heavy lifting for the search
    template <NodeType nodeType>
    int negamax(thc::ChessRules &board, int depth, int alpha, int beta);

    // the quiesce search
    // searches possible captures to make sure we aren't mis-evaluating certain positions
    template <NodeType nodeType>
    int quiesce(thc::ChessRules &board, int alpha, int beta, int Qdepth);

    // generates a static evaluation of the board
    int evaluateBoard(thc::ChessRules &board);

    // calls negamax and keeps track of the best move
    void go(thc::ChessRules &board, int depth);

    // getter and setter for moves explored
    int getMovesExplored() const { return movesExplored; }

    void setMovesExplored(int moves) { movesExplored = moves; }

    // generates a move list in the order we want to search them
    std::vector<std::tuple<int, thc::Move>> getMoveList(thc::ChessRules &board, Node *node);

    // generates a move list with only captures
    std::vector<std::tuple<int, thc::Move>> getQMoveList(thc::ChessRules &board, Node *node);

    // finds and returns the principal variation
    std::vector<thc::Move> getPV(thc::ChessRules &board, int depth, thc::Move bestMove);

private:
    // total number of moves Anduril searched
    int movesExplored = 0;

    // total amount of cut nodes
    int cutNodes = 0;

    // amount of moves we hashed from the transpo table
    int movesTransposed = 0;

    // amount of moves we searched in quiescence
    int quiesceExplored = 0;

    // amount of nodes searched at the current depth
    int depthNodes = 0;

    // the ply of the root node
    int rootPly = 0;

    // piece location lists
    std::vector<thc::Square> whitePawns;
    std::vector<thc::Square> blackPawns;

    std::vector<thc::Square> whiteKnights;
    std::vector<thc::Square> blackKnights;

    std::vector<thc::Square> whiteBishops;
    std::vector<thc::Square> blackBishops;

    std::vector<thc::Square> whiteRooks;
    std::vector<thc::Square> blackRooks;

    std::vector<thc::Square> whiteQueens;
    std::vector<thc::Square> blackQueens;

    // returns the amount of non pawn material (excluding kings)
    int nonPawnMaterial(bool whiteToPlay);

    // can we reduce the depth of our search for this move?
    bool isLateReduction(thc::ChessRules &board, thc::Move &move);

    // returns the next move to search
    thc::Move pickNextMove(std::vector<std::tuple<int, thc::Move>> &moveListWithScores, int currentIndex);

    // creates the piece lists for the board
    void fillPieceLists(thc::ChessRules &board);

    // clears the piece lists
    void clearPieceLists();

    // make a move and update the board and piece lists
    void makeMove(thc::ChessRules &board, thc::Move move);

    // undo a move and update the board and piece lists
    void undoMove (thc::ChessRules &board, thc::Move move);

    // insert a move to the killer list
    void insertKiller(int ply, thc::Move move) {
        if (std::count(killers[ply].begin(), killers[ply].end(), move) == 0) {
            killers[ply][0] = killers[ply][1];
            killers[ply][1] = move;
        }
    }

    // finds the raw material score for the position
    // it returns a pointer to an array containing a middlegame and endgame value
    std::vector<int> getMaterialScore(thc::ChessRules &board);

    // gets the phase of the game for evaluation
    double getPhase(thc::ChessRules &board);

    // finds the pawn structure bonus for the position
    int getPawnScore(thc::ChessRules &board);

    // finds the king safety bonus for the position
    int getKingSafety(thc::ChessRules &board, thc::Square whiteKing, thc::Square blackKing);

    // evaluates knights for tropism
    void evaluateKnights(thc::ChessRules &board, thc::Square square, bool white);

    // evaluates bishops for tropism
    void evaluateBishops(thc::ChessRules &board, thc::Square square, bool white);

    // evaluates rooks for tropism
    void evaluateRooks(thc::ChessRules &board, thc::Square square, bool white);

    // evaluates queens for tropism
    void evaluateQueens(thc::ChessRules &board, thc::Square square, bool white);

    // gives a score to each capture
    int MVVLVA(thc::ChessRules &board, int src, int dst);

    // the transposition table
    TranspositionTable table = TranspositionTable(1024);

    // pawn transposition table
    TranspositionTable pawnTable = TranspositionTable(256);

    // killer moves
    std::vector<std::vector<thc::Move>> killers;

    // futility margins
    int margin[4] = {0, 200, 300, 500};

    // point bonus that increases the value of knights with more pawns on the board
    int knightPawnBonus[9] = { -20, -16, -12, -8, -4,  0,  4,  8, 12 };

    // point bonus that increases the value of rooks with less pawns
    int rookPawnBonus[9] = { 15,  12,   9,  6,  3,  0, -3, -6, -9 };

    // number of pieces attacking the king zone
    int attackCount[2] = {0};

    // weight of the attackers on the king zone
    int attackWeight[2] = {0};

    // contains the squares that are the "king zone"
    std::unordered_set<thc::Square> kingZoneW;
    std::unordered_set<thc::Square> kingZoneB;

    // the piece values
    std::unordered_map<char, int> pieceValues = {{'P', 100},
                                                 {'N', 300},
                                                 {'B', 300},
                                                 {'R', 500},
                                                 {'Q', 900},
                                                 {'K', 20000},
                                                 {'p', -100},
                                                 {'n', -300},
                                                 {'b', -300},
                                                 {'r', -500},
                                                 {'q', -900},
                                                 {'k', -20000}};

    // this will be used to make sure the same color doesn't do a null move twice in a row
    bool nullAllowed = true;

    // piece square tables
    // these are set up from white's perspective, and their indexing is
    // the same as the representation for squares

    /* values from Rofchade: http://www.talkchess.com/forum3/viewtopic.php?f=2&t=68311&start=19 */
    int pawnSquareTableMG[64] = {
             0,   0,   0,   0,   0,   0,  0,   0,
            98, 134,  61,  95,  68, 126, 34, -11,
            -6,   7,  26,  31,  65,  56, 25, -20,
            -14,  13,   6,  21,  23,  12, 17, -23,
            -27,  -2,  -5,  22,  27,   6, 10, -25,
            -26,  -4,  -4, -10,   3,   3, 33, -12,
            -35,  -1, -20, -33, -27,  24, 38, -22,
              0,   0,   0,   0,   0,   0,  0,   0,
    };

    int pawnSquareTableEG[64] = {
              0,   0,   0,   0,   0,   0,   0,   0,
            178, 173, 158, 134, 147, 132, 165, 187,
             94, 100,  85,  67,  56,  53,  82,  84,
             32,  24,  13,   5,  -2,   4,  17,  17,
             13,   9,  -3,  -7,  -7,  -8,   3,  -1,
              4,   7,  -6,   1,   0,  -5,  -1,  -8,
             13,   8,   8,  10,  13,   0,   2,  -7,
              0,   0,   0,   0,   0,   0,   0,   0,
    };

    int knightSquareTableMG[64] = {
            -167, -89, -34, -49,  61, -97, -15, -107,
             -73, -41,  72,  36,  23,  62,   7,  -17,
             -47,  60,  37,  65,  84, 129,  73,   44,
              -9,  17,  19,  53,  37,  69,  18,   22,
             -13,   4,  16,  13,  28,  19,  21,   -8,
             -23,  -9,  12,  10,  19,  17,  25,  -16,
             -29, -53, -12,  -3,  -1,  18, -14,  -19,
            -105, -29, -58, -33, -17, -28, -29,  -23,
    };

    int knightSquareTableEG[64] = {
            -58, -38, -13, -28, -31, -27, -63, -99,
            -25,  -8, -25,  -2,  -9, -25, -24, -52,
            -24, -20,  10,   9,  -1,  -9, -19, -41,
            -17,   3,  22,  22,  22,  11,   8, -18,
            -18,  -6,  16,  25,  16,  17,   4, -18,
            -23,  -3,  -1,  15,  10,  -3, -20, -22,
            -42, -20, -10,  -5,  -2, -20, -23, -44,
            -29, -51, -23, -15, -22, -18, -50, -64,
    };

    int bishopSquareTableMG[64] = {
            -29,   4, -82, -37, -25, -42,   7,  -8,
            -26,  16, -18, -13,  30,  59,  18, -47,
            -16,  37,  43,  40,  35,  50,  37,  -2,
             -4,   5,  19,  50,  37,  37,   7,  -2,
             -6,  13,  13,  26,  34,  12,  10,   4,
              0,  15,  15,  15,  14,  27,  18,  10,
              4,  15,  16,   0,   7,  21,  33,   1,
            -33,  -3, -27, -21, -13, -22, -39, -21,
    };

    int bishopSquareTableEG[64] = {
            -14, -21, -11,  -8, -7,  -9, -17, -24,
             -8,  -4,   7, -12, -3, -13,  -4, -14,
              2,  -8,   0,  -1, -2,   6,   0,   4,
             -3,   9,  12,   9, 14,  10,   3,   2,
             -6,   3,  13,  19,  7,  10,  -3,  -9,
            -12,  -3,   8,  10, 13,   3,  -7, -15,
            -14, -18,  -7,  -1,  4,  -9, -15, -27,
            -23,  -9, -23,  -5, -9, -16,  -5, -17,
    };

    int rookSquareTableMG[64] = {
             32,  42,  32,  51, 63,  9,  31,  43,
             27,  32,  58,  62, 80, 67,  26,  44,
             -5,  19,  26,  36, 17, 45,  61,  16,
            -24, -11,   7,  26, 24, 35,  -8, -20,
            -36, -26, -12,  -1,  9, -7,   6, -23,
            -45, -25, -16, -17,  3,  0,  -5, -33,
            -44, -16, -20,  -9, -1, 11,  -6, -71,
            -19, -13,   1,  17, 16,  7, -37, -26,
    };

    int rookSquareTableEG[64] = {
            13, 10, 18, 15, 12,  12,   8,   5,
            11, 13, 13, 11, -3,   3,   8,   3,
             7,  7,  7,  5,  4,  -3,  -5,  -3,
             4,  3, 13,  1,  2,   1,  -1,   2,
             3,  5,  8,  4, -5,  -6,  -8, -11,
            -4,  0, -5, -1, -7, -12,  -8, -16,
            -6, -6,  0,  2, -9,  -9, -11,  -3,
            -9,  2,  3, -1, -5, -13,   4, -20,
    };

    int queenSquareTableMG[64] = {
            -28,   0,  29,  12,  59,  44,  43,  45,
            -24, -39,  -5,   1, -16,  57,  28,  54,
            -13, -17,   7,   8,  29,  56,  47,  57,
            -27, -27, -16, -16,  -1,  17,  -2,   1,
            -9, -26,  -9, -10,  -2,  -4,   3,  -3,
            -14,   2, -11,  -2,  -5,   2,  14,   5,
            -35,  -8,  11,   2,   8,  15,  -3,   1,
            -1, -18,  -9,  10, -15, -25, -31, -50,
    };

    int queenSquareTableEG[64] = {
             -9,  22,  22,  27,  27,  19,  10,  20,
            -17,  20,  32,  41,  58,  25,  30,   0,
            -20,   6,   9,  49,  47,  35,  19,   9,
              3,  22,  24,  45,  57,  40,  57,  36,
            -18,  28,  19,  47,  31,  34,  39,  23,
            -16, -27,  15,   6,   9,  17,  10,   5,
            -22, -23, -30, -16, -16, -23, -36, -32,
            -33, -28, -22, -43,  -5, -32, -20, -41,
    };

    int kingSquareTableMG[64] = {
            -65,  23,  16, -15, -56, -34,   2,  13,
             29,  -1, -20,  -7,  -8,  -4, -38, -29,
             -9,  24,   2, -16, -20,   6,  22, -22,
            -17, -20, -12, -27, -30, -25, -14, -36,
            -49,  -1, -27, -39, -46, -44, -33, -51,
            -14, -14, -22, -46, -44, -30, -15, -27,
              1,   7,  -8, -64, -43, -16,   9,   8,
            -15,  36,  12, -54, -25, -28,  24,  14,
    };

    int kingSquareTableEG[64] = {
            -74, -35, -18, -18, -11,  15,   4, -17,
            -12,  17,  14,  17,  17,  38,  23,  11,
             10,  17,  23,  15,  20,  45,  44,  13,
             -8,  22,  24,  27,  26,  33,  26,   3,
            -18,  -4,  21,  24,  27,  23,   9, -11,
            -19,  -3,  11,  21,  23,  16,   7,  -9,
            -27, -11,   4,  13,  14,   4,  -5, -17,
            -53, -34, -21, -11, -28, -14, -24, -43
    };

    const int SafetyTable[100] = {
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

};

#endif //ANDURIL_ENGINE_ANDURIL_H
