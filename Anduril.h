//
// Created by Hughe on 4/1/2022.
//

#ifndef ANDURIL_ENGINE_ANDURIL_H
#define ANDURIL_ENGINE_ANDURIL_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>

#include "limit.h"
#include "libchess/Position.h"
#include "Node.h"
#include "TranspositionTable.h"
#include "PolyglotBook.h"

class Anduril {
public:

    // the type of node we are searching
    enum NodeType {PV, NonPV, Root};

    // enumeration type for the pieces
    enum PieceType : int
            { PAWN,
              KNIGHT,
              BISHOP,
              ROOK,
              QUEEN,
              KING,
              ALL_PIECES
            };

    // types of stat bonus and penalties
    enum StatType
            { FAIL_HIGH_TRANSPO,
              LMR_BETA_CUT,
              PRIOR_FAIL_LOW,
              CAP_HISTORY_BONUS,
              BEST_CONTINUATION,
              BEST_BUTTERFLY,
              PREV_EARLY_QUIET,
              NON_FAIL_HIGH_TRANSPO,
              LMR_FAIL_LOW,
              NON_BEST_CONTINUATION,
              NON_BEST_BUTTERFLY,
              EARLY_NON_TRANSPO_KILLER,
              NON_BEST_CAPTURE,
            };

    Anduril(int id) : id(id) { resetHistories(); }

    // calls negamax and keeps track of the best move
    // this version will also interact with UCI
    void go(libchess::Position board);

    // the negamax function.  Does the heavy lifting for the search
    template <NodeType nodeType>
    int negamax(libchess::Position &board, int depth, int alpha, int beta, bool cutNode);

    // the quiescence search
    // searches possible captures to make sure we aren't mis-evaluating certain positions
    template <NodeType nodeType>
    int quiescence(libchess::Position &board, int alpha, int beta, int depth = 0);

    // generates a static evaluation of the board
    int evaluateBoard(libchess::Position &board);

    // getter and setter for moves explored
    uint64_t getMovesExplored();

    inline void setMovesExplored(int moves) { movesExplored = moves; }

    // getter and setter for tb hits
    uint64_t getTbHits();

    void setTbHits(int hits) { tbHits = hits; }

    // checks if the search should stop due to time (or node) limit
    bool shouldStop();

    // benchmarks the engine
    void bench(libchess::Position &board);

    // runs perft to verify move generation
    void perft(libchess::Position &board, int depth);

    // reset the ply
    inline void resetPly() { ply = 0; }

    // increment and decrement the ply
    inline void incPly() { ply++; }
    inline void decPly() { ply--; }

    // finds and returns the principal variation
    std::vector<libchess::Move> getPV(libchess::Position &board, int depth, libchess::Move bestMove);

    // reset the move and counter move tables
    inline void resetHistories() {
        for (int i = 0; i < 64; i++) {
            for (int j = 0; j < 64; j++) {
                counterMoves[i][j] = libchess::Move(0);
                moveHistory[0][i][j] = 0;
                moveHistory[1][i][j] = 0;
            }
        }
        // what the fuck
        for (auto &i : continuationHistory) {
            for (auto &j : i) {
                for (auto &k : j) {
                    for (auto &l : k) {
                        for (auto &m : l) {
                            for (auto &n : m) {
                                n = 0;
                            }
                        }
                    }
                }
            }
        }
        for (auto &i : captureHistory) {
            for (auto &j : i) {
                for (auto &k : j) {
                    k = 0;
                }
            }
        }
    }
    // pawn transposition table
    //HashTable<PawnEntry, 8> pTable = HashTable<PawnEntry, 8>();

    // transposition table for evaluations
    //HashTable<SimpleNode, 8> evalTable = HashTable<SimpleNode, 8>();

    // the limit the GUI could send
    limit limits;

    // time we started the search
    std::chrono::time_point<std::chrono::steady_clock> startTime;

    // time we should stop the search
    std::chrono::time_point<std::chrono::steady_clock> stopTime;

    // endgame values used for qsearch
    static int pieceValues[16];

    // continuation history table
    // 2x2 because we separate check and capture moves
    // this makes the actual structure 2x2x13x64x13x64.  Yikes.
    ContinuationHistory continuationHistory[2][2];

    // capture history table
    CaptureHistory captureHistory;

    const int id;

    // total number of moves Anduril searched
    std::atomic<uint64_t> movesExplored;

    // total number of tb hits
    std::atomic<uint64_t> tbHits;

private:

    // the ply of the root node
    int rootPly = 0;

    // the depth at the root of our current ID search
    int rDepth = 0;

    // current ply
    int ply = 0;

