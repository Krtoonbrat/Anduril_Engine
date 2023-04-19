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

// prefetches an address in memory to CPU cache that doesn't block execution
// this should speed up the program by reducing the amount of time we wait for transposition table probes
// copied from Stockfish
void prefetch(void* addr) {

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

// the quiescence search
// searches possible captures to make sure we aren't mis-evaluating certain positions
template <Anduril::NodeType nodeType>
int Anduril::quiescence(libchess::Position &board, int alpha, int beta) {
    movesExplored++;
    constexpr bool PvNode = nodeType != NonPV;

    // is the time up?
    if (stopped || (limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime)) {
        return 0;
    }

    // check for a draw
    if (board.halfmoves() >= 100 || board.is_repeat(2)) {
        return 0;
    }

    // represents the best score we have found so far
    int bestScore = -32001;

    // transposition lookup
    uint64_t hash = board.hash();
    bool found = false;
    Node *node = table.probe(hash, found);
    libchess::Move nMove = found ? libchess::Move(node->bestMove) : libchess::Move(0);
	if (!PvNode
		&& found
		&& node->nodeDepth >= -1
        && (node->nodeType == 1 || (node->nodeScore >= beta ? node->nodeType == 2 : node->nodeType == 3))) {
		movesTransposed++;
		return node->nodeScore;
	}

    // are we in check?
    bool check = board.in_check();

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
                node->save(hash, standPat, 2, -1, 0, age, standPat);
			}
            return standPat;
        }
        if (PvNode && standPat > alpha) {
            alpha = standPat;
        }
    }

    MovePicker picker(board, nMove, &moveHistory, &seeValues);

    // arbitrarily low score to make sure its replaced
    int score = -32001;

    // holds the move we want to search
    libchess::Move move;
    libchess::Move bestMove;

    int moveCounter = 0;
    // loop through the moves and score them
    while ((move = picker.nextMove()).value() != 0) {

        // search only legal moves
        if (!board.is_legal_move(move)) {
            continue;
        }

        moveCounter++;

        // just in case we are searching evasions
        if (!check) {
            if (!board.is_capture_move(move)) {
                continue;
            }

            // don't search moves with negative see
            if (board.see_for(move, seeValues) < 0) {
                continue;
            }

            // delta pruning
            // the extra check at the beginning is to get rid of seg faults when enpassant is the capture
            // we can just skip delta pruning there and it shouldn't cost too much
            if (board.piece_type_on(move.to_square())
                && standPat + seeValues[board.piece_type_on(move.to_square())->value()] + 200 < alpha
                && nonPawnMaterial(board.side_to_move(), board) -
                   seeValues[board.piece_type_on(move.to_square())->value()] > 1600
                && move.type() != libchess::Move::Type::CAPTURE_PROMOTION) {
                continue;
            }
        }

        // prefetch before we make a move
        prefetch(table.firstEntry(board.hashAfter(move)));

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

    // check for mate
    if (check && moveCounter == 0) {
        return -32000 + (ply - rootPly);
    }

    node->save(hash, bestScore, bestScore >= beta ? 2 : 3, -1, bestMove.value(), age, standPat);
    return bestScore;

}

