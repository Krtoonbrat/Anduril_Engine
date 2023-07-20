//
// Created by 80hugkev on 7/6/2022.
//
#include "Anduril.h"
#include "PolyglotBook.h"

#ifndef ANDURIL_ENGINE_UCI_H
#define ANDURIL_ENGINE_UCI_H

// all the UCI protocol stuff is going to be implemented
// with C code because the tutorial I am following is written in C
namespace UCI {
    // main UCI loop
    void loop();

    // parses the go command from the GUI
    void parseGo(char* line, libchess::Position &board, std::unique_ptr<Anduril> &AI, Book &openingBook, bool &bookOpen);

    // parses position commands from the GUI
    void parsePosition(char* line, libchess::Position &board, std::unique_ptr<Anduril> &AI);

    // parses setoptions
    void parseOption(char* line, std::unique_ptr<Anduril> &AI, bool &bookOpen);

    // parks thread waiting for a search
    void waitForSearch(std::unique_ptr<Anduril> &AI, libchess::Position &board);

    static int queenOrderVal = 500000;
    static int rookOrderVal =  250000;
    static int minorOrderVal = 150000;
    static int maxHistoryVal = 16384;
    static int maxContinuationVal = 16384;
}

#endif //ANDURIL_ENGINE_UCI_H
