//
// Created by Hughe on 4/1/2022.
//

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

#include "Anduril.h"
#include "MovePicker.h"
#include "Syzygy.h"
#include "Thread.h"
#include "UCI.h"
#include "Pyrrhic/tbprobe.h"

// reduction table
// its oversize just in case something weird happens
int reductionsQuiet[150][150];
int reductionsTactical[150][150];

// maximum values for the history tables
int maxHistoryVal = 8250;
int maxContinuationVal = 31035;
int maxCaptureVal = 7321;

// stat bonus values
int bonusMult = 407;
int bonusSub = 133;
int bonusMin = 181;
int bonusMax = 6969;

// stat penalty values
int penaltyMult = 317;
int penaltySub = 137;
int penaltyMin = 204;
int penaltyMax = 6922;

// our thread pool
extern ThreadPool gondor;

// the other formula simplifies down to this.  This should be easier to tune than 4 separate variables
void initReductions(double nem, double neb, double tem, double teb) {
    for (int i = 0; i < 150; i++) {
        for (int j = 0; j < 150; j++) {
            if (i == 0 || j == 0) {
                reductionsQuiet[i][j] = 0;
                reductionsTactical[i][j] = 0;
            }
            else {
                reductionsQuiet[i][j] = int(neb + (log(i) * log(j) / nem));
                reductionsTactical[i][j] = int(teb + (log(i) * log(j) / tem));
            }
        }
    }
}

// calculates the score added to a history table when we reward a move
int stat_bonus(int depth) {
    return std::clamp(bonusMult * depth - bonusSub, bonusMin, bonusMax);
}

// calculates the score subtracted from a history table when we punish a move
int stat_penalty(int depth) {
    return -std::clamp(penaltyMult * depth - penaltySub, penaltyMin, penaltyMax);
}

// returns the amount of moves we need to search before we can use move count based pruning
constexpr int moveCountPruningThreshold(bool improving, int depth) {
    return improving ? (4 + depth * depth) : (4 + depth * depth) / 2;
}

// returns the margin for reverse futility pruning.  Returns the same values as the list we used before, except this is
// able to a higher depth, and we introduced the improving factor to it
constexpr int reverseFutilityMargin(int depth, bool improving) {
    return 234 * (depth - improving);
}

// changes mate scores from being relative to the root to being relative to the current ply
int tableScore(int score, int ply) {
    return score >= 31508 ? score + ply : score <= -31508 ? score - ply : score;
}

int scoreFromTable(int score, int ply, int rule50) {
    // if the score we are given is our "none" flag just return it
    if (score == -32001) {
        return score;
    }

    // if a mate score is stored, we need to adjust it to be relative to the current ply
    if (score >= 31507) {
        // don't return a mate score if we are going to hit the 50 move rule
        if (score >= 31754 && 32000 - score > 99 - rule50) {
            return 31506; // this is below what checkmate in maximum search would be
        }

        // downgrade false tb scores
        if (31753 - score > 99 - rule50) {
            return 31506;
        }
        
        return score - ply;
    }

    if (score <= -31507) {
        // don't return a mate score if we are going to hit the 50 move rule
        if (score <= -31754 && 32000 + score > 99 - rule50) {
            return -31506; // this is below what checkmate in maximum search would be
        }

        // downgrade false tb scores
        if (-31753 + score > 99 - rule50) {
            return -31506;
        }

        return score + ply;
    }

    return score;
}

