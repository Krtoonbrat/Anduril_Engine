//
// Created by Hughe on 4/1/2022.
//
#include <iostream>
#include <regex>

#include "ConsoleGame.h"

// goes through the process of a turn
void Game::turn(thc::ChessRules &board) {
    std::string move = "";
    thc::Move playerMove;

    while (move == ""){
        move = getUserMove();
    }

    thc::Square possiblePromotion = thc::make_square(move[0], move[1]);

    if ((thc::get_rank(possiblePromotion) == 7 && board.squares[possiblePromotion] == 'P') || (thc::get_rank(possiblePromotion) == 2 && board.squares[possiblePromotion] == 'p')) {
        while (true) {
            std::string promotion;
            std::cout << "Please chose a piece to promote to.  q:Queen, r:Rook, n:Knight, b:Bishop: ";

            std::cin >> promotion;

            if (promotion == "q" || promotion == "r" || promotion == "n" || promotion == "b"){
                move += promotion;
                break;
            }

        }
    }

    if (playerMove.TerseIn(&board, move.c_str())){
        board.PlayMove(playerMove);
    }
    else{
        std::cout << "Illegal Move." << std::endl;
        turn(board);
    }
}

// gets a move from the user
std::string Game::getUserMove() {
    // one string to represent the square the piece is on currently
    // and one we want to move to
    std::string fromSquare;
    std::string toSquare;

    // regex match
    std::regex validMove("^([a-h])([1-8])");

    std::cout << "Your Move? ";

    std::cin >> fromSquare;
    std::cin >> toSquare;

    // if the strings are formatted correctly, we return them, otherwise return an empty string
    if (std::regex_match(fromSquare, validMove) && std::regex_match(toSquare, validMove)) {
        return fromSquare + toSquare;
    }
    else {
        std::cout << "Ill-formatted move." << std::endl;
        return "";
    }
}
