//
// Created by 80hugkev on 7/6/2022.
//

// these includes are for finding/reading input only
#ifdef WIN32
#include "windows.h"
#else
#include "sys/time.h"
#include "sys/select.h"
#include "unistd.h"
#include "string.h"
#endif

#include <chrono>
#include <cmath>
#include <iostream>
#include <io.h>
#include <string>

#include "Anduril.h"
#include "ConsoleGame.h"
#include "UCI.h"
#include "thc.h"
#include "ZobristHasher.h"

// all the UCI protocol stuff is going to be implemented
// with C (C++ dispersed for when I know which C++ item to use over the C implementation) code because the tutorial
// I am following is written in C
namespace UCI {
    // this is the largest input we expect from the GUI
    constexpr int INPUTBUFFER = 400 * 6;

    // FEN for the start position
    const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    void loop() {

        // first we turn off the stdin and stdout buffers
        setbuf(stdin, NULL);
        setbuf(stdout, NULL);

        // send engine info to the GUI
        char line[INPUTBUFFER];
        std::cout << "id name Anduril" << std::endl;
        std::cout << "id author Krtoonbrat" << std::endl;
        std::cout << "uciok" << std::endl;
        std::cout << "option name pMG type spin default 100 min 50 max 300" << std::endl;
        std::cout << "option name pEG type spin default 110 min 50 max 300" << std::endl;
        std::cout << "option name kMG type spin default 337 min 100 max 500" << std::endl;
        std::cout << "option name kEG type spin default 281 min 100 max 500" << std::endl;
        std::cout << "option name bMG type spin default 365 min 100 max 500" << std::endl;
        std::cout << "option name bEG type spin default 297 min 100 max 500" << std::endl;
        std::cout << "option name rMG type spin default 477 min 300 max 700" << std::endl;
        std::cout << "option name rEG type spin default 512 min 300 max 700" << std::endl;
        std::cout << "option name qMG type spin default 1025 min 700 max 1200" << std::endl;
        std::cout << "option name qEG type spin default 936 min 700 max 1200" << std::endl;

        // set up the board, engine, book, and game state
        thc::ChessRules board;
        Anduril AI;
        Book openingBook = Book(R"(..\book\Performance.bin)");
        thc::TERMINAL gameOver = thc::NOT_TERMINAL;

        while (true) {
            // clear the line and flush stdout in case of buffer issues
            memset(&line[0], 0, sizeof(line));
            fflush(stdout);

            // continue if we don't receive anything
            if (!fgets(line, INPUTBUFFER, stdin)) { continue; }

            // continue if all we get is a new line
            if (line[0] == '\n') { continue; }

            // these are the commands we can get from the GUI
            if (!strncmp(line, "isready", 7)) {
                std::cout << "readyok" << std::endl;
            }
            else if (!strncmp(line, "position", 8)) {
                parsePosition(line, board, AI);
            }
            else if (!strncmp(line, "setoption", 9)) {
                parseOption(line, AI);
            }
            else if (!strncmp(line, "ucinewgame", 10)) {
                if (!openingBook.getBookOpen()) { openingBook.flipBookOpen(); }
                AI.table.clear();
                AI.pTable = HashTable<SimpleNode, 128>();
                AI.evalTable = HashTable<SimpleNode, 128>();
                parsePosition(line, board, AI);
            }
            else if (!strncmp(line, "go", 2)) {
                parseGo(line, board, AI, openingBook);
            }
            else if (!strncmp(line, "quit", 4)) {
                AI.quit = true;
                break;
            }
            else if (!strncmp(line, "uci", 3)) {
                std::cout << "id name Anduril" << std::endl;
                std::cout << "id author Krtoonbrat" << std::endl;
                std::cout << "uciok" << std::endl;
                std::cout << "option name pMG type spin default 100 min 50 max 300" << std::endl;
                std::cout << "option name pEG type spin default 110 min 50 max 300" << std::endl;
                std::cout << "option name kMG type spin default 337 min 100 max 500" << std::endl;
                std::cout << "option name kEG type spin default 281 min 100 max 500" << std::endl;
                std::cout << "option name bMG type spin default 365 min 100 max 500" << std::endl;
                std::cout << "option name bEG type spin default 297 min 100 max 500" << std::endl;
                std::cout << "option name rMG type spin default 477 min 300 max 700" << std::endl;
                std::cout << "option name rEG type spin default 512 min 300 max 700" << std::endl;
                std::cout << "option name qMG type spin default 1025 min 700 max 1200" << std::endl;
                std::cout << "option name qEG type spin default 936 min 700 max 1200" << std::endl;
            }

            // break the loop if quit is received
            if (AI.quit) { break; }
        }
    }