// the quiescence search
// searches possible captures to make sure we aren't mis-evaluating certain positions
template <Anduril::NodeType nodeType>
int Anduril::quiescence(libchess::Position &board, int alpha, int beta, int depth) {
    constexpr bool PvNode = nodeType != NonPV;

    // are we in check?
    bool check = board.in_check();

    if ((ply - rootPly) >= 100) {
        return !check ? evaluateBoard(board) : 0;
    }

    // check for a draw, add variance to draw score to avoid 3-fold blindness
    if (board.is_draw()) {
        return 0;
    }

    if (PvNode && selDepth < (ply - rootPly) + 1) {
        selDepth = (ply - rootPly) + 1;
    }

    // represents the best score we have found so far
    int bestScore = -32001;

    // threshold for futility pruning
    int futility = -32001;
    int futilityValue;

    // tDepth will be -1 when searching evasions or when we include checks and promotions, -2 otherwise
    int tDepth = check || depth >= 0 ? -1 : -2;

    // transposition lookup
    uint64_t hash = board.hash();
    Node *node = table.probe(hash, board.found());
    int nType = board.found() ? node->nodeTypeGenBound & 0x3 : 0;
    int nEval = board.found() ? node->nodeEval : -32001;
    int nDepth = board.found() ? node->nodeDepth : -1;
    int nScore = board.found() ? scoreFromTable(node->nodeScore, (ply - rootPly), board.halfmoves()) : -32001;
    libchess::Move nMove = board.found() ? board.from_table(node->bestMove) : libchess::Move(0);
	if (!PvNode
		&& board.found()
        && nScore != -32001
		&& nDepth >= tDepth
        && (nType & (nScore >= beta ? 2 : 1))) {
		movesTransposed++;
		return nScore;
	}

    // we can't stand pat if we are in check
    if (!check) {
        // stand pat score to see if we can exit early
        if (board.found()) {
            if ((bestScore = board.staticEval() = nEval) == -32001) {
                bestScore = board.staticEval() = evaluateBoard(board);
            }

            // previously saved transposition score can be used as a better position evaluation
            if (nScore != -32001
                && (nType & (nScore > bestScore ? 2 : 1))) {
                bestScore = nScore;
            }
        }
        else {
            bestScore = board.staticEval() = board.prevMoveType(ply) != libchess::Move::Type::NONE ? evaluateBoard(board) : -board.staticEval(ply - 1);
        }

        // adjust alpha based on the stand pat
        if (bestScore >= beta) {
			if (!board.found()) {
                node->save(hash, tableScore(bestScore, (ply - rootPly)), 2, -3, 0, board.staticEval(), false);
			}
            return bestScore;
        }
        if (PvNode && bestScore > alpha) {
            alpha = bestScore;
        }

        // calculate futility threshold
        futility = bestScore + 280;
    }

    // holds the continuation history from previous moves
    const PieceHistory* contHistory[] = {board.continuationHistory(ply - 1), board.continuationHistory(ply - 2),
                                         nullptr                               , board.continuationHistory(ply - 4),
                                         nullptr                               , board.continuationHistory(ply - 6)};

    libchess::Square prevMoveSq = board.prevMoveType(ply) == libchess::Move::Type::NONE ? libchess::Square(-1) : board.previous_move()->to_square();
    MovePicker picker(board, nMove, &moveHistory, contHistory, &captureHistory, depth);

    // arbitrarily low score to make sure its replaced
    int score = -32001;

    // holds the move we want to search
    libchess::Move move;
    libchess::Move bestMove;

    bool givesCheck = false;
    bool isCapture = false;

    int moveCounter = 0;
    int quietCheckCounter = 0;
    // loop through the moves and score them
    while ((move = picker.nextMove()).value() != 0) {

        // search only legal moves
        if (!board.is_legal_move(move)) {
            continue;
        }

        givesCheck = board.gives_check(move);
        isCapture = board.is_capture_move(move);

        moveCounter++;


        // pruning steps
        if (bestScore > -30000) { // if we are in check, bestScore will by default be -32001, this makes sure we at least search the first value when in check
            // futility pruning
            if (!givesCheck
                && move.to_square() != prevMoveSq
                && futility > -30000
                && !board.is_promotion_move(move)) {
                    if (moveCounter > 2) {
                        continue;
                    }

                    if (board.piece_on(move.to_square())) {
                        futilityValue = futility + pieceValues[board.piece_on(move.to_square())->value()];
                    }
                    else {
                        futilityValue = futility;
                    }

                    if (futilityValue <= alpha) {
                        bestScore = std::max(bestScore, futilityValue);
                        continue;
                    }

                    if (futility <= alpha && !board.see_ge(move, 1)) {
                        bestScore = std::max(bestScore, futility);
                        continue;
                    }
                }

                // prune quiet check evasions after the second move
                if (quietCheckCounter > 1) {
                    break;
                }

                // continuation history based pruning
                if (!isCapture
                    && (*contHistory[0])[board.piece_on(move.from_square())->value()][move.to_square()] < 0
                    && (*contHistory[1])[board.piece_on(move.from_square())->value()][move.to_square()] < 0) {
                        continue;
                    }

                // see pruning
                if (!board.see_ge(move, -105)) {
                    continue;
                }
        }

        // prefetch before we make a move
        prefetch(table.firstEntry(board.hashAfter(move)));

        quietCheckCounter += !isCapture && check;

        // update continuation history
        board.continuationHistory() = &continuationHistory[check]
                                                          [board.is_capture_move(move)]
                                                          [board.piece_on(move.from_square())->value()]
                                                          [move.to_square()];

        movesExplored++;
        board.make_move(move);

        quiesceExplored++;
        incPly();
        score = -quiescence<nodeType>(board, -beta, -alpha, depth - 1);
        decPly();
        board.unmake_move();

        if (score > bestScore) {
            bestScore = score;
            if (score > alpha) {
                bestMove = move;
                if (PvNode && score < beta) {
                    alpha = score;
                }
                else {
                    cutNodes++;
                    break;
                }
            }
        }
    }

    // check for mate
    if (check && moveCounter == 0) {
        return -32000 + (ply - rootPly);
    }

    node->save(hash, tableScore(bestScore, (ply - rootPly)), bestScore >= beta ? 2 : 1, tDepth, bestMove.to_table(), board.staticEval(), board.found());
    return bestScore;

}

