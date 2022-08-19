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
    void parseGo(char* line, libchess::Position &board, Anduril &AI, Book &openingBook);

    // parses position commands from the GUI
    void parsePosition(char* line, libchess::Position &board, Anduril &AI);

    // parses setoptions
    void parseOption(char* line, Anduril &AI);

    // looks to see if there is an input waiting for us to read
    // http://home.arcor.de/dreamlike/chess/
    int InputWaiting();

    void ReadInput(Anduril &AI);
}

#endif //ANDURIL_ENGINE_UCI_H
