//
// Created by 80hugkev on 7/6/2022.
//

#ifndef ANDURIL_ENGINE_UCI_H
#define ANDURIL_ENGINE_UCI_H

#include "Anduril.h"
#include "PolyglotBook.h"

namespace UCI {
    // main UCI loop
    void loop(int argc, char* argv[]);

    // parses the go command from the GUI
    void parseGo(std::stringstream &stream, libchess::Position &board, Book &openingBook, bool &bookOpen);

    // parses position commands from the GUI
    void parsePosition(std::stringstream &stream, libchess::Position &board);

    // parses options
    void parseOption(std::stringstream &stream, libchess::Position &board, bool &bookOpen);

    static double nem = 2.3807;
    static double neb = -0.2408;
    static double tem = 3.1961;
    static double teb = 0.2167;
}

#endif //ANDURIL_ENGINE_UCI_H