// the negamax function.  Does the heavy lifting for the search
template <Anduril::NodeType nodeType>
int Anduril::negamax(libchess::Position &board, int depth, int alpha, int beta, bool cutNode) {
    constexpr bool PvNode = nodeType != NonPV;
    constexpr bool rootNode = nodeType == Root;

    // if we are at max depth, start a quiescence search
    if (depth <= 0){
        return quiescence<PvNode ? PV : NonPV>(board, alpha, beta);
    }

    // are we in check?
    int check = board.in_check();

    // update the selective depth info to send to the GUI
    if (PvNode && selDepth < (ply - rootPly) + 1) {
        selDepth = (ply - rootPly) + 1;
    }

    // should we abort the search?
    if (id == 0 && shouldStop()) {
        gondor.stop = true;
    }

    // is the time up?  Mate distance pruning?
    if constexpr (!rootNode) {
        // check for aborted search
        if (gondor.stop) {
            return 0;
        }

        if (ply - rootPly >= 100) {
            return !check ? evaluateBoard(board) : 0;
        }

        // check for a draw, add variance to draw score to avoid 3-fold blindness
        if (board.is_draw()) {
            return 0 - 1 + (movesExplored.load() & 0x2);
        }

        // Mate distance pruning.  If we mate at the next move, our score would be mate in current ply + 1.  If alpha
        // is already bigger because we found a shorter mate further up, there is no need to search because we will
        // never beat the current alpha.  Same logic but reversed applies to beta.  If the mate distance is higher
        // than a line that we have found, we return a fail-high score.
        alpha = std::max(-32000 + (ply - rootPly), alpha);
        beta = std::min(32000 - (ply - rootPly + 1), beta);
        if (alpha >= beta) {
            return alpha;
        }
    }

    // did alpha change?
    bool alphaChange = false;

    // represents our next move to search
    libchess::Move move(0);
    libchess::Move bestMove = move;
    libchess::Move excludedMove = board.getExcluded();

    // reset killers for next ply
    killers[ply - rootPly + 1][0] = libchess::Move(0);
    killers[ply - rootPly + 1][1] = libchess::Move(0);

    // amount of quiet moves we searched and what moves they were
    int quietCount = 0;
    libchess::Move quietMoves[64];

    // amount of capture moves we searched and what they were
    int captureCount = 0;
    libchess::Move captureMoves[32];

    // represents the best score we have found so far
    int bestScore = -32001;

    // holds the score of the move we just searched
    int score = -32001;

    // max score in case we find a tb score
    int maxScore = 32000;

    int moveCounter;
    moveCounter = board.moveCount() = 0;

    // Idea from Stockfish: these variable represent if we are improving our score over our last turn, and how much
    bool improving;
    int improvement;

    libchess::Square prevMoveSq = board.prevMoveType(ply) == libchess::Move::Type::NONE ? libchess::Square(-1) : board.previous_move()->to_square();

    // transposition lookup
    uint64_t hash = board.hash();
    Node *node = table.probe(hash, board.found());
    int nDepth = board.found() ? node->nodeDepth : -99;
    int nType = board.found() ? node->nodeTypeGenBound & 0x3 : 0;
    int nEval = board.found() ? node->nodeEval : -32001;
    int nScore = board.found() ? scoreFromTable(node->nodeScore, (ply - rootPly), board.halfmoves()) : -32001;
    libchess::Move nMove = board.found() ? board.from_table(node->bestMove) : libchess::Move(0);
    bool transpositionCapture = board.found() && board.is_capture_move(nMove);
    board.ttPv() = excludedMove.value() != 0 ? board.ttPv() : PvNode || (board.found() && node->nodeTypeGenBound & 0x4);

	if (!PvNode
		&& board.found()
        && nScore != -32001
        && excludedMove.value() == 0
		&& nDepth >= depth
        && (nType & (nScore >= beta ? 2 : 1))) {
        movesTransposed++;

        // if the transposition move is quiet, we can update our sorting statistics
        if (nMove.value() != 0 && board.is_legal_move(nMove)) {
            // bonus for transpositions that fail high
            if (nScore >= beta) {
                if (!transpositionCapture) {
                    updateQuietStats(board, nMove, stat_bonus(depth));
                }

                // extra penalty for early quiet moves on the previous ply
                if (prevMoveSq.value() != -1 && board.moveCount(ply - 1) <= 2 && !board.previously_captured_piece()) {
                    int bonus = stat_penalty(depth + 1);
                    updateContinuationHistory(board,*board.piece_on(prevMoveSq),prevMoveSq, bonus,1);
                }

            }
            // penalty for those that don't
            else if (!transpositionCapture) {
                int bonus = stat_penalty(depth);

                // update move history and continuation history for best move
                moveHistory[board.side_to_move()][nMove.from_square()][nMove.to_square()] += bonus - moveHistory[board.side_to_move()][nMove.from_square()][nMove.to_square()] * abs(bonus) / maxHistoryVal;
                updateContinuationHistory(board,*board.piece_on(nMove.from_square()),nMove.to_square(), bonus);
            }
        }

        // stockfish does this to get around the graph history interaction problem, which is an issue where the same
        // game position will behave differently when reached using different paths.  When halfmoves is high, we don't
        // produce transposition cutoffs because the engine might accidentally hit the 50 move rule if we do
        if (board.halfmoves() < 90) {
            return nScore;
        }
	}

    // probe the tablebases
    unsigned tbScore = 0;
    if ((tbScore = Tablebase::probeTablebaseWDL(board, depth, (ply - rootPly))) != TB_RESULT_FAILED) {
        ++tbHits;

        // convert WDL to a score
        score = tbScore == TB_LOSS ? -31753 + (ply - rootPly)
              : tbScore == TB_WIN  ?  31753 - (ply - rootPly)
              : 0;

        // find the node type for the transposition table
        int tbType = tbScore == TB_LOSS ? 1
                   : tbScore == TB_WIN  ? 2
                   : 3;

        // check to see if tb score causes a cutoff
        if (tbType == 3 || (tbType == 2 ? score >= beta : score <= alpha)) {
            node->save(hash, tableScore(score, (ply - rootPly)), tbType, depth, 0, -32001, board.ttPv());
            return score;
        }

        // if we are in a pv node, we need to make sure we don't return a score lower than the tb
        if constexpr(PvNode) {
            if (tbType == 2) {
                bestScore = score;
                alpha = std::max(alpha, bestScore);
            }
            else {
                maxScore = score;
            }
        }
    }


	// get the static evaluation
    // it won't be needed if in check however
    int staticEval;
    if (check) {
        board.staticEval() = staticEval = 32001;
    }
    else if (excludedMove.value() != 0) {
        // if there is an excluded move, its the same position so the static eval is already saved
        staticEval = board.staticEval();
    }
    else if (board.found()) {
        // little check in case something gets messed up
        if (nEval == -32001) {
            board.staticEval() = staticEval = evaluateBoard(board);
        }
        else {
            board.staticEval() = staticEval = nEval;
        }

        // previously saved transposition score can be used as a better position evaluation
        if (nScore != -32001
            && (nType & (nScore > staticEval ? 2 : 1))) {
            staticEval = nScore;
        }
    }
    else {
        board.staticEval() = staticEval = evaluateBoard(board);
        node->save(hash, -32001, 0, -99, 0, staticEval, board.ttPv());
    }

    // set up the improvement variable, which is the difference in static evals between the current turn and
    // our last turn.  If we were in check the last turn, we try the move prior to that
    if (ply - rootPly >= 2 && board.staticEval(ply - 2) != 32001) {
        improvement = board.staticEval() - board.staticEval(ply - 2);
    }
    else if (ply - rootPly >= 4 && board.staticEval(ply - 4) != 32001) {
        improvement = board.staticEval() - board.staticEval(ply - 4);
    }
    else {
        improvement = 1; // if we were in check the last two turns, assume we are improving by some amount
    }
    improving = !check && improvement > 0; // if we are currently in check, we assume we are not improving

    // razoring
    if (!check
        && staticEval < alpha - 577 - 147 * depth * depth) {
        // verification that the value is indeed less than alpha
        score = quiescence<NonPV>(board, alpha - 1, alpha);
        if (score < alpha && abs(score) < 31507) {
            return score;
        }
    }

    // reverse futility pruning
    if (!PvNode
        && !check
        && excludedMove.value() == 0
        && depth < 6
        && staticEval - reverseFutilityMargin(depth, improving) >= beta
        && staticEval >= beta
        && staticEval < 31000) {
        return staticEval;
    }

    // internal iterative reductions
    // decrease depth on cut nodes and pv nodes that don't have a transposition move
    if ((cutNode || PvNode)
        && depth >= 7
        && nMove.value() == 0) {
        depth -= 1;
    }

    // null move pruning
    if (!PvNode
        && !check
        && beta > -30000
        && board.prevMoveType(ply) != libchess::Move::Type::NONE
        && staticEval >= beta
        && excludedMove.value() == 0
        && nonPawnMaterial(!board.side_to_move(), board)
        && (ply - rootPly) >= minNullPly
        && (!board.found() || nScore >= beta)) {

        // set reduction based on depth, eval, and whether the last move made was tactical

        int R = 2 + depth / 3 + std::min(5, (staticEval - beta) / 252) + (board.is_capture_move(*board.previous_move()) || board.is_promotion_move(*board.previous_move()));

        board.continuationHistory() = &continuationHistory[0][0][15][0]; // no piece has a value of 15 so we can use that as our "null" flag

        movesExplored++;
        board.make_null_move();
        // prefetch after null move
        prefetch(table.firstEntry(board.hash()));
        incPly();
        int nullScore = -negamax<NonPV>(board, depth - R, -beta, -beta + 1, !cutNode);
        decPly();
        board.unmake_move();

        if (nullScore >= beta && nullScore < 31507) {

            if (minNullPly || depth < 13) {
                return nullScore;
            }

            // verification search at high depths, null pruning disabled until minNullPly is reached
            minNullPly = (ply - rootPly) + 265 * (depth - R) / 487;

            int s = negamax<NonPV>(board, depth - R, beta - 1, beta, false);

            minNullPly = 0;

            if (s >= beta) {
                return s;
            }
        }
    }

    // probcut
    // if we find a position that returns a cutoff when beta is padded a little, we can assume
    // the position would most likely cut at a full depth search as well
    int probCutBeta = beta + 249 - 132 * improving;
    if (!PvNode
        && depth > 3
        && !check
        && abs(beta) < 31507
        && !(board.found()
        && nDepth >= depth - 3
        && nScore != -32001
        && nScore < probCutBeta)) {

        // get a move picker
        MovePicker picker(board, nMove, &captureHistory, probCutBeta - board.staticEval());

        // we loop through all the moves to try and find one that
        // causes a beta cut on a reduced search
        while ((move = picker.nextMove()).value() != 0) {

            // search only legal moves
            if (!board.is_legal_move(move) || move == excludedMove) {
                continue;
            }

            board.continuationHistory() = &continuationHistory[check]
                                                              [true]
                                                              [board.piece_on(move.from_square())->value()]
                                                              [move.to_square()];

            movesExplored++;
            board.make_move(move);

            // perform a qsearch to verify that the move still is less than beta
            incPly();
            score = -quiescence<NonPV>(board, -probCutBeta, -probCutBeta + 1);
            decPly();

            // if the qsearch holds, do a regular one
            if (score >= probCutBeta) {
                incPly();
                score = -negamax<NonPV>(board, depth - 4, -probCutBeta, -probCutBeta + 1, !cutNode);
                decPly();
            }

            board.unmake_move();

            if (score >= probCutBeta) {
                // write to the node
                node->save(hash, tableScore(score, (ply - rootPly)), 2, depth - 3, move.to_table(), board.staticEval(), board.ttPv());
                cutNodes++;
                return score;
            }

        }
    }

    // holds the continuation history from previous moves
    const PieceHistory *contHistory[] = {board.continuationHistory(ply - 1), board.continuationHistory(ply - 2),
                                         nullptr                               , board.continuationHistory(ply - 4),
                                         nullptr                               , board.continuationHistory(ply - 6)};

    libchess::Move counterMove = prevMoveSq != -1 ? counterMoves[board.previous_move()->from_square()][board.previous_move()->to_square()] : libchess::Move(0);

    // get a move picker
    MovePicker picker(board, nMove, killers[ply - rootPly].data(),
                      counterMove,
                      &moveHistory, contHistory, &captureHistory);

    // at root, we are going to ignore the picker object.  The amount of time spent allocating and selecting moves should be negligible because this only happens for one position per search
    // here we set the pointer for the current root move to the beginning of the move list, and sort the list
    if constexpr (rootNode) {
        partial_insertion_sort(rootMoves.begin(), rootMoves.end(), std::numeric_limits<int>::min());
        currRootMove = rootMoves.begin();
    }

    // indicates that a PvNode will probably fail low if the node was searched, and we found a fail low already
    bool likelyFailLow = PvNode && nMove.value() != 0 && (nType & 1) && nDepth >= depth;

    // indicates that the transposition move has been singularly extended
    bool singularQuietLMR = false;

    // can we use move count pruning?
    bool moveCountPruning = false;

    int extension = 0;
    int hist;
    // loop through the possible moves and score each
    while ((move = picker.nextMove(moveCountPruning)).value() != 0) {

        // at root, we do this check just in case the move picker has some illegal moves in it
        if constexpr (rootNode) {
            if (currRootMove == rootMoves.end()) {
                break;
            }
        }

        // at root, replace the move with the current root move
        if constexpr (rootNode) {
            move = *currRootMove;
        }

        if (move == excludedMove) {
            continue;
        }

        // search only legal moves
        // all moves are legal at root because we generated a legal move list
        if (!rootNode && !board.is_legal_move(move)) {
            continue;
        }

        // if we are at root, give the gui some information
        if constexpr (rootNode) {
            if (id == 0) {
                auto now = std::chrono::steady_clock::now();
                std::chrono::duration<double, std::milli> elapsed = now - startTime;
                if (elapsed.count() > 2000 && depth > 5) {
                    std::cout << "info currmove " << move.to_str() << " currmovenumber " << moveCounter + 1
                              << std::endl;
                }
            }
        }

        board.moveCount() = ++moveCounter;
        bool capture = board.is_capture_move(move);
        bool promotion = board.is_promotion_move(move);
        bool givesCheck = board.gives_check(move);
        int movePieceType = board.piece_on(move.from_square())->value();
        hist = (*contHistory[0])[movePieceType][move.to_square()]
                + (*contHistory[1])[movePieceType][move.to_square()]
                + (*contHistory[3])[movePieceType][move.to_square()];

        // quiet move pruning and move count pruning
        if (!rootNode
            && nonPawnMaterial(!board.side_to_move(), board)
            && bestScore > -30000) {

            // move count pruning
            // this doesn't require that the move we are searching is quiet so we do it here
            moveCountPruning = moveCounter >= moveCountPruningThreshold(improving, depth);

            // quiet move pruning
            if (!capture
                && !givesCheck) {

                int lmrDepth = depth - reductionsQuiet[depth][moveCounter];

                // futility pruning
                if (lmrDepth <= 9
                    && !check
                    && board.staticEval() + 276 + lmrDepth * 78 <= alpha) {
                        continue;
                }

                // continuation history pruning
                if (lmrDepth <= 5
                    && hist < -14462 * depth) {
                        continue;
                    }

                }

        }

        int seeMargin = (!capture && !promotion && !givesCheck) ? -5 * depth * depth : -62 * depth;

        // see pruning
        if (bestScore > -31000
            && depth <= 9
            && picker.getStage() > 2 // value of 2 == good captures
            && !board.see_ge(move, seeMargin)) {
                continue;
            }

        // the actual depth we will search this move to
        int actualDepth = depth - 1;
        extension = 0;

        // search extensions
        if (ply - rootPly < rDepth * 2) {
            // Singular extension search
            // based on the stockfish implementation
            if (!rootNode
                && depth >= 5 - (rDepth > 22) + (PvNode && board.ttPv())
                && move == nMove
                && excludedMove.value() == 0
                && abs(nScore) < 31000
                && (nType & 2)
                && nDepth >= depth - 3) {
                int singleBeta = nScore - (101 + 20 * (board.ttPv() && !PvNode)) * depth / 64;
                int singleDepth = (depth - 1) * 16 / 26;
                singularAttempts++;

                board.setExcluded(move);
                score = negamax<NonPV>(board, singleDepth, singleBeta - 1, singleBeta, cutNode);
                board.setExcluded(libchess::Move(0));

                if (score < singleBeta) {
                    extension = 1;
                    singularQuietLMR = !transpositionCapture;
                    singularExtensions++;
                }

                // Stockfish calls this Mult-Cut pruning
                // basically, if we searched without the excluded move and still failed high, then there are two different
                // moves that fail high, making this a Cut-node, not singular.  We can prune the whole subtree by returning a soft bound
                else if (singleBeta >= beta) {
                    return singleBeta;
                }

                // if our node score is greater than beta, we can further reduce the search
                else if (nScore >= beta) {
                    extension = -2 - !PvNode;
                }

                // if our node score is less than the score without that move searched, we can reduce this move by 1
                else if (nScore <= score) {
                    extension = -1;
                }

                // if our node score is less than alpha, it will likely fail low, and we can reduce
                else if (nScore <= alpha) {
                    extension = -1;
                }
            }

            // check extension
            else if (givesCheck && depth > 12) {
                extension = 1;
            }

            // quiet transposition extension
            else if (PvNode
                     && move == nMove
                     && move == killers[ply - rootPly][0]
                      && (*contHistory[0])[movePieceType][move.to_square()] >= 4167) {
                extension = 1;
            }

            // recapture extension
            else if (PvNode
                     && capture
                     && board.previous_move()
                     && board.is_capture_move(*board.previous_move())
                     && move.to_square() == prevMoveSq) {
                extension = 1;
            }

        }

        // add extensions to depth
        actualDepth += extension;

        // update continuation history (must be done after singular extensions)
        board.continuationHistory() = &continuationHistory[check]
                                                          [capture]
                                                          [movePieceType]
                                                          [move.to_square()];

        // prefetch before we make a move
        prefetch(table.firstEntry(board.hashAfter(move)));

        movesExplored++;

        // make the move
        board.make_move(move);

        // search with zero window
        // first check if we can reduce
        if (depth >= 2 && moveCounter > 2) {
            // find our reduction
            int reduction;
            if (capture || promotion) {
                reduction = reductionsTactical[depth][moveCounter];
            }
            else {
                reduction = reductionsQuiet[depth][moveCounter];

                // decrease if position is not likely to fail low
                if (PvNode && !likelyFailLow) {
                    reduction += -1;
                }

                // increase reduction for cut nodes
                if (cutNode) {
                    reduction += 3;
                }

                // increase reduction if transposition move is a capture
                if (transpositionCapture) {
                    reduction += 1;
                }

                // decrease reduction if the transposition move has been singularly extended
                if (singularQuietLMR) {
                    reduction += -1;
                }

                // decrease reduction if opponent move count is high
                if (board.moveCount(ply - 1) > 5) {
                    reduction += -2;
                }

                // increase reduction on repetition
                if (move == *board.previousMove(ply - 3) && board.is_repeat()) {
                    reduction += 1;
                }

                // adjust based on history stats
                // first index is !side_to_move because we already called make_move!
                hist += moveHistory[!board.side_to_move()][move.from_square()][move.to_square()];
                reduction -= hist / 21765;
            }

            // new depth we will search with
            // we need to search at least one more move, and cap the reduction at actualDepth so that we don't extend the search
            int newDepth = std::max(1, std::min(actualDepth - reduction, actualDepth));

            incPly();
            score = -negamax<NonPV>(board, newDepth, -(alpha + 1), -alpha, true);

            // full depth search when LMR fails high
            if (score > alpha && newDepth < actualDepth) {
                score = -negamax<NonPV>(board, actualDepth, -(alpha + 1), -alpha, !cutNode);
            }
            decPly();
            int bonus = score <= alpha ? stat_penalty(actualDepth)
                      : score >= beta  ? stat_bonus(actualDepth)
                                       : 0;
            updateContinuationHistory(board, libchess::Piece(movePieceType), move.to_square(), bonus);
        }
        else if (!PvNode || moveCounter > 1) {
            incPly();
            score = -negamax<NonPV>(board, actualDepth, -(alpha + 1), -alpha, !cutNode);
            decPly();
        }

        // full PV search for the first move and for moves that fail in the zero window
        if (PvNode && (moveCounter == 1 || score > alpha)) {
            incPly();
            score = -negamax<PV>(board, actualDepth, -beta, -alpha, false);
            decPly();
        }

        // undo the move
        board.unmake_move();

        // if the search was stopped for whatever reason, return immediately
        if (gondor.stop || (limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime)) {
            return 0;
        }

        // update the move score if at root
        if constexpr (rootNode) {
            currRootMove->score = score;
            currRootMove++;
        }

        if (score > bestScore) {
            bestScore = score;
            if (score > alpha) {
                bestMove = move;
                if (score >= beta) {
                    cutNodes++;
                    break;
                }
                else {
                    alpha = score;
                    alphaChange = true;
                }
            }
        }

        // if the move was worse than a previous move, remember it for update later
        if (move != bestMove) {
            if (capture && captureCount < 32) {
                captureMoves[captureCount++] = move;
            }
            else if (quietCount < 64) {
                quietMoves[quietCount++] = move;
            }
        }
    }

    // check for mate and stalemate
    if (moveCounter == 0) {
        bestScore = excludedMove.value() != 0 ? alpha :
                    check                     ? -32000 + (ply - rootPly)
                                              : 0;
    }

    // update statistics if we found a move that exceeds alpha
    else if (bestMove.value() != 0) {
        updateStatistics(board, bestMove, bestScore, depth, beta, quietMoves, quietCount, captureMoves, captureCount);
    }

    // bonus for prior move that caused fail low
    else if (!board.previously_captured_piece() && prevMoveSq.value() != -1) {
        int bonus = (depth > 5) + (PvNode || cutNode) + (bestScore < alpha - 25 * depth) + (board.moveCount(ply - 1) > 10);
        updateContinuationHistory(board, *board.piece_on(prevMoveSq), prevMoveSq, stat_bonus(depth) * bonus, 1);
    }

    // best score cannot be greater than the tb score
    if constexpr(PvNode) {
        bestScore = std::min(bestScore, maxScore);
    }

    // If no good move is found and the previous position was ttPv, then the previous
    // opponent move is probably good and the new position is added to the search tree.
    if (bestScore <= alpha) {
        board.ttPv() = board.ttPv() || (board.ttPv(ply - 1) && depth > 3);
    }

    // save the node
    if (excludedMove.value() == 0) {
        node->save(hash, tableScore(bestScore, (ply - rootPly)), bestScore >= beta ? 2 : alphaChange && PvNode ? 3 : 1, depth, bestMove.to_table(), board.staticEval(), board.ttPv());
    }
    return bestScore;

}

