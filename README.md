# Anduril_Engine

Anduril is a UCI compatable chess engine written in C++.
Currently, the engine uses thc (tripplehappychess: https://github.com/billforsternz/thc-chess-library) for board representation and move generation.  I am in the process of switching to libchess (https://github.com/Mk-Chan/libchess).
The engine uses the negamax algorithm with many enhancements such as alpha-beta pruning, null move pruning, a simplified version of probcut, futility pruning, and more techinques.

This is a passion project and is my third attempt at writing a chess engine.  My first engine was built from the ground up and was extremely buggy and slow.
My second attempt was built using python-chess and was better, but I started to reach the limitations of the python language and my own programming abilities.
I decided to try to make a third attempt in C++ once I got comfortable enough in the language, and this is the fruits of that labor.
