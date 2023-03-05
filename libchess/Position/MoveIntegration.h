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

inline void Position::make_see_move(Move move) {
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

    next_state.castling_rights_ = CastlingRights{prev_state.castling_rights_.value() &
                                                 castling_spoilers[from_square.value()] &
                                                 castling_spoilers[to_square.value()]};

    std::optional<PieceType> moving_pt = piece_type_on(from_square);
    std::optional<PieceType> captured_pt = piece_type_on(to_square);
    std::optional<PieceType> promotion_pt = move.promotion_piece_type();

    Move::Type move_type = move_type_of(move);

    if (moving_pt == constants::PAWN || captured_pt) {
        next_state.halfmoves_ = 0;
    }

    switch (move_type) {
        case Move::Type::NORMAL:
            move_piece(from_square, to_square, *moving_pt, stm);
            break;
        case Move::Type::CAPTURE:
            remove_piece(to_square, *captured_pt, !stm);
            move_piece(from_square, to_square, *moving_pt, stm);
            break;
        case Move::Type::DOUBLE_PUSH:
            move_piece(from_square, to_square, constants::PAWN, stm);
            next_state.enpassant_square_ =
                    stm == constants::WHITE ? Square(from_square + 8) : Square(from_square - 8);
            break;
        case Move::Type::ENPASSANT:
            move_piece(from_square, to_square, constants::PAWN, stm);
            remove_piece(stm == constants::WHITE ? Square(to_square - 8) : Square(to_square + 8),
                         constants::PAWN,
                         !stm);
            break;
        case Move::Type::CASTLING:
            move_piece(from_square, to_square, constants::KING, stm);
            switch (to_square) {
                case constants::C1:
                    move_piece(constants::A1, constants::D1, constants::ROOK, stm);
                    break;
                case constants::G1:
                    move_piece(constants::H1, constants::F1, constants::ROOK, stm);
                    break;
                case constants::C8:
                    move_piece(constants::A8, constants::D8, constants::ROOK, stm);
                    break;
                case constants::G8:
                    move_piece(constants::H8, constants::F8, constants::ROOK, stm);
                    break;
                default:
                    break;
            }
            break;
        case Move::Type::PROMOTION:
            remove_piece(from_square, constants::PAWN, stm);
            put_piece(to_square, *promotion_pt, stm);
            break;
        case Move::Type::CAPTURE_PROMOTION:
            remove_piece(to_square, *captured_pt, !stm);
            remove_piece(from_square, constants::PAWN, stm);
            put_piece(to_square, *promotion_pt, stm);
            break;
        case Move::Type::NONE:
            break;
    }

    next_state.captured_pt_ = captured_pt;
    next_state.move_type_ = move_type;
    reverse_side_to_move();

    // commented out because they are the time wasters
    //next_state.hash_ = calculate_hash();
    //next_state.pawn_hash_ = calculate_pawn_hash();
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

    switch (move_type) {
        case Move::Type::NORMAL:
            move_piece(from_square, to_square, *moving_pt, stm);
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            if (*moving_pt == constants::PAWN) {
                phash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            }
            break;
        case Move::Type::CAPTURE:
            remove_piece(to_square, *captured_pt, !stm);
            hash ^= zobrist::piece_square_key(to_square, *captured_pt, !stm);
            if (*captured_pt == constants::PAWN) {
                phash ^= zobrist::piece_square_key(to_square, *captured_pt, !stm);
            }
            move_piece(from_square, to_square, *moving_pt, stm);
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            if (*moving_pt == constants::PAWN) {
                phash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            }
            break;
        case Move::Type::DOUBLE_PUSH:
            move_piece(from_square, to_square, constants::PAWN, stm);
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            phash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
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
            remove_piece(stm == constants::WHITE ? Square(to_square - 8) : Square(to_square + 8),
                         constants::PAWN,
                         !stm);
            hash ^= zobrist::piece_square_key(stm == constants::WHITE ? Square(to_square - 8) : Square(to_square + 8), constants::PAWN, !stm);
            phash ^= zobrist::piece_square_key(stm == constants::WHITE ? Square(to_square - 8) : Square(to_square + 8), constants::PAWN, !stm);
            break;
        case Move::Type::CASTLING:
            move_piece(from_square, to_square, constants::KING, stm);
            hash ^= zobrist::piece_square_key(from_square, *moving_pt, stm) ^ zobrist::piece_square_key(to_square, *moving_pt, stm);
            switch (to_square) {
                case constants::C1:
                    move_piece(constants::A1, constants::D1, constants::ROOK, stm);
                    hash ^= zobrist::piece_square_key(constants::A1, constants::ROOK, stm) ^ zobrist::piece_square_key(constants::D1, constants::ROOK, stm);
                    break;
                case constants::G1:
                    move_piece(constants::H1, constants::F1, constants::ROOK, stm);
                    hash ^= zobrist::piece_square_key(constants::H1, constants::ROOK, stm) ^ zobrist::piece_square_key(constants::F1, constants::ROOK, stm);
                    break;
                case constants::C8:
                    move_piece(constants::A8, constants::D8, constants::ROOK, stm);
                    hash ^= zobrist::piece_square_key(constants::A8, constants::ROOK, stm) ^ zobrist::piece_square_key(constants::D8, constants::ROOK, stm);
                    break;
                case constants::G8:
                    move_piece(constants::H8, constants::F8, constants::ROOK, stm);
                    hash ^= zobrist::piece_square_key(constants::H8, constants::ROOK, stm) ^ zobrist::piece_square_key(constants::F8, constants::ROOK, stm);
                    break;
                default:
                    break;
            }
            break;
        case Move::Type::PROMOTION:
            remove_piece(from_square, constants::PAWN, stm);
            hash ^= zobrist::piece_square_key(from_square, constants::PAWN, stm);
            phash ^= zobrist::piece_square_key(from_square, constants::PAWN, stm);
            put_piece(to_square, *promotion_pt, stm);
            hash ^= zobrist::piece_square_key(to_square, *promotion_pt, stm);
            break;
        case Move::Type::CAPTURE_PROMOTION:
            remove_piece(to_square, *captured_pt, !stm);
            hash ^= zobrist::piece_square_key(to_square, *captured_pt, !stm);
            if (*captured_pt == constants::PAWN) {
                phash ^= zobrist::piece_square_key(to_square, *captured_pt, !stm);
            }
            remove_piece(from_square, constants::PAWN, stm);
            hash ^= zobrist::piece_square_key(from_square, constants::PAWN, stm);
            phash ^= zobrist::piece_square_key(from_square, constants::PAWN, stm);
            put_piece(to_square, *promotion_pt, stm);
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
}

}  // namespace libchess

#endif  // LIBCHESS_MOVEINTEGRATION_H