// template definition so that we can call from UCI.cpp
template int Anduril::negamax<Anduril::Root>(libchess::Position &board, int depth, int alpha, int beta, bool cutNode);

void Anduril::bench(libchess::Position &board) {
    // setup for the search
    limits.timeSet = false;
    movesExplored = 0;

    // set the killer vector to have the correct number of slots
    for (auto i : killers) {
        i[0] = libchess::Move(0);
        i[1] = libchess::Move(0);
    }

    ply = board.ply();
    rootPly = ply;

    // initialize the oversize state array
    for (int i = 7; i > 0; i--) {
        board.continuationHistory(ply - i) = &continuationHistory[0][0][15][0];
    }

    // start the timer and search
    // using PV instead of root will silence any console activity that would have occurred.
    // In return, this is slightly inaccurate to the "real thing", but realistically it should be close enough to not matter
    startTime = std::chrono::steady_clock::now();
    negamax<PV>(board, 25, -32001, 32001, false);

    // stop the timer and report the nodes searched and speed
    stopTime = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> elapsed = stopTime - startTime;
    std::cout << getMovesExplored() << " nodes " << uint64_t(getMovesExplored() / (elapsed.count() / 1000)) << " nps" << std::endl;
}

int Anduril::nonPawnMaterial(bool whiteToPlay, libchess::Position &board) {
    int material = 0;
    if (whiteToPlay) {
        material += board.piece_type_bb(libchess::constants::KNIGHT, libchess::constants::WHITE).popcount() * libchess::Position::pieceValuesMG[1];
        material += board.piece_type_bb(libchess::constants::BISHOP, libchess::constants::WHITE).popcount() * libchess::Position::pieceValuesMG[2];
        material += board.piece_type_bb(libchess::constants::ROOK, libchess::constants::WHITE).popcount() * libchess::Position::pieceValuesMG[3];
        material += board.piece_type_bb(libchess::constants::QUEEN, libchess::constants::WHITE).popcount() * libchess::Position::pieceValuesMG[4];

        return material;
    }
    else {
        material += board.piece_type_bb(libchess::constants::KNIGHT, libchess::constants::BLACK).popcount() * libchess::Position::pieceValuesMG[1];
        material += board.piece_type_bb(libchess::constants::BISHOP, libchess::constants::BLACK).popcount() * libchess::Position::pieceValuesMG[2];
        material += board.piece_type_bb(libchess::constants::ROOK, libchess::constants::BLACK).popcount() * libchess::Position::pieceValuesMG[3];
        material += board.piece_type_bb(libchess::constants::QUEEN, libchess::constants::BLACK).popcount() * libchess::Position::pieceValuesMG[4];

        return material;
    }
}

