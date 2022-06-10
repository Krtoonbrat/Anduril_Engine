cmake_minimum_required(VERSION 3.20)
project(Anduril_Engine)

set(CMAKE_CXX_STANDARD 20)

add_executable(Anduril_Engine main.cpp thc.cpp thc.h ConsoleGame.cpp ConsoleGame.h Anduril.cpp Anduril.h Node.h PolyglotBook.cpp PolyglotBook.h TranspositionTable.cpp TranspositionTable.h evaluation.cpp)
