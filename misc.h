//
// Created by kjhughes4 on 10/25/2023.
//

#ifndef ANDURIL_ENGINE_MISC_H
#define ANDURIL_ENGINE_MISC_H

#include <cstddef>

// memory allocation stuff
// large page allocation based on stockfish 16
void* std_aligned_alloc(size_t alignment, size_t size);
void std_aligned_free(void* ptr);
void* aligned_large_pages_alloc(size_t size);
void aligned_large_pages_free(void* ptr);


#endif //ANDURIL_ENGINE_MISC_H
