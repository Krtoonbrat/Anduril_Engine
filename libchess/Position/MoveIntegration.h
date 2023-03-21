#ifndef LIBCHESS_MOVEINTEGRATION_H
#define LIBCHESS_MOVEINTEGRATION_H
#include <iostream>
#include "../Move.h"
#include "../Position.h"

namespace libchess {

inline Move::Type Position::move_type_of(Move move) const {
    Move::Type move_type = move.type();
    if (move_type != Move::Type::NONE) {
        return move_type;
    } else {
        Square to_square = move.to_square();
        Square from_square = move.from_square();
        auto moving_pt = piece_type_on(from_square);
        auto captured_pt = piece_type_on(to_square);
        auto promotion_pt = move.promotion_piece_type();
        if (promotion_pt) {
            return captured_pt ? Move::Type::CAPTURE_PROMOTION : Move::Type::PROMOTION;
        } else if (captured_pt) {
            return Move::Type::CAPTURE;
        } else if (moving_pt == constants::PAWN) {
            int sq_diff = std::abs(to_square - from_square);
            if (sq_diff == 16) {
                return Move::Type::DOUBLE_PUSH;
            } else if (sq_diff == 9 || sq_diff == 7) {
                return Move::Type::ENPASSANT;
            } else {
                return Move::Type::NORMAL;
            }
        } else if (moving_pt == constants::KING && std::abs(to_square - from_square) == 2) {
            return Move::Type::CASTLING;
        } else {
            return Move::Type::NORMAL;
        }
    }
}

inline bool Position::is_capture_move(Move move) const {
    Move::Type move_type = move.type();
    switch (move_type) {
        case Move::Type::CAPTURE:
        case Move::Type::CAPTURE_PROMOTION:
        case Move::Type::ENPASSANT:
            return true;
        default:
            return false;
    }
}

inline bool Position::is_promotion_move(Move move) const {
    Move::Type move_type = move.type();
    switch (move_type) {
        case Move::Type::PROMOTION:
        case Move::Type::CAPTURE_PROMOTION:
            return true;
        default:
            return false;
    }
}

inline void Position::unmake_move() {
    auto move = state().previous_move_;
    if (side_to_move() == constants::WHITE) {
        --fullmoves_;
    }
    Move::Type move_type = state().move_type_;
    auto captured_pt = state().captured_pt_;
    --ply_;
    history_.pop_back();
    reverse_side_to_move();
    if (!move) {
        return;
    }
    Color stm = side_to_move();

    Square from_square = move->from_square();
    Square to_square = move->to_square();

    auto moving_pt = piece_type_on(to_square);
    switch (move_type) {
        case Move::Type::NORMAL:
            move_piece(to_square, from_square, *moving_pt, stm);
            break;
        case Move::Type::CAPTURE:
            move_piece(to_square, from_square, *moving_pt, stm);
            put_piece(to_square, *captured_pt, !stm);
            break;
        case Move::Type::DOUBLE_PUSH:
            move_piece(to_square, from_square, constants::PAWN, stm);
            break;
        case Move::Type::ENPASSANT:
            put_piece(stm == constants::WHITE ? Square{to_square - 8} : Square(to_square + 8),
                      constants::PAWN,
                      !stm);
            move_piece(to_square, from_square, constants::PAWN, stm);
            break;
        case Move::Type::CASTLING:
            move_piece(to_square, from_square, constants::KING, stm);
            switch (to_square) {
                case constants::C1:
                    move_piece(constants::D1, constants::A1, constants::ROOK, stm);
                    break;
                case constants::G1:
                    move_piece(constants::F1, constants::H1, constants::ROOK, stm);
                    break;
                case constants::C8:
                    move_piece(constants::D8, constants::A8, constants::ROOK, stm);
                    break;
                case constants::G8:
                    move_piece(constants::F8, constants::H8, constants::ROOK, stm);
                    break;
                default:
                    break;
            }
            break;
        case Move::Type::PROMOTION:
            remove_piece(to_square, *move->promotion_piece_type(), stm);
            put_piece(from_square, constants::PAWN, stm);
            break;
        case Move::Type::CAPTURE_PROMOTION:
            remove_piece(to_square, *move->promotion_piece_type(), stm);
            put_piece(from_square, constants::PAWN, stm);
            put_piece(to_square, *captured_pt, !stm);
            break;
        case Move::Type::NONE:
            break;
    }
}

inline Position::hash_type Position::hashAfter(Move move) {
    Color stm = side_to_move();

    State prev_state = state();
    hash_type hash = prev_state.hash_;

    Square from_square = move.from_square();
    Square to_square = move.to_square();

    Square possiblePassant(0);
    Bitboard enpassant(0);

    CastlingRights rights = CastlingRights{prev_state.castling_rights_.value() &
                                           castling_spoilers[from_square.value()] &
                                           castling_spoilers[to_square.value()]};

    std::optional<PieceType> moving_pt = piece_type_on(from_square);
    std::optional<PieceType> captured_pt = piece_type_on(to_square);
    std::optional<PieceType> promotion_pt = move.promotion_piece_type();

    Move::Type move_type = move_type_of(move);

    if (prev_state.enpassant_square_) {
        hash ^= zobrist::enpassant_key(*prev_state.enpassant_square_);
    }
    hash ^= zobrist::side_to_move_key(constants::WHITE);

    switch (move_type) {
        case Move::Type::NORMAL:
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            break;

        case Move::Type::CAPTURE:
            hash ^= zobrist::piece_square_key(to_square, *captured_pt, !stm);
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            break;

        case Move::Type::DOUBLE_PUSH:
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            // is enpassant even possible on the next turn?
            possiblePassant = stm == constants::WHITE ? Square(from_square + 8) : Square(from_square - 8);
            enpassant = piece_type_bb(constants::PAWN, !stm) & lookups::pawn_attacks(possiblePassant, stm);
            if (enpassant) {
                hash ^= zobrist::enpassant_key(possiblePassant);
            }
            break;

        case Move::Type::ENPASSANT:
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            hash ^= zobrist::piece_square_key(stm == constants::WHITE ? Square(to_square - 8) : Square(to_square + 8), constants::PAWN, !stm);
            break;

        case Move::Type::CASTLING:
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            switch (to_square) {
                case constants::C1:
                    hash ^= zobrist::piece_square_key(constants::A1, constants::ROOK, stm) ^ zobrist::piece_square_key(constants::D1, constants::ROOK, stm);
                    break;
                case constants::G1:
                    hash ^= zobrist::piece_square_key(constants::H1, constants::ROOK, stm) ^ zobrist::piece_square_key(constants::F1, constants::ROOK, stm);
                    break;
                case constants::C8:
                    hash ^= zobrist::piece_square_key(constants::A8, constants::ROOK, stm) ^ zobrist::piece_square_key(constants::D8, constants::ROOK, stm);
                    break;
                case constants::G8:
                    hash ^= zobrist::piece_square_key(constants::H8, constants::ROOK, stm) ^ zobrist::piece_square_key(constants::F8, constants::ROOK, stm);
                    break;
                default:
                    break;
            }
        break;

        case Move::Type::PROMOTION:
            hash ^= zobrist::piece_square_key(from_square, constants::PAWN, stm);
            hash ^= zobrist::piece_square_key(to_square, *promotion_pt, stm);
            break;

        case Move::Type::CAPTURE_PROMOTION:
            hash ^= zobrist::piece_square_key(to_square, *captured_pt, !stm);
            hash ^= zobrist::piece_square_key(from_square, constants::PAWN, stm);
            hash ^= zobrist::piece_square_key(to_square, *promotion_pt, stm);
            break;

        case Move::Type::NONE:
            break;
    }

    // I do castling rights separately because they depend on the last positions rights and this positions rights
    // because you can't gain back castling rights, we check to see if they were allowed, but no longer are
    if (prev_state.castling_rights_.is_allowed(constants::WHITE_KINGSIDE) && !rights.is_allowed(constants::WHITE_KINGSIDE)) {
        hash ^= zobrist::castling_rights_key(CastlingRights(constants::WHITE_KINGSIDE));
    }
    if (prev_state.castling_rights_.is_allowed(constants::WHITE_QUEENSIDE) && !rights.is_allowed(constants::WHITE_QUEENSIDE)) {
        hash ^= zobrist::castling_rights_key(CastlingRights(constants::WHITE_QUEENSIDE));
    }
    if (prev_state.castling_rights_.is_allowed(constants::BLACK_KINGSIDE) && !rights.is_allowed(constants::BLACK_KINGSIDE)) {
        hash ^= zobrist::castling_rights_key(CastlingRights(constants::BLACK_KINGSIDE));
    }
    if (prev_state.castling_rights_.is_allowed(constants::BLACK_QUEENSIDE) && !rights.is_allowed(constants::BLACK_QUEENSIDE)) {
        hash ^= zobrist::castling_rights_key(CastlingRights(constants::BLACK_QUEENSIDE));
    }

    return hash;
}

// edited by Krtoonbrat
// changed to incrementally update hashes
inline void Position::make_move(Move move) {
    Color stm = side_to_move();
    if (stm == constants::BLACK) {
        ++fullmoves_;
    }
    ++ply_;
    history_.push_back(State{});
    State& prev_state = state_mut_ref(ply_ - 1);
    State& next_state = state_mut_ref();
    next_state.halfmoves_ = prev_state.halfmoves_ + 1;
    next_state.previous_move_ = move;
    next_state.enpassant_square_ = {};

    Square from_square = move.from_square();
    Square to_square = move.to_square();

    Square possiblePassant(0);
    Bitboard enpassant(0);

    next_state.castling_rights_ = CastlingRights{prev_state.castling_rights_.value() &
                                                 castling_spoilers[from_square.value()] &
                                                 castling_spoilers[to_square.value()]};

    std::optional<PieceType> moving_pt = piece_type_on(from_square);
    std::optional<PieceType> captured_pt = piece_type_on(to_square);
    std::optional<PieceType> promotion_pt = move.promotion_piece_type();

    Move::Type move_type = move_type_of(move);

    hash_type hash = prev_state.hash_;
    hash_type phash = prev_state.pawn_hash_;
    if (prev_state.enpassant_square_) {
        hash ^= zobrist::enpassant_key(*prev_state.enpassant_square_);
    }
    hash ^= zobrist::side_to_move_key(constants::WHITE);

    if (moving_pt == constants::PAWN || captured_pt) {
        next_state.halfmoves_ = 0;
    }

    int scoreMG = prev_state.scoreMG;
    int scoreEG = prev_state.scoreEG;
    Square epCapSquare = stm == constants::WHITE ? Square(to_square - 8) : Square(to_square + 8);
    int tableCoordsFrom;
    int tableCoordsTo;

    switch (move_type) {
        case Move::Type::NORMAL:
            move_piece(from_square, to_square, *moving_pt, stm);
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            if (stm == constants::WHITE) {
                tableCoordsFrom = ((7 - (from_square / 8)) * 8) + from_square % 8;
                tableCoordsTo = ((7 - (to_square / 8)) * 8) + to_square % 8;
                scoreMG -= pieceSquareTableMG[moving_pt->value()][tableCoordsFrom];
                scoreEG -= pieceSquareTableEG[moving_pt->value()][tableCoordsFrom];
                scoreMG += pieceSquareTableMG[moving_pt->value()][tableCoordsTo];
                scoreEG += pieceSquareTableEG[moving_pt->value()][tableCoordsTo];
            }
            else {
                scoreMG += pieceSquareTableMG[moving_pt->value()][from_square];
                scoreEG += pieceSquareTableEG[moving_pt->value()][from_square];
                scoreMG -= pieceSquareTableMG[moving_pt->value()][to_square];
                scoreEG -= pieceSquareTableEG[moving_pt->value()][to_square];
            }
            if (*moving_pt == constants::PAWN) {
                phash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            }
            break;
        case Move::Type::CAPTURE:
            remove_piece(to_square, *captured_pt, !stm);
            hash ^= zobrist::piece_square_key(to_square, *captured_pt, !stm);
            if (stm == constants::WHITE) {
                scoreMG += pieceValuesMG[captured_pt->value()] + pieceSquareTableMG[captured_pt->value()][to_square];
                scoreEG += pieceValuesEG[captured_pt->value()] + pieceSquareTableEG[captured_pt->value()][to_square];
            }
            else {
                tableCoordsTo = ((7 - (to_square / 8)) * 8) + to_square % 8;
                scoreMG -= pieceValuesMG[captured_pt->value()] + pieceSquareTableMG[captured_pt->value()][tableCoordsTo];
                scoreEG -= pieceValuesEG[captured_pt->value()] + pieceSquareTableEG[captured_pt->value()][tableCoordsTo];
            }
            if (*captured_pt == constants::PAWN) {
                phash ^= zobrist::piece_square_key(to_square, *captured_pt, !stm);
            }
            move_piece(from_square, to_square, *moving_pt, stm);
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            if (stm == constants::WHITE) {
                tableCoordsFrom = ((7 - (from_square / 8)) * 8) + from_square % 8;
                tableCoordsTo = ((7 - (to_square / 8)) * 8) + to_square % 8;
                scoreMG -= pieceSquareTableMG[moving_pt->value()][tableCoordsFrom];
                scoreEG -= pieceSquareTableEG[moving_pt->value()][tableCoordsFrom];
                scoreMG += pieceSquareTableMG[moving_pt->value()][tableCoordsTo];
                scoreEG += pieceSquareTableEG[moving_pt->value()][tableCoordsTo];
            }
            else {
                scoreMG += pieceSquareTableMG[moving_pt->value()][from_square];
                scoreEG += pieceSquareTableEG[moving_pt->value()][from_square];
                scoreMG -= pieceSquareTableMG[moving_pt->value()][to_square];
                scoreEG -= pieceSquareTableEG[moving_pt->value()][to_square];
            }
            if (*moving_pt == constants::PAWN) {
                phash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            }
            break;
        case Move::Type::DOUBLE_PUSH:
            move_piece(from_square, to_square, constants::PAWN, stm);
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            phash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            if (stm == constants::WHITE) {
                tableCoordsFrom = ((7 - (from_square / 8)) * 8) + from_square % 8;
                tableCoordsTo = ((7 - (to_square / 8)) * 8) + to_square % 8;
                scoreMG -= pieceSquareTableMG[moving_pt->value()][tableCoordsFrom];
                scoreEG -= pieceSquareTableEG[moving_pt->value()][tableCoordsFrom];
                scoreMG += pieceSquareTableMG[moving_pt->value()][tableCoordsTo];
                scoreEG += pieceSquareTableEG[moving_pt->value()][tableCoordsTo];
            }
            else {
                scoreMG += pieceSquareTableMG[moving_pt->value()][from_square];
                scoreEG += pieceSquareTableEG[moving_pt->value()][from_square];
                scoreMG -= pieceSquareTableMG[moving_pt->value()][to_square];
                scoreEG -= pieceSquareTableEG[moving_pt->value()][to_square];
            }
            // is enpassant even possible on the next turn?
            possiblePassant = stm == constants::WHITE ? Square(from_square + 8) : Square(from_square - 8);
            enpassant = piece_type_bb(constants::PAWN, !stm) & lookups::pawn_attacks(possiblePassant, stm);
            if (enpassant) {
                next_state.enpassant_square_ = possiblePassant;
                hash ^= zobrist::enpassant_key(*next_state.enpassant_square_);
            }
            break;
        case Move::Type::ENPASSANT:
            move_piece(from_square, to_square, constants::PAWN, stm);
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            phash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            if (stm == constants::WHITE) {
                tableCoordsFrom = ((7 - (from_square / 8)) * 8) + from_square % 8;
                tableCoordsTo = ((7 - (to_square / 8)) * 8) + to_square % 8;
                scoreMG -= pieceSquareTableMG[constants::PAWN.value()][tableCoordsFrom];
                scoreEG -= pieceSquareTableEG[constants::PAWN.value()][tableCoordsFrom];
                scoreMG += pieceSquareTableMG[constants::PAWN.value()][tableCoordsTo];
                scoreEG += pieceSquareTableEG[constants::PAWN.value()][tableCoordsTo];
            }
            else {
                scoreMG += pieceSquareTableMG[constants::PAWN.value()][from_square];
                scoreEG += pieceSquareTableEG[constants::PAWN.value()][from_square];
                scoreMG -= pieceSquareTableMG[constants::PAWN.value()][to_square];
                scoreEG -= pieceSquareTableEG[constants::PAWN.value()][to_square];
            }
            remove_piece(epCapSquare,constants::PAWN,!stm);
            hash ^= zobrist::piece_square_key(epCapSquare, constants::PAWN, !stm);
            phash ^= zobrist::piece_square_key(epCapSquare, constants::PAWN, !stm);
            if (stm == constants::WHITE) {
                scoreMG += pieceValuesMG[constants::PAWN.value()] + pieceSquareTableMG[constants::PAWN.value()][epCapSquare];
                scoreEG += pieceValuesEG[constants::PAWN.value()] + pieceSquareTableEG[constants::PAWN.value()][epCapSquare];
            }
            else {
                tableCoordsTo = ((7 - (epCapSquare / 8)) * 8) + epCapSquare % 8;
                scoreMG -= pieceValuesMG[constants::PAWN.value()] + pieceSquareTableMG[constants::PAWN.value()][tableCoordsTo];
                scoreEG -= pieceValuesEG[constants::PAWN.value()] + pieceSquareTableEG[constants::PAWN.value()][tableCoordsTo];
            }
            break;
        case Move::Type::CASTLING:
            move_piece(from_square, to_square, constants::KING, stm);
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            if (stm == constants::WHITE) {
                tableCoordsFrom = ((7 - (from_square / 8)) * 8) + from_square % 8;
                tableCoordsTo = ((7 - (to_square / 8)) * 8) + to_square % 8;
                scoreMG -= pieceSquareTableMG[moving_pt->value()][tableCoordsFrom];
                scoreEG -= pieceSquareTableEG[moving_pt->value()][tableCoordsFrom];
                scoreMG += pieceSquareTableMG[moving_pt->value()][tableCoordsTo];
                scoreEG += pieceSquareTableEG[moving_pt->value()][tableCoordsTo];
            }
            else {
                scoreMG += pieceSquareTableMG[moving_pt->value()][from_square];
                scoreEG += pieceSquareTableEG[moving_pt->value()][from_square];
                scoreMG -= pieceSquareTableMG[moving_pt->value()][to_square];
                scoreEG -= pieceSquareTableEG[moving_pt->value()][to_square];
            }
            switch (to_square) {
                case constants::C1:
                    move_piece(constants::A1, constants::D1, constants::ROOK, stm);
                    tableCoordsFrom = ((7 - (constants::A1 / 8)) * 8) + constants::A1 % 8;
                    tableCoordsTo = ((7 - (constants::D1 / 8)) * 8) + constants::D1 % 8;
                    scoreMG -= pieceSquareTableMG[constants::ROOK.value()][tableCoordsFrom];
                    scoreEG -= pieceSquareTableEG[constants::ROOK.value()][tableCoordsFrom];
                    scoreMG += pieceSquareTableMG[constants::ROOK.value()][tableCoordsTo];
                    scoreEG += pieceSquareTableEG[constants::ROOK.value()][tableCoordsTo];
                    hash ^= zobrist::piece_square_key(constants::A1, constants::ROOK, stm) ^ zobrist::piece_square_key(constants::D1, constants::ROOK, stm);
                    break;
                case constants::G1:
                    move_piece(constants::H1, constants::F1, constants::ROOK, stm);
                    tableCoordsFrom = ((7 - (constants::H1 / 8)) * 8) + constants::H1 % 8;
                    tableCoordsTo = ((7 - (constants::F1 / 8)) * 8) + constants::F1 % 8;
                    scoreMG -= pieceSquareTableMG[constants::ROOK.value()][tableCoordsFrom];
                    scoreEG -= pieceSquareTableEG[constants::ROOK.value()][tableCoordsFrom];
                    scoreMG += pieceSquareTableMG[constants::ROOK.value()][tableCoordsTo];
                    scoreEG += pieceSquareTableEG[constants::ROOK.value()][tableCoordsTo];
                    hash ^= zobrist::piece_square_key(constants::H1, constants::ROOK, stm) ^ zobrist::piece_square_key(constants::F1, constants::ROOK, stm);
                    break;
                case constants::C8:
                    move_piece(constants::A8, constants::D8, constants::ROOK, stm);
                    scoreMG += pieceSquareTableMG[constants::ROOK.value()][constants::A8.value()];
                    scoreEG += pieceSquareTableEG[constants::ROOK.value()][constants::A8.value()];
                    scoreMG -= pieceSquareTableMG[constants::ROOK.value()][constants::D8.value()];
                    scoreEG -= pieceSquareTableEG[constants::ROOK.value()][constants::D8.value()];
                    hash ^= zobrist::piece_square_key(constants::A8, constants::ROOK, stm) ^ zobrist::piece_square_key(constants::D8, constants::ROOK, stm);
                    break;
                case constants::G8:
                    move_piece(constants::H8, constants::F8, constants::ROOK, stm);
                    scoreMG += pieceSquareTableMG[constants::ROOK.value()][constants::H8.value()];
                    scoreEG += pieceSquareTableEG[constants::ROOK.value()][constants::H8.value()];
                    scoreMG -= pieceSquareTableMG[constants::ROOK.value()][constants::F8.value()];
                    scoreEG -= pieceSquareTableEG[constants::ROOK.value()][constants::F8.value()];
                    hash ^= zobrist::piece_square_key(constants::H8, constants::ROOK, stm) ^ zobrist::piece_square_key(constants::F8, constants::ROOK, stm);
                    break;
                default:
                    break;
            }
            break;
        case Move::Type::PROMOTION:
            remove_piece(from_square, constants::PAWN, stm);
            if (stm == constants::WHITE) {
                tableCoordsFrom = ((7 - (from_square / 8)) * 8) + from_square % 8;
                scoreMG -= pieceValuesMG[constants::PAWN.value()] + pieceSquareTableMG[constants::PAWN.value()][tableCoordsFrom];
                scoreEG -= pieceValuesEG[constants::PAWN.value()] + pieceSquareTableEG[constants::PAWN.value()][tableCoordsFrom];
            }
            else {
                scoreMG += pieceValuesMG[constants::PAWN.value()] + pieceSquareTableMG[constants::PAWN.value()][from_square];
                scoreEG += pieceValuesEG[constants::PAWN.value()] + pieceSquareTableEG[constants::PAWN.value()][from_square];
            }
            hash ^= zobrist::piece_square_key(from_square, constants::PAWN, stm);
            phash ^= zobrist::piece_square_key(from_square, constants::PAWN, stm);
            put_piece(to_square, *promotion_pt, stm);
            if (stm == constants::WHITE) {
                tableCoordsTo = ((7 - (to_square / 8)) * 8) + to_square % 8;
                scoreMG += pieceValuesMG[promotion_pt->value()] + pieceSquareTableMG[promotion_pt->value()][tableCoordsTo];
                scoreEG += pieceValuesEG[promotion_pt->value()] + pieceSquareTableEG[promotion_pt->value()][tableCoordsTo];
            }
            else {
                scoreMG -= pieceValuesMG[promotion_pt->value()] + pieceSquareTableMG[promotion_pt->value()][to_square];
                scoreEG -= pieceValuesEG[promotion_pt->value()] + pieceSquareTableEG[promotion_pt->value()][to_square];
            }
            hash ^= zobrist::piece_square_key(to_square, *promotion_pt, stm);
            break;
        case Move::Type::CAPTURE_PROMOTION:
            remove_piece(to_square, *captured_pt, !stm);
            if (stm == constants::WHITE) {
                scoreMG += pieceValuesMG[captured_pt->value()] + pieceSquareTableMG[captured_pt->value()][to_square];
                scoreEG += pieceValuesEG[captured_pt->value()] + pieceSquareTableEG[captured_pt->value()][to_square];
            }
            else {
                tableCoordsTo = ((7 - (to_square / 8)) * 8) + to_square % 8;
                scoreMG -= pieceValuesMG[captured_pt->value()] + pieceSquareTableMG[captured_pt->value()][tableCoordsTo];
                scoreEG -= pieceValuesEG[captured_pt->value()] + pieceSquareTableEG[captured_pt->value()][tableCoordsTo];
            }
            hash ^= zobrist::piece_square_key(to_square, *captured_pt, !stm);
            if (*captured_pt == constants::PAWN) {
                phash ^= zobrist::piece_square_key(to_square, *captured_pt, !stm);
            }
            remove_piece(from_square, constants::PAWN, stm);
            if (stm == constants::WHITE) {
                tableCoordsFrom = ((7 - (from_square / 8)) * 8) + from_square % 8;
                scoreMG -= pieceValuesMG[constants::PAWN.value()] + pieceSquareTableMG[constants::PAWN.value()][tableCoordsFrom];
                scoreEG -= pieceValuesEG[constants::PAWN.value()] + pieceSquareTableEG[constants::PAWN.value()][tableCoordsFrom];
            }
            else {
                scoreMG += pieceValuesMG[constants::PAWN.value()] + pieceSquareTableMG[constants::PAWN.value()][from_square];
                scoreEG += pieceValuesEG[constants::PAWN.value()] + pieceSquareTableEG[constants::PAWN.value()][from_square];
            }
            hash ^= zobrist::piece_square_key(from_square, constants::PAWN, stm);
            phash ^= zobrist::piece_square_key(from_square, constants::PAWN, stm);
            put_piece(to_square, *promotion_pt, stm);
            if (stm == constants::WHITE) {
                tableCoordsTo = ((7 - (to_square / 8)) * 8) + to_square % 8;
                scoreMG += pieceValuesMG[promotion_pt->value()] + pieceSquareTableMG[promotion_pt->value()][tableCoordsTo];
                scoreEG += pieceValuesEG[promotion_pt->value()] + pieceSquareTableEG[promotion_pt->value()][tableCoordsTo];
            }
            else {
                scoreMG -= pieceValuesMG[promotion_pt->value()] + pieceSquareTableMG[promotion_pt->value()][to_square];
                scoreEG -= pieceValuesEG[promotion_pt->value()] + pieceSquareTableEG[promotion_pt->value()][to_square];
            }
            hash ^= zobrist::piece_square_key(to_square, *promotion_pt, stm);
            break;
        case Move::Type::NONE:
            break;
    }

    // I do castling rights separately because they depend on the last positions rights and this positions rights
    // because you can't gain back castling rights, we check to see if they were allowed, but no longer are
    if (prev_state.castling_rights_.is_allowed(constants::WHITE_KINGSIDE) && !next_state.castling_rights_.is_allowed(constants::WHITE_KINGSIDE)) {
        hash ^= zobrist::castling_rights_key(CastlingRights(constants::WHITE_KINGSIDE));
    }
    if (prev_state.castling_rights_.is_allowed(constants::WHITE_QUEENSIDE) && !next_state.castling_rights_.is_allowed(constants::WHITE_QUEENSIDE)) {
        hash ^= zobrist::castling_rights_key(CastlingRights(constants::WHITE_QUEENSIDE));
    }
    if (prev_state.castling_rights_.is_allowed(constants::BLACK_KINGSIDE) && !next_state.castling_rights_.is_allowed(constants::BLACK_KINGSIDE)) {
        hash ^= zobrist::castling_rights_key(CastlingRights(constants::BLACK_KINGSIDE));
    }
    if (prev_state.castling_rights_.is_allowed(constants::BLACK_QUEENSIDE) && !next_state.castling_rights_.is_allowed(constants::BLACK_QUEENSIDE)) {
        hash ^= zobrist::castling_rights_key(CastlingRights(constants::BLACK_QUEENSIDE));
    }

    next_state.captured_pt_ = captured_pt;
    next_state.move_type_ = move_type;
    reverse_side_to_move();
    next_state.hash_ = hash;
    next_state.pawn_hash_ = phash;
    next_state.scoreMG = scoreMG;
    next_state.scoreEG = scoreEG;
}

inline void Position::make_null_move() {
    if (side_to_move() == constants::BLACK) {
        ++fullmoves_;
    }
    ++ply_;
    history_.push_back(State{});
    State& prev = state_mut_ref(ply_ - 1);
    State& next = state_mut_ref();
    reverse_side_to_move();
    next.previous_move_ = {};
    next.halfmoves_ = prev.halfmoves_ + 1;
    next.enpassant_square_ = {};
    next.castling_rights_ = prev.castling_rights_;

    // editied by Krtoonbrat
    // We should be able to incrementally update the hash for null moves.  The only things that change are the turn
    // and enpassant
    next.hash_ = prev.hash_ ^ zobrist::side_to_move_key(constants::WHITE);
    if (prev.enpassant_square_) {
        next.hash_ ^= zobrist::enpassant_key(*prev.enpassant_square_);
    }
    // the pawn hash shouldn't change at all
    next.pawn_hash_ = prev.pawn_hash_;
    next.scoreMG = prev.scoreMG;
    next.scoreEG = prev.scoreEG;
}

}  // namespace libchess

#endif  // LIBCHESS_MOVEINTEGRATION_H
