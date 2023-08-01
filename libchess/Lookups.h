#ifndef LIBCHESS_LOOKUPS_H
#define LIBCHESS_LOOKUPS_H

#include <cassert>
#include <array>
#include <memory>
#include <random>

#include "Bitboard.h"
#include "Color.h"
#include "PieceType.h"

// for PEXT instructions
#if defined(USE_PEXT)
#include <immintrin.h>
#define pext(b, m) _pext_u64(b, m)
constexpr bool has_pext = true;
#else
#define pext(b, m) 0
constexpr bool has_pext = false;
#endif

namespace libchess::lookups {

static Bitboard RANK_1_MASK{std::uint64_t(0xff)};
static Bitboard RANK_2_MASK{std::uint64_t(0xff00)};
static Bitboard RANK_3_MASK{std::uint64_t(0xff0000)};
static Bitboard RANK_4_MASK{std::uint64_t(0xff000000)};
static Bitboard RANK_5_MASK{std::uint64_t(0xff00000000)};
static Bitboard RANK_6_MASK{std::uint64_t(0xff0000000000)};
static Bitboard RANK_7_MASK{std::uint64_t(0xff000000000000)};
static Bitboard RANK_8_MASK{std::uint64_t(0xff00000000000000)};
static Bitboard FILE_A_MASK{std::uint64_t(0x0101010101010101)};
static Bitboard FILE_B_MASK{std::uint64_t(0x0202020202020202)};
static Bitboard FILE_C_MASK{std::uint64_t(0x0404040404040404)};
static Bitboard FILE_D_MASK{std::uint64_t(0x0808080808080808)};
static Bitboard FILE_E_MASK{std::uint64_t(0x1010101010101010)};
static Bitboard FILE_F_MASK{std::uint64_t(0x2020202020202020)};
static Bitboard FILE_G_MASK{std::uint64_t(0x4040404040404040)};
static Bitboard FILE_H_MASK{std::uint64_t(0x8080808080808080)};

static std::array<Bitboard, 8> RANK_MASK = {RANK_1_MASK,
                                            RANK_2_MASK,
                                            RANK_3_MASK,
                                            RANK_4_MASK,
                                            RANK_5_MASK,
                                            RANK_6_MASK,
                                            RANK_7_MASK,
                                            RANK_8_MASK};
static std::array<Bitboard, 8> FILE_MASK = {FILE_A_MASK,
                                            FILE_B_MASK,
                                            FILE_C_MASK,
                                            FILE_D_MASK,
                                            FILE_E_MASK,
                                            FILE_F_MASK,
                                            FILE_G_MASK,
                                            FILE_H_MASK};

inline Bitboard rank_mask(Rank rank) {
    return RANK_MASK[rank.value()];
}
inline Bitboard file_mask(File file) {
    return FILE_MASK[file.value()];
}

namespace init {

inline std::array<Bitboard, 64> north() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::A1; sq <= constants::H7; ++sq) {
        Bitboard bb;
        for (Square atk_sq = sq + 8; atk_sq <= constants::H8; atk_sq = atk_sq + 8) {
            bb |= Bitboard{atk_sq};
        }
        attacks[sq] = bb;
    }
    return attacks;
}

inline std::array<Bitboard, 64> south() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::A2; sq <= constants::H8; ++sq) {
        Bitboard bb;
        for (Square atk_sq = sq - 8; atk_sq >= constants::A1; atk_sq = atk_sq - 8) {
            bb |= Bitboard{atk_sq};
        }
        attacks[sq] = bb;
    }
    return attacks;
}

inline std::array<Bitboard, 64> east() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::A1; sq <= constants::G8; ++sq) {
        Bitboard bb;
        for (Square atk_sq = sq + 1; atk_sq <= constants::H8; atk_sq = atk_sq + 1) {
            if (Bitboard{atk_sq} & FILE_A_MASK) {
                break;
            }
            bb |= Bitboard{atk_sq};
        }
        attacks[sq] = bb;
    }
    return attacks;
}

inline std::array<Bitboard, 64> west() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::B1; sq <= constants::H8; ++sq) {
        Bitboard bb;
        for (Square atk_sq = sq - 1; atk_sq >= constants::A1; atk_sq = atk_sq - 1) {
            if (Bitboard{atk_sq} & FILE_H_MASK) {
                break;
            }
            bb |= Bitboard{atk_sq};
        }
        attacks[sq] = bb;
    }
    return attacks;
}