// the negamax function.  Does the heavy lifting for the search
template <Anduril::NodeType nodeType>
int Anduril::negamax(libchess::Position &board, int depth, int alpha, int beta, bool cutNode) {
    constexpr bool PvNode = nodeType != NonPV;
    constexpr bool rootNode = nodeType == Root;

    // is the time up?  Mate distance pruning?
    if constexpr (!rootNode) {
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

    // check for a draw
    if (board.halfmoves() >= 100 || board.is_repeat(2)) {
        return 0;
    }

    // are we in check?
    int check = board.in_check();

    // did alpha change?
    bool alphaChange = false;

    // if we are at max depth, start a quiescence search
    if (depth <= 0){
        return quiescence<PvNode ? PV : NonPV>(board, alpha, beta);
    }

    // increment counters now that we know we aren't going into quiescence
    depthNodes++;
    movesExplored++;
    if (PvNode && selDepth < (ply - rootPly) + 1) {
        selDepth = (ply - rootPly) + 1;
    }

    // represents our next move to search
    libchess::Move move;
    libchess::Move bestMove = move;
    libchess::Move excludedMove = board.getExcluded();

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
    libchess::Move nMove = found ? libchess::Move(node->bestMove) : libchess::Move(0);

	if (!PvNode
		&& found
        && excludedMove.value() == 0
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
        node->save(hash, -32001, -1, -99, 0, 0, staticEval);
    }

    // razoring
    if (!check
        && staticEval < alpha - pMG * 3 - pMG * depth) {
        incPly();
        // verification that the value is indeed less than alpha
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
        && excludedMove.value() == 0
        && nonPawnMaterial(!board.side_to_move(), board) > 1600) {

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

    // probcut
    // if we find a position that returns a cutoff when beta is padded a little, we can assume
    // the position would most likely cut at a full depth search as well
    int probCutBeta = beta + 264;
    if (!PvNode
        && depth > 4
        && !check
        && !(found
        && nDepth >= depth - 3
        && nScore != -32001
        && nScore < probCutBeta)) {

        // get a move picker
        MovePicker picker(board, nMove, probCutBeta - staticEval, &seeValues);

        // we loop through all the moves to try and find one that
        // causes a beta cut on a reduced search
        while ((move = picker.nextMove()).value() != 0) {

            // search only legal moves
            if (!board.is_legal_move(move) || move == excludedMove) {
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
                    node->save(hash, score, 2, depth - 3, move.value(), age, staticEval);

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

    // get a move picker
    MovePicker picker(board, nMove, killers[ply - rootPly], counterMoves[board.previous_move()->from_square()][board.previous_move()->to_square()], &moveHistory, &seeValues);

    // at root, we are going to ignore the picker object.  The amount of time spent allocating and selecting moves should be negligible because this only happens for one position per search
    // here we set the pointer for the current root move to the beginning of the move list, and sort the list
    if constexpr (rootNode) {
        partial_insertion_sort(rootMoves.begin(), rootMoves.end(), std::numeric_limits<int>::min());
        currRootMove = rootMoves.begin();
    }

    // indicates that a PvNode will probably fail low if the node was searched, and we found a fail low already
    bool likelyFailLow = PvNode && nMove.value() != 0 && nType == 3;

    // indicates that the transposition move has been singularly extended
    bool singularQuietLMR = false;

    int moveCounter = 0;
    int extension = 0;
    // loop through the possible moves and score each
    while ((move = picker.nextMove()).value() != 0) {

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
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::milli> elapsed = now - startTime;
            if (elapsed.count() > 2000 && depth > 5) {
                std::cout << "info currmove " << move.to_str() << " currmovenumber " << moveCounter + 1 << std::endl;
            }
        }

        moveCounter++;

        // futility pruning
        if (futile
            && move.type() != libchess::Move::Type::CAPTURE
            && move.type() != libchess::Move::Type::PROMOTION
            && move.type() != libchess::Move::Type::CAPTURE_PROMOTION
            && !board.gives_check(move)) {
            continue;
        }

        // the actual depth we will search this move to
        int actualDepth = depth;
        extension = 0;

        // search extensions
        if (ply - rootPly < rDepth * 2) {
            // Singular extension search
            // based on the stockfish implementation
            if (!rootNode
                && depth >= 4 - (rDepth > 21) + 2 * (PvNode && nType == 1)
                && move == nMove
                && excludedMove.value() == 0
                && abs(nScore) < 31000
                && (nType == 1 || nType == 2)
                && nDepth >= depth - 3) {
                int singleBeta = nScore - (3 + 2 * (nType == 1 && !PvNode)) * depth / 2;
                int singleDepth = (depth - 1) / 2;
                singularAttempts++;

                board.setExcluded(move);
                score = negamax<NonPV>(board, singleDepth, singleBeta - 1, singleBeta, cutNode);
                board.setExcluded(libchess::Move(0));

                if (score < singleBeta) {
                    extension = 1;
                    singularQuietLMR = !board.gives_check(nMove);
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
            if (board.gives_check(move) && depth > 12) {
                extension = 1;
            }

        }

        // add extensions to depth
        actualDepth += extension;

        // prefetch before we make a move
        prefetch(table.firstEntry(board.hashAfter(move)));

        // search with zero window
        // first check if we can reduce
        if (depth >= 2
            && moveCounter > 2
            && !board.is_capture_move(move)
            && !board.is_promotion_move(move)
            && !check
            && !board.gives_check(move)) {
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
            if (moveCounter > 6 && depth >= 3) {
                reduction += depth / 3;
            }

            // decrease reduction if the transposition move has been singularly extended
            if (singularQuietLMR) {
                reduction--;
            }

            // new depth we will search with
            // we need to search at least one more move, we will also allow an "extension" of up to two ply
            int newDepth = std::clamp(actualDepth - reduction, 1, actualDepth + 1);

            board.make_move(move);
            incPly();
            score = -negamax<NonPV>(board, newDepth, -(alpha + 1), -alpha, true);

            // full depth search when LMR fails high
            if (score > alpha && newDepth < actualDepth - 1) {
                score = -negamax<NonPV>(board, actualDepth - 1, -(alpha + 1), -alpha, !cutNode);
            }
            decPly();
            board.unmake_move();
        }
        else if (!PvNode || moveCounter > 1) {
            board.make_move(move);
            incPly();
            score = -negamax<NonPV>(board, actualDepth - 1, -(alpha + 1), -alpha, !cutNode);
            decPly();
            board.unmake_move();
        }

        // full PV search for the first move and for moves that fail in the zero window
        if (PvNode && (moveCounter == 1 || (score > alpha && (rootNode || score < beta)))) {
            board.make_move(move);
            incPly();
            score = -negamax<PV>(board, actualDepth - 1, -beta, -alpha, false);
            decPly();
            board.unmake_move();
        }

        // if the search was stopped for whatever reason, return immediately
        if (stopped || (limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime)) {
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
                if (PvNode && score < beta) {
                    alpha = score;
                    alphaChange = true;
                }
                else {
                    cutNodes++;
                    if (move.type() == libchess::Move::Type::CAPTURE) {
                        insertKiller(move, ply - rootPly);
                        counterMoves[board.previous_move()->from_square()][board.previous_move()->to_square()] = move;
                        moveHistory[board.side_to_move()][move.from_square()][move.to_square()] += depth;
                    }
                    break;
                }
            }
        }
    }

    if (moveCounter == 0) {
        bestScore = check ? -32000 + (ply - rootPly) : 0;
    }

    if (excludedMove.value() == 0) {
        node->save(hash, bestScore, bestScore >= beta ? 2 : alphaChange ? 1 : 3, depth, bestMove.value(), age, staticEval);
    }
    return bestScore;

}

// template definition so that we can call from UCI.cpp
template int Anduril::negamax<Anduril::Root>(libchess::Position &board, int depth, int alpha, int beta, bool cutNode);

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
        if (board.is_capture_move(tmp) && board.piece_type_on(tmp.to_square()) == std::nullopt) {
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