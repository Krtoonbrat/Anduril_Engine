//
// Created by kjhughes4 on 10/25/2023.
//

#ifndef ANDURIL_ENGINE_MISC_H
#define ANDURIL_ENGINE_MISC_H

#include <cstddef>
#include <cstdint>

// cache alignment stuff from scorpio
#include <cstdlib>
#define CACHE_LINE_SIZE  64

#if defined (__GNUC__)
#   define CACHE_ALIGN  __attribute__ ((aligned(CACHE_LINE_SIZE)))
#else
#   define CACHE_ALIGN __declspec(align(CACHE_LINE_SIZE))
#endif

namespace NNUE {

    // nnue stuff.  From scorpio
    typedef struct DirtyPiece {
        int dirtyNum;
        int pc[3];
        int from[3];
        int to[3];
    } DirtyPiece;

    typedef struct Accumulator {
        CACHE_ALIGN int16_t accumulation[2][256];
        int computedAccumulation;
    } Accumulator;

    typedef struct NNUEdata {
        Accumulator accumulator;
        DirtyPiece dirtyPiece;
    } NNUEdata;

    void LoadNNUE();
    int NNUE_evaluate(int player, int *pieces, int *squares);
    int NNUE_incremental(int player, int *pieces, int *squares, NNUEdata** data);

}

// memory allocation stuff
// large page allocation based on stockfish 16
void* std_aligned_alloc(size_t alignment, size_t size);
void std_aligned_free(void* ptr);
void* aligned_large_pages_alloc(size_t size);
void aligned_large_pages_free(void* ptr);

// portable byteswap function used in libchess/Position/utilities.h
uint64_t byteSwap(uint64_t value);


#endif //ANDURIL_ENGINE_MISC_H
