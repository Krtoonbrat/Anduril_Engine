#ifndef LIBCHESS_POSITION_H
#define LIBCHESS_POSITION_H

#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "Bitboard.h"
#include "CastlingRights.h"
#include "Color.h"
#include "Lookups.h"
#include "Move.h"
#include "Piece.h"
#include "PieceType.h"
#include "Square.h"
#include "internal/Zobrist.h"

namespace libchess {
    // values for the pieces
    static constexpr int pieceValuesMG[6] = {88, 337, 365, 477, 1025, 0};
    static constexpr int pieceValuesEG[6] = {138, 281, 297, 512, 936, 0};

    // piece square tables
    // values from Rofchade: http://www.talkchess.com/forum3/viewtopic.php?f=2&t=68311&start=19
    static constexpr int pieceSquareTableMG[6][64] = {
            // pawns
            {
                    0,   0,   0,   0,   0,   0,  0,   0,
                    98, 134,  61,  95,  68, 126, 34, -11,
                    -6,   7,  26,  31,  65,  56, 25, -20,
                    -14,  13,   6,  21,  23,  12, 17, -23,
                    -27,  -2,  -5,  22,  27,   6, 10, -25,
                    -26,  -4,  -4, -10,   3,   3, 33, -12,
                    -35,  -1, -20, -33, -27,  24, 38, -22,
                    0,   0,   0,   0,   0,   0,  0,   0,
            },
            // knights
            {
                    -167, -89, -34, -49,  61, -97, -15, -107,
                    -73, -41,  72,  36,  23,  62,   7,  -17,
                    -47,  60,  37,  65,  84, 129,  73,   44,
                    -9,  17,  19,  53,  37,  69,  18,   22,
                    -13,   4,  16,  13,  28,  19,  21,   -8,
                    -23,  -9,  12,  10,  19,  17,  25,  -16,
                    -29, -53, -12,  -3,  -1,  18, -14,  -19,
                    -105, -29, -58, -33, -17, -28, -29,  -23,
            },
            // bishops
            {
                    -29,   4, -82, -37, -25, -42,   7,  -8,
                    -26,  16, -18, -13,  30,  59,  18, -47,
                    -16,  37,  43,  40,  35,  50,  37,  -2,
                    -4,   5,  19,  50,  37,  37,   7,  -2,
                    -6,  13,  13,  26,  34,  12,  10,   4,
                    0,  15,  15,  15,  14,  27,  18,  10,
                    4,  15,  16,   0,   7,  21,  33,   1,
                    -33,  -3, -27, -21, -13, -22, -39, -21,
            },
            // rooks
            {
                    32,  42,  32,  51, 63,  9,  31,  43,
                    27,  32,  58,  62, 80, 67,  26,  44,
                    -5,  19,  26,  36, 17, 45,  61,  16,
                    -24, -11,   7,  26, 24, 35,  -8, -20,
                    -36, -26, -12,  -1,  9, -7,   6, -23,
                    -45, -25, -16, -17,  3,  0,  -5, -33,
                    -44, -16, -20,  -9, -1, 11,  -6, -71,
                    -19, -13,   1,  17, 16,  7, -37, -26,
            },
            // queens
            {
                    -28,   0,  29,  12,  59,  44,  43,  45,
                    -24, -39,  -5,   1, -16,  57,  28,  54,
                    -13, -17,   7,   8,  29,  56,  47,  57,
                    -27, -27, -16, -16,  -1,  17,  -2,   1,
                    -9, -26,  -9, -10,  -2,  -4,   3,  -3,
                    -14,   2, -11,  -2,  -5,   2,  14,   5,
                    -35,  -8,  11,   2,   8,  15,  -3,   1,
                    -1, -18,  -9,  10, -15, -25, -31, -50,
            },
            // kings
            {
                    -65,  23,  16, -15, -56, -34,   2,  13,
                    29,  -1, -20,  -7,  -8,  -4, -38, -29,
                    -9,  24,   2, -16, -20,   6,  22, -22,
                    -17, -20, -12, -27, -30, -25, -14, -36,
                    -49,  -1, -27, -39, -46, -44, -33, -51,
                    -14, -14, -22, -46, -44, -30, -15, -27,
                    1,   7,  -8, -64, -43, -16,   9,   8,
                    -15,  36,  12, -54, -25, -28,  24,  14,
            }
    };