// updates all history statistics
void Anduril::updateStatistics(libchess::Position &board, libchess::Move bestMove, int bestScore, int depth, int beta,
                               libchess::Move *quietsSearched, int quietCount, libchess::Move *capturesSearched, int captureCount) {
    int largerBonus = stat_bonus(depth + 1);
    int bonus = bestScore > beta + 53 ? largerBonus : stat_bonus(depth);
    int nonBestPenalty;

    if (!board.is_capture_move(bestMove)) {

        // update stats for best quiet
        updateQuietStats(board, bestMove, bonus);

        // decrease stats for non-best quiets
        for (int i = 0; i < quietCount; i++) {
            nonBestPenalty = bestScore > beta + 53 ? stat_penalty(depth + 1) : stat_penalty(depth);
            updateContinuationHistory(board, *board.piece_on(quietsSearched[i].from_square()), quietsSearched[i].to_square(), nonBestPenalty);
            moveHistory[board.side_to_move()][quietsSearched[i].from_square()][quietsSearched[i].to_square()] += nonBestPenalty - moveHistory[board.side_to_move()][quietsSearched[i].from_square()][quietsSearched[i].to_square()] * abs(nonBestPenalty) / maxHistoryVal;
        }
    }
    else {
        // update stats for best capture
        int capturedIndex = board.piece_type_on(bestMove.to_square()) ? board.piece_type_on(bestMove.to_square())->value() : 0; // condition for enpassant
        captureHistory[board.piece_on(bestMove.from_square())->to_nnue()][bestMove.to_square()][capturedIndex] += largerBonus - captureHistory[board.piece_on(bestMove.from_square())->to_nnue()][bestMove.to_square()][capturedIndex] * abs(largerBonus) / maxCaptureVal;
    }

    // extra penalty for early move that was not a transposition or main killer in previous ply
    if (board.previous_move() && ((board.moveCount(ply - 1) == 1 + board.found(ply - 1) || *board.previous_move() == killers[ply - rootPly - 1][0])) && !board.previously_captured_piece()) {
        updateContinuationHistory(board, *board.piece_on(board.previous_move()->to_square()), board.previous_move()->to_square(), stat_penalty(depth + 1), 1);
    }

    // decrease stats for non-best captures
    for (int i = 0; i < captureCount; i++) {
        nonBestPenalty = stat_penalty(depth + 1);
        int movedPieceIdx = board.piece_on(capturesSearched[i].from_square())->to_nnue();
        int capturedType = board.piece_type_on(bestMove.to_square()) ? board.piece_type_on(bestMove.to_square())->value() : 0; // condition for enpassant
        captureHistory[movedPieceIdx][capturesSearched[i].to_square()][capturedType] += nonBestPenalty - captureHistory[movedPieceIdx][capturesSearched[i].to_square()][capturedType] * abs(nonBestPenalty) / maxCaptureVal;
    }
}

