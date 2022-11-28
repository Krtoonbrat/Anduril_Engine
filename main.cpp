#include <chrono>
#include <iostream>

#include "Anduril.h"
#include "ConsoleGame.h"
#include "libchess/Position.h"
#include "UCI.h"

std::array<libchess::Bitboard, 64> libchess::lookups::SQUARES;

std::array<libchess::Bitboard, 64> libchess::lookups::NORTH;
std::array<libchess::Bitboard, 64> libchess::lookups::SOUTH;
std::array<libchess::Bitboard, 64> libchess::lookups::EAST;
std::array<libchess::Bitboard, 64> libchess::lookups::WEST;
std::array<libchess::Bitboard, 64> libchess::lookups::NORTHWEST;
std::array<libchess::Bitboard, 64> libchess::lookups::SOUTHWEST;
std::array<libchess::Bitboard, 64> libchess::lookups::NORTHEAST;
std::array<libchess::Bitboard, 64> libchess::lookups::SOUTHEAST;
std::array<std::array<libchess::Bitboard, 64>, 64> libchess::lookups::INTERVENING;

std::array<std::array<libchess::Bitboard, 64>, 2> libchess::lookups::PAWN_ATTACKS;
std::array<libchess::Bitboard, 64> libchess::lookups::KNIGHT_ATTACKS;
std::array<libchess::Bitboard, 64> libchess::lookups::KING_ATTACKS;
std::array<libchess::Bitboard, 64> libchess::lookups::BISHOP_ATTACKS;
std::array<libchess::Bitboard, 64> libchess::lookups::ROOK_ATTACKS;
std::array<libchess::Bitboard, 64> libchess::lookups::QUEEN_ATTACKS;

libchess::lookups::MagicAttacksLookup<libchess::lookups::SlidingPieceType::ROOK> libchess::lookups::rook_magic_attacks_lookup;
libchess::lookups::MagicAttacksLookup<libchess::lookups::SlidingPieceType::BISHOP> libchess::lookups::bishop_magic_attacks_lookup;

std::array<std::array<libchess::Bitboard, 64>, 64> libchess::lookups::FULL_RAY;

int main() {
    libchess::lookups::SQUARES = libchess::lookups::init::squares();

    libchess::lookups::NORTH = libchess::lookups::init::north();
    libchess::lookups::SOUTH = libchess::lookups::init::south();
    libchess::lookups::EAST = libchess::lookups::init::east();
    libchess::lookups::WEST = libchess::lookups::init::west();
    libchess::lookups::NORTHWEST = libchess::lookups::init::northwest();
    libchess::lookups::SOUTHWEST = libchess::lookups::init::southwest();
    libchess::lookups::NORTHEAST = libchess::lookups::init::northeast();
    libchess::lookups::SOUTHEAST = libchess::lookups::init::southeast();
    libchess::lookups::INTERVENING = libchess::lookups::init::intervening();

    libchess::lookups::PAWN_ATTACKS = libchess::lookups::init::pawn_attacks();
    libchess::lookups::KNIGHT_ATTACKS = libchess::lookups::init::knight_attacks();
    libchess::lookups::KING_ATTACKS = libchess::lookups::init::king_attacks();
    libchess::lookups::BISHOP_ATTACKS = libchess::lookups::init::bishop_attacks();
    libchess::lookups::ROOK_ATTACKS = libchess::lookups::init::rook_attacks();
    libchess::lookups::QUEEN_ATTACKS = libchess::lookups::init::queen_attacks();

    libchess::lookups::rook_magic_attacks_lookup = libchess::lookups::init::rook_magic_attacks_lookup();
    libchess::lookups::bishop_magic_attacks_lookup = libchess::lookups::init::bishop_magic_attacks_lookup();

    libchess::lookups::FULL_RAY = libchess::lookups::init::full_ray();

    // flag for switching to debug or UCI
    bool useUCI = true;

    if (useUCI) {
        // for profiling
        /*
        Anduril AI;
        AI.limits.depth = 15;
        libchess::Position board("r1bn1rk1/pp2ppbp/6p1/3P4/4P3/5N2/q2BBPPP/1R1Q1RK1 w - - 1 14");
        AI.startTime = std::chrono::steady_clock::now();
        AI.go(board);
        */
        UCI::loop();
    }

    else {
        libchess::Position board(libchess::constants::STARTPOS_FEN);
        Anduril AI;
        Book openingBook = Book(R"(..\book\Performance.bin)");
        libchess::Position::GameState state;

        // this exists so that I can test problematic board positions if and when they arise
        //board.Forsyth("7r/2kr2pp/2p2p2/p4N2/1bnBP3/2P2P2/P4KPP/3R3R b - - 0 24");

        //board.Forsyth("8/4Q2k/6pp/8/1P6/4PKPP/4rP2/5q2 b - - 9 60");
        board.from_fen("r1bn1rk1/pp2ppbp/6p1/3P4/4P3/5N2/q2BBPPP/1R1Q1RK1 w - - 1 14");

        Game::displayBoard(board);
        std::cout << "Board FEN: " << board.fen() << std::endl;
        AI.positionStack.push_back(board.hash());

        // main game loop
        while (board.game_state() == libchess::Position::GameState::IN_PROGRESS) {
            std::cout << (!board.side_to_move() ? "White to move" : "Black to move") << std::endl;

            if (openingBook.getBookOpen()) {
                libchess::Move bestMove = openingBook.getBookMove(board);
                if (bestMove.value() != 0) {
                    std::cout << "Moving " << bestMove.to_str() << " from book" << std::endl;
                    board.make_move(bestMove);
                } else {
                    std::cout << "End of opening book, starting search" << std::endl;
                    openingBook.flipBookOpen();
                }
            } else {
                AI.goDebug(board, 10);
            }



            /*
            if (!board.side_to_move()) {
                Game::turn(board, AI);
            }
            else {
                if (openingBook.getBookOpen()) {
                    libchess::Move bestMove = openingBook.getBookMove(board);
                    if (bestMove.value() != 0) {
                        std::cout << "Moving " << bestMove.to_str() << " from book" << std::endl;
                        board.make_move(bestMove);
                    }
                    else {
                        std::cout << "End of opening book, starting search" << std::endl;
                        openingBook.flipBookOpen();
                    }
                }
                else {
                    AI.goDebug(board, 10);
                }
            }
             */
             


            Game::displayBoard(board);
            std::cout << "Board FEN: " << board.fen() << std::endl;

        }

        if (!board.side_to_move() && board.game_state() == libchess::Position::GameState::CHECKMATE) {
            std::cout << "Checkmate.  Black Wins." << std::endl;
        }
        else if (board.side_to_move() && board.game_state() == libchess::Position::GameState::CHECKMATE) {
            std::cout << "Checkmate.  White Wins." << std::endl;
        }
        else {
            std::cout << "Stalemate.  It's a draw." << std::endl;
        }

    }





    return 0;
}