    static constexpr int pieceSquareTableEG[6][64] = {
            // pawns
            {
                    0,   0,   0,   0,   0,   0,   0,   0,
                    178, 173, 158, 134, 147, 132, 165, 187,
                    94, 100,  85,  67,  56,  53,  82,  84,
                    32,  24,  13,   5,  -2,   4,  17,  17,
                    13,   9,  -3,  -7,  -7,  -8,   3,  -1,
                    4,   7,  -6,   1,   0,  -5,  -1,  -8,
                    13,   8,   8,  10,  13,   0,   2,  -7,
                    0,   0,   0,   0,   0,   0,   0,   0,
            },
            // knights
            {
                    -58, -38, -13, -28, -31, -27, -63, -99,
                    -25,  -8, -25,  -2,  -9, -25, -24, -52,
                    -24, -20,  10,   9,  -1,  -9, -19, -41,
                    -17,   3,  22,  22,  22,  11,   8, -18,
                    -18,  -6,  16,  25,  16,  17,   4, -18,
                    -23,  -3,  -1,  15,  10,  -3, -20, -22,
                    -42, -20, -10,  -5,  -2, -20, -23, -44,
                    -29, -51, -23, -15, -22, -18, -50, -64,
            },
            // bishops
            {
                    -14, -21, -11,  -8, -7,  -9, -17, -24,
                    -8,  -4,   7, -12, -3, -13,  -4, -14,
                    2,  -8,   0,  -1, -2,   6,   0,   4,
                    -3,   9,  12,   9, 14,  10,   3,   2,
                    -6,   3,  13,  19,  7,  10,  -3,  -9,
                    -12,  -3,   8,  10, 13,   3,  -7, -15,
                    -14, -18,  -7,  -1,  4,  -9, -15, -27,
                    -23,  -9, -23,  -5, -9, -16,  -5, -17,
            },
            // rooks
            {
                    13, 10, 18, 15, 12,  12,   8,   5,
                    11, 13, 13, 11, -3,   3,   8,   3,
                    7,  7,  7,  5,  4,  -3,  -5,  -3,
                    4,  3, 13,  1,  2,   1,  -1,   2,
                    3,  5,  8,  4, -5,  -6,  -8, -11,
                    -4,  0, -5, -1, -7, -12,  -8, -16,
                    -6, -6,  0,  2, -9,  -9, -11,  -3,
                    -9,  2,  3, -1, -5, -13,   4, -20,
            },
            // queens
            {
                    -9,  22,  22,  27,  27,  19,  10,  20,
                    -17,  20,  32,  41,  58,  25,  30,   0,
                    -20,   6,   9,  49,  47,  35,  19,   9,
                    3,  22,  24,  45,  57,  40,  57,  36,
                    -18,  28,  19,  47,  31,  34,  39,  23,
                    -16, -27,  15,   6,   9,  17,  10,   5,
                    -22, -23, -30, -16, -16, -23, -36, -32,
                    -33, -28, -22, -43,  -5, -32, -20, -41,
            },
            // kings
            {
                    -74, -35, -18, -18, -11,  15,   4, -17,
                    -12,  17,  14,  17,  17,  38,  23,  11,
                    10,  17,  23,  15,  20,  45,  44,  13,
                    -8,  22,  24,  27,  26,  33,  26,   3,
                    -18,  -4,  21,  24,  27,  23,   9, -11,
                    -19,  -3,  11,  21,  23,  16,   7,  -9,
                    -27, -11,   4,  13,  14,   4,  -5, -17,
                    -53, -34, -21, -11, -28, -14, -24, -43
            }
    };

namespace constants {

static std::string STARTPOS_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

}  // namespace constants

class Position {
   private:
    Position() : side_to_move_(constants::WHITE), ply_(0) {
    }

   public:
    explicit Position(const std::string& fen_str) : Position() {
        *this = *Position::from_fen(fen_str);
    }
    using hash_type = std::uint64_t;

    enum class GameState
    {
        IN_PROGRESS,
        CHECKMATE,
        STALEMATE,
        THREEFOLD_REPETITION,
        FIFTY_MOVES
    };

    // Getters
    [[nodiscard]] Bitboard piece_type_bb(PieceType piece_type) const;
    [[nodiscard]] Bitboard piece_type_bb(PieceType piece_type, Color color) const;
    [[nodiscard]] Bitboard color_bb(Color color) const;
    [[nodiscard]] Bitboard occupancy_bb() const;
    [[nodiscard]] Color side_to_move() const;
    [[nodiscard]] CastlingRights castling_rights() const;
    [[nodiscard]] std::optional<Square> enpassant_square() const;
    [[nodiscard]] std::optional<Move> previous_move() const;
    [[nodiscard]] std::optional<PieceType> previously_captured_piece() const;
    [[nodiscard]] std::optional<PieceType> piece_type_on(Square square) const;
    [[nodiscard]] std::optional<Color> color_of(Square square) const;
    [[nodiscard]] std::optional<Piece> piece_on(Square square) const;
    [[nodiscard]] hash_type hash() const;
    [[nodiscard]] hash_type hashAfter(Move move);
    [[nodiscard]] hash_type pawn_hash() const;
    [[nodiscard]] Square king_square(Color color) const;
    [[nodiscard]] int halfmoves() const;
    [[nodiscard]] int fullmoves() const;
    [[nodiscard]] bool in_check() const;
    [[nodiscard]] bool is_repeat(int times = 1) const;
    [[nodiscard]] int repeat_count() const;
    [[nodiscard]] const std::string& start_fen() const;
    [[nodiscard]] GameState game_state() const;