    // minimum ply for null move
    int minNullPly = 0;

    // selective depth
    int selDepth = 0;

    uint64_t singularAttempts = 0;
    uint64_t singularExtensions = 0;

    // list of moves at root position
    libchess::MoveList rootMoves;
    libchess::Move *currRootMove;

    // this version actually performs the perft search
    template<bool root>
    uint64_t perft(libchess::Position &board, int depth);

    // returns the amount of non pawn material (excluding kings)
    int nonPawnMaterial(bool whiteToPlay, libchess::Position &board);

    // insert a move to the killer list
    inline void insertKiller(libchess::Move move, int depth) {
        if (killers[depth][0] != move) {
            killers[depth][1] = killers[depth][0];
            killers[depth][0] = move;
        }
    }

    void updateStatistics(libchess::Position &board, libchess::Move bestMove, int bestScore, int depth, int beta,
                          libchess::Move *quietsSearched, int quietCount, libchess::Move *capturesSearched,
                          int captureCount);

    void updateQuietStats(libchess::Position &board, libchess::Move bestMove, int depth);

    // updates the continuation history
    void updateContinuationHistory(libchess::Position &board, libchess::Piece piece, libchess::Square to, int bonus, int start = 0);

    // gets the phase of the game for evaluation
    int getPhase(libchess::Position &board);

    // finds the pawn structure bonus for the position
    template<bool color>
    std::pair<int16_t, int16_t> getPawnScore(libchess::Position &board);

    // calculates the space score for the position
    // only needs to be int because we don't return an endgame score
    template<bool color>
    int space(libchess::Position &board);

    // finds the king safety bonus for the position
    template<bool color>
    std::pair<int, int> getKingSafety(libchess::Position &board);

    // calculates shelter bonus and storm penalty for a king
    template<bool color>
    std::pair<int, int> evaluateShelter(libchess::Position &board, libchess::Square ksq);

    // calculates bonuses according to attacking and attacked pieces
    template<bool color>
    std::pair<int, int> threats(libchess::Position &board);

    // evaluates knights for tropism and calculates mobility
    template<bool white>
    void evaluateKnights(libchess::Position &board, libchess::Square square);

    // evaluates bishops for tropism and calculates mobility
    template<bool white>
    void evaluateBishops(libchess::Position &board, libchess::Square square);

    // evaluates rooks for tropism and calculates mobility
    template<bool white>
    void evaluateRooks(libchess::Position &board, libchess::Square square);

    // evaluates queens for tropism and calculates mobility
    template<bool white>
    void evaluateQueens(libchess::Position &board, libchess::Square square);

    // calculates all mobility scores and tropism
    std::pair<int, int> positionalMobilityTropism(libchess::Position &board);

    // killer moves
    // oversize array just to be sure we dont seg fault
    KillerMoves killers;

    // counter moves
    CounterMoves counterMoves;

    // history table
    ButterflyHistory moveHistory;

    // number of pieces attacking the king zone
    int attackCount[2] = {0};

    // weight of the attackers on the king zone
    int attackWeight[2] = {0};

    // mobility scores
    int whiteMobility[2] = {0};
    int blackMobility[2] = {0};

    // blockedPawnsCount pawn
    int blockedPawnsCount[2] = {0};

    // contains the squares that are the "king zone"
    libchess::Bitboard kingZoneWBB;
    libchess::Bitboard kingZoneBBB;

    // contains the attack maps for each team
    libchess::Bitboard attackMap[2][7];
    libchess::Bitboard attackByTwo[2];

    // mobility area
    libchess::Bitboard mobilityArea[2];

};

// total amount of cut nodes
static std::atomic<uint64_t> cutNodes;

// amount of moves we hashed from the transpo table
static std::atomic<uint64_t> movesTransposed;

// amount of moves we searched in quiescence
static std::atomic<uint64_t> quiesceExplored;

// initialize the reduction table
void initReductions(double nem, double neb, double tem, double teb);

// prefetches an address in memory to CPU cache that doesn't block execution
// this should speed up the program by reducing the amount of time we wait for transposition table probes
// copied from Stockfish
inline void prefetch(void* addr) {

#  if defined(__INTEL_COMPILER)
    // This hack prevents prefetches from being optimized away by
   // Intel compiler. Both MSVC and gcc seem not be affected by this.
   __asm__ ("");
#  endif

#  if defined(__INTEL_COMPILER) || defined(_MSC_VER)
    _mm_prefetch((char*)addr, _MM_HINT_T0);
#  else
    __builtin_prefetch(addr);
#  endif
}

#endif //ANDURIL_ENGINE_ANDURIL_H
