/*
 * (c) 2015 basil, all rights reserved,
 * Modifications Copyright (c) 2016-2019 by Jon Dart
 * Modifications Copyright (c) 2020-2024 by Andrew Grant
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

/*
 * You are in charge of defining each of these macros. The macros already
 * defined here are simply an example of what to do. This configuration is
 * used by Ethereal to implement Pyrrhic.
 *
 * See Ethereal's source <https://github.com/AndyGrant/Ethereal> if it is
 * not readily clear what these definfitions mean. The relevant files are
 * are the ones included below.
 *
 * Note that for the Pawn Attacks, we invert the colour. This is because
 * Pyrrhic defines White as 1, where as Ethereal (any many others) choose
 * to define White as 0 and Black as 1.
 */

// I wrote this one because the libchess poplsb only works on bitboard objects, and the process is not a oneliner
inline int poplsb(uint64_t *x) {
    int lsb = __builtin_ctzll(*x);
    *x &= *x - 1;
    return lsb;
}

#include "../libchess/Lookups.h"

#define PYRRHIC_POPCOUNT(x)              (__builtin_popcountll(x))
#define PYRRHIC_LSB(x)                   (__builtin_ctzll(x))
#define PYRRHIC_POPLSB(x)                (poplsb(x))

#define PYRRHIC_PAWN_ATTACKS(sq, c)      (libchess::lookups::pawn_attacks(libchess::Square{sq}, libchess::Color{static_cast<int>(!c)}))
#define PYRRHIC_KNIGHT_ATTACKS(sq)       (libchess::lookups::knight_attacks(libchess::Square{sq}))
#define PYRRHIC_BISHOP_ATTACKS(sq, occ)  (libchess::lookups::bishop_attacks(libchess::Square{sq}, libchess::Bitboard{occ}))
#define PYRRHIC_ROOK_ATTACKS(sq, occ)    (libchess::lookups::rook_attacks(libchess::Square{sq}, libchess::Bitboard{occ}))
#define PYRRHIC_QUEEN_ATTACKS(sq, occ)   (libchess::lookups::queen_attacks(libchess::Square{sq}, libchess::Bitboard{occ}))
#define PYRRHIC_KING_ATTACKS(sq)         (libchess::lookups::king_attacks(libchess::Square{sq}))
