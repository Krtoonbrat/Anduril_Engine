//
// Created by 80hugkev on 6/8/2022.
//

#ifndef ANDURIL_ENGINE_POLYGLOTBOOK_H
#define ANDURIL_ENGINE_POLYGLOTBOOK_H

#include <fstream>

#include "thc.h"

// this struct contains the information for book entries
struct bookEntry {
    uint64_t key;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
};

class Book {
public:
    Book(const char *file) { bookOpen = openBook(file); }

    ~Book() { closeBook(); }

    // getters and setters
    bookEntry* getEntries() { return entries; }
    long getNumEntries() const { return numEntries; }
    bool getBookOpen() const { return bookOpen; }
    void flipBookOpen() { bookOpen = !bookOpen; }

    bool openBook(const char *file);

    void closeBook();

    // finds and returns a book move for the position
    thc::Move getBookMove(thc::ChessRules &board);

    thc::Move convertPolyToInternal(uint16_t move, thc::ChessRules &board);

    uint16_t endian_swap_u16(uint16_t x);

    uint32_t endian_swap_u32(uint32_t x);

    uint64_t endian_swap_u64(uint64_t x);

private:

    FILE *book;

    bool bookOpen;

    bookEntry *entries;

    long numEntries;

    std::unordered_map<int, std::string>intToFile = {{0, "a"},
                                                     {1, "b"},
                                                     {2, "c"},
                                                     {3, "d"},
                                                     {4, "e"},
                                                     {5, "f"},
                                                     {6, "g"},
                                                     {7, "h"}};

    std::unordered_map<int, std::string>intToRank = {{0, "1"},
                                                     {1, "2"},
                                                     {2, "3"},
                                                     {3, "4"},
                                                     {4, "5"},
                                                     {5, "6"},
                                                     {6, "7"},
                                                     {7, "8"}};

    std::unordered_map<int, std::string>intToPro = {{1, "n"},
                                                    {2, "b"},
                                                    {3, "r"},
                                                    {4, "q"}};

};

#endif //ANDURIL_ENGINE_POLYGLOTBOOK_H
