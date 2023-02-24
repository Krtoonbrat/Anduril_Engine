//
// Created by Hughe on 4/1/2022.
//

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

#include "Anduril.h"
#include "ConsoleGame.h"
#include "UCI.h"

// Most Valuable Victim, Least Valuable Attacker
int Anduril::MVVLVA(libchess::Position &board, libchess::Square src, libchess::Square dst) {
    return std::abs(pieceValues[board.piece_on(dst)->to_char()]) - std::abs(pieceValues[board.piece_on(src)->to_char()]);
}

// generates a move list in the order we want to search them
std::vector<std::tuple<int, libchess::Move>> Anduril::getMoveList(libchess::Position &board, Node *node) {
    // this is the list of moves with their assigned scores
    std::vector<std::tuple<int, libchess::Move>> movesWithScores;

    // get a list of possible moves
    libchess::MoveList moves = board.pseudo_legal_move_list();

    // find pieces threatened by pieces of lesser value
    libchess::Bitboard pawnThreat, minorThreat, rookThreat, threatenedPieces;
    int threatValue = 0;

    if (!board.side_to_move()) {
        pawnThreat = attackByPiece<PAWN, false>(board);
        minorThreat = attackByPiece<KNIGHT, false>(board) | attackByPiece<BISHOP, false>(board) | pawnThreat;
        rookThreat = attackByPiece<ROOK, false>(board) | minorThreat;
    }
    else {
        pawnThreat = attackByPiece<PAWN, true>(board);
        minorThreat = attackByPiece<KNIGHT, true>(board) | attackByPiece<BISHOP, true>(board) | pawnThreat;
        rookThreat = attackByPiece<ROOK, true>(board) | minorThreat;
    }

    threatenedPieces =  (board.piece_type_bb(libchess::constants::QUEEN, board.side_to_move()) & rookThreat)
                      | (board.piece_type_bb(libchess::constants::ROOK, board.side_to_move()) & minorThreat)
                      | ((board.piece_type_bb(libchess::constants::KNIGHT, board.side_to_move()) | board.piece_type_bb(libchess::constants::BISHOP, board.side_to_move())) & pawnThreat);

    // loop through the board, give each move a score, and add it to the list
    for (auto i = moves.begin(); i < moves.end(); i++) {
        // first check if the move is a hash move
        if (node != nullptr && *i == node->bestMove) {
            movesWithScores.emplace_back(100000, *i);
            continue;
        }

        // is the piece threatened?
        threatValue = 0;
        if (threatenedPieces & libchess::lookups::square(i->from_square())) {
            switch (*board.piece_type_on(i->from_square())) {
                case libchess::constants::QUEEN:
                    threatValue = !(libchess::lookups::square(i->to_square()) & rookThreat) ? 10 : 0;
                    break;
                case libchess::constants::ROOK:
                    threatValue = !(libchess::lookups::square(i->to_square()) & minorThreat) ? 5 : 0;
                    break;
                case libchess::constants::BISHOP:
                case libchess::constants::KNIGHT:
                    threatValue = !(libchess::lookups::square(i->to_square()) & pawnThreat) ? 3 : 0;
            }
        }

        // next check if the move is a capture
        if (board.is_capture_move(*i)) {
            movesWithScores.emplace_back(board.see_for(*i, seeValues) + threatValue + 50000, *i);
            continue;
        }

        // we want to search castling before the equal captures
        // this seemed to fix a weird issue with the engine preferring to just move the king
        // manually instead of castling
        if (i->type() == libchess::Move::Type::CASTLING) {
            movesWithScores.emplace_back(50050, *i);
            continue;
        }

        // next check for killer moves
        // the value of -1 makes sure the killers are searched directly after equal captures
        if (std::count(killers[ply - rootPly].begin(), killers[ply - rootPly].end(), *i) != 0) {
            movesWithScores.emplace_back(49999, *i);
            continue;
        }

        // next check for counter moves.  Value of -2 makes sure they are placed directly after killers
        if (counterMoves[board.previous_move()->from_square()][board.previous_move()->to_square()] == *i) {
            movesWithScores.emplace_back(49998, *i);
        }

        // the move isn't special, mark it to be searched with every other non-capture
        // the value of -1000 was chosen because it makes sure the moves are searched after killers and losing captures
        movesWithScores.emplace_back(-1000 + threatValue +
                                    moveHistory[board.side_to_move()][i->from_square()][i->to_square()], *i);
    }

    return movesWithScores;
}

