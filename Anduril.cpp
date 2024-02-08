//
// Created by Hughe on 4/1/2022.
//

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

#include "Anduril.h"
#include "MovePicker.h"
#include "UCI.h"

// reduction table
// its oversize just in case something weird happens
int reductions[150][150];

// stat bonus values
int sbc = 101;
int sbm = 201;
int msb = 4296;

// razoring values
int rvc = 460;
int rvs = 148;

extern int maxHistoryVal;
extern int maxContinuationVal;

// the other formula simplifies down to this.  This should be easier to tune than 4 separate variables
void initReductions(double nem, double neb) {
    for (int i = 0; i < 150; i++) {
        for (int j = 0; j < 150; j++) {
            reductions[i][j] = int(neb + (log(i) * log(j) / nem));
        }
    }
}

int stat_bonus(int depth) {
    return std::min(sbm * depth - sbc, msb);
    //return 2 * depth * depth;
}

// returns the amount of moves we need to search before we can use move count based pruning
constexpr int moveCountPruningThreshold(bool improving, int depth) {
    return improving ? (3 + depth * depth) : (3 + depth * depth) / 2;
}

// returns the margin for reverse futility pruning.  Returns the same values as the list we used before, except this is
// able to a higher depth, and we introduced the improving factor to it
constexpr int reverseFutilityMargin(int depth, bool improving) {
    return 200 * (depth - improving);
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
    if (score >= 31508) {
        // don't return a mate score if we are going to hit the 50 move rule
        if (score >= 31754 && 32000 - score > 99 - rule50) {
            return 31753; // this is below what checkmate in maximum search would be
        }
        
        return score - ply;
    }

    if (score <= -31508) {
        // don't return a mate score if we are going to hit the 50 move rule
        if (score <= -31754 && 32000 + score > 99 - rule50) {
            return -31753; // this is below what checkmate in maximum search would be
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
    int nScore = board.found() ? scoreFromTable(node->nodeScore, (ply - rootPly), board.halfmoves()) : -32001;
    libchess::Move nMove = board.found() ? libchess::Move(node->bestMove) : libchess::Move(0);
	if (!PvNode
		&& board.found()
        && nScore != -32001
		&& node->nodeDepth >= tDepth
        && (nType & (nScore >= beta ? 2 : 1))) {
		movesTransposed++;
		return nScore;
	}

    int standPat = -32001;
    // we can't stand pat if we are in check
    if (!check) {
        // stand pat score to see if we can exit early
        if (board.found()) {
            if (node->nodeEval != -32001) {
                bestScore = standPat = node->nodeEval;
            }
            else {
                bestScore = standPat = evaluateBoard(board);
            }

            // previously saved transposition score can be used as a better position evaluation
            if (nScore != -32001
                && (nType & (nScore > standPat ? 2 : 1))) {
                bestScore = nScore;
            }
        }
        else {
            bestScore = standPat = board.prevMoveType(ply) != libchess::Move::Type::NONE ? evaluateBoard(board) : -board.staticEval(ply - 1);
        }

        // adjust alpha and beta based on the stand pat
        if (bestScore >= beta) {
			if (!board.found()) {
                node->save(hash, tableScore(bestScore, (ply - rootPly)), 2, -3, 0, standPat);
			}
            return standPat;
        }
        if (PvNode && bestScore > alpha) {
            alpha = bestScore;
        }

        // calculate futility threshold
        futility = bestScore + 200;
    }

    // holds the continuation history from previous moves
    const PieceHistory* contHistory[] = {board.continuationHistory(ply - 1), board.continuationHistory(ply - 2),
                                         nullptr                               , board.continuationHistory(ply - 4),
                                         nullptr                               , board.continuationHistory(ply - 6)};

    libchess::Square prevMoveSq = board.prevMoveType(ply) == libchess::Move::Type::NONE ? libchess::Square(-1) : board.previous_move()->to_square();
    MovePicker picker(board, nMove, &moveHistory, contHistory, depth);

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
                // TODO: change after CLOP tuning
                if (!board.see_ge(move, svq)) {
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

    node->save(hash, tableScore(bestScore, (ply - rootPly)), bestScore >= beta ? 2 : 1, tDepth, bestMove.value(), standPat);
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

    // is the time up?  Mate distance pruning?
    if constexpr (!rootNode) {
        // check for aborted search
        if (stopped.load() || (limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime)) {
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

    if (PvNode && selDepth < (ply - rootPly) + 1) {
        selDepth = (ply - rootPly) + 1;
    }

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

    // represents the best score we have found so far
    int bestScore = -32001;

    // holds the score of the move we just searched
    int score = -32001;

    int moveCounter;
    moveCounter = board.moveCount() = 0;

    // Idea from Stockfish: these variable represent if we are improving our score over our last turn, and how much
    bool improving;
    int improvement;

    // transposition lookup
    uint64_t hash = board.hash();
    Node *node = table.probe(hash, board.found());
    int nDepth = board.found() ? node->nodeDepth : -99;
    int nType = board.found() ? node->nodeTypeGenBound & 0x3 : 0;
    int nScore = board.found() ? scoreFromTable(node->nodeScore, (ply - rootPly), board.halfmoves()) : -32001;
    libchess::Move nMove = board.found() ? libchess::Move(node->bestMove) : libchess::Move(0);
    bool transpositionCapture = board.found() && board.is_capture_move(nMove);

	if (!PvNode
		&& board.found()
        && nScore != -32001
        && excludedMove.value() == 0
		&& nDepth > depth
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
                if (board.prevMoveType(ply) != libchess::Move::Type::NONE && board.moveCount(ply - 1) <= 2 && !board.previously_captured_piece()) {
                    int bonus = -stat_bonus(depth + 1);
                    updateContinuationHistory(board,*board.piece_on(board.previous_move()->to_square()),board.previous_move()->to_square(), bonus,1);
                }

            }
            // penalty for those that don't
            else if (!transpositionCapture) {
                int bonus = -stat_bonus(depth);

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

	// get the static evaluation
    // it won't be needed if in check however
    int staticEval;
    if (check) {
        board.staticEval() = staticEval = 0;
    }
    else if (excludedMove.value() != 0) {
        // if there is an excluded move, its the same position so the static eval is already saved
        staticEval = board.staticEval();
    }
    else if (board.found()) {
        // little check in case something gets messed up
        if (node->nodeEval == -32001) {
            board.staticEval() = staticEval = evaluateBoard(board);
        }
        else {
            board.staticEval() = staticEval = node->nodeEval;
        }

        // previously saved transposition score can be used as a better position evaluation
        if (nScore != -32001
            && (nType & (nScore > staticEval ? 2 : 1))) {
            staticEval = nScore;
        }
    }
    else {
        board.staticEval() = staticEval = evaluateBoard(board);
        node->save(hash, -32001, 0, -99, 0, staticEval);
    }

    // set up the improvement variable, which is the difference in static evals between the current turn and
    // our last turn.  If we were in check the last turn, we try the move prior to that
    if (ply - rootPly >= 2 && board.staticEval(ply - 2) != 0) {
        improvement = board.staticEval() - board.staticEval(ply - 2);
    }
    else if (ply - rootPly >= 4 && board.staticEval(ply - 4) != 0) {
        improvement = board.staticEval() - board.staticEval(ply - 4);
    }
    else {
        improvement = 1; // if we were in check the last two turns, assume we are improving by some amount
    }
    improving = improvement > 0;

    // razoring
    // TODO: change after CLOP tuning
    if (!check
        && staticEval < alpha - rvc - rvs * depth * depth) {
        // verification that the value is indeed less than alpha
        score = quiescence<NonPV>(board, alpha - 1, alpha);
        if (score < alpha) {
            return score;
        }
    }

    // reverse futility pruning
    if (!PvNode
        && !check
        && excludedMove.value() == 0
        && depth < 8
        && staticEval - reverseFutilityMargin(depth, improving) >= beta
        && staticEval >= beta
        && staticEval < 31000) {
        return staticEval;
    }

    // null move pruning
    if (!PvNode
        && !check
        && beta > -30000
        && board.prevMoveType(ply) != libchess::Move::Type::NONE
        && staticEval >= beta
        && staticEval >= board.staticEval()
        && depth >= 2
        && excludedMove.value() == 0
        && nonPawnMaterial(!board.side_to_move(), board)
        && (ply - rootPly) >= minNullPly) {

        // set reduction based on depth, eval, and whether the last move made was tactical
        int R = 4 + depth / 5 + std::min(3, (staticEval - beta) / 50) + (board.is_capture_move(*board.previous_move()) || board.is_promotion_move(*board.previous_move()));

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

            if (minNullPly || depth < 12) {
                return nullScore;
            }

            // verification search at high depths, null pruning disabled until minNullPly is reached
            minNullPly = (ply - rootPly) + 3 * (depth - R) / 4;

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
    // TODO: change after CLOP tuning
    int probCutBeta = beta + pcc - pci * improving;
    if (!PvNode
        && depth > 3
        && !check
        && abs(beta) < 31000
        && !(board.found()
        && nDepth >= depth - 3
        && nScore != -32001
        && nScore < probCutBeta)) {

        // get a move picker
        MovePicker picker(board, nMove, probCutBeta - board.staticEval());

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
                node->save(hash, tableScore(score, (ply - rootPly)), 2, depth - 3, move.value(), board.staticEval());
                cutNodes++;
                return score;
            }

        }
    }

    // Internal Iterative Reduction.  Idea taken from Ethereal.  We lower the depth on cutnodes that are high in the
    // search tree where we expected to find a transposition, but didn't.  This is a modernized approach to Internal Iterative Deepening
    if (cutNode
        && depth >= 7
        && nMove.value() == 0) {
        depth -= 1;
    }

    // holds the continuation history from previous moves
    const PieceHistory *contHistory[] = {board.continuationHistory(ply - 1), board.continuationHistory(ply - 2),
                                         nullptr                               , board.continuationHistory(ply - 4),
                                         nullptr                               , board.continuationHistory(ply - 6)};

    libchess::Move counterMove = board.previous_move() ? counterMoves[board.previous_move()->from_square()][board.previous_move()->to_square()] : libchess::Move(0);

    // get a move picker
    MovePicker picker(board, nMove, killers[ply - rootPly],
                      counterMove,
                      &moveHistory, contHistory);

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
        hist = (*contHistory[0])[board.piece_on(move.from_square())->value()][move.to_square()]
                + (*contHistory[1])[board.piece_on(move.from_square())->value()][move.to_square()]
                + (*contHistory[3])[board.piece_on(move.from_square())->value()][move.to_square()];

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

                int lmrDepth = depth - reductions[depth][moveCounter];

                // futility pruning
                if (lmrDepth <= 8
                    && !check
                    && board.staticEval() + fpc + lmrDepth * fpm <= alpha) {
                        continue;
                }

                // continuation history pruning
                if (lmrDepth <= 5
                    && hist < hpv * depth) {
                        continue;
                    }

                }

        }

        int seeMargin = (!capture && !promotion && !givesCheck) ? smq * depth : smt * depth * depth;

        // see pruning
        if (bestScore > -31000
            && depth <= 8
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
                && depth >= 4 - (rDepth > 21) + 2 * (PvNode && nType == 3)
                && move == nMove
                && excludedMove.value() == 0
                && abs(nScore) < 31000
                && (nType & 2)
                && nDepth >= depth - 3) {
                int singleBeta = nScore - (sec + sem * (nType == 3 && !PvNode)) * depth / sed;
                int singleDepth = (depth - 1) / 2;
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
                     && (*contHistory[0])[board.piece_on(move.from_square())->value()][move.to_square()] >= 4500) {
                extension = 1;
            }

        }

        // add extensions to depth
        actualDepth += extension;

        // update continuation history (must be done after singular extensions)
        board.continuationHistory() = &continuationHistory[check]
                                                          [capture]
                                                          [board.piece_on(move.from_square())->value()]
                                                          [move.to_square()];

        // prefetch before we make a move
        prefetch(table.firstEntry(board.hashAfter(move)));

        movesExplored++;

        // make the move
        board.make_move(move);

        // search with zero window
        // first check if we can reduce
        if (depth >= 2
            && moveCounter > 2
            && (!PvNode     // at PV nodes, we want to make sure we don't reduce tactical moves.  At non-PV nodes, we don't care
            || !capture
            || (cutNode && board.moveCount(ply - 1)  > 1))) {
            // find our reduction
            int reduction = reductions[depth][moveCounter];

            // decrease if position is not likely to fail low
            if (PvNode && !likelyFailLow) {
                reduction--;
            }

            // increase reduction for cut nodes
            if (cutNode) {
                reduction += 2;
            }

            // increase reduction if transposition move is a capture
            if (transpositionCapture) {
                reduction++;
            }

            // decrease reduction for PvNodes based on depth (values from stockfish)
            if (PvNode) {
                reduction -= 1 + 11 / (3 + depth);
            }

            // decrease reduction if the transposition move has been singularly extended
            if (singularQuietLMR) {
                reduction--;
            }

            // decrease reduction if opponent move count is high
            if (board.moveCount(ply - 1) > 7) {
                reduction--;
            }

            // increase on repetition
            if (move == *board.previousMove(ply - 3) && board.is_repeat()) {
                reduction += 2;
            }

            // adjust based on history stats
            hist += moveHistory[board.side_to_move()][move.from_square()][move.to_square()];
            reduction -= std::max(-2, std::min(2, hist / 8192));

            // new depth we will search with
            // we need to search at least one more move
            // replaced clamp with this solution.  actualDepth was very rarely less than 1 (due to negative extensions), causing undefined behavior
            int newDepth = std::max(1, std::min(actualDepth - reduction, actualDepth + 1));

            incPly();
            score = -negamax<NonPV>(board, newDepth, -(alpha + 1), -alpha, true);

            // full depth search when LMR fails high
            if (score > alpha && newDepth < actualDepth - 1) {
                score = -negamax<NonPV>(board, actualDepth, -(alpha + 1), -alpha, !cutNode);
            }
            decPly();
            int bonus = score <= alpha ? -stat_bonus(actualDepth)
                      : score >= beta  ? stat_bonus(actualDepth)
                                       : 0;
            updateContinuationHistory(board, *board.piece_on(move.to_square()), move.to_square(), bonus);
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
        if (stopped.load() || (limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime)) {
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
            if (!capture && quietCount < 64) {
                quietMoves[quietCount++] = move;
            }
        }
    }

    if (moveCounter == 0) {
        bestScore = excludedMove.value() != 0 ? alpha :
                    check                     ? -32000 + (ply - rootPly)
                                              : 0;
    }

    else if (bestMove.value() != 0) {
        updateStatistics(board, bestMove, bestScore, depth, beta, quietMoves, quietCount);
    }

    // bonus for prior move that caused fail low
    else if (!board.previously_captured_piece() && board.previous_move()) {
        int bonus = (depth > 5) + (PvNode || cutNode) + (bestScore < alpha - 25 * depth) + (board.moveCount(ply - 1) > 10);
        updateContinuationHistory(board, *board.piece_on(board.previous_move()->to_square()), board.previous_move()->to_square(), stat_bonus(depth) * bonus, 1);
    }

    if (excludedMove.value() == 0) {
        node->save(hash, tableScore(bestScore, (ply - rootPly)), bestScore >= beta ? 2 : alphaChange && PvNode ? 3 : 1, depth, bestMove.value(), board.staticEval());
    }
    return bestScore;

}

// template definition so that we can call from UCI.cpp
template int Anduril::negamax<Anduril::Root>(libchess::Position &board, int depth, int alpha, int beta, bool cutNode);

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
                               libchess::Move *quietsSearched, int quietCount) {
    int largerBonus = stat_bonus(depth + 1);
    int bonus = bestScore > beta + 40 ? largerBonus : stat_bonus(depth);
    int orderPenalty;
    libchess::Piece movedPiece = *board.piece_on(bestMove.from_square());
    libchess::PieceType capturedPiece(0);

    if (!board.is_capture_move(bestMove)) {

        updateQuietStats(board, bestMove, bonus);

        // decrease stats for non-best quiets
        for (int i = 0; i < quietCount; i++) {
            orderPenalty = -bonus - stat_bonus(quietCount - i);
            updateContinuationHistory(board, *board.piece_on(quietsSearched[i].from_square()), quietsSearched[i].to_square(), orderPenalty);
            moveHistory[board.side_to_move()][quietsSearched[i].from_square()][quietsSearched[i].to_square()] += orderPenalty - moveHistory[board.side_to_move()][quietsSearched[i].from_square()][quietsSearched[i].to_square()] * abs(orderPenalty) / maxHistoryVal;
        }
    }

    // extra penalty for early move that was not a transposition or main killer in previous ply
    if (board.previous_move() && ((board.moveCount(ply - 1) == 1 + board.found(ply - 1) || *board.previous_move() == killers[ply - rootPly - 1][0])) && !board.previously_captured_piece()) {
        updateContinuationHistory(board, movedPiece, bestMove.to_square(), -largerBonus, -1);
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
            //(*board.continuationHistory(ply - i))[piece.value()][to] = std::clamp((*board.continuationHistory(ply - i))[piece.value()][to] + bonus, -UCI::maxContinuationVal, UCI::maxContinuationVal);
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
        libchess::Move tmp(node->bestMove);
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

// gets the attacks by a team of a certain piece
template<Anduril::PieceType pt, bool white>
libchess::Bitboard Anduril::attackByPiece(libchess::Position &board) {
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