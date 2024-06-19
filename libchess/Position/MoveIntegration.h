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

inline bool Position::gives_check(Move move) const {
    Color stm = side_to_move();
    Bitboard occ_after_move = occupancy_bb();
    Bitboard king = piece_type_bb(constants::KING, !stm);

    // we need this because the king won't be the one checking if we are castling
    Square moving_square(0);

    switch (move.type()) {
        // normal, double push, promotion all just need to move the piece
        case Move::Type::NORMAL:
        case Move::Type::DOUBLE_PUSH:
        case Move::Type::PROMOTION:
            occ_after_move ^= Bitboard{ move.from_square() } ^ Bitboard{ move.to_square() };
            moving_square = move.to_square();
            break;
        // captures and capture promotions need to move the piece, but don't change the to square as that was already occupied
        case Move::Type::CAPTURE:
        case Move::Type::CAPTURE_PROMOTION:
            occ_after_move ^= Bitboard{ move.from_square() };
            moving_square = move.to_square();
            break;
        case Move::Type::ENPASSANT:
            occ_after_move ^= Bitboard{ move.from_square() } ^ Bitboard{ move.to_square() };
            occ_after_move ^= Bitboard{ stm == constants::WHITE ? move.to_square() - 8 : move.to_square() + 8 };
            moving_square = move.to_square();
            break;
        case Move::Type::CASTLING:
            occ_after_move ^= Bitboard{ move.from_square() } ^ Bitboard{ move.to_square() };
            if (move.to_square().file() == constants::FILE_C) {
                occ_after_move ^= (lookups::FILE_A_MASK ^ lookups::FILE_D_MASK) & lookups::rank_mask(move.to_square().rank());
                moving_square = move.to_square() + 1;
            }
            else {
                occ_after_move ^= (lookups::FILE_H_MASK ^ lookups::FILE_F_MASK) & lookups::rank_mask(move.to_square().rank());
                moving_square = move.to_square() - 1;
            }
            break;
        case Move::Type::NONE:
            return false;

    }

    // first find direct checks from the moving piece
    if (move.type() == Move::Type::PROMOTION || move.type() == Move::Type::CAPTURE_PROMOTION) {
        if (lookups::non_pawn_piece_type_attacks(*move.promotion_piece_type(), moving_square, occ_after_move) & king) {
            return true;
        }
    }
    else if (piece_type_on(move.from_square()) == constants::PAWN) {
        if (lookups::pawn_attacks(moving_square, stm) & king) {
            return true;
        }
    }
    else {
        if (lookups::non_pawn_piece_type_attacks(*piece_type_on(move.from_square()), moving_square, occ_after_move) & king) {
            return true;
        }
    }

    // look for discovered checks if there were no direct checks
    return attackers_to(king.forward_bitscan(), occ_after_move) & ~color_bb(!stm);
}

