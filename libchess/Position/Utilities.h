#ifndef LIBCHESS_UTILITIES_H
#define LIBCHESS_UTILITIES_H

#include "../../misc.h"

namespace libchess {

inline void Position::display_raw(std::ostream& ostream) const {
    ostream << "Pawn\n";
    ostream << piece_type_bb(constants::PAWN) << "\n";
    ostream << "Knight\n";
    ostream << piece_type_bb(constants::KNIGHT) << "\n";
    ostream << "Bishop\n";
    ostream << piece_type_bb(constants::BISHOP) << "\n";
    ostream << "Rook\n";
    ostream << piece_type_bb(constants::ROOK) << "\n";
    ostream << "Queen\n";
    ostream << piece_type_bb(constants::QUEEN) << "\n";
    ostream << "King\n";
    ostream << piece_type_bb(constants::KING) << "\n";
    ostream << "White\n";
    ostream << color_bb(constants::WHITE) << "\n";
    ostream << "Black\n";
    ostream << color_bb(constants::BLACK) << "\n";
    ostream << "Side to move: " << side_to_move() << "\n";
    ostream << "Castling rights: " << castling_rights() << "\n";
    ostream << "Enpassant square: " << (enpassant_square() ? enpassant_square()->to_str() : "-")
            << "\n";
    ostream << "Halfmoves: " << halfmoves() << "\n";
    ostream << "Fullmoves: " << fullmoves() << "\n";
    ostream << "Previous move: " << (previous_move() ? previous_move()->to_str() : "0000") << "\n";
    ostream << "Previously captured: "
            << (previously_captured_piece() ? previously_captured_piece()->to_char() : '-') << "\n";
    ostream << "Ply: " << ply() << "\n";
    ostream << "\n";
}

inline void Position::display(std::ostream& ostream) const {
    ostream << "8 ";
    for (Square square : constants::SQUARES) {
        Square sq = square;
        if (sq && !(sq & 7)) {
            ostream << "\n";
            ostream << 8 - sq.rank() << " ";
        }
        auto piece = piece_on(sq.flipped());
        if (!piece) {
            ostream << "- ";
        } else {
            ostream << *piece << " ";
        }
    }
    ostream << "\n" << "  A B C D E F G H";
    ostream << "\n";
}

inline std::string Position::fen() const {
    std::string fen_str;
    for (Rank rank = constants::RANK_8; rank >= constants::RANK_1; --rank) {
        int empty_sq_count = 0;
        for (File file = constants::FILE_A; file <= constants::FILE_H; ++file) {
            Square sq = *Square::from(file, rank);
            auto sq_piece = piece_on(sq);
            if (sq_piece) {
                if (empty_sq_count) {
                    fen_str += std::to_string(empty_sq_count);
                    empty_sq_count = 0;
                }
                fen_str += sq_piece->to_char();
            } else {
                ++empty_sq_count;
            }
        }
        if (empty_sq_count) {
            fen_str += std::to_string(empty_sq_count);
        }
        if (rank != constants::RANK_1) {
            fen_str += '/';
        }
    }

    fen_str += ' ';
    fen_str += side_to_move().to_char();
    fen_str += ' ';
    fen_str += castling_rights().to_str();
    fen_str += ' ';
    fen_str += (enpassant_square() ? enpassant_square()->to_str() : "-");
    fen_str += ' ';
    fen_str += std::to_string(halfmoves());
    fen_str += ' ';
    fen_str += std::to_string(fullmoves());

    return fen_str;
}

inline std::string Position::uci_line() const {
    std::string result = "position " + start_fen();
    result += " moves";
    for (int p = 1; p <= ply(); ++p) {
        auto prev_move = state(p).previous_move_;
        result += " " + (prev_move ? prev_move->to_str() : "0000");
    }
    return result;
}

inline void Position::vflip() {
    for (auto& pt : constants::PIECE_TYPES) {
        Bitboard* bb = piece_type_bb_ + pt;
        *bb = Bitboard{byteSwap(*bb)};
    }
    for (auto& c : constants::COLORS) {
        Bitboard* bb = color_bb_ + c;
        *bb = Bitboard{byteSwap(*bb)};
    }

    Bitboard tmp = color_bb_[0];
    color_bb_[0] = color_bb_[1];
    color_bb_[1] = tmp;

    State& curr_state = state_mut_ref();
    if (curr_state.enpassant_square_) {
        *curr_state.enpassant_square_ = curr_state.enpassant_square_->flipped();
    }

    CastlingRights tmp_cr = CastlingRights{(curr_state.castling_rights_.value() & 3) << 2};
    curr_state.castling_rights_.value_mut_ref() >>= 2;
    curr_state.castling_rights_.value_mut_ref() ^= tmp_cr.value();

    side_to_move_ = !side_to_move_;

    state_mut_ref().hash_ = calculate_hash();
    state_mut_ref().pawn_hash_ = calculate_pawn_hash();
}

inline std::optional<Move> Position::smallest_capture_move_to(Square square) const {
    Bitboard pawn_attackers_bb = lookups::pawn_attacks(square, !side_to_move()) &
                                 piece_type_bb(constants::PAWN, side_to_move());
    if (pawn_attackers_bb) {
        Square from_square = pawn_attackers_bb.forward_bitscan();
        auto enpassant_sq = enpassant_square();
        if (enpassant_sq && *enpassant_sq == square) {
            return Move{from_square, square, Move::Type::ENPASSANT};
        }
        if (lookups::relative_rank(square.rank(), side_to_move()) == constants::RANK_8) {
            return Move{from_square, square, constants::QUEEN, Move::Type::CAPTURE_PROMOTION};
        }
        return Move{from_square, square, Move::Type::CAPTURE};
    }

    auto piece_types_iter = constants::PIECE_TYPES + 1;
    auto piece_types_end = constants::PIECE_TYPES + 6;
    for (; piece_types_iter != piece_types_end; ++piece_types_iter) {
        Bitboard attackers_bb =
            lookups::non_pawn_piece_type_attacks(*piece_types_iter, square, occupancy_bb()) &
            piece_type_bb(*piece_types_iter, side_to_move());
        // don't take with the king if it puts us in check, cuz then the algorithm will take the king
        if (*piece_types_iter == constants::KING && attackers_to(square, !side_to_move())) {
            return std::nullopt;
        }
        if (attackers_bb) {
            return Move{attackers_bb.forward_bitscan(), square, Move::Type::CAPTURE};
        }
    }

    return std::nullopt;
}

// added by Krtoonbrat, based on the Stockfish implementation
inline bool Position::see_ge(Move move, int threshold) {
    // stockfish has this early exit for moves that are not a capture, double push, or normal
    if (move.type() != Move::Type::CAPTURE && move.type() != Move::Type::DOUBLE_PUSH &&
        move.type() != Move::Type::NORMAL) {
        return 0 >= threshold;
    }

    Square from_square = move.from_square(), to_square = move.to_square();
    auto from_pt = piece_type_on(from_square), to_pt = piece_type_on(to_square);

    int swap = pieceValuesMG[to_pt ? to_pt->value() : 5] - threshold;
    if (swap < 0) {
        return false;
    }

    swap = pieceValuesMG[from_pt ? from_pt->value() : 5] - swap;
    if (swap <= 0) {
        return true;
    }

    Bitboard occupied = occupancy_bb() ^ lookups::square(from_square) ^ lookups::square(to_square);
    Color stm = side_to_move();
    Bitboard attackers = attackers_to(to_square, occupied);
    Bitboard pinners[2] = {this->pinners(constants::WHITE), this->pinners(constants::BLACK)};
    Bitboard blockers[2] = {this->pinned_pieces_of(constants::WHITE), this->pinned_pieces_of(constants::BLACK)};
    Bitboard stmAttackers, bb;
    int res = 1;

    while (true) {
        stm = !stm;
        attackers &= occupied;

        // if stm has no more attackers then give up, stm loses
        if (!(stmAttackers = attackers & color_bb(stm))) {
            break;
        }

        // Don't allow pinned pieces to attack as long as there are pinners on their original square
        if (pinners[!stm] & occupied) {
            stmAttackers &= ~blockers[stm];

            if (!stmAttackers) {
                break;
            }

        }

        res ^= 1;

        // locate and remove the next least valuable attacker, and add to the bitboard
        // 'attackers' any x-ray attackers behind it
        if ((bb = stmAttackers & piece_type_bb(constants::PAWN))) {
            occupied ^= lookups::least_significant_square_bb(bb);
            if ((swap = pieceValuesMG[constants::PAWN] - swap) < res) {
                break;
            }

            attackers |= lookups::bishop_attacks(to_square, occupied) & (piece_type_bb(constants::BISHOP) | piece_type_bb(constants::QUEEN));

        }
        
        else if ((bb = stmAttackers & piece_type_bb(constants::KNIGHT))) {
            occupied ^= lookups::least_significant_square_bb(bb);
            if ((swap = pieceValuesMG[constants::KNIGHT] - swap) < res) {
                break;
            }
        }

        else if ((bb = stmAttackers & piece_type_bb(constants::BISHOP))) {
            occupied ^= lookups::least_significant_square_bb(bb);
            if ((swap = pieceValuesMG[constants::BISHOP] - swap) < res) {
                break;
            }

            attackers |= lookups::bishop_attacks(to_square, occupied) & (piece_type_bb(constants::BISHOP) | piece_type_bb(constants::QUEEN));

        }

        else if ((bb = stmAttackers & piece_type_bb(constants::ROOK))) {
            occupied ^= lookups::least_significant_square_bb(bb);
            if ((swap = pieceValuesMG[constants::ROOK] - swap) < res) {
                break;
            }

            attackers |= lookups::rook_attacks(to_square, occupied) & (piece_type_bb(constants::ROOK) | piece_type_bb(constants::QUEEN));

        }

        else if ((bb = stmAttackers & piece_type_bb(constants::QUEEN))) {
            occupied ^= lookups::least_significant_square_bb(bb);
            if ((swap = pieceValuesMG[constants::QUEEN] - swap) < res) {
                break;
            }

            attackers |= (lookups::bishop_attacks(to_square, occupied) & (piece_type_bb(constants::BISHOP) | piece_type_bb(constants::QUEEN))) |
                         (lookups::rook_attacks(to_square, occupied) & (piece_type_bb(constants::ROOK) | piece_type_bb(constants::QUEEN)));

        }
        
        // king, if we "capture" with the king but the opponent has attackers, reverse the result
        else {
            return (attackers & ~color_bb(stm)) ? res ^ 1 : res;
        }

    }

    return bool(res);

}

inline bool Position::is_on_semiopen_file(libchess::Color c, libchess::Square s) const {
    return !(piece_type_bb(constants::PAWN, c) & lookups::file_mask(s.file()));
}

// modified by Krtoonbrat
inline int Position::see_to(Square square, std::array<int, 6> piece_values) {
    auto smallest_capture_move = smallest_capture_move_to(square);
    if (!smallest_capture_move) {
        return 0;
    }
    bool is_enpassant = smallest_capture_move->type() == Move::Type::ENPASSANT;

    auto square_pt = piece_on(square);
    if (!square_pt && !is_enpassant) {
        return 0;
    }

    int piece_val = is_enpassant ? piece_values.at(0) : piece_values.at(square_pt->type().value());
    auto smallest_capture_move_prom_piece_type = smallest_capture_move->promotion_piece_type();
    if (smallest_capture_move_prom_piece_type) {
        piece_val +=
            piece_values.at(smallest_capture_move_prom_piece_type->value()) - piece_values.at(0);
    }

    // removed the need to copy the entire position and operate on the new position.  This improved performance as
    // there is now no need to allocate memory, which was a huge time waster.
    //Position pos = *this;
    this->make_move(*smallest_capture_move);
    int seeVal = piece_val - this->see_to(square, piece_values);
    this->unmake_move();

    return seeVal;
}

// modified by Krtoonbrat
inline int Position::see_for(Move move, std::array<int, 6> piece_values) {
    bool is_enpassant = move_type_of(move) == Move::Type::ENPASSANT;

    auto square_pt = piece_on(move.to_square());
    if (!square_pt && !is_enpassant) {
        return 0;
    }

    int piece_val = is_enpassant ? piece_values.at(0) : piece_values.at(square_pt->type().value());
    auto move_prom_piece_type = move.promotion_piece_type();
    if (move_prom_piece_type) {
        piece_val += piece_values.at(move_prom_piece_type->value()) - piece_values.at(0);
    }

    // removed the need to copy the entire position and operate on the new position.  This improved performance as
    // there is now no need to allocate memory, which was a huge time waster.
    //Position pos = *this;
    this->make_move(move);
    int seeVal = piece_val - this->see_to(move.to_square(), piece_values);
    this->unmake_move();

    return seeVal;

    }

inline std::optional<Position> Position::from_fen(const std::string& fen) {
    Position pos;
    pos.createHistory();
    State& curr_state = pos.state_mut_ref();

    std::istringstream fen_stream{fen};
    std::string fen_part;

    // Piece list
    fen_stream >> fen_part;
    Square current_square = constants::A8;
    for (char c : fen_part) {
        if (c >= '1' && c <= '9') {
            current_square += (c - '0');
        } else if (c == '/') {
            current_square -= 16;
        } else {
            auto piece = Piece::from(c);
            if (piece) {
                pos.put_piece(current_square, piece->type(), piece->color());
            }
            ++current_square;
        }
    }

    // Side to move
    fen_stream >> fen_part;
    pos.side_to_move_ = Color::from(fen_part[0]).value_or(constants::WHITE);

    // Castling rights
    fen_stream >> fen_part;
    curr_state.castling_rights_ = CastlingRights::from(fen_part);

    // Enpassant square
    fen_stream >> fen_part;
    curr_state.enpassant_square_ = Square::from(fen_part);

    // Halfmoves
    fen_stream >> fen_part;
    const char* fen_part_cstr = fen_part.c_str();
    char* end;
    curr_state.halfmoves_ = std::strtol(fen_part_cstr, &end, 10);

    // Fullmoves
    fen_stream >> fen_part;
    fen_part_cstr = fen_part.c_str();
    pos.fullmoves_ = std::strtol(fen_part_cstr, &end, 10);

    pos.state_mut_ref().hash_ = pos.calculate_hash();
    pos.state_mut_ref().pawn_hash_ = pos.calculate_pawn_hash();
    pos.start_fen_ = fen;

    // nnue
    pos.state_mut_ref().nnue.accumulator.computedAccumulation = 0;

    return pos;
}

inline std::optional<Position> Position::from_uci_position_line(const std::string& line) {
    /// This function expects a string as a parameter in one of the following formats:
    /// * `"position <fen> moves <move-list>"`.
    std::istringstream line_stream{line};
    std::string tmp;

    // "position"
    line_stream >> tmp;
    if (tmp != "position") {
        return {};
    }

    std::string fen;
    // Piece list
    line_stream >> tmp;
    fen += tmp + " ";
    // Side to move
    line_stream >> tmp;
    fen += tmp + " ";
    // Castling rights
    line_stream >> tmp;
    fen += tmp + " ";
    // Enpassant square
    line_stream >> tmp;
    fen += tmp + " ";
    // Halfmoves
    line_stream >> tmp;
    fen += tmp + " ";
    // Fullmoves
    line_stream >> tmp;
    fen += tmp;

    // "moves"
    line_stream >> tmp;
    if (tmp != "moves") {
        return {};
    }

    // Moves
    auto pos = Position::from_fen(fen);
    if (!pos) {
        return {};
    }
    std::string move_str;
    while (line_stream >> move_str) {
        auto move = Move::from(move_str);
        if (!move) {
            return {};
        }
        pos->make_move(*move);
    }

    return pos;
}

}  // namespace libchess

#endif  // LIBCHESS_UTILITIES_H
