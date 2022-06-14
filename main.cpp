#include <chrono>
#include <iostream>

#include "Anduril.h"
#include "ConsoleGame.h"
#include "thc.h"

int main() {
    thc::ChessRules board;
    Anduril AI;
    thc::TERMINAL gameOver = thc::NOT_TERMINAL;

    // this exists so that I can test problematic board positions if and when they arise
    board.Forsyth("2r1k2r/pp3p2/3p1p1p/3Pp2B/3n4/8/P4PPP/4K2R w Kk - 1 20");

    //board.Forsyth("8/4Q2k/6pp/8/1P6/4PKPP/4rP2/5q2 b - - 9 60");
    //board.Forsyth("r1bn1rk1/pp2ppbp/6p1/3P4/4P3/5N2/q2BBPPP/1R1Q1RK1 w - - 1 14");

    Game::displayBoard(board);
    std::cout << "Board FEN: " << board.ForsythPublish() << std::endl;

    // main game loop
    while (board.Evaluate(gameOver) && gameOver == thc::NOT_TERMINAL) {
        std::cout << (board.WhiteToPlay() ? "White to move" : "Black to move") << std::endl;

        /*
        auto start = std::chrono::high_resolution_clock::now();
        AI.go(board, 9);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> timeElapsed = end - start;
        std::cout << "Time spent searching: " << timeElapsed.count() / 1000 << " seconds" << std::endl;
        std::cout << "Nodes per second: " << AI.getMovesExplored() / (timeElapsed.count()/1000) << std::endl;
        AI.setMovesExplored(0);
         */


        if (!board.WhiteToPlay()) {
            Game::turn(board);
        }
        else {
            auto start = std::chrono::high_resolution_clock::now();
            AI.go(board, 10);
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> timeElapsed = end - start;
            std::cout << "Time spent searching: " << timeElapsed.count() / 1000 << " seconds" << std::endl;
            std::cout << "Nodes per second: " << AI.getMovesExplored() / (timeElapsed.count()/1000) << std::endl;
            AI.setMovesExplored(0);
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