inline std::array<Bitboard, 64> northwest() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::A1; sq <= constants::H7; ++sq) {
        Bitboard bb;
        for (Square atk_sq = sq + 7; atk_sq <= constants::H8; atk_sq = atk_sq + 7) {
            if (Bitboard{atk_sq} & FILE_H_MASK) {
                break;
            }
            bb |= Bitboard{atk_sq};
        }
        attacks[sq] = bb;
    }
    return attacks;
}

inline std::array<Bitboard, 64> southwest() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::B2; sq <= constants::H8; ++sq) {
        Bitboard bb;
        for (Square atk_sq = sq - 9; atk_sq >= constants::A1; atk_sq = atk_sq - 9) {
            if (Bitboard{atk_sq} & FILE_H_MASK) {
                break;
            }
            bb |= Bitboard{atk_sq};
        }
        attacks[sq] = bb;
    }
    return attacks;
}

inline std::array<Bitboard, 64> northeast() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::A1; sq <= constants::G7; ++sq) {
        Bitboard bb;
        for (Square atk_sq = sq + 9; atk_sq <= constants::H8; atk_sq = atk_sq + 9) {
            if (Bitboard{atk_sq} & FILE_A_MASK) {
                break;
            }
            bb |= Bitboard{atk_sq};
        }
        attacks[sq] = bb;
    }
    return attacks;
}

inline std::array<Bitboard, 64> southeast() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::A2; sq <= constants::H8; ++sq) {
        Bitboard bb;
        for (Square atk_sq = sq - 7; atk_sq >= constants::A1; atk_sq = atk_sq - 7) {
            if (Bitboard{atk_sq} & FILE_A_MASK) {
                break;
            }
            bb |= Bitboard{atk_sq};
        }
        attacks[sq] = bb;
    }
    return attacks;
}

inline std::array<std::array<Bitboard, 64>, 64> intervening() {
    std::array<std::array<Bitboard, 64>, 64> intervening_bb{};
    for (Square from = constants::A1; from <= constants::H8; ++from) {
        for (Square to = constants::A1; to <= constants::H8; ++to) {
            if (from == to) {
                continue;
            }
            Square high = to;
            Square low = from;
            if (low > high) {
                high = from;
                low = to;
            }
            if (high.file() == low.file()) {
                for (high -= 8; high > low; high -= 8) {
                    intervening_bb[from][to] |= Bitboard{high};
                }
            }
            if (high.rank() == low.rank()) {
                for (--high; high > low; --high) {
                    intervening_bb[from][to] |= Bitboard{high};
                }
            }
            if (high.file() - low.file() == high.rank() - low.rank()) {
                for (high -= 9; high > low; high -= 9) {
                    intervening_bb[from][to] |= Bitboard{high};
                }
            }
            if (low.file() - high.file() == high.rank() - low.rank()) {
                for (high -= 7; high > low; high -= 7) {
                    intervening_bb[from][to] |= Bitboard{high};
                }
            }
        }
    }
    return intervening_bb;
}

// added by krtoonbrat
inline std::array<Bitboard, 64> squares() {
    std::array<Bitboard, 64> squares_bb{};
    for (Square s = constants::A1; s <= constants::H8; s++) {
        squares_bb[s] = Bitboard(s);
    }
    return squares_bb;
}

}  // namespace init

// added by krtoonbrat
// Square bitboards
extern std::array<Bitboard, 64> SQUARES;

// Direction bitboards
extern std::array<Bitboard, 64> NORTH;
extern std::array<Bitboard, 64> SOUTH;
extern std::array<Bitboard, 64> EAST;
extern std::array<Bitboard, 64> WEST;
extern std::array<Bitboard, 64> NORTHWEST;
extern std::array<Bitboard, 64> SOUTHWEST;
extern std::array<Bitboard, 64> NORTHEAST;
extern std::array<Bitboard, 64> SOUTHEAST;
extern std::array<std::array<Bitboard, 64>, 64> INTERVENING;

// added by krtoonbrat
static Bitboard square(Square square) {
    return SQUARES[square];
}

