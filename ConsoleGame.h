//
// Created by Hughe on 4/1/2022.
//

#ifndef ANDURIL_ENGINE_CONSOLEGAME_H
#define ANDURIL_ENGINE_CONSOLEGAME_H

#include "thc.h"

class Game {
public:

    // goes through the process of a turn
    static void turn(thc::ChessRules &board);

    // prints the board to the console
    static void displayBoard(thc::ChessRules &board){std::cout << board.ToDebugStr() << std::endl;}

    // gets a user's move and makes sure the move is formatted correctly
    static std::string getUserMove();

};

#endif //ANDURIL_ENGINE_CONSOLEGAME_H