void Anduril::updateQuietStats(libchess::Position &board, libchess::Move bestMove, int bonus) {
    // update killers
    insertKiller(bestMove, ply - rootPly);

    // update move history and continuation history for best move
    moveHistory[board.side_to_move()][bestMove.from_square()][bestMove.to_square()] += bonus - moveHistory[board.side_to_move()][bestMove.from_square()][bestMove.to_square()] * abs(bonus) / maxHistoryVal;
    updateContinuationHistory(board, *board.piece_on(bestMove.from_square()), bestMove.to_square(), bonus);

    // update countermoves
    if (board.prevMoveType(ply) != libchess::Move::Type::NONE) {
        counterMoves[board.previous_move()->from_square()][board.previous_move()->to_square()] = bestMove;
    }
}

// updates continuation history
void Anduril::updateContinuationHistory(libchess::Position &board, libchess::Piece piece, libchess::Square to, int bonus, int start) {
    for (int i : {1, 2, 4, 6}) {
        // only update first 2 if we are in check
        if (i > 2 && board.in_check()) {
            break;
        }
        // we index ply - i + 1 because we need to check that the ply we are looking at wasn't null, so we have to access the next ply's previous move
        if (board.prevMoveType(ply - start - i + 1) != libchess::Move::Type::NONE) {
            (*board.continuationHistory(ply - start - i))[piece.value()][to] += bonus - (*board.continuationHistory(ply - start - i))[piece.value()][to] * abs(bonus) / maxContinuationVal;
        }
    }
}