// generates a move list of legal moves we want to search
// this one checks for legality when generating moves.  This is because at root the extra time to check is negligent (that's my guess at least)
// also uses see instead of MVVLVA because we only have to generate these moves once and the extra accuracy could help the search and the extra time
// is small enough to justify (again because its only run once per search)
std::vector<std::tuple<int, libchess::Move>> Anduril::getMoveList(libchess::Position &board) {
    // this is the list of moves with their assigned scores
    std::vector<std::tuple<int, libchess::Move>> movesWithScores;

    // get a list of possible moves
    libchess::MoveList moves = board.legal_move_list();

    // find pieces threatened by pieces of lesser value
    libchess::Bitboard pawnThreat, minorThreat, rookThreat, threatenedPieces;
    int threatValue = 0;

    if (!board.side_to_move()) {
        pawnThreat = attackByPiece<PAWN, false>(board);
        minorThreat = attackByPiece<KNIGHT, false>(board) | attackByPiece<BISHOP, false>(board) | pawnThreat;
        rookThreat = attackByPiece<ROOK, false>(board) | minorThreat;
    }
    else {
        pawnThreat = attackByPiece<PAWN, true>(board);
        minorThreat = attackByPiece<KNIGHT, true>(board) | attackByPiece<BISHOP, true>(board) | pawnThreat;
        rookThreat = attackByPiece<ROOK, true>(board) | minorThreat;
    }

    threatenedPieces =  (board.piece_type_bb(libchess::constants::QUEEN, board.side_to_move()) & rookThreat)
                        | (board.piece_type_bb(libchess::constants::ROOK, board.side_to_move()) & minorThreat)
                        | ((board.piece_type_bb(libchess::constants::KNIGHT, board.side_to_move()) | board.piece_type_bb(libchess::constants::BISHOP, board.side_to_move())) & pawnThreat);


    // loop through the board, give each move a score, and add it to the list
    for (auto i = moves.begin(); i < moves.end(); i++) {
        // is the piece threatened?
        threatValue = 0;
        if (threatenedPieces & libchess::lookups::square(i->from_square())) {
            switch (*board.piece_type_on(i->from_square())) {
                case libchess::constants::QUEEN:
                    threatValue = !(libchess::lookups::square(i->to_square()) & rookThreat) ? 10 : 0;
                    break;
                case libchess::constants::ROOK:
                    threatValue = !(libchess::lookups::square(i->to_square()) & minorThreat) ? 5 : 0;
                    break;
                case libchess::constants::BISHOP:
                case libchess::constants::KNIGHT:
                    threatValue = !(libchess::lookups::square(i->to_square()) & pawnThreat) ? 3 : 0;
            }
        }

        // check if the move is a capture
        if (board.is_capture_move(*i)) {
            movesWithScores.emplace_back(board.see_for(*i, seeValues) + threatValue + 50000, *i);
            continue;
        }

        // we want to search castling before the equal captures
        if (i->type() == libchess::Move::Type::CASTLING) {
            movesWithScores.emplace_back(50050, *i);
            continue;
        }

        // next check for killer moves
        // the value of -1 makes sure the killers are searched directly after equal captures
        if (std::count(killers[ply - rootPly].begin(), killers[ply - rootPly].end(), *i) != 0) {
            movesWithScores.emplace_back(49999, *i);
            continue;
        }

        // next check for counter moves.  Value of -2 makes sure they are placed directly after killers
        if (counterMoves[board.previous_move()->from_square()][board.previous_move()->to_square()] == *i) {
            movesWithScores.emplace_back(49998, *i);
        }

        // the move isn't special, mark it to be searched with every other non-capture
        // the value of -1000 was chosen because it makes sure the moves are searched after killers and losing captures
        movesWithScores.emplace_back(-1000 + threatValue +
                                     moveHistory[board.side_to_move()][i->from_square()][i->to_square()], *i);
    }

    return movesWithScores;
}