// added by Krtoonbrat
// constructs a move from a 16 bit integer
inline Move Position::from_table(uint16_t move) {
    Move m;

    // grab the to and from squares
    Square from = Square{move & 0x3f};
    Square to = Square{(move & 0xfc0) >> 6};

    // we get the move type before promotion type because we might not need promotion type
    uint8_t type = (0xc000 & move) >> 14;

    // construct and return the move
    switch(type) {
        case 2:
            return Move{from, to, Move::Type::ENPASSANT};
        case 3:
            return Move{from, to, Move::Type::CASTLING};
        default:
            m = Move{from, to};
            return Move{from, to, move_type_of(m)};
        case 1:
            uint8_t promotion = (0x3000 & move) >> 12;
            PieceType promotion_pt = promotion == 0 ? constants::KNIGHT
                                        : promotion == 1 ? constants::BISHOP
                                        : promotion == 2 ? constants::ROOK
                                        : constants::QUEEN;
            m = Move{from, to, promotion_pt};
            return Move{from, to, promotion_pt, move_type_of(m)};
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
    State& prev_state = state_mut_ref(ply_ - 1);
    State& next_state = state_mut_ref();
    next_state.halfmoves_ = prev_state.halfmoves_ + 1;
    next_state.pliesSinceNull = prev_state.pliesSinceNull + 1;
    next_state.previous_move_ = move;
    next_state.enpassant_square_ = {};

    Square from_square = move.from_square();
    Square to_square = move.to_square();

    Square possiblePassant(0);
    Bitboard enpassant(0);

    next_state.castling_rights_ = CastlingRights{prev_state.castling_rights_.value() &
                                                 castling_spoilers[from_square.value()] &
                                                 castling_spoilers[to_square.value()]};

    Move::Type move_type = move_type_of(move);

    std::optional<PieceType> moving_pt = piece_type_on(from_square);
    std::optional<PieceType> captured_pt = move_type == Move::Type::ENPASSANT ?
                                           stm == constants::WHITE            ? piece_type_on(to_square - 8)
                                                                              : piece_type_on(to_square + 8)
                                                                              : piece_type_on(to_square);
    std::optional<PieceType> promotion_pt = move.promotion_piece_type();

    hash_type hash = prev_state.hash_;
    hash_type phash = prev_state.pawn_hash_;
    if (prev_state.enpassant_square_) {
        hash ^= zobrist::enpassant_key(*prev_state.enpassant_square_);
    }
    hash ^= zobrist::side_to_move_key(constants::WHITE);

    if (moving_pt == constants::PAWN || captured_pt) {
        next_state.halfmoves_ = 0;
    }

    Square epCapSquare = stm == constants::WHITE ? Square(to_square - 8) : Square(to_square + 8);

    // update nnue data
    NNUEdata& nnue_next = next_state.nnue;
    DirtyPiece *dp = &(nnue_next.dirtyPiece);
    nnue_next.accumulator.computedAccumulation = 0;
    dp->dirtyNum = 0;

    // move piece
    dp->pc[0] = piece_on(from_square)->to_nnue();
    dp->from[0] = from_square.value();
    dp->to[0] = to_square.value();
    dp->dirtyNum++;

    // remove captured piece
    if (captured_pt) {
        dp->pc[1] = piece_on(to_square)->to_nnue();
        dp->from[1] = move_type == Move::Type::ENPASSANT ? epCapSquare.value() : to_square.value();
        dp->to[1] = 64;
        dp->dirtyNum++;
    }

    // promotion
    if (promotion_pt) {
        dp->to[0] = 64;
        dp->pc[2] = Piece::from(promotion_pt, stm)->to_nnue();
        dp->from[2] = 64;
        dp->to[2] = to_square.value();
        dp->dirtyNum++;
    }

    // castling
    if (move_type == Move::Type::CASTLING) {
        dp->pc[1] = Piece::from(constants::ROOK, stm)->to_nnue();
        switch (to_square) {
            case constants::C1:
                dp->from[1] = constants::A1.value();
                dp->to[1] = constants::D1.value();
                break;
            case constants::G1:
                dp->from[1] = constants::H1.value();
                dp->to[1] = constants::F1.value();
                break;
            case constants::C8:
                dp->from[1] = constants::A8.value();
                dp->to[1] = constants::D8.value();
                break;
            case constants::G8:
                dp->from[1] = constants::H8.value();
                dp->to[1] = constants::F8.value();
                break;
            default:
                break;
        }
        dp->dirtyNum++;
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

            remove_piece(epCapSquare,constants::PAWN,!stm);
            hash ^= zobrist::piece_square_key(epCapSquare, constants::PAWN, !stm);
            phash ^= zobrist::piece_square_key(epCapSquare, constants::PAWN, !stm);
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
    State& prev = state_mut_ref(ply_ - 1);
    State& next = state_mut_ref();
    reverse_side_to_move();
    next.previous_move_ = {};
    next.halfmoves_ = prev.halfmoves_ + 1;
    next.pliesSinceNull = 0;
    next.enpassant_square_ = {};
    next.castling_rights_ = prev.castling_rights_;
    next.captured_pt_ = {};
    next.move_type_ = Move::Type::NONE;

    // editied by Krtoonbrat
    // We should be able to incrementally update the hash for null moves.  The only things that change are the turn
    // and enpassant
    next.hash_ = prev.hash_ ^ zobrist::side_to_move_key(constants::WHITE);
    if (prev.enpassant_square_) {
        next.hash_ ^= zobrist::enpassant_key(*prev.enpassant_square_);
    }
    // the pawn hash shouldn't change at all
    next.pawn_hash_ = prev.pawn_hash_;

    // update nnue data
    memcpy(&next.nnue.accumulator, &prev.nnue.accumulator, sizeof(Accumulator));
    DirtyPiece *dp = &(next.nnue.dirtyPiece);
    dp->dirtyNum = 0;
}

}  // namespace libchess

#endif  // LIBCHESS_MOVEINTEGRATION_H
