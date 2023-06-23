cmake_minimum_required(VERSION 3.16)
project(Anduril_Engine)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")

add_executable(Anduril_Engine main.cpp Anduril.cpp Anduril.h Node.h PolyglotBook.cpp PolyglotBook.h TranspositionTable.cpp TranspositionTable.h evaluation.cpp UCI.cpp UCI.h limit.h ZobristHasher.cpp ZobristHasher.h MovePicker.cpp MovePicker.h History.h)