std::vector<std::tuple<int, libchess::Move>> Anduril::getQMoveList(libchess::Position &board, Node *node) {
    // if we are in check, we need to generate evasions.  These are better sorted with the regular function
    if (board.in_check()) {
        return getMoveList(board, node);
    }

    // this is the list of moves with their assigned scores
    std::vector<std::tuple<int, libchess::Move>> movesWithScores;

    // get a list of possible moves
    libchess::MoveList moves;
    board.generate_capture_moves(moves, board.side_to_move());

    // find pieces threatened by pieces of lesser value
    libchess::Bitboard pawnThreat, minorThreat, rookThreat, threatenedPieces;
    int threatValue = 0;

    if (!board.side_to_move()) {
        pawnThreat = attackByPiece<PAWN, false>(board);
        minorThreat = attackByPiece<KNIGHT, false>(board) | attackByPiece<BISHOP, false>(board) | pawnThreat;
        rookThreat = attackByPiece<ROOK, false>(board) | minorThreat;
    }
    else {
        pawnThreat = attackByPiece<PAWN, true>(board);
        minorThreat = attackByPiece<KNIGHT, true>(board) | attackByPiece<BISHOP, true>(board) | pawnThreat;
        rookThreat = attackByPiece<ROOK, true>(board) | minorThreat;
    }

    threatenedPieces =  (board.piece_type_bb(libchess::constants::QUEEN, board.side_to_move()) & rookThreat)
                        | (board.piece_type_bb(libchess::constants::ROOK, board.side_to_move()) & minorThreat)
                        | ((board.piece_type_bb(libchess::constants::KNIGHT, board.side_to_move()) | board.piece_type_bb(libchess::constants::BISHOP, board.side_to_move())) & pawnThreat);


    for (auto i = moves.begin(); i < moves.end(); i++) {
        // first check if the move is a capture
        if (board.is_capture_move(*i)) {
            // next check if it's the hash move
            if (node != nullptr && *i == node->bestMove) {
                movesWithScores.emplace_back(100000, *i);
                continue;
            }

            // is the piece threatened?
            threatValue = 0;
            if (threatenedPieces & libchess::lookups::square(i->from_square())) {
                switch (*board.piece_type_on(i->from_square())) {
                    case libchess::constants::QUEEN:
                        threatValue = !(libchess::lookups::square(i->to_square()) & rookThreat) ? 10 : 0;
                        break;
                    case libchess::constants::ROOK:
                        threatValue = !(libchess::lookups::square(i->to_square()) & minorThreat) ? 5 : 0;
                        break;
                    case libchess::constants::BISHOP:
                    case libchess::constants::KNIGHT:
                        threatValue = !(libchess::lookups::square(i->to_square()) & pawnThreat) ? 3 : 0;
                }
            }

            // if it isn't a hash move, add it with its MVVLVA score
            movesWithScores.emplace_back(board.see_for(*i, seeValues) + threatValue + 50000, *i);
        }
    }

    return movesWithScores;
}