static Bitboard north(Square square) {
    return NORTH[square];
}
static Bitboard south(Square square) {
    return SOUTH[square];
}
static Bitboard east(Square square) {
    return EAST[square];
}
static Bitboard west(Square square) {
    return WEST[square];
}
static Bitboard northwest(Square square) {
    return NORTHWEST[square];
}
static Bitboard southwest(Square square) {
    return SOUTHWEST[square];
}
static Bitboard northeast(Square square) {
    return NORTHEAST[square];
}
static Bitboard southeast(Square square) {
    return SOUTHEAST[square];
}
static Bitboard intervening(Square from, Square to) {
    return INTERVENING[from][to];
}

namespace init {

inline std::array<std::array<libchess::Bitboard, 64>, 2> pawn_attacks() {
    std::array<std::array<libchess::Bitboard, 64>, 2> attacks{};
    for (Square sq = constants::A1; sq <= constants::H8; ++sq) {
        if (sq <= constants::H7) {
            attacks[constants::WHITE][sq] |= Bitboard{sq + 7} & ~FILE_H_MASK;
        }
        if (sq <= constants::G7) {
            attacks[constants::WHITE][sq] |= Bitboard{sq + 9} & ~FILE_A_MASK;
        }
        if (sq >= constants::A2) {
            attacks[constants::BLACK][sq] |= Bitboard{sq - 7} & ~FILE_A_MASK;
        }
        if (sq >= constants::B2) {
            attacks[constants::BLACK][sq] |= Bitboard{sq - 9} & ~FILE_H_MASK;
        }
    }
    return attacks;
}

inline std::array<Bitboard, 64> knight_attacks() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::A1; sq <= constants::H8; ++sq) {
        if (sq <= constants::G6) {
            attacks[sq] |= Bitboard{sq + 17} & ~FILE_A_MASK;
        }
        if (sq <= constants::H6) {
            attacks[sq] |= Bitboard{sq + 15} & ~FILE_H_MASK;
        }
        if (sq >= constants::B3) {
            attacks[sq] |= Bitboard{sq - 17} & ~FILE_H_MASK;
        }
        if (sq >= constants::A3) {
            attacks[sq] |= Bitboard{sq - 15} & ~FILE_A_MASK;
        }
        if (sq <= constants::F7) {
            attacks[sq] |= Bitboard{sq + 10} & ~(FILE_A_MASK | FILE_B_MASK);
        }
        if (sq <= constants::H7) {
            attacks[sq] |= Bitboard{sq + 6} & ~(FILE_H_MASK | FILE_G_MASK);
        }
        if (sq >= constants::C2) {
            attacks[sq] |= Bitboard{sq - 10} & ~(FILE_H_MASK | FILE_G_MASK);
        }
        if (sq >= constants::A2) {
            attacks[sq] |= Bitboard{sq - 6} & ~(FILE_A_MASK | FILE_B_MASK);
        }
    }
    return attacks;
}

inline std::array<Bitboard, 64> king_attacks() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::A1; sq <= constants::H8; ++sq) {
        if (sq <= constants::G7) {
            attacks[sq] |= Bitboard{sq + 9} & ~FILE_A_MASK;
        }
        if (sq <= constants::H7) {
            attacks[sq] |= Bitboard{sq + 8};
            attacks[sq] |= Bitboard{sq + 7} & ~FILE_H_MASK;
        }
        if (sq <= constants::G8) {
            attacks[sq] |= Bitboard{sq + 1} & ~FILE_A_MASK;
        }
        if (sq >= constants::B1) {
            attacks[sq] |= Bitboard{sq - 1} & ~FILE_H_MASK;
        }
        if (sq >= constants::A2) {
            attacks[sq] |= Bitboard{sq - 7} & ~FILE_A_MASK;
            attacks[sq] |= Bitboard{sq - 8};
        }
        if (sq >= constants::B2) {
            attacks[sq] |= Bitboard{sq - 9} & ~FILE_H_MASK;
        }
    }
    return attacks;
}

inline std::array<Bitboard, 64> bishop_attacks() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::A1; sq <= constants::H8; ++sq) {
        attacks[sq] = lookups::northeast(sq) | lookups::southeast(sq) | lookups::southwest(sq) |
                      lookups::northwest(sq);
    }
    return attacks;
}

