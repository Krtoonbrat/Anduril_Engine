#include <chrono>
#include <iostream>

#include "Anduril.h"
#include "ConsoleGame.h"
#include "thc.h"
#include "ZobristHasher.h"

int main() {
    thc::ChessRules board;
    Anduril AI;
    Book openingBook = Book(R"(..\book\Performance.bin)");
    thc::TERMINAL gameOver = thc::NOT_TERMINAL;

    // this exists so that I can test problematic board positions if and when they arise
    //board.Forsyth("7r/2kr2pp/2p2p2/p4N2/1bnBP3/2P2P2/P4KPP/3R3R b - - 0 24");

    //board.Forsyth("8/4Q2k/6pp/8/1P6/4PKPP/4rP2/5q2 b - - 9 60");
    //board.Forsyth("r1bn1rk1/pp2ppbp/6p1/3P4/4P3/5N2/q2BBPPP/1R1Q1RK1 w - - 1 14");

    Game::displayBoard(board);
    std::cout << "Board FEN: " << board.ForsythPublish() << std::endl;
    AI.positionStack.push_back(Zobrist::hashBoard(board));

    // main game loop
    while (board.Evaluate(gameOver) && gameOver == thc::NOT_TERMINAL) {
        std::cout << (board.WhiteToPlay() ? "White to move" : "Black to move") << std::endl;

        /*
        AI.go(board, 9);
         */


        if (board.WhiteToPlay()) {
            Game::turn(board, AI);
        }
        else {
            if (openingBook.getBookOpen()) {
                thc::Move bestMove = openingBook.getBookMove(board);
                if (bestMove.Valid()) {
                    std::cout << "Moving " << bestMove.TerseOut() << " from book" << std::endl;
                    AI.makeMovePlay(board, bestMove);
                }
                else {
                    std::cout << "End of opening book, starting search" << std::endl;
                    openingBook.flipBookOpen();
                }
            }
            else {
                AI.go(board, 10);
            }
        }


        Game::displayBoard(board);
        std::cout << "Board FEN: " << board.ForsythPublish() << std::endl;

    }

    // tell 'em who won
    switch (gameOver){
        case thc::TERMINAL_WCHECKMATE:
            std::cout << "Checkmate.  Black wins." << std::endl;
            break;
        case thc::TERMINAL_BCHECKMATE:
            std::cout << "Checkmate.  White wins." << std::endl;
            break;
        case thc::TERMINAL_BSTALEMATE:
        case thc::TERMINAL_WSTALEMATE:
            std::cout << "Stalemate.  It's a draw." << std::endl;
            break;

    }




    return 0;
}
