#ifndef LIBCHESS_MOVE_H
#define LIBCHESS_MOVE_H

#include <algorithm>
#include <optional>
#include <vector>
#include <array>

#include "PieceType.h"
#include "Square.h"

namespace libchess {

class Move {
   private:
    enum BitOpLookup : std::uint32_t
    {
        TO_SQUARE_SHIFT = 6,
        PROMOTION_TYPE_SHIFT = 12,
        MOVE_TYPE_SHIFT = 15,

        PROMOTION_TYPE_MASK = 7 << PROMOTION_TYPE_SHIFT,
        MOVE_TYPE_MASK = 7 << MOVE_TYPE_SHIFT
    };

   public:
    enum class Type : std::uint8_t
    {
        NONE,
        NORMAL,
        CASTLING,
        ENPASSANT,
        PROMOTION,
        DOUBLE_PUSH,
        CAPTURE,
        CAPTURE_PROMOTION,
    };

    using value_type = int;

    constexpr Move() : value_(0) {
    }
    constexpr explicit Move(std::uint32_t value) : value_(value) {
    }
    constexpr Move(Square from_square, Square to_square, Move::Type type = Move::Type::NONE)
        : value_(from_square.value() | (to_square.value() << TO_SQUARE_SHIFT) |
                 (constants::PAWN.value() << PROMOTION_TYPE_SHIFT) |
                 (std::uint32_t(type) << MOVE_TYPE_SHIFT)) {
    }
    constexpr Move(Square from_square,
                   Square to_square,
                   PieceType promotion_pt,
                   Move::Type type = Move::Type::NONE)
        : value_(from_square.value() | (to_square.value() << TO_SQUARE_SHIFT) |
                 (promotion_pt.value() << PROMOTION_TYPE_SHIFT) |
                 (std::uint32_t(type) << MOVE_TYPE_SHIFT)) {
    }

    // added by Krtoonbrat
    // this converts the move to a 16 bit integer for storage in the transposition table
    // first 6 are from square, next 6 are to square
    // bits 12 and 13 mark promotion piece type
    // bits 14 and 15 mark move type
    uint16_t to_table() const {
        uint16_t move = 0;

        // add the to and from squares
        move |= from_square().value();
        move |= to_square().value() << 6;

        // add potential promotion
        PieceType promotion_pt = PieceType{int((value() & PROMOTION_TYPE_MASK) >> PROMOTION_TYPE_SHIFT)};
        // we can skip knights since their value is 0
        if (promotion_pt == constants::BISHOP) {
            move |= 1 << 12;
        } else if (promotion_pt == constants::ROOK) {
            move |= 2 << 12;
        } else if (promotion_pt == constants::QUEEN) {
            move |= 3 << 12;
        }

        // add the move type
        Type move_type = type();
        switch(move_type) {
            case Type::PROMOTION:
            case Type::CAPTURE_PROMOTION:
                move |= 1 << 14;
                break;
            case Type::ENPASSANT:
                move |= 2 << 14;
                break;
            case Type::CASTLING:
                move |= 3 << 14;
                break;
            default:
                break;
        }

        return move;
    }

    static std::optional<Move> from(const std::string& str) {
        auto strlen = str.length();
        if (strlen > 5 || strlen < 4) {
            return std::nullopt;
        }
        auto from = Square::from(str.substr(0, 2));
        auto to = Square::from(str.substr(2, 4));
        if (!(from && to)) {
            return std::nullopt;
        }

        if (str.size() == 5) {
            auto promotion_pt = PieceType::from(str[4]);
            if (!promotion_pt) {
                return std::nullopt;
            }
            return Move{*from, *to, *promotion_pt};
        }
        return Move{*from, *to};
    }

    std::string to_str() const {
        std::string move_str = from_square().to_str() + to_square().to_str();
        auto promotion_pt = promotion_piece_type();
        if (promotion_pt) {
            move_str += promotion_pt->to_char();
        }
        return move_str;
    }

    constexpr bool operator==(const Move rhs) const {
        return value() == rhs.value();
    }
    constexpr bool operator!=(const Move rhs) const {
        return value() != rhs.value();
    }

    constexpr Square from_square() const {
        return Square{value() & 0x3f};
    }
    constexpr Square to_square() const {
        return Square{(value() & 0xfc0) >> 6};
    }
    constexpr Move::Type type() const {
        return static_cast<Move::Type>((value() & MOVE_TYPE_MASK) >> MOVE_TYPE_SHIFT);
    }
    constexpr std::optional<PieceType> promotion_piece_type() const {
        PieceType promotion_pt =
            PieceType{int((value() & PROMOTION_TYPE_MASK) >> PROMOTION_TYPE_SHIFT)};
        if (promotion_pt == constants::PAWN || promotion_pt == constants::KING) {
            return std::nullopt;
        }
        return promotion_pt;
    }

    constexpr value_type value_sans_type() const {
        return value_ & ~MOVE_TYPE_MASK;
    }
    constexpr value_type value() const {
        return value_;
    }

    // added by Krtoonbrat
    // each move gets a score for sorting in the move list
    int score = 0;

   private:
    value_type value_;
};

// added by Krtoonbrat
inline bool operator<(const Move& f, const Move& s) {
    return f.score < s.score;
}

// modified by Krtoonbrat
// I changed the container from a vector to an array.  This was done to get rid of the allocation that is necessary
// for vectors, increasing performance.
class MoveList {
   public:
    using value_type = std::array<Move, 256>;
    using iterator = value_type::iterator;
    using const_iterator = value_type::const_iterator;

    iterator begin() {
        return values_.begin();
    }
    iterator end() {
        return values_.begin() + last;
    }
    const_iterator cbegin() const {
        return values_.cbegin();
    }
    const_iterator cend() const {
        return values_.cbegin() + last;
    }

    void pop_back() {
        last--;
        values_[last] = Move();
    }
    void add(Move move) {
        values_[last] = move;
        last++;
    }
    void add(const MoveList& move_list) noexcept {
        for (auto iter = move_list.cbegin(); iter != move_list.cend(); ++iter) {
            add(*iter);
        }
    }
    template <class F>
    void sort(F move_evaluator) {
        auto& moves = values_mut_ref();
        std::vector<int> scores;
        scores.reserve(values_.size());
        for (int i = 0; i < size(); ++i) {
            scores[i] = move_evaluator(moves[i]);
        }
        for (int i = 1; i < size(); ++i) {
            Move moving_move = moves[i];
            int moving_score = scores[i];
            int j = i;
            for (; j > 0; --j) {
                if (scores[j - 1] < moving_score) {
                    scores[j] = scores[j - 1];
                    moves[j] = moves[j - 1];
                } else {
                    break;
                }
            }
            scores[j] = moving_score;
            moves[j] = moving_move;
        }
    }
    bool empty() const noexcept {
        return values_[0].value() == 0;
    }
    int size() const {
        return last;
    }
    const value_type& values() const {
        return values_;
    }
    bool contains(Move move) const {
        return std::find(cbegin(), cend(), move) != cend();
    }

   protected:
    value_type& values_mut_ref() {
        return values_;
    }

   private:
    value_type values_;
    int last = 0;
};

inline std::ostream& operator<<(std::ostream& ostream, Move move) {
    return ostream << move.to_str();
}

}  // namespace libchess

namespace std {

template <>
struct hash<libchess::Move> : public hash<libchess::Move::value_type> {};

}  // namespace std

#endif  // LIBCHESS_MOVE_H