// the quiescence search
// searches possible captures to make sure we aren't mis-evaluating certain positions
template <Anduril::NodeType nodeType>
int Anduril::quiescence(libchess::Position &board, int alpha, int beta) {
    movesExplored++;
    constexpr bool PvNode = nodeType != NonPV;

    // did we receive a stop command?
    if (movesExplored % 5000 == 0) {
        UCI::ReadInput(*this);
    }

    // is the time up?
    if (stopped || (limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime)) {
        return 0;
    }

    // check for a game over
    switch (board.game_state()) {
        case libchess::Position::GameState::CHECKMATE:
            return -32000 + (ply - rootPly);
        case libchess::Position::GameState::STALEMATE:
        case libchess::Position::GameState::THREEFOLD_REPETITION:
        case libchess::Position::GameState::FIFTY_MOVES:
            return 0;
        case libchess::Position::GameState::IN_PROGRESS:
            break;
    }

    // represents the best score we have found so far
    int bestScore = -32001;

    // transposition lookup
    uint64_t hash = board.hash();
    bool found = false;
    Node *node = table.probe(hash, found);
	if (!PvNode
		&& found
		&& node->nodeDepth >= -1
        && (node->nodeType == 1 || (node->nodeScore >= beta ? node->nodeType == 2 : node->nodeType == 3))) {
		movesTransposed++;
		return node->nodeScore;
	}

    // are we in check?
    bool check = board.in_check();


    // get a move list
    std::vector<std::tuple<int, libchess::Move>> moveList;

    int standPat = -32001;
    // we can't stand pat if we are in check
    if (!check) {
        // stand pat score to see if we can exit early
        if (found) {
            if (node->nodeEval != -32001) {
                standPat = node->nodeEval;
            }
            else {
                standPat = evaluateBoard(board);
            }
        }
        else {
            standPat = evaluateBoard(board);
        }
        bestScore = standPat;

        // adjust alpha and beta based on the stand pat
        if (standPat >= beta) {
			if (!found) {
                node->save(hash, standPat, 2, -1, libchess::Move(0), age, standPat);
			}
            return standPat;
        }
        if (PvNode && standPat > alpha) {
            alpha = standPat;
        }

        // generate a move list
        if (found) {
            moveList = getQMoveList(board, node);
        }
        else {
            moveList = getQMoveList(board, nullptr);
        }

        // if there are no moves, we can just return our stand pat
        if (moveList.empty()){
            return standPat;
        }
    }
        // if we are in check, we need to generate all the possible evasions and search them
    else {
        // generate a move list
        if (found) {
            moveList = getMoveList(board, node);
        }
        else {
            moveList = getMoveList(board, nullptr);
        }

        // if there are no moves, we can just return our stand pat
        if (moveList.empty()){
            return standPat;
        }
    }

    // arbitrarily low score to make sure its replaced
    int score = -32001;

    // holds the move we want to search
    libchess::Move move;
    libchess::Move bestMove;

    // loop through the moves and score them
    for (int i = 0; i < moveList.size(); i++) {
        move = pickNextMove(moveList, i);

        // search only legal moves
        if (!board.is_legal_move(move)) {
            continue;
        }

        // just in case we are searching evasions
        if (board.is_capture_move(move)) {
            // don't search moves with negative see
            if (board.see_for(move, seeValues) < 0) {
                continue;
            }

            // delta pruning
            if (standPat + abs(pieceValues[board.piece_on(move.to_square())->to_char()]) + 200 < alpha
                && nonPawnMaterial(board.side_to_move(), board) -
                   abs(pieceValues[board.piece_on(move.to_square())->to_char()]) > 1300
                && move.type() != libchess::Move::Type::PROMOTION) {
                continue;
            }
        }

        board.make_move(move);

        quiesceExplored++;
        incPly();
        score = -quiescence<nodeType>(board, -beta, -alpha);
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

    node->save(hash, bestScore, bestScore >= beta ? 2 : 3, -1, bestMove, age, standPat);
    return bestScore;

}

// the negamax function.  Does the heavy lifting for the search
template <Anduril::NodeType nodeType>
int Anduril::negamax(libchess::Position &board, int depth, int alpha, int beta, bool cutNode) {
    constexpr bool PvNode = nodeType != NonPV;
    constexpr bool rootNode = nodeType == Root;

    // did we receive a stop command?
    if (!rootNode && movesExplored % 5000 == 0) {
        UCI::ReadInput(*this);
    }

    // is the time up?  Mate distance pruning?
    if (!rootNode) {
        // check for aborted search
        if (stopped || (limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime)) {
            return 0;
        }

        // Mate distance pruning.  If we mate at the next move, our score would be mate in current ply + 1.  If alpha
        // is already bigger because we found a shorter mate further up, there is no need to search because we will
        // never beat the current alpha.  Same logic but reversed applies to beta.  If the mate distance is higher
        // than a line that we have found, we return a fail-high score.
        alpha = std::max(-32000 + (ply - rootPly), alpha);
        beta = std::min(32000 - (ply - rootPly), beta);
        if (alpha >= beta) {
            return alpha;
        }
    }

    // check for a game over
    switch (board.game_state()) {
        case libchess::Position::GameState::CHECKMATE:
            return -32000 + (ply - rootPly);
        case libchess::Position::GameState::STALEMATE:
        case libchess::Position::GameState::THREEFOLD_REPETITION:
        case libchess::Position::GameState::FIFTY_MOVES:
            return 0;
        case libchess::Position::GameState::IN_PROGRESS:
            break;
    }

    // are we in check?
    int check = board.in_check();

    // did alpha change?
    bool alphaChange = false;

    // extend the search by one if we are in check
    // depth condition makes sure the search doesn't explode in endgames where constant check is common
    if (check && depth <= 8) {
        depth++;
    }

    // if we are at max depth, start a quiescence search
    if (depth <= 0){
        return quiescence<PvNode ? PV : NonPV>(board, alpha, beta);
    }

    // increment counters now that we know we aren't going into quiescence
    depthNodes++;
    movesExplored++;

    // represents our next move to search
    libchess::Move move;
    libchess::Move bestMove = move;

    // represents the best score we have found so far
    int bestScore = -32001;

    // holds the score of the move we just searched
    int score = -32001;

    // transposition lookup
    uint64_t hash = board.hash();
    bool found = false;
    Node *node = table.probe(hash, found);
    int nDepth = found ? node->nodeDepth : -99;
    int nType = found ? node->nodeType : -1;
    int nScore = found ? node->nodeScore : -32001;
    libchess::Move nMove = found ? node->bestMove : libchess::Move(0);

	if (!PvNode
		&& found
		&& nDepth > depth - (nType == 1)
        && (nType == 1 || (nScore >= beta ? nType == 2 : nType == 3))) {
        movesTransposed++;

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
        staticEval = 32001;
    }
    else if (found) {
        // little check in case something gets messed up
        if (node->nodeEval == -32001) {
            staticEval = evaluateBoard(board);
        }
        else {
            staticEval = node->nodeEval;
        }
    }
    else {
        staticEval = evaluateBoard(board);
        node->save(hash, -32001, -1, -99, libchess::Move(0), 0, staticEval);
    }

    // razoring
    // weird magic values stolen straight from stockfish
    if (!check
        && staticEval < alpha - 258 * depth * depth) {
        incPly();
        score = quiescence<NonPV>(board, alpha - 1, alpha);
        decPly();
        if (score < alpha) {
            return score;
        }
    }

    // reverse futility pruning
    if (!PvNode
        && !check
        && depth < 5
        && staticEval - reverseMargin[depth] >= beta) {
        return staticEval;
    }

    // null move pruning
    if (!PvNode
        && nullAllowed
        && !check
        && staticEval >= beta
        && depth >= 3
        && nonPawnMaterial(!board.side_to_move(), board) > 1300) {

        nullAllowed = false;

        int R = depth / 2 + depth % 2;

        board.make_null_move();
        incPly();
        int nullScore = -negamax<NonPV>(board, depth - R, -beta, -beta + 1, !cutNode);
        decPly();
        board.unmake_move();

        nullAllowed = true;

        if (nullScore >= beta) {
            return nullScore;
        }
    }
    // need to set this to true here because we can't razor if we just made a null move
    // but the flag needs to be reset for the next node
    nullAllowed = true;


    // generate a move list
    std::vector<std::tuple<int, libchess::Move>> moveList;
    if constexpr(rootNode) {
        moveList = rootMoves;
    }
    else if (found) {
        moveList = getMoveList(board, node);
    }
    else {
        moveList = getMoveList(board, nullptr);
    }

    // probcut
    // if we find a position that returns a cutoff when beta is padded a little, we can assume
    // the position would most likely cut at a full depth search as well
    int probCutBeta = beta + 300;
    if (!PvNode
        && depth > 4
        && !check
        && !(found
        && nDepth >= depth - 3
        && nScore != -32001
        && nScore < probCutBeta)) {

        // we loop through all the moves to try and find one that
        // causes a beta cut on a reduced search
        for (int i = 0; i < moveList.size(); i++) {
            move = pickNextMove(moveList, i);

            // search only legal moves
            if (!board.is_legal_move(move)) {
                continue;
            }

            // only search captures that can improve the position
            if (!board.is_capture_move(move) || board.see_for(move, seeValues) < probCutBeta - staticEval) {
                continue;
            }

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
                // write to the node if we have better information
                if (!(found
                    && nDepth >= depth - 3
                    && nScore != -32001)) {
                    node->save(hash, score, 2, depth - 3, move, age, staticEval);

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
        && depth <= 4
        && !check
        && abs(alpha) < 9000     // this condition makes sure we aren't thinking about mate
        && staticEval + margin[depth] <= alpha) {
        futile = true;
    }

    // indicates that a PvNode will probably fail low if the node was searched, and we found a fail low already
    bool likelyFailLow = PvNode && nMove.value() != 0 && nType == 3;

    // loop through the possible moves and score each
    for (int i = 0; i < moveList.size(); i++) {
        move = pickNextMove(moveList, i);

        // search only legal moves
        if (!board.is_legal_move(move)) {
            continue;
        }

        // if we are at root, give the gui some information
        if constexpr (rootNode) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = now - startTime;
            if (elapsed.count() >= 1000 && depth > 5) {
                std::cout << "info currmove " << move.to_str() << " currmovenumber " << i + 1 << std::endl;
            }
        }

        // futility pruning
        if (futile
            && move.type() != libchess::Move::Type::CAPTURE
            && move.type() != libchess::Move::Type::PROMOTION) {
            // there is one check we need to do before pruning that requires the move to
            // be made.  I put it separately so that we don't make the move and unmake it
            // every single iteration
            board.make_see_move(move);
            if (!board.in_check()) {
                board.unmake_move();
                continue;
            }
            board.unmake_move();
        }


        // search with zero window
        // first check if we can reduce
        if (depth > 1 && i > 2 && isLateReduction(board, move)) {
            // find our reduction
            int reduction = 4;
            // decrease if position is not likely to fail low
            if (PvNode && !likelyFailLow) {
                reduction--;
            }

            // increase reduction for cut nodes
            if (cutNode) {
                reduction += 2;
            }

            // increase reduction if transposition move is a capture
            if (board.is_capture_move(nMove)) {
                reduction++;
            }

            // decrease reduction for PvNodes based on depth (values from stockfish)
            if (PvNode) {
                reduction -= 1 + 11 / (3 + depth);
            }

            // increase reduction for later moves
            if (i > 6 && depth >= 3) {
                reduction += depth / 3;
            }

            // new depth we will search with
            // we need to search at least one more move, we will also allow an "extension" of up to one ply by passing
            // in depth unchanged if the reduction returns as 0 or less
            int newDepth = std::clamp(depth - reduction, 1, depth);

            board.make_move(move);
            incPly();
            score = -negamax<NonPV>(board, newDepth, -(alpha + 1), -alpha, true);

            // full depth search when LMR fails high
            if (score > alpha && newDepth < depth - 1) {
                score = -negamax<NonPV>(board, depth - 1, -(alpha + 1), -alpha, !cutNode);
            }
            decPly();
            board.unmake_move();
        }
        else if (!PvNode || i > 0) {
            board.make_move(move);
            incPly();
            score = -negamax<NonPV>(board, depth - 1, -(alpha + 1), -alpha, !cutNode);
            decPly();
            board.unmake_move();
        }

        // full PV search for the first move and for moves that fail in the zero window
        if (PvNode && (i == 0 || (score > alpha && (rootNode || score < beta)))) {
            board.make_move(move);
            incPly();
            score = -negamax<PV>(board, depth - 1, -beta, -alpha, false);
            decPly();
            board.unmake_move();
        }

        // if the search was stopped for whatever reason, return immediately
        if (stopped || (limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime)) {
            return 0;
        }

        // if we are at root, update the move score in the rootMoves list
        if constexpr (rootNode) {
            rootMoves[i] = std::tuple<int, libchess::Move>(score, move);
        }

        if (score > bestScore) {
            bestScore = score;
            if (score > alpha) {
                bestMove = move;
                if (PvNode && score < beta) {
                    alpha = score;
                    alphaChange = true;
                }
                else {
                    cutNodes++;
                    if (move.type() == libchess::Move::Type::CAPTURE) {
                        insertKiller(move, depth);
                        counterMoves[board.previous_move()->from_square()][board.previous_move()->to_square()] = move;
                        moveHistory[board.side_to_move()][move.from_square()][move.to_square()] += depth * depth;
                    }
                    break;
                }
            }
        }
    }

    node->save(hash, bestScore, bestScore >= beta ? 2 : alphaChange ? 1 : 3, depth, bestMove, age, staticEval);
    return bestScore;

}

// template definition so that we can call from UCI.cpp
template int Anduril::negamax<Anduril::Root>(libchess::Position &board, int depth, int alpha, int beta, bool cutNode);

// calls negamax and keeps track of the best move
// this is the debug version, it only uses console control and has zero UCI function
void Anduril::goDebug(libchess::Position &board, int depth) {
    libchess::Move bestMove(0);

    // start the clock
    auto start = std::chrono::high_resolution_clock::now();

    // this is for debugging
    std::string boardFENs = board.fen();
    char *boardFEN = &boardFENs[0];

    int alpha = -999999999;
    int beta = 999999999;
    int bestScore = -999999999;

    // set the killer vector to have the correct number of slots
    // and the root node's ply
    // the vector is padded a little at the end in case of the search being extended
    killers = std::vector<std::vector<libchess::Move>>(218); // 218 is the "max moves" defined in thc.h
    for (auto & killer : killers) {
        killer = std::vector<libchess::Move>(2);
    }

    rootPly = ply;

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
    std::vector<std::tuple<int, libchess::Move>> moveListWithScores = getMoveList(board);

    // this starts our move to search at the first move in the list
    libchess::Move move = std::get<1>(moveListWithScores[0]);

    if (board.side_to_move()){
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

            // search only legal moves
            if (!board.is_legal_move(move)) {
                continue;
            }

            board.make_move(move);
            deep--;
            movesExplored++;
            depthNodes++;
            incPly();
            score = -negamax<PV>(board, deep, -beta, -alphaTheSecond, false);
            decPly();
            deep++;
            board.unmake_move();

            if (score > bestScore) {
                bestScore = score;
                alphaTheSecond = score;
                bestMove = move;
            }

            // see if we need to reset the aspiration window
            if ((bestScore <= alpha || bestScore >= beta) && !(bestScore >= 99000 || bestScore <= -99000)) {
                break;
            }

            // update the score within the tuple
            std::get<0>(moveListWithScores[i]) = score;

        }

        // check if we found mate
        if (bestScore >= 99000 || bestScore <= -99000) {
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
        // for depths less than 5
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
        if (boardFEN != board.fen()) {
            std::cout << "Board does not match original at depth: " << deep << std::endl;
            board.from_fen(boardFEN);
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

    std::vector<libchess::Move> PV = getPV(board, depth, bestMove);

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
        PVout += PV[i].to_str();
        if (i != PV.size() - 1) {
            PVout += ", ";
        }
    }
    PVout += "]";
    std::cout << PVout + "\n";
    std::cout << "Moving: " << bestMove.to_str() << " with a score of " << (bestScore/ 100.0) * flipped << std::endl;
    std::chrono::duration<double, std::milli> timeElapsed = end - start;
    std::cout << "Time spent searching: " << timeElapsed.count() / 1000 << " seconds" << std::endl;
    std::cout << "Nodes per second: " << getMovesExplored() / (timeElapsed.count()/1000) << std::endl;
    setMovesExplored(0);
    cutNodes = 0;
    movesTransposed = 0;
    quiesceExplored = 0;
    incPly();

    board.make_move(bestMove);
}

int Anduril::nonPawnMaterial(bool whiteToPlay, libchess::Position &board) {
    int material = 0;
    if (whiteToPlay) {
        material += board.piece_type_bb(libchess::constants::KNIGHT, libchess::constants::WHITE).popcount() * kMG;
        material += board.piece_type_bb(libchess::constants::BISHOP, libchess::constants::WHITE).popcount() * bMG;
        material += board.piece_type_bb(libchess::constants::ROOK, libchess::constants::WHITE).popcount() * rMG;
        material += board.piece_type_bb(libchess::constants::QUEEN, libchess::constants::WHITE).popcount() * qMG;

        return material;
    }
    else {
        material += board.piece_type_bb(libchess::constants::KNIGHT, libchess::constants::BLACK).popcount() * kMG;
        material += board.piece_type_bb(libchess::constants::BISHOP, libchess::constants::BLACK).popcount() * bMG;
        material += board.piece_type_bb(libchess::constants::ROOK, libchess::constants::BLACK).popcount() * rMG;
        material += board.piece_type_bb(libchess::constants::QUEEN, libchess::constants::BLACK).popcount() * qMG;

        return material;
    }
}

// can we reduce the depth of our search for this move?
bool Anduril::isLateReduction(libchess::Position &board, libchess::Move &move) {
    // don't reduce if the move is a capture
    if (board.is_capture_move(move)) {
        return false;
    }

    // don't reduce if the move is a promotion
    if (board.is_promotion_move(move)) {
        return false;
    }

    // don't reduce if the side to move is in check, or on moves that give check
    if (board.in_check()) {
        return false;
    }
    board.make_see_move(move);
    if (board.in_check()) {
        board.unmake_move();
        return false;
    }
    board.unmake_move();

    // if we passed everything else, we can reduce
    return true;
}

// returns the next move to search
libchess::Move Anduril::pickNextMove(std::vector<std::tuple<int, libchess::Move>> &moveListWithScores, int currentIndex) {
    for (int j = currentIndex + 1; j < moveListWithScores.size(); j++) {
        if (std::get<0>(moveListWithScores[currentIndex]) < std::get<0>(moveListWithScores[j])) {
            moveListWithScores[currentIndex].swap(moveListWithScores[j]);
        }
    }

    return std::get<1>(moveListWithScores[currentIndex]);
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
    while (found && node->bestMove.value() != 0) {
        if (board.is_capture_move(node->bestMove) && board.piece_type_on(node->bestMove.to_square()) == std::nullopt) {
            break;
        }
        // this will make sure we don't segfault
        // I don't know the problem but this fixed it and the search does not appear to be affected
        if (board.is_legal_move(node->bestMove)) {
            PV.push_back(node->bestMove);
            board.make_move(node->bestMove);
        }
        else {
            break;
        }
        hash = board.hash();
        node = table.probe(hash, found);
        // in case of infinite loops of repeating moves
        // also check to see if the pv is still in the main search
        if (PV.size() > depth || node->nodeDepth == -1) {
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