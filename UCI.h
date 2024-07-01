//
// Created by 80hugkev on 7/6/2022.
//

#ifndef ANDURIL_ENGINE_UCI_H
#define ANDURIL_ENGINE_UCI_H

#include "Anduril.h"
#include "PolyglotBook.h"

// all the UCI protocol stuff is going to be implemented
// with C code because the tutorial I am following is written in C
namespace UCI {
    // main UCI loop
    void loop(int argc, char* argv[]);

    // parses the go command from the GUI
    void parseGo(std::stringstream &stream, libchess::Position &board, Book &openingBook, bool &bookOpen);

    // parses position commands from the GUI
    void parsePosition(std::stringstream &stream, libchess::Position &board);

    // parses options
    void parseOption(std::stringstream &stream, libchess::Position &board, bool &bookOpen);

    static int queenOrderVal = 50000;
    static int rookOrderVal =  25000;
    static int minorOrderVal = 15000;

    static double nem = 3.5782;
    static double neb = -0.5656;
}

#endif //ANDURIL_ENGINE_UCI_H