inline std::array<Bitboard, 64> rook_attacks() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::A1; sq <= constants::H8; ++sq) {
        attacks[sq] =
            lookups::north(sq) | lookups::east(sq) | lookups::south(sq) | lookups::west(sq);
    }
    return attacks;
}

inline std::array<Bitboard, 64> queen_attacks() {
    std::array<Bitboard, 64> attacks{};
    for (Square sq = constants::A1; sq <= constants::H8; ++sq) {
        attacks[sq] = lookups::north(sq) | lookups::east(sq) | lookups::south(sq) |
                      lookups::west(sq) | lookups::northwest(sq) | lookups::northeast(sq) |
                      lookups::southwest(sq) | lookups::southeast(sq);
    }
    return attacks;
}

}  // namespace init

// Piece attack bitboards
extern std::array<std::array<Bitboard, 64>, 2> PAWN_ATTACKS;
extern std::array<Bitboard, 64> KNIGHT_ATTACKS;
extern std::array<Bitboard, 64> KING_ATTACKS;
extern std::array<Bitboard, 64> BISHOP_ATTACKS;
extern std::array<Bitboard, 64> ROOK_ATTACKS;
extern std::array<Bitboard, 64> QUEEN_ATTACKS;

inline Bitboard pawn_attacks(Square square, Color color) {
    return PAWN_ATTACKS[color][square];
}
inline Bitboard knight_attacks(Square square, Bitboard = Bitboard{0}) {
    return KNIGHT_ATTACKS[square];
}
inline Bitboard king_attacks(Square square, Bitboard = Bitboard{0}) {
    return KING_ATTACKS[square];
}
inline Bitboard bishop_attacks(Square square) {
    return BISHOP_ATTACKS[square];
}
inline Bitboard rook_attacks(Square square) {
    return ROOK_ATTACKS[square];
}
inline Bitboard queen_attacks(Square square) {
    return QUEEN_ATTACKS[square];
}
inline Bitboard pawn_shift(Bitboard bb, Color c, int times = 1) {
    return c == constants::WHITE ? bb << (8 * times) : bb >> (8 * times);
}
inline Square pawn_shift(Square sq, Color c, int times = 1) {
    return c == constants::WHITE ? sq + (8 * times) : sq - (8 * times);
}
inline Rank relative_rank(Rank rank, Color c) {
    return c == constants::WHITE
               ? rank
               : Rank{static_cast<Rank::value_type>(constants::RANK_8.value() - rank.value())};
}
inline Bitboard relative_rank_mask(Rank rank, Color c) {
    return rank_mask(relative_rank(rank, c));
}

// everything from here down to least significant square function is inspired/taken from stockfish
inline Bitboard forward_ranks_mask(Square square, Color color) {
    return color == constants::WHITE ? RANK_1_MASK << 8 * relative_rank(Rank(square.value() / 8), constants::WHITE)
                                     : RANK_8_MASK >> 8 * relative_rank(Rank(square.value() / 8), constants::BLACK);
}

inline Bitboard forward_file_mask(Square square, Color color) {
    return forward_ranks_mask(square, color) & FILE_MASK[square.file()];
}

inline Bitboard adjacent_files_mask(Square square) {
    return ((FILE_MASK[square.file()] & ~FILE_H_MASK) << 1) | ((FILE_MASK[square.file()] & ~FILE_A_MASK) >> 1);
}

inline Bitboard pawn_attack_span(Color color, Square square) {
    return forward_ranks_mask(square, color) & adjacent_files_mask(square);
}

inline Bitboard passed_pawn_span(Color color, Square square) {
    return pawn_attack_span(color, square) | forward_file_mask(square, color);
}

inline Bitboard pawn_double_attacks(Color color, Bitboard pawns) {
    return color == constants::WHITE ? ((pawns & ~FILE_H_MASK) << 9) | ((pawns & ~FILE_A_MASK) << 7)
                                     : ((pawns & ~FILE_H_MASK) >> 7) | ((pawns & ~FILE_A_MASK) >> 9);
}

inline Bitboard least_significant_square_bb(Bitboard b) {
    return square(b.forward_bitscan());
}

