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
    //board.Forsyth("r1b1kb1r/ppp1pppp/2nq1n2/3p4/3P4/P1P1P1P1/1P3P1P/RNBQKBNR b KQkq - 0 5");

    //board.Forsyth("rn2r3/4k1p1/ppp5/4NP2/3RB3/2P3Bp/P1P4P/2K5 w - - 1 27");
    //board.Forsyth("2rk4/1B1brp2/pP1Qp1pp/4P3/8/8/1q4PP/3R1R1K b - - 0 38");

    Game::displayBoard(board);
    std::cout << "Board FEN: " << board.ForsythPublish() << std::endl;

    // main game loop
    while (board.Evaluate(gameOver) && gameOver == thc::NOT_TERMINAL) {
        /*
        std::cout << (board.WhiteToPlay() ? "White to move" : "Black to move") << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        AI.go(board, 6);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> timeElapsed = end - start;
        std::cout << "Time spent searching: " << timeElapsed.count() / 1000 << " seconds" << std::endl;
        std::cout << "Nodes per second: " << AI.getMovesExplored() / (timeElapsed.count()/1000) << std::endl;
        AI.setMovesExplored(0);
         */

        if (board.WhiteToPlay()) {
            Game::turn(board);
        }
        else {
            auto start = std::chrono::high_resolution_clock::now();
            AI.go(board, 8);
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
