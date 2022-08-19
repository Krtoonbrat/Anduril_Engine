//
// Created by Hughe on 4/1/2022.
//

#ifndef ANDURIL_ENGINE_CONSOLEGAME_H
#define ANDURIL_ENGINE_CONSOLEGAME_H

#include "Anduril.h"

class Game {
public:

    // goes through the process of a turn
    static void turn(libchess::Position &board, Anduril &AI);

    // prints the board to the console
    static void displayBoard(libchess::Position &board){ board.display(); }

    // gets a user's move and makes sure the move is formatted correctly
    static std::string getUserMove();

};

#endif //ANDURIL_ENGINE_CONSOLEGAME_H