inline Bitboard bishop_attacks_classical(Square square, Bitboard occupancy) {
    Bitboard attacks = bishop_attacks(square);
    Bitboard nw_blockers = (northwest(square) & occupancy) | Bitboard{constants::A8};
    Bitboard ne_blockers = (northeast(square) & occupancy) | Bitboard{constants::H8};
    Bitboard sw_blockers = (southwest(square) & occupancy) | Bitboard{constants::A1};
    Bitboard se_blockers = (southeast(square) & occupancy) | Bitboard{constants::H1};

    attacks ^= northwest(nw_blockers.forward_bitscan());
    attacks ^= northeast(ne_blockers.forward_bitscan());
    attacks ^= southwest(sw_blockers.reverse_bitscan());
    attacks ^= southeast(se_blockers.reverse_bitscan());
    return attacks;
}
inline Bitboard rook_attacks_classical(Square square, Bitboard occupancy) {
    Bitboard attacks = rook_attacks(square);
    Bitboard n_blockers = (north(square) & occupancy) | Bitboard{constants::H8};
    Bitboard s_blockers = (south(square) & occupancy) | Bitboard{constants::A1};
    Bitboard w_blockers = (west(square) & occupancy) | Bitboard{constants::A1};
    Bitboard e_blockers = (east(square) & occupancy) | Bitboard{constants::H8};

    attacks ^= north(n_blockers.forward_bitscan());
    attacks ^= south(s_blockers.reverse_bitscan());
    attacks ^= west(w_blockers.reverse_bitscan());
    attacks ^= east(e_blockers.forward_bitscan());
    return attacks;
}

// Written by Krtoonbrat.  This is my attempt at replacing the plain magic bitboards with fancy magic bitboards.
// The reason for doing this is so that we can use PEXT on systems that support it to hopefully provide a small speedup.
// All of this is based on the stockfish implementation.
struct Magic {
    Bitboard mask;
    Bitboard magic;
    Bitboard* attacks;
    unsigned shift;

    // Computes the attack index
    unsigned index(Bitboard occupancy) const {
        if (has_pext) {
            return unsigned(pext(occupancy, mask));
        }
        else {
            return unsigned(((occupancy & mask) * magic) >> shift);
        }
    }
};

extern Magic rook_magics[64];
extern Magic bishop_magics[64];

extern Bitboard rook_table[0x19000];
extern Bitboard bishop_table[0x1480];

/// xorshift64star Pseudo-Random Number Generator
/// This class is based on original code written and dedicated
/// to the public domain by Sebastiano Vigna (2014).
/// It has the following characteristics:
///
///  -  Outputs 64-bit numbers
///  -  Passes Dieharder and SmallCrush test batteries
///  -  Does not require warm-up, no zeroland to escape
///  -  Internal state is a single 64-bit integer
///  -  Period is 2^64 - 1
///  -  Speed: 1.60 ns/call (Core i7 @3.40GHz)
///
/// For further analysis see
///   <http://vigna.di.unimi.it/ftp/papers/xorshift.pdf>

class PRNG {

  uint64_t s;

  uint64_t rand64() {

    s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
    return s * 2685821657736338717LL;
  }

public:
  PRNG(uint64_t seed) : s(seed) { assert(seed); }

  template<typename T> T rand() { return T(rand64()); }

  /// Special generator used to fast init magic numbers.
  /// Output values only have 1/8th of their bits set on average.
  template<typename T> T sparse_rand()
  { return T(rand64() & rand64() & rand64()); }
};

