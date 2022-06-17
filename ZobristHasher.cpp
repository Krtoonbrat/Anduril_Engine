//
// Created by 80hugkev on 6/17/2022.
//

#include "thc.h"
#include "ZobristHasher.h"

namespace Zobrist {

    // calculates a zobristHash hash of the position
    uint64_t zobristHash(thc::ChessRules &board) {
        return hashBoard(board) ^ hashCastling(board) ^ hashEP(board) ^ hashTurn(board);
    }

    // calculates the board part of a zobristHash hash
    uint64_t hashBoard(thc::ChessRules &board) {
        int pieceIndex = 0;
        uint64_t hash = 0;

        for (int i = 0; i < 64; i++) {
            if (board.squares[i] != ' ') {
                // grabs the correct index for the piece
                switch (board.squares[i]) {
                    case 'P':
                    case 'p':
                        pieceIndex = PAWN * 2;
                        break;
                    case 'N':
                    case 'n':
                        pieceIndex = KNIGHT * 2;
                        break;
                    case 'B':
                    case 'b':
                        pieceIndex = BISHOP * 2;
                        break;
                    case 'R':
                    case 'r':
                        pieceIndex = ROOK * 2;
                        break;
                    case 'Q':
                    case 'q':
                        pieceIndex = QUEEN * 2;
                        break;
                    case 'K':
                    case 'k':
                        pieceIndex = KING * 2;
                        break;
                }

                // if the piece is white, add one to the piece index
                if (board.squares[i] < 90){
                    pieceIndex++;
                }

                // xor the pseudorandom value from the array with the hash
                // magic values at the end of the expression mirror i to the opposite
                // side of the board because the zobirst array is mirrored along the
                // x axis compared to our squares
                hash ^= zobristArray[64 * pieceIndex + (((7 - (i / 8)) * 8) + i % 8)];
            }
        }

        return hash;
    }

    // calculates hashing for castling rights
    uint64_t hashCastling(thc::ChessRules &board) {
        uint64_t hash = 0;

        // each type of castling right has its own pseudorandom value
        if (board.wking_allowed()){
            hash ^= zobristArray[768];
        }
        if (board.wqueen_allowed()){
            hash ^= zobristArray[769];
        }
        if (board.bking_allowed()){
            hash ^= zobristArray[770];
        }
        if (board.bqueen_allowed()){
            hash ^= zobristArray[771];
        }

        return hash;
    }

    // calculates hashing for en passant
    uint64_t hashEP(thc::ChessRules &board) {
        uint64_t hash = 0;
        int fileMask = 0;

        // this finds the square where en passant is possible
        // then it applies an offset based on the file en passant
        // is possible in
        if (board.enpassant_target != thc::SQUARE_INVALID){
            thc::Square target = board.groomed_enpassant_target();
            if (target != thc::SQUARE_INVALID){
                char file = thc::get_file(target);
                switch (file){
                    case 'a':
                        fileMask = 0;
                        break;
                    case 'b':
                        fileMask = 1;
                        break;
                    case 'c':
                        fileMask = 2;
                        break;
                    case 'd':
                        fileMask = 3;
                        break;
                    case 'e':
                        fileMask = 4;
                        break;
                    case 'f':
                        fileMask = 5;
                        break;
                    case 'g':
                        fileMask = 6;
                        break;
                    case 'h':
                        fileMask = 7;
                        break;
                }

                return zobristArray[772 + fileMask];
            }
        }

        return 0;
    }

    // calculates the hash for who's turn it is
    uint64_t hashTurn(thc::ChessRules &board) {
        return board.WhiteToPlay() ? zobristArray[780] : 0;
    }

    // calculates a hash for just pawns
    // this is used for the pawn transposition table
    // since the pawn score only depends on the position of the pawns
    // we can just calculate the hash of the pawns position
    uint64_t hashPawns(thc::ChessRules &board) {
        int pieceIndex = 0;
        uint64_t hash = 0;

        for (int i = 0; i < 64; i++) {
            if (board.squares[i] == 'p' || board.squares[i] == 'P') {
                // grabs the correct index for the piece
                pieceIndex = PAWN * 2;

                // if the piece is white, add one to the piece index
                if (board.squares[i] < 90){
                    pieceIndex++;
                }

                // xor the pseudorandom value from the array with the hash
                // magic values at the end of the expression mirror i to the opposite
                // side of the board because the zobirst array is mirrored along the
                // x axis compared to our squares
                hash ^= zobristArray[64 * pieceIndex + (((7 - (i / 8)) * 8) + i % 8)];
            }
        }

        return hash;
    }


} // namespace Zobrist
