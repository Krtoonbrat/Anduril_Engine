//
// Created by Krtoonbrat on 7/3/2024.
//

#include "Anduril.h"

static int captures = 0;
static int ep = 0;
static int castling = 0;
static int promotion = 0;
static int check = 0;

// this version is visible to the outside world
void Anduril::perft(libchess::Position &board, int depth) {
    uint64_t total = perft<true>(board, depth);
    std::cout << "Total Nodes: " << total << std::endl;
}

// this version is private and actually performs the perft search
template<bool root>
uint64_t Anduril::perft(libchess::Position &board, int depth) {
    if (depth == 0) {
        return 1;
    }
    uint64_t count, total = 0;

    // reset the move type counters in case we did a perft already
    if constexpr (root) {
        captures = ep = castling = promotion = check = 0;
    }

    for (auto& m : board.legal_move_list(board.side_to_move())) {
        if (board.is_capture_move(m)) {
            captures++;
        }

        if (board.is_promotion_move(m)) {
            promotion++;
        }

        if (m.type() == libchess::Move::Type::ENPASSANT) {
            ep++;
        }

        if (m.type() == libchess::Move::Type::CASTLING) {
            castling++;
        }

        if (board.gives_check(m)) {
            check++;
        }

        board.make_move(m);
        count = perft<false>(board, depth - 1);
        total += count;
        board.unmake_move();

        if constexpr (root) {
            std::cout << m.to_str() << ": " << count << std::endl;
        }
    }

    if constexpr (root) {
        std::cout << "Captures: " << captures << "\n"
                  << "En Passant: " << ep << "\n"
                  << "Castling: " << castling << "\n"
                  << "Promotion: " << promotion << "\n"
                  << "Checks: " << check
                  << std::endl;
    }

    return total;
}