    // Move Integration
    [[nodiscard]] Move::Type move_type_of(Move move) const;
    [[nodiscard]] bool is_capture_move(Move move) const;
    [[nodiscard]] bool is_promotion_move(Move move) const;
    [[nodiscard]] bool is_legal_move(Move move) const;
    [[nodiscard]] bool is_legal_generated_move(Move move) const;
    void unmake_move();
    void make_move(Move move);
    void make_null_move();

    // added by Krtoonbrat
    [[nodiscard]] bool gives_check(Move move) const;

    // Attacks
    [[nodiscard]] Bitboard checkers_to(Color c) const;
    [[nodiscard]] Bitboard attackers_to(Square square) const;
    [[nodiscard]] Bitboard attackers_to(Square square, Color c) const;
    [[nodiscard]] Bitboard attackers_to(Square square, Bitboard occupancy) const;
    [[nodiscard]] Bitboard attackers_to(Square square, Bitboard occupancy, Color c) const;
    [[nodiscard]] Bitboard attacks_of_piece_on(Square square) const;
    [[nodiscard]] Bitboard pinned_pieces_of(Color c) const;

    // Move Generation
    void generate_quiet_promotions(MoveList& move_list, Color stm) const;
    void generate_capture_promotions(MoveList& move_list, Color stm) const;
    void generate_promotions(MoveList& move_list, Color stm) const;
    void generate_pawn_quiets(MoveList& move_list, Color stm) const;
    void generate_pawn_captures(MoveList& move_list, Color stm) const;
    void generate_pawn_moves(MoveList& move_list, Color stm) const;
    void generate_non_pawn_quiets(PieceType pt, MoveList& move_list, Color stm) const;
    void generate_non_pawn_captures(PieceType pt, MoveList& move_list, Color stm) const;
    void generate_knight_moves(MoveList& move_list, Color stm) const;
    void generate_bishop_moves(MoveList& move_list, Color stm) const;
    void generate_rook_moves(MoveList& move_list, Color stm) const;
    void generate_queen_moves(MoveList& move_list, Color stm) const;
    void generate_king_moves(MoveList& move_list, Color stm) const;
    void generate_castling(MoveList& move_list, Color stm) const;
    void generate_checker_block_moves(MoveList& move_list, Color stm) const;
    void generate_checker_capture_moves(MoveList& move_list, Color stm) const;
    void generate_quiet_moves(MoveList& move_list, Color stm) const;
    void generate_capture_moves(MoveList& move_list, Color stm) const;
    [[nodiscard]] MoveList check_evasion_move_list(Color stm) const;
    [[nodiscard]] MoveList pseudo_legal_move_list(Color stm) const;
    [[nodiscard]] MoveList legal_move_list(Color stm) const;
    [[nodiscard]] MoveList check_evasion_move_list() const;
    [[nodiscard]] MoveList pseudo_legal_move_list() const;
    [[nodiscard]] MoveList legal_move_list() const;

    // Utilities
    void display_raw(std::ostream& ostream = std::cout) const;
    void display(std::ostream& ostream = std::cout) const;
    [[nodiscard]] std::string fen() const;
    [[nodiscard]] std::string uci_line() const;
    void vflip();
    [[nodiscard]] std::optional<Move> smallest_capture_move_to(Square square) const;
    int see_to(Square square, std::array<int, 6> piece_values);
    int see_for(Move move, std::array<int, 6> piece_values);
    static std::optional<Position> from_fen(const std::string& fen);
    static std::optional<Position> from_uci_position_line(const std::string& line);

    // added by Krtoonbrat
    // One of the largest performance hits in Anduril currently is allocating memory for the state vector when we
    // make a move.  If we make sure the capacity of the vector is ply+100 (100 being Anduril's max depth) at root, the
    // vector shouldn't have to dynamically allocate, and if it does it won't be often enough to be significant
    void prep_search() {
        return;
    }