    void parseOption(char* line, Anduril &AI) {
        char *ptr = NULL;

        // setoption name pMG value 100
        if ((ptr = strstr(line, "pMG"))) {
            AI.pMG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "pEG"))) {
            AI.pEG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "kMG"))) {
            AI.kMG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "kEG"))) {
            AI.kEG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "bMG"))) {
            AI.bMG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "bEG"))) {
            AI.bEG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "rMG"))) {
            AI.rMG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "rEG"))) {
            AI.rEG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "qMG"))) {
            AI.qMG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "qEG"))) {
            AI.qEG = atoi(ptr + 10);
        }
    }

    void parseGo(char* line, thc::ChessRules &board, Anduril &AI, Book &openingBook) {
        // reset all the limits
        int depth = -1; int moveTime = -1; int mtg = 25;
        int time = -1;
        int increment = -1;
        AI.limits.timeSet = false;
        char *ptr = NULL;

        // the opening book stays open unless we are doing an infinite search
        openingBook.openBook();

        // commands
        if ((ptr = strstr(line, "infinite"))) {
            openingBook.closeBook();
            ;
        }

        if ((ptr = strstr(line, "btime")) && !board.WhiteToPlay()) {
            time = atoi(ptr + 6);
        }

        if ((ptr = strstr(line, "wtime")) && board.WhiteToPlay()) {
            time = atoi(ptr + 6);
        }

        if ((ptr = strstr(line, "binc")) && !board.WhiteToPlay()) {
            increment = atoi(ptr + 5);
        }

        if ((ptr = strstr(line, "winc")) && board.WhiteToPlay()) {
            increment = atoi(ptr + 5);
        }

        if ((ptr = strstr(line, "movestogo"))) {
            mtg = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "movetime"))) {
            moveTime = atoi(ptr + 9);
        }

        if ((ptr = strstr(line, "depth"))) {
            depth = atoi(ptr + 6);
        }

        if (moveTime != -1) {
            time = moveTime;
            mtg = 1;
        }

        AI.startTime = std::chrono::steady_clock::now();
        AI.limits.depth = depth;

        if (time != -1) {
            AI.limits.timeSet = true;
            time /= mtg;
            time -= 50;
            std::chrono::milliseconds searchTime(time + increment);
            AI.stopTime = std::chrono::steady_clock::now();
            AI.stopTime += searchTime;
        }

        if (depth == -1) {
            // we won't ever hit a depth of 100, so it stands in as a "max" or "infinite" depth
            AI.limits.depth = 100;
        }

        /*
        std::cout << "time: " << time << " start: " << AI.startTime.time_since_epoch().count() <<
        " stop: " << AI.stopTime.time_since_epoch().count() << " depth: " << AI.limits.depth << " timeset: " << AI.limits.timeSet << std::endl;
         */

        if (openingBook.getBookOpen()) {
            thc::Move bestMove = openingBook.getBookMove(board);
            if (bestMove.Valid()) {
                std::cout << "bestmove " << bestMove.TerseOut() << std::endl;
                AI.makeMovePlay(board, bestMove);
            }
            else {
                //std::cout << "End of opening book, starting search" << std::endl;
                openingBook.flipBookOpen();
                AI.go(board);
            }
        }
        else {
            AI.go(board);
        }
    }

    void parsePosition(char* line, thc::ChessRules &board, Anduril &AI) {
        // first move the pointer past the "position" token
        line += 9;

        // save the start position for the pointer
        char *ptrToken = line;

        // instructions for different commands we could receive
        if (strncmp(line, "startpos", 8) == 0) {
            board.Forsyth(StartFEN);
            AI.positionStack.push_back(Zobrist::hashBoard(board));
        }
        else {
            ptrToken = strstr(line, "fen");
            if (ptrToken == NULL) {
                board.Forsyth(StartFEN);
                AI.positionStack.push_back(Zobrist::hashBoard(board));
            }
            else {
                ptrToken += 4;
                board.Forsyth(ptrToken);
                AI.positionStack.push_back(Zobrist::hashBoard(board));
            }
        }

        // set up the variable for parsing the moves
        ptrToken = strstr(line, "moves");
        thc::Move move;
        move.Invalid();

        // parse the moves the GUI sent
        if (ptrToken != NULL) {
            ptrToken += 6;
            while (*ptrToken) {
                move.TerseIn(&board, ptrToken);
                if (!move.Valid()) { break; }
                AI.makeMovePlay(board, move);
                AI.positionStack.push_back(Zobrist::hashBoard(board));
                while (*ptrToken && *ptrToken != ' ') { ptrToken++; }
                ptrToken++;
            }
        }

        /*
        Game::displayBoard(board);
        std::cout << "Board FEN: " << board.ForsythPublish() << std::endl;
        std::cout << (board.WhiteToPlay() ? "White to move" : "Black to move") << std::endl;
         */
    }

    // http://home.arcor.de/dreamlike/chess/
    int InputWaiting()
    {
#ifndef WIN32
        fd_set readfds;
  struct timeval tv;
  FD_ZERO (&readfds);
  FD_SET (fileno(stdin), &readfds);
  tv.tv_sec=0; tv.tv_usec=0;
  select(16, &readfds, 0, 0, &tv);

  return (FD_ISSET(fileno(stdin), &readfds));
#else
        static int init = 0, pipe;
        static HANDLE inh;
        DWORD dw;

        if (!init) {
            init = 1;
            inh = GetStdHandle(STD_INPUT_HANDLE);
            pipe = !GetConsoleMode(inh, &dw);
            if (!pipe) {
                SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT|ENABLE_WINDOW_INPUT));
                FlushConsoleInputBuffer(inh);
            }
        }
        if (pipe) {
            if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) return 1;
            return dw;
        } else {
            GetNumberOfConsoleInputEvents(inh, &dw);
            return dw <= 1 ? 0 : dw;
        }
