//
// Created by Hughe on 4/1/2022.
//
#include <iostream>
#include <regex>

#include "Anduril.h"
#include "ConsoleGame.h"

// goes through the process of a turn
// we need an instance of Anduril so that we can update its position stack through the makeMovePlay method
void Game::turn(libchess::Position &board, Anduril &AI) {
    std::string move = "";
    libchess::Move playerMove;

    while (move == ""){
        move = getUserMove();
    }

    libchess::Square possiblePromotion = *libchess::Square::from(move.substr(0, 2));
    libchess::Rank proRank = possiblePromotion.rank();

    if ((proRank == libchess::constants::RANK_7 && *board.piece_on(possiblePromotion) == libchess::constants::WHITE_PAWN)
        || (proRank == libchess::constants::RANK_2 && *board.piece_on(possiblePromotion) == libchess::constants::BLACK_PAWN)) {
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

    playerMove.from(move);

    if (board.is_legal_move(playerMove)){
        board.make_move(playerMove);
    }
    else{
        std::cout << "Illegal Move." << std::endl;
        turn(board, AI);
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