    int getPSQTMG() { return state().scoreMG; }
    int getPSQTEG() { return state().scoreEG; }
    Move getExcluded() { return state().excludedMove; }
    void setExcluded(Move move) { state_mut_ref().excludedMove = move; }

protected:
    // clang-format off
    constexpr static int castling_spoilers[64] = {
        13, 15, 15, 15, 12, 15, 15, 14,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        7,  15, 15, 15, 3,  15, 15, 11
    };
    // clang-format on

    struct State {
        CastlingRights castling_rights_;
        std::optional<Square> enpassant_square_;
        std::optional<Move> previous_move_;
        std::optional<PieceType> captured_pt_;
        Move::Type move_type_ = Move::Type::NONE;
        hash_type hash_ = 0;
        hash_type pawn_hash_ = 0;
        int halfmoves_ = 0;
        int scoreMG = 0;
        int scoreEG = 0;
        Move excludedMove = Move(0);
    };

    [[nodiscard]] int ply() const {
        return ply_;
    }

    [[nodiscard]] const std::array<State, 1000>& history() const {
        return history_;
    }
    State& state_mut_ref() {
        return history_[ply()];
    }
    State& state_mut_ref(int ply) {
        return history_[ply];
    }
    [[nodiscard]] const State& state() const {
        return history_[ply()];
    }
    [[nodiscard]] const State& state(int ply) const {
        return history_[ply];
    }
    [[nodiscard]] hash_type calculate_hash() const {
        hash_type hash_value = 0;
        for (Color c : constants::COLORS) {
            for (PieceType pt : constants::PIECE_TYPES) {
                Bitboard bb = piece_type_bb(pt, c);
                while (bb) {
                    hash_value ^= zobrist::piece_square_key(bb.forward_bitscan(), pt, c);
                    bb.forward_popbit();
                }
            }
        }
        auto ep_sq = enpassant_square();
        if (ep_sq) {
            hash_value ^= zobrist::enpassant_key(*ep_sq);
        }
        CastlingRights rights = castling_rights();
        if (rights.is_allowed(constants::WHITE_KINGSIDE)) {
            hash_value ^= zobrist::castling_rights_key(rights);
            rights.disallow(constants::WHITE_KINGSIDE);
        }
        if (rights.is_allowed(constants::WHITE_QUEENSIDE)) {
            hash_value ^= zobrist::castling_rights_key(rights);
            rights.disallow(constants::WHITE_QUEENSIDE);
        }
        if (rights.is_allowed(constants::BLACK_KINGSIDE)) {
            hash_value ^= zobrist::castling_rights_key(rights);
            rights.disallow(constants::BLACK_KINGSIDE);
        }
        if (rights.is_allowed(constants::BLACK_QUEENSIDE)) {
            hash_value ^= zobrist::castling_rights_key(rights);
            rights.disallow(constants::BLACK_QUEENSIDE);
        }
        //hash_value ^= zobrist::castling_rights_key(castling_rights());
        hash_value ^= zobrist::side_to_move_key(side_to_move());
        return hash_value;
    }
    [[nodiscard]] hash_type calculate_pawn_hash() const {
        hash_type hash_value = 0;
        for (Color c : constants::COLORS) {
            Bitboard bb = piece_type_bb(constants::PAWN, c);
            while (bb) {
                hash_value ^= zobrist::piece_square_key(bb.forward_bitscan(), constants::PAWN, c);
                bb.forward_popbit();
            }
        }
        return hash_value;
    }

    void put_piece(Square square, PieceType piece_type, Color color) {
        Bitboard square_bb = Bitboard{square};
        piece_type_bb_[piece_type.value()] |= square_bb;
        color_bb_[color.value()] |= square_bb;
    }
    void remove_piece(Square square, PieceType piece_type, Color color) {
        Bitboard square_bb = Bitboard{square};
        piece_type_bb_[piece_type.value()] &= ~square_bb;
        color_bb_[color.value()] &= ~square_bb;
    }
    void move_piece(Square from_square, Square to_square, PieceType piece_type, Color color) {
        Bitboard from_to_sqs_bb = Bitboard{from_square} ^ Bitboard { to_square };
        piece_type_bb_[piece_type.value()] ^= from_to_sqs_bb;
        color_bb_[color.value()] ^= from_to_sqs_bb;
    }
    void reverse_side_to_move() {
        side_to_move_ = !side_to_move_;
    }

   private:
    Bitboard piece_type_bb_[6];
    Bitboard color_bb_[2];
    Color side_to_move_;
    int fullmoves_;
    int ply_;
    std::array<State, 1000> history_;

    std::string start_fen_;
};

}  // namespace libchess

#include "Position/Attacks.h"
#include "Position/Getters.h"
#include "Position/MoveGeneration.h"
#include "Position/MoveIntegration.h"
#include "Position/Utilities.h"

#endif  // LIBCHESS_POSITION_H