#endif
    }

    void ReadInput(Anduril &AI) {
        int             bytes;
        char            input[256] = "", *endc;

        if (InputWaiting()) {
            AI.stopped = true;
            do {
                bytes=read(fileno(stdin),input,256);
            } while (bytes<0);
            endc = strchr(input,'\n');
            if (endc) *endc=0;

            if (strlen(input) > 0) {
                if (!strncmp(input, "quit", 4))    {
                    AI.quit = true;
                }
            }
            return;
        }
    }

}  // namespace UCI

// calls negamax and keeps track of the best move
// this version will also interact with UCI
void Anduril::go(thc::ChessRules &board) {
    thc::Move bestMove;
    bestMove.Invalid();
    thc::Move prevBestMove = bestMove;

    // this is for debugging
    std::string boardFENs = board.ForsythPublish();
    char *boardFEN = &boardFENs[0];

    int alpha = -999999999;
    int beta = 999999999;
    int bestScore = -999999999;
    int prevBestScore = bestScore;

    // set up the piece lists
    clearPieceLists();
    fillPieceLists(board);

    // set the killer vector to have the correct number of slots
    // and the root node's ply
    // the vector is padded a little at the end in case of the search being extended
    killers = std::vector<std::vector<thc::Move>>(218); // 218 is the "max moves" defined in thc.h
    for (auto & killer : killers) {
        killer = std::vector<thc::Move>(2);
    }

    rootPly = board.WhiteToPlay() ? (board.full_move_count * 2) - 1 : board.full_move_count * 2;

    // these variables are for debugging
    int aspMissesL = 0, aspMissesH = 0;
    int n1 = 0;
    double branchingFactor = 0;
    double avgBranchingFactor = 0;
    bool research = false;
    std::vector<int> misses;
    // this makes sure the score is reported correctly
    // if the AI is controlling black, we need to
    // multiply the score by -1 to report it correctly to the player
    int flipped = 1;

    // we need to values for alpha because we need to change alpha based on
    // our results, but we also need a copy of the original alpha for the
    // aspiration window
    int alphaTheSecond = alpha;

    // get the move list
    std::vector<std::tuple<int, thc::Move>> moveListWithScores = getMoveList(board, nullptr);

    // this starts our move to search at the first move in the list
    thc::Move move = std::get<1>(moveListWithScores[0]);

    if (!board.white){
        flipped = -1;
    }

    int score = -999999999;
    int deep = 1;
    bool finalDepth = false;
    bool incomplete = false;
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> timeElapsed = end - startTime;
    // iterative deepening loop
    while (!finalDepth) {
        UCI::ReadInput(*this);

        if (deep == limits.depth){
            finalDepth = true;
        }

        alphaTheSecond = alpha;
        incomplete = false;

        for (int i = 0; i < moveListWithScores.size(); i++) {
            // stop the search it time is up
            if (stopped || (limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime)) {
                incomplete = true;
                finalDepth = true;
                break;
            }

            // find the next best move to search
            // skips this step if we are on the first depth because all scores will be zero
            if (deep != 1) {
                move = pickNextMove(moveListWithScores, i);
            }
            else {
                move = std::get<1>(moveListWithScores[i]);
            }
            end = std::chrono::steady_clock::now();
            timeElapsed = end - startTime;
            /*
            if (timeElapsed.count() >= 1000) {
                std::cout << "info currmove " << move.TerseOut() << " currmovenumber " << i + 1 << std::endl;
            }
             */

            makeMove(board, move);
            deep--;
            movesExplored++;
            depthNodes++;
            score = -negamax<PV>(board, deep, -beta, -alphaTheSecond);
            deep++;
            undoMove(board, move);

            if (score > bestScore || (score == -999999999 && bestScore == -999999999)) {
                bestScore = score;
                alphaTheSecond = score;
                bestMove = move;
            }

            // see if we need to reset the aspiration window
            if ((bestScore <= alpha || bestScore >= beta) && !(bestScore == 999999999 || bestScore == -999999999)) {
                incomplete = true;
                break;
            }

            // update the score within the tuple
            std::get<0>(moveListWithScores[i]) = score;

        }

        if (!incomplete) {
            prevBestMove = bestMove;
            prevBestScore = bestScore;
        }

        // send info to the GUI
        end = std::chrono::steady_clock::now();
        timeElapsed = end - startTime;

        /*
        std::cout << "info score cp " << prevBestScore << " depth " << deep << " nodes " <<
        movesExplored << " nps " << getMovesExplored() / (timeElapsed.count()/1000) << " time " << timeElapsed.count();
         */

        std::vector<thc::Move> PV = getPV(board, limits.depth, bestMove);
        std::string pv = "";
        for (auto m : PV) {
            pv += " " + m.TerseOut();
        }

        std::cout << " pv" << pv << std::endl;

        // check if we found mate
        if (bestScore == 999999999 || bestScore == -999999999) {
            break;
        }

        // set the aspiration window
        // we only actually use the aspiration window at depths greater than 4.
        // this is because we don't have a decent picture of the position yet
        // and the search falls outside the window often causing instability
        if (deep > 5) {
            // search was outside the window, need to redo the search
            // fail low
            if (bestScore <= alpha) {
                if (!limits.timeSet) { finalDepth = false; }
                misses.push_back(deep);
                aspMissesL++;
                alpha = bestScore - 50 * (std::pow(3, aspMissesL));
                beta = bestScore + 50 * (std::pow(3, aspMissesH));
                research = true;
            }
                // fail high
            else if (bestScore >= beta) {
                if (!limits.timeSet) { finalDepth = false; }
                misses.push_back(deep);
                aspMissesH++;
                alpha = bestScore - 50 * (std::pow(3, aspMissesL));
                beta = bestScore + 50 * (std::pow(3, aspMissesH));
                research = true;
            }
                // the search didn't fall outside the window, we can move to the next depth
            else {
                alpha = bestScore - 50 * (std::pow(3, aspMissesL));
                beta = bestScore + 50 * (std::pow(3, aspMissesH));
                deep++;
                research = false;
            }
        }
            // for depths less than 5
        else {
            deep++;
            research = false;
        }

        // add the current depth to the branching factor
        // if we searched to depth 1, then we need to do another search
        // to get a branching factor
        if (n1 == 0){
            n1 = movesExplored;
        }
            // we can't calculate a branching factor if we researched because of
            // a window miss, so we first check if the last entry to misses is our current depth
        else if (!research) {
            branchingFactor = (double) depthNodes / n1;
            avgBranchingFactor = ((avgBranchingFactor * (deep - 2)) + branchingFactor) / (deep - 1);
            n1 = depthNodes;
            depthNodes = 0;
        }


        // for debugging
        if (boardFEN != board.ForsythPublish()) {
            std::cout << "Board does not match original at depth: " << deep << std::endl;
            board.Forsyth(boardFEN);
            clearPieceLists();
            fillPieceLists(board);
        }


        // reset the variables to prepare for the next loop
        if (!finalDepth) {
            bestScore = -999999999;
            alphaTheSecond = -999999999;
            score = -999999999;
        }
    }

    std::vector<thc::Move> PV = getPV(board, limits.depth, bestMove);

    setMovesExplored(0);
    cutNodes = 0;
    movesTransposed = 0;
    quiesceExplored = 0;
    clearPieceLists();
    stopped = false;

    // tell the GUI what move we want to make
    std::cout << "info score cp " << prevBestScore << std::endl;
    std::cout << "bestmove " << prevBestMove.TerseOut() << std::endl;

    makeMovePlay(board, bestMove);
}