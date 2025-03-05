//
// Created by Kevin on 12/5/2024.
//

#include "Pyrrhic/tbprobe.h"
#include "Syzygy.h"

// from tbprobe.cpp
extern int TB_LARGEST;

extern int syzygyProbeDepth;

namespace Tablebase{

unsigned probeTablebaseWDL(libchess::Position& board, int depth, int ply) {

    // Do not probe at root, in nodes with castling rights, in nodes with 50 move rule risk, or nodes that have too many pieces
    if (ply == 0
        || board.castling_rights().value()
        || board.halfmoves()
        || board.occupancy_bb().popcount() > TB_LARGEST) {
        return TB_RESULT_FAILED;
    }

    // do not probe if depth is lower than the probe depth unless the board has fewer pieces than the largest tablebase
    if (depth < syzygyProbeDepth && board.occupancy_bb().popcount() == TB_LARGEST) {
        return TB_RESULT_FAILED;
    }

    // probe the tablebase using the pyrrhic api
    return tb_probe_wdl(board.color_bb(libchess::constants::WHITE),
                        board.color_bb(libchess::constants::BLACK),
                        board.piece_type_bb(libchess::constants::KING),
                        board.piece_type_bb(libchess::constants::QUEEN),
                        board.piece_type_bb(libchess::constants::ROOK),
                        board.piece_type_bb(libchess::constants::BISHOP),
                        board.piece_type_bb(libchess::constants::KNIGHT),
                        board.piece_type_bb(libchess::constants::PAWN),
                        board.enpassant_square() ? board.enpassant_square().value() : 0,
                        board.side_to_move() == libchess::constants::WHITE);
}


}

