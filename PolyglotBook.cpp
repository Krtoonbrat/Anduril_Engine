//
// Created by 80hugkev on 6/8/2022.
//

#include <iostream>

#include "Anduril.h"
#include "PolyglotBook.h"
#include "thc.h"

// opens a book
bool Book::openBook(const char *file) {
    book = fopen(file, "rb");

    if (book == nullptr) {
        bookOpen = false;
        return false;
    }
    else {
        fseek(book, 0, SEEK_END);
        long position = ftell(book);

        if (position < sizeof(bookEntry)) {
            bookOpen = false;
            return false;
        }

        numEntries = position / sizeof(bookEntry);

        entries = (bookEntry*)malloc(numEntries * sizeof(bookEntry));
        rewind(book);

        size_t returnValue;
        returnValue = fread(entries, sizeof(bookEntry), numEntries, book);

        std::cout << returnValue << " Entries read from file" << std::endl;
    }

    return true;
}

// closes the book
void Book::closeBook() {
    free(entries);
}

thc::Move Book::convertPolyToInternal(uint16_t move, thc::ChessRules &board) {
    int fromFile = (move >> 6) & 7;
    int fromRank = (move >> 9) & 7;
    int toFile = (move >> 0) & 7;
    int toRank = (move >> 3) & 7;
    int promotion = (move >> 12) & 7;
    std::string movestr = "";
    thc::Move output;

    movestr += intToFile[fromFile];
    movestr += intToRank[fromRank];
    movestr += intToFile[toFile];
    movestr += intToRank[toRank];
    if (promotion > 0) {
        movestr += intToPro[promotion];
    }
    if (movestr[0] == 'e') {
        if (movestr[2] == 'h') {
            movestr[2] = 'g';
        }
        else if (movestr[2] == 'a') {
            movestr[2] = 'c';
        }
    }

    output.TerseIn(&board, movestr.c_str());

    return output;

}

uint16_t Book::endian_swap_u16(uint16_t x) {
    x = (x>>8) | (x<<8);
    return x;
}

uint32_t Book::endian_swap_u32(uint32_t x) {
    x = (x>>24) | ((x<<8) & 0x00FF0000) | ((x>>8) & 0x0000FF00) | (x<<24);
    return x;
}

uint64_t Book::endian_swap_u64(uint64_t x) {
    x = (x>>56) |
            ((x<<40) & 0x00FF000000000000) |
            ((x<<24) & 0x0000FF0000000000) |
            ((x<<8)  & 0x000000FF00000000) |
            ((x>>8)  & 0x00000000FF000000) |
            ((x>>24) & 0x0000000000FF0000) |
            ((x>>40) & 0x000000000000FF00) |
            (x<<56);
    return x;
}