// finds and returns the principal variation
std::vector<libchess::Move> Anduril::getPV(libchess::Position &board, int depth, libchess::Move bestMove) {
    std::vector<libchess::Move> PV;
    uint64_t hash = 0;
    Node *node;
    bool found = true;

    // push the best move and add to the PV
    PV.push_back(bestMove);
    board.make_move(bestMove);

    // This loop hashes the board, finds the node associated with the current board
    // adds the best move we found to the PV, then pushes it to the board
    hash = board.hash();
    node = table.probe(hash, found);
    while (found && node->bestMove != 0) {
        libchess::Move tmp = board.from_table(node->bestMove);
        if (board.is_capture_move(tmp) && board.piece_type_on(tmp.to_square()) == std::nullopt && tmp.type() != libchess::Move::Type::ENPASSANT) {
            break;
        }
        // this will make sure we don't segfault
        // I don't know the problem but this fixed it and the search does not appear to be affected
        if (board.is_legal_move(tmp)) {
            PV.push_back(tmp);
            board.make_move(tmp);
        }
        else {
            break;
        }
        hash = board.hash();
        node = table.probe(hash, found);
        // in case of infinite loops of repeating moves
        // also check to see if the pv is still in the main search
        if (PV.size() > depth || node->nodeDepth <= 0) {
            break;
        }
    }

    // undoes each move in the PV so that we don't have a messed up board
    for (int i = PV.size() - 1; i >= 0; i--){
        board.unmake_move();
    }

    return PV;
}

uint64_t Anduril::getMovesExplored() {
    uint64_t total = 0;
    for (auto &t : gondor) {
        total += t->engine->movesExplored.load();
    }
    return total;
}

uint64_t Anduril::getTbHits() {
    uint64_t total = 0;
    for (auto &t : gondor) {
        total += t->engine->tbHits.load();
    }
    return total;
}


bool Anduril::shouldStop() {
    if (limits.nodes != -1) {
        return getMovesExplored() >= limits.nodes;
    }

    return limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime;

}
