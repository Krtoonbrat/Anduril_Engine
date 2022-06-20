//
// Created by Hughe on 4/1/2022.
//

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

#include "Anduril.h"
#include "ConsoleGame.h"
#include "ZobristHasher.h"

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

        // we want to search castling before the equal captures
        if (moves.moves[i].special >= thc::SPECIAL_WK_CASTLING && moves.moves[i].special <= thc::SPECIAL_BQ_CASTLING) {
            movesWithScores.emplace_back(50, moves.moves[i]);
            continue;
        }

        // next check for killer moves
        if (std::count(killers[ply - rootPly].begin(), killers[ply - rootPly].end(), moves.moves[i]) != 0) {
            movesWithScores.emplace_back(-10, moves.moves[i]);
            continue;
        }

        // next check the history array
        if (history[board.WhiteToPlay()][pieceIndex[board.squares[static_cast<int>(moves.moves[i].src)]]][static_cast<int>(moves.moves[i].dst)] != 0) {
            movesWithScores.emplace_back(std::min(-history[board.WhiteToPlay()][pieceIndex[board.squares[static_cast<int>(moves.moves[i].src)]]][static_cast<int>(moves.moves[i].dst)], -15), moves.moves[i]);
        }

        // the move isn't special, mark it to be searched with every other non-capture
        // the value of -1000 was chosen because it makes sure the moves are searched after killers and losing captures
        movesWithScores.emplace_back(-1000, moves.moves[i]);
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

    // current ply (white moves + black moves from the starting position)
    int ply = board.WhiteToPlay() ? (board.full_move_count * 2) - 1 : (board.full_move_count * 2);

    // transposition lookup
    uint64_t hash = Zobrist::zobristHash(board);
    bool found = false;
    Node *node = table.probe(hash, found);
    if (found) {
        movesTransposed++;
        if (!PvNode && node->nodeDepth >= -(ply + Qdepth)) {
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
        node->nodeDepth = -(ply + Qdepth);
    }
    else {
        // reset the node information so that we can rewrite it
        // this isn't the best way to do it, but imma do it anyways
        node->nodeScore = bestScore; // bestScore is already set to our "replace me" flag
        node->nodeType = 3;
        node->bestMove.Invalid();
        node->key = hash;
        node->nodeDepth = -(ply + Qdepth);
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

        // delta pruning
        if (standPat + abs(pieceValues[board.squares[move.dst]]) + 200 < alpha) {
            continue;
        }

        makeMove(board, move);

        movesExplored++;
        quiesceExplored++;
        score = -quiesce<nodeType>(board, -beta, -alpha, Qdepth - 1);
        undoMove(board, move);

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

    // check for draw
    if (isDraw(board)) {
        return 0;
    }

    // are we in check?
    int check = board.WhiteToPlay() ? board.AttackedPiece(board.wking_square) : board.AttackedPiece(board.bking_square);

    // extend the search by one if we are in check
    // this makes sure we don't enter a qsearch while in check
    if (check && depth == 0) {
        depth++;
    }

    // if we are at max depth, start a quiescence search
    if (depth <= 0){
        return quiesce<PvNode ? PV : NonPV>(board, alpha, beta, 6);
    }

    // represents our next move to search
    thc::Move move;

    // represents the best score we have found so far
    int bestScore = -999999999;

    // holds the score of the move we just searched
    int score = -999999999;

    // current ply (white moves + black moves from the starting position)
    int ply = board.WhiteToPlay() ? (board.full_move_count * 2) - 1 : (board.full_move_count * 2);

    // transposition lookup
    uint64_t hash = Zobrist::zobristHash(board);
    bool found = false;
    Node *node = table.probe(hash, found);
    if (found) {
        movesTransposed++;
        if (!PvNode && node->nodeDepth >= ply + depth) {
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
        node->nodeDepth = ply + depth;
    }
    else {
        // reset the node information so that we can rewrite it
        // this isn't the best way to do it, but imma do it anyways
        node->nodeScore = bestScore; // bestScore is already set to our "replace me" flag
        node->nodeType = 3;
        node->bestMove.Invalid();
        node->key = hash;
        node->nodeDepth = ply + depth;
    }

    // get the static evaluation
    // it won't be needed if in check however
    int staticEval = check ? 999999999 : evaluateBoard(board);

    // razoring
    // weird magic values stolen straight from stockfish
    if (!PvNode
        && !check
        && depth <= 7
        && staticEval < alpha - 348 - 258 * depth * depth) {
        score = quiesce<NonPV>(board, alpha - 1, alpha, 6);
        if (score < alpha) {
            return score;
        }
    }

    // null move pruning
    if (!PvNode
        && nullAllowed
        && board.getMoveFromStack(ply - 1).Valid()
        && !check
        && staticEval >= beta
        && depth >= 4
        && nonPawnMaterial(board.WhiteToPlay()) > 1300) {

        // we pass an invalid move to make move to represent a passing turn
        thc::Move nullMove;
        nullMove.Invalid();

        nullAllowed = false;

        makeMove(board, nullMove);
        int nullScore = -negamax<NonPV>(board, depth - 3, -beta, -beta + 1);
        undoMove(board, nullMove);

        nullAllowed = true;

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

    // probcut
    // if we find a position that returns a cutoff when beta is padded a little, we can assume
    // the position would cut at a full depth search as well
    int probCutBeta = beta + 350;
    if (!PvNode
        && depth > 4
        && !check
        && !(found
        && node->nodeDepth >= depth - 3
        && node->nodeScore != -999999999
        && node->nodeScore < probCutBeta)) {

        // we loop through all the moves to try and find one that
        // causes a beta cut on a reduced search
        for (int i = 0; i < moveList.size(); i++) {
            move = pickNextMove(moveList, i);

            makeMove(board, move);

            // perform a qsearch to verify that the move still is less than beta
            score = -quiesce<NonPV>(board, -probCutBeta, -probCutBeta + 1, 6);

            // if the qsearch holds, do a regular one
            if (score >= probCutBeta) {
                score = -negamax<NonPV>(board, depth - 4, -probCutBeta, -probCutBeta + 1);
            }

            undoMove(board, move);

            if (score >= probCutBeta) {
                // write to the node if we have better information
                if (!(found
                    && node->nodeDepth >= depth - 3
                    && node->nodeScore != -999999999)) {
                    node->nodeDepth = depth - 3;
                    node->nodeScore = score;
                    node->nodeType = 2;
                    node->bestMove = move;
                }
                cutNodes++;
                return score;
            }

        }
    }

    // decide if we can use futility pruning
    // if the eval is below (alpha - margin), searching non-tactical moves at lower
    // depths wastes time, so we set a flag to prune them
    bool futile = false;
    if (!PvNode
        && depth <= 3
        && !check
        && abs(alpha) < 9000     // this condition makes sure we aren't thinking about mate
        && staticEval + margin[depth] <= alpha) {
        futile = true;
    }




    // do we need to search a move with the full depth?
    bool doFullSearch = false;

    // loop through the possible moves and score each
    for (int i = 0; i < moveList.size(); i++) {
        move = pickNextMove(moveList, i);

        movesExplored++;

        // futility pruning
        if (futile
            && move.capture == ' '
            && !(move.special >= thc::SPECIAL_PROMOTION_QUEEN && move.special <= thc::SPECIAL_PROMOTION_KNIGHT)) {
            // there is one check we need to do before pruning that requires the move to
            // be made.  I put it separately so that we don't make the move and unmake it
            // every single iteration
            makeMove(board, move);
            if (!(board.WhiteToPlay() ? board.AttackedPiece(board.wking_square) : board.AttackedPiece(board.bking_square))) {
                undoMove(board, move);
                continue;
            }
            undoMove(board, move);
        }


        // search with zero window
        // first check if we can reduce
        doFullSearch = false;
        if (depth > 1 && i > 2 && isLateReduction(board, move)) {
            makeMove(board, move);
            score = -negamax<NonPV>(board, depth - 2, -(alpha + 1), -alpha);
            doFullSearch = score > alpha && score < beta;
            undoMove(board, move);
        }
        else {
            doFullSearch = !PvNode || i > 0;
        }
        if (doFullSearch) {
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
                        history[board.WhiteToPlay()][pieceIndex[board.squares[static_cast<int>(move.src)]]][board.squares[static_cast<int>(move.dst)]] += depth*depth;
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

// calls negamax and keeps track of the best move
void Anduril::go(thc::ChessRules &board, int depth) {
    thc::Move bestMove;
    bestMove.Invalid();

    // start the clock
    auto start = std::chrono::high_resolution_clock::now();

    // this is for debugging
    std::string boardFENs = board.ForsythPublish();
    char *boardFEN = &boardFENs[0];

    int alpha = -999999999;
    int beta = 999999999;
    int bestScore = -999999999;

    // set up the piece lists
    clearPieceLists();
    fillPieceLists(board);

    // set the killer vector to have the correct number of slots
    // and the root node's ply
    // the vector is padded a little at the end in case of the search being extended
    killers = std::vector<std::vector<thc::Move>>(depth + 5);
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
            deep--;
            movesExplored++;
            depthNodes++;
            score = -negamax<PV>(board, deep, -beta, -alphaTheSecond);
            deep++;
            undoMove(board, move);

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

        // set the aspiration window
        // we only actually use the aspiration window at depths greater than 4.
        // this is because we don't have a decent picture of the position yet
        // and the search falls outside the window often causing instability
        if (deep > 5) {
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


        // for debugging
        if (boardFEN != board.ForsythPublish()) {
            std::cout << "Board does not match original at depth: " << deep << std::endl;
            board.Forsyth(boardFEN);
            clearPieceLists();
            fillPieceLists(board);
        }


        // reset the variables to prepare for the next loop
        if (!finalDepth) {
            bestScore = -999999999;
            alphaTheSecond = -999999999;
            score = -999999999;
        }
    }

    // stop the clock
    auto end = std::chrono::high_resolution_clock::now();

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
    std::chrono::duration<double, std::milli> timeElapsed = end - start;
    std::cout << "Time spent searching: " << timeElapsed.count() / 1000 << " seconds" << std::endl;
    std::cout << "Nodes per second: " << getMovesExplored() / (timeElapsed.count()/1000) << std::endl;
    setMovesExplored(0);
    cutNodes = 0;
    movesTransposed = 0;
    quiesceExplored = 0;
    clearPieceLists();

    board.PlayMove(bestMove);
}

bool Anduril::isDraw(thc::ChessRules &board) {
    // first check for 50 move rule
    if (board.half_move_clock >= 100) {
        return true;
    }

    // next check for repetition
    uint64_t currentHash = Zobrist::zobristHash(board);
    int repetitions = 0;
    for (int i = 0; i < positionStack.size(); i++) {
        if (currentHash == positionStack[i]) {
            repetitions++;
        }
    }

    if (repetitions >= 3) {
        return true;
    }

    return false;
}

int Anduril::nonPawnMaterial(bool whiteToPlay) {
    if (whiteToPlay) {
        return whiteKnights.size() * 300 + whiteBishops.size() * 300 + whiteRooks.size() * 500 + whiteQueens.size() * 900;
    }
    else {
        return blackKnights.size() * 300 + blackBishops.size() * 300 + blackRooks.size() * 500 + blackQueens.size() * 900;
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

    // if the move was null, push it and skip the rest
    if (!move.Valid()) {
        board.PushMove(move);
        positionStack.push_back(Zobrist::zobristHash(board));
        return;
    }

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

    positionStack.push_back(Zobrist::zobristHash(board));
}

void Anduril::undoMove(thc::ChessRules &board, thc::Move move) {
    int src = static_cast<int>(move.src);
    int dst = static_cast<int>(move.dst);

    board.PopMove(move);

    positionStack.pop_back();

    // if the move was null, skip the rest
    if (!move.Valid()) {
        return;
    }

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
    bool found = true;

    // push the best move and add to the PV
    PV.push_back(bestMove);
    board.PushMove(bestMove);

    // This loop hashes the board, finds the node associated with the current board
    // adds the best move we found to the PV, then pushes it to the board
    hash = Zobrist::zobristHash(board);
    node = table.probe(hash, found);
    while (found && node->bestMove.src != node->bestMove.dst) {
        PV.push_back(node->bestMove);
        board.PushMove(node->bestMove);
        hash = Zobrist::zobristHash(board);
        node = table.probe(hash, found);
        // in case of infinite loops of repeating moves
        // I just capped it at 9 because I honestly don't care about lines
        // calculated that long anyways
        if (PV.size() > 9) {
            break;
        }
    }

    // undoes each move in the PV so that we don't have a messed up board
    for (int i = PV.size() - 1; i >= 0; i--){
        board.PopMove(PV[i]);
    }

    return PV;
}