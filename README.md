# Anduril_Engine

Anduril is a UCI compatable chess engine written in C++.
Anduril is built off of libchess (https://github.com/Mk-Chan/libchess) with a few changes (mostly to improve performance).
The engine uses the negamax algorithm with many enhancements such as alpha-beta pruning, null move pruning, probcut, futility pruning, and more techinques.

This is a passion project and is my third attempt at writing a chess engine.  My first engine was built from the ground up and was extremely buggy and slow.
My second attempt was built using python-chess and was better, but I started to reach the limitations of my own programming abilities.
I decided to try to make a third attempt in C++ once I got comfortable enough in the language, and this is the fruits of that labor.

Because this is a passion project, parts of the engine are underdeveloped compared to other parts.  My development strategy has been to work on whatever
I find most interesting or fun to work on in the moment.  There is no pattern to what I decide to work on, and thats probably how it will stay.

Anduril has a lichess account (https://lichess.org/@/Anduril_Engine), but unfortunately it is a local script I have to run, so it is not available 24/7.
