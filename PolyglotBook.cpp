//
// Created by 80hugkev on 6/8/2022.
//

#include <iostream>
#include <random>

#include "Anduril.h"
#include "PolyglotBook.h"
#include "ZobristHasher.h"

// opens a book
bool Book::readBook(const char *file) {
    book = fopen(file, "rb");

    if (book == nullptr) {
        //std::cout << "Failed to read book, file did not open" << std::endl;
        bookOpen = false;
        return false;
    }
    else {
        fseek(book, 0, SEEK_END);
        long position = ftell(book);

        if (position < sizeof(bookEntry)) {
            //std::cout << "Failed to read book." << std::endl;
            bookOpen = false;
            return false;
        }

        numEntries = position / sizeof(bookEntry);

        entries = (bookEntry*)malloc(numEntries * sizeof(bookEntry));
        rewind(book);

        size_t returnValue;
        returnValue = fread(entries, sizeof(bookEntry), numEntries, book);

        //std::cout << returnValue << " Entries read from file" << std::endl;
    }

    return true;
}

// closes the book
void Book::freeBook() {
    if (entries) {
        free(entries);
    }
}

// finds and returns a book move for the position
libchess::Move Book::getBookMove(libchess::Position &board) {
    uint64_t key = Zobrist::zobristHash(board);
    int index = 0;
    bookEntry *entry;
    uint16_t move;
    libchess::Move bookMoves[32];
    libchess::Move tempMove;
    int count = 0;
    std::random_device random;

    for (entry = getEntries(); entry < getEntries() + getNumEntries(); entry++) {
        if (key == endian_swap_u64(entry->key)) {
            move = endian_swap_u16(entry->move);
            tempMove = convertPolyToInternal(move, board);
            if (tempMove.value() != 0) {
                bookMoves[count++] = tempMove;
                if (count > 32) {
                    break;
                }
            }

        }
    }

    if (count != 0) {
        tempMove = bookMoves[random() % count];
    }
    else {
        tempMove = libchess::Move(0);
    }

    return tempMove;
}

libchess::Move Book::convertPolyToInternal(uint16_t move, libchess::Position &board) {
    int fromFile = (move >> 6) & 7;
    int fromRank = (move >> 9) & 7;
    int toFile = (move >> 0) & 7;
    int toRank = (move >> 3) & 7;
    int promotion = (move >> 12) & 7;
    std::string movestr = "";
    std::optional<libchess::Move> output;

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

    output = libchess::Move::from(movestr);

    if (output.has_value()) {
        return *output;
    }
    return libchess::Move(0);

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