namespace init {

inline void init_magics(PieceType pt, Bitboard table[], Magic magics[]) {
    // Optimal PRNG seeds to pick the correct magics in the shortest time
    int seeds[][8] = { { 8977, 44560, 54343, 38998,  5731, 95205, 104912, 17020 },
                             {  728, 10316, 55013, 32803, 12281, 15100,  16645,   255 } };

    Bitboard occupancy[4096], reference[4096], edges, b;
    int epoch[4096] = {}, cnt = 0, size = 0;

    for (Square square = constants::A1; square <= constants::H8; ++square) {
        Bitboard edges = ((FILE_A_MASK | FILE_H_MASK) & ~file_mask(square.file())) |
                         ((RANK_1_MASK | RANK_8_MASK) & ~rank_mask(square.rank()));

        Magic& m = magics[square];
        m.mask = (pt == constants::ROOK ? lookups::rook_attacks(square) : lookups::bishop_attacks(square)) & ~edges;
        m.shift = 64 - m.mask.popcount();

        m.attacks = square == constants::A1 ? table : magics[square - 1].attacks + size;

        size = 0;
        b = Bitboard();
        do {
            occupancy[size] = b;
            reference[size] = (pt == constants::ROOK ? lookups::rook_attacks_classical(square, b) : lookups::bishop_attacks_classical(square, b));

            if (has_pext) {
                m.attacks[pext(b, m.mask)] = reference[size];
            }

            size++;
            b = (b - m.mask) & m.mask;
        } while (b);

        if (has_pext) {
            continue;
        }

        PRNG rng(seeds[1][square.rank()]);

        for (int i = 0; i < size;) {
            for (m.magic = Bitboard(); Bitboard(((m.magic * m.mask) >> 56)).popcount() < 6;) {
                m.magic = rng.sparse_rand<Bitboard>();
            }

            for (++cnt, i = 0; i < size; ++i) {
                unsigned idx = m.index(occupancy[i]);

                if (epoch[idx] < cnt) {
                    epoch[idx] = cnt;
                    m.attacks[idx] = reference[i];
                }
                else if (m.attacks[idx] != reference[i]) {
                    break;
                }

            }

        }

    }

}

}

inline Bitboard rook_attacks(Square square, Bitboard occupancy) {
    return rook_magics[square].attacks[rook_magics[square].index(occupancy)];
}

inline Bitboard bishop_attacks(Square square, Bitboard occupancy) {
    return bishop_magics[square].attacks[bishop_magics[square].index(occupancy)];
}

inline Bitboard queen_attacks(Square square, Bitboard occupancy) {
    return rook_attacks(square, occupancy) | bishop_attacks(square, occupancy);
}
inline Bitboard non_pawn_piece_type_attacks(PieceType piece_type,
                                            Square square,
                                            Bitboard occupancies = Bitboard{0}) {
    switch (piece_type) {
        case constants::KNIGHT:
            return knight_attacks(square);
        case constants::BISHOP:
            return bishop_attacks(square, occupancies);
        case constants::ROOK:
            return rook_attacks(square, occupancies);
        case constants::QUEEN:
            return queen_attacks(square, occupancies);
        case constants::KING:
            return king_attacks(square);
        default:
            return Bitboard{0};
    }
}

namespace init {

inline std::array<std::array<Bitboard, 64>, 64> full_ray() {
    std::array<std::array<Bitboard, 64>, 64> full_ray_bb{};
    for (Square from = constants::A1; from <= constants::H8; ++from) {
        for (Square to = constants::A1; to <= constants::H8; ++to) {
            if (from == to) {
                continue;
            }
            Square high = to;
            Square low = from;
            if (low > high) {
                high = from;
                low = to;
            }
            if (high.file() == low.file()) {
                full_ray_bb[from][to] = (lookups::rook_attacks(high) & lookups::rook_attacks(low)) |
                                        Bitboard{from} | Bitboard{to};
            }
            if (high.rank() == low.rank()) {
                full_ray_bb[from][to] = (lookups::rook_attacks(high) & lookups::rook_attacks(low)) |
                                        Bitboard{from} | Bitboard{to};
            }
            if (high.file() - low.file() == high.rank() - low.rank()) {
                full_ray_bb[from][to] =
                    (lookups::bishop_attacks(high) & lookups::bishop_attacks(low)) |
                    Bitboard{from} | Bitboard{to};
            }
            if (low.file() - high.file() == high.rank() - low.rank()) {
                full_ray_bb[from][to] =
                    (lookups::bishop_attacks(high) & lookups::bishop_attacks(low)) |
                    Bitboard{from} | Bitboard{to};
            }
        }
    }

    return full_ray_bb;

}

}  // namespace init

extern std::array<std::array<Bitboard, 64>, 64> FULL_RAY;

inline Bitboard full_ray(Square from, Square to) {
    return FULL_RAY[from][to];
}

}  // namespace libchess::lookups

#endif  // LIBCHESS_LOOKUPS_H
