//
// Created by 80hugkev on 6/17/2022.
//

#include "libchess/Position.h"
#include "ZobristHasher.h"

namespace Zobrist {

    // calculates a zobristHash hash of the position
    uint64_t zobristHash(libchess::Position &board) {
        return hashBoard(board) ^ hashCastling(board) ^ hashEP(board) ^ hashTurn(board);
    }

    // calculates the board part of a zobristHash hash
    uint64_t hashBoard(libchess::Position &board) {
        int pieceIndex = 0;
        uint64_t hash = 0;
        libchess::Bitboard pieces = board.occupancy_bb();

        while (pieces) {
            libchess::Square piece = pieces.forward_bitscan();
            // grab the correct index for the piece
            switch (*board.piece_type_on(piece)) {
                case libchess::constants::PAWN:
                    pieceIndex = PAWN * 2;
                    break;
                case libchess::constants::KNIGHT:
                    pieceIndex = KNIGHT * 2;
                    break;
                case libchess::constants::BISHOP:
                    pieceIndex = BISHOP * 2;
                    break;
                case libchess::constants::ROOK:
                    pieceIndex = ROOK * 2;
                    break;
                case libchess::constants::QUEEN:
                    pieceIndex = QUEEN * 2;
                    break;
                case libchess::constants::KING:
                    pieceIndex = KING * 2;
                    break;
            }

            // if the piece is white, add one to the piece index
            if (!*board.color_of(piece)) {
                pieceIndex++;
            }

            // xor the pseudorandom value from the array with the hash
            // magic values at the end of the expression mirror i to the opposite
            // side of the board because the zobirst array is mirrored along the
            // x axis compared to our squares
            hash ^= zobristArray[64 * pieceIndex + piece];

            pieces.forward_popbit();
        }

        return hash;
    }

    // calculates hashing for castling rights
    uint64_t hashCastling(libchess::Position &board) {
        uint64_t hash = 0;
        libchess::CastlingRights rights = board.castling_rights();

        // each type of castling right has its own pseudorandom value
        if (rights.is_allowed(libchess::constants::WHITE_KINGSIDE)){
            hash ^= zobristArray[768];
        }
        if (rights.is_allowed(libchess::constants::WHITE_QUEENSIDE)){
            hash ^= zobristArray[769];
        }
        if (rights.is_allowed(libchess::constants::BLACK_KINGSIDE)){
            hash ^= zobristArray[770];
        }
        if (rights.is_allowed(libchess::constants::BLACK_QUEENSIDE)){
            hash ^= zobristArray[771];
        }

        return hash;
    }

    // calculates hashing for en passant
    uint64_t hashEP(libchess::Position &board) {
        uint64_t hash = 0;
        int fileMask = 0;
        std::optional<libchess::Square> enpassantSquare = board.enpassant_square();

        // this finds the square where en passant is possible
        // then it applies an offset based on the file en passant
        // is possible in
        if (enpassantSquare){
            libchess::Square target = *enpassantSquare;
            switch (target.file()){
                case libchess::constants::FILE_A:
                    fileMask = 0;
                    break;
                case libchess::constants::FILE_B:
                    fileMask = 1;
                    break;
                case libchess::constants::FILE_C:
                    fileMask = 2;
                    break;
                case libchess::constants::FILE_D:
                    fileMask = 3;
                    break;
                case libchess::constants::FILE_E:
                    fileMask = 4;
                    break;
                case libchess::constants::FILE_F:
                    fileMask = 5;
                    break;
                case libchess::constants::FILE_G:
                    fileMask = 6;
                    break;
                case libchess::constants::FILE_H:
                    fileMask = 7;
                    break;
                }
            return zobristArray[772 + fileMask];
        }

        return 0;
    }

    // calculates the hash for who's turn it is
    uint64_t hashTurn(libchess::Position &board) {
        return !board.side_to_move() ? zobristArray[780] : 0;
    }


} // namespace Zobrist
