//
// Created by 80hugkev on 7/6/2022.
//

// these includes are for finding/reading input only
#ifdef WIN32
#include <io.h>
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
#include <string>

#include "Anduril.h"
#include "libchess/Position.h"
#include "UCI.h"

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
        //setbuf(stdin, NULL);
        //setbuf(stdout, NULL);

        char line[INPUTBUFFER];



        // set up the board, engine, book, and game state
        libchess::Position board(StartFEN);
        Anduril AI;
        Book openingBook = Book(R"(..\book\Performance.bin)");
        bool bookOpen = true;

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
                parseOption(line, AI, bookOpen);
            }
            else if (!strncmp(line, "ucinewgame", 10)) {
                if (!openingBook.getBookOpen()) { openingBook.flipBookOpen(); }
                AI.table.clear();
                AI.pTable = HashTable<PawnEntry, 8>();
                AI.evalTable = HashTable<SimpleNode, 16>();
                AI.resetHistories();
                board = *board.from_fen(StartFEN);
                parsePosition(line, board, AI);
            }
            else if (!strncmp(line, "go", 2)) {
                parseGo(line, board, AI, openingBook, bookOpen);
            }
            else if (!strncmp(line, "quit", 4)) {
                AI.quit = true;
                break;
            }
            else if (!strncmp(line, "uci", 3)) {
                std::cout << "id name Anduril" << std::endl;
                std::cout << "id author Krtoonbrat" << std::endl;

                std::cout << "option name Hash type spin default 256 min 16 max 33554432" << std::endl;
                std::cout << "option name Book type check default true" << std::endl;

                std::cout << "option name kMG type spin default 337 min -4000 max 4000" << std::endl;
                std::cout << "option name kEG type spin default 281 min -4000 max 4000" << std::endl;
                std::cout << "option name oMG type spin default 10  min -1000 max 1000" << std::endl;
                std::cout << "option name oEG type spin default 10  min -1000 max 1000" << std::endl;
                std::cout << "option name tMG type spin default 150 min -1000 max 1000" << std::endl;
                std::cout << "option name tEG type spin default 150 min -1000 max 1000" << std::endl;

                std::cout << "option name pMG type spin default 88 min -4000 max 4000" << std::endl;
                std::cout << "option name pEG type spin default 138 min -4000 max 4000" << std::endl;

                std::cout << "option name bMG type spin default 365 min -4000 max 4000" << std::endl;
                std::cout << "option name bEG type spin default 297 min -4000 max 4000" << std::endl;

                std::cout << "option name rMG type spin default 477 min -4000 max 4000" << std::endl;
                std::cout << "option name rEG type spin default 512 min -4000 max 4000" << std::endl;

                std::cout << "option name qMG type spin default 1025 min -4000 max 4000" << std::endl;
                std::cout << "option name qEG type spin default 936 min -4000 max 4000" << std::endl;

                std::cout << "option name Pph type spin default 125 min 0 max 10000" << std::endl;
                std::cout << "option name Kph type spin default 1000 min 0 max 10000" << std::endl;
                std::cout << "option name Bph type spin default 1000 min 0 max 10000" << std::endl;
                std::cout << "option name Rph type spin default 2000 min 0 max 10000" << std::endl;
                std::cout << "option name Qph type spin default 4000 min 0 max 10000" << std::endl;

                std::cout << "option name QOV type spin default 10000 min 0 max 100000" << std::endl;
                std::cout << "option name ROV type spin default 5000 min 0 max 100000" << std::endl;
                std::cout << "option name MOV type spin default 2500 min 0 max 100000" << std::endl;
                std::cout << "option name MHV type spin default 1000000 min 0 max 1000000" << std::endl;

                std::cout << "uciok" << std::endl;


            }

            // break the loop if quit is received
            if (AI.quit) { break; }
        }
    }

    void parseOption(char* line, Anduril &AI, bool &bookOpen) {
        char *ptr = NULL;

        // set hash size
        if ((ptr = strstr(line, "Hash"))) {
            AI.table.resize(atoi(ptr + 10));
        }

        // set book open or closed
        if ((ptr = strstr(line, "Book"))) {
            if ((ptr = strstr(line, "true"))) {
                bookOpen = true;
            }
            else {
                bookOpen = false;
            }
        }

        // setoption name pMG value 100
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

        if ((ptr = strstr(line, "oMG"))) {
            AI.oMG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "oEG"))) {
            AI.oEG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "tMG"))) {
            AI.tMG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "tEG"))) {
            AI.tEG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "pMG"))) {
            AI.pMG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "pEG"))) {
            AI.pEG = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "Pph"))) {
            AI.Pph = atof(ptr + 10) / 1000;
        }

        if ((ptr = strstr(line, "Kph"))) {
            AI.Kph = atof(ptr + 10) / 1000;
        }

        if ((ptr = strstr(line, "Bph"))) {
            AI.Bph = atof(ptr + 10) / 1000;
        }

        if ((ptr = strstr(line, "Rph"))) {
            AI.Rph = atof(ptr + 10) / 1000;
        }

        if ((ptr = strstr(line, "Qph"))) {
            AI.Qph = atof(ptr + 10) / 1000;
        }

        if ((ptr = strstr(line, "QOV"))) {
            queenOrderVal = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "ROV"))) {
            rookOrderVal = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "MOV"))) {
            minorOrderVal = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "MHV"))) {
            maxHistoryVal = atoi(ptr + 10);
        }
    }

    void parseGo(char* line, libchess::Position &board, Anduril &AI, Book &openingBook, bool &bookOpen) {
        // reset all the limit
        int depth = -1; int moveTime = -1; int mtg = 35;
        int time = -1;
        int increment = -1;
        AI.limits.timeSet = false;
        char *ptr = NULL;

        // the opening book stays open unless we are doing an infinite search
        // jk no opening book rn cuz of CLOP
        if (!bookOpen) {
            openingBook.closeBook();
        }

        // commands
        if ((ptr = strstr(line, "infinite"))) {
            openingBook.closeBook();
            ;
        }

        if ((ptr = strstr(line, "btime")) && board.side_to_move()) {
            time = atoi(ptr + 6);
        }

        if ((ptr = strstr(line, "wtime")) && !board.side_to_move()) {
            time = atoi(ptr + 6);
        }

        if ((ptr = strstr(line, "binc")) && board.side_to_move()) {
            increment = atoi(ptr + 5);
        }

        if ((ptr = strstr(line, "winc")) && !board.side_to_move()) {
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
        " stop: " << AI.stopTime.time_since_epoch().count() << " depth: " << AI.limit.depth << " timeset: " << AI.limit.timeSet << std::endl;
         */

        if (openingBook.getBookOpen()) {
            libchess::Move bestMove = openingBook.getBookMove(board);
            if (bestMove.value() != 0) {
                std::cout << "bestmove " << bestMove.to_str() << std::endl;
                board.make_move(bestMove);
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

    void parsePosition(char* line, libchess::Position &board, Anduril &AI) {
        // first move the pointer past the "position" token
        line += 9;

        // save the start position for the pointer
        char *ptrToken = line;

        // instructions for different commands we could receive
        if (strncmp(line, "startpos", 8) == 0) {
            board = *libchess::Position::from_fen(StartFEN);
        }
        else {
            ptrToken = strstr(line, "fen");
            if (ptrToken == NULL) {
                board = *libchess::Position::from_fen(StartFEN);
            }
            else {
                ptrToken += 4;
                board = *libchess::Position::from_fen(ptrToken);
            }
        }

        // reset the ply counter
        AI.resetPly();

        // set up the variable for parsing the moves
        ptrToken = strstr(line, "moves");
        libchess::Move move(0);

        // parse the moves the GUI sent
        if (ptrToken != NULL) {
            ptrToken += 6;
            while (*ptrToken) {
                std::string moveStr;
                if (ptrToken[4] == ' ' || ptrToken[4] == '\n') {
                    moveStr = std::string(ptrToken, 4);
                }
                else {
                    moveStr = std::string(ptrToken, 5);
                }
                move = *libchess::Move::from(moveStr);
                if (move.value() == 0) { break; }
                board.make_move(move);
                AI.incPly();
                while (*ptrToken && *ptrToken != ' ') {
                    ptrToken++;
                }
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
void Anduril::go(libchess::Position &board) {
    //std::cout << board.fen() << std::endl;
    libchess::Move bestMove(0);
    libchess::Move prevBestMove = bestMove;

    // prep the board for search
    board.prep_search();

    // this is for debugging
    std::string boardFENs = board.fen();
    char *boardFEN = &boardFENs[0];

    int alpha = -32001;
    int beta = 32001;
    int bestScore = -32001;
    int prevBestScore = bestScore;

    // set the killer vector to have the correct number of slots
    // the vector is padded a little at the end in case of the search being extended
    for (auto i : killers) {
        i[0] = libchess::Move(0);
        i[1] = libchess::Move(0);
    }

    rootPly = ply;
    age++;

    // these variables are for debugging
    int aspMissesL = 0, aspMissesH = 0;
    int n1 = 0;
    double branchingFactor = 0;
    double avgBranchingFactor = 0;
    std::vector<int> misses;

    rDepth = 1;
    singularAttempts = 0;
    singularExtensions = 0;
    bool finalDepth = false;
    bool incomplete = false;
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> timeElapsed = end - startTime;
    bool upper = false;
    bool lower = false;

    // grab the hash for the board, then the node.
    // if this is our first search, no node will be found, we will look again later if this is the case
    uint64_t hash = board.hash();
    bool found = false;
    Node *node = table.probe(hash, found);

    // get the move list for root position
    rootMoves = board.legal_move_list();

    // iterative deepening loop
    while (!finalDepth) {
        UCI::ReadInput(*this);

        if (rDepth == limits.depth){
            finalDepth = true;
        }

        // reset selDepth
        if (!incomplete) {
            selDepth = 0;
        }

        incomplete = false;

        // search for the best score
        bestScore = negamax<Root>(board, rDepth, alpha, beta, false);

        // if we didn't find a node before, try again now that we have searched
        if (!found) {
            node = table.probe(hash, found);
        }

        // was the search stopped?
        // stop the search it time is up
        if (stopped || (limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime)) {
            incomplete = true;
            finalDepth = true;
        }

        // set the aspiration window
        if (rDepth >= 5) {
            // search was outside the window, need to redo the search
            // fail low
            if (bestScore <= alpha) {
                //std::cout << "Low miss at: " << rDepth << std::endl;
                if (!limits.timeSet) { finalDepth = false; }
                misses.push_back(rDepth);
                aspMissesL++;
                alpha = bestScore - 40 * (std::pow(2, aspMissesL));
                beta = bestScore + 40 * (std::pow(2, aspMissesH));
                incomplete = true;
                upper = true;
            }
                // fail high
            else if (bestScore >= beta) {
                if (!limits.timeSet) { finalDepth = false; }
                //std::cout << "High miss at: " << rDepth << std::endl;
                misses.push_back(rDepth);
                aspMissesH++;
                alpha = bestScore - 40 * (std::pow(2, aspMissesL));
                beta = bestScore + 40 * (std::pow(2, aspMissesH));
                incomplete = true;
                lower = true;
            }
                // the search didn't fall outside the window, we can move to the next depth
            else {
                alpha = bestScore - 40 * (std::pow(2, aspMissesL));
                beta = bestScore + 40 * (std::pow(2, aspMissesH));
                rDepth++;
                upper = lower = false;
            }
        }
            // for depths less than 5
        else {
            rDepth++;
        }

        if (!incomplete && found) {
            prevBestMove = node->bestMove;
            prevBestScore = bestScore;
            //std::cout << "Total low misses: " << aspMissesL << std::endl;
            //std::cout << "Total high misses: " << aspMissesH << std::endl;
        }

        // send info to the GUI
        end = std::chrono::steady_clock::now();
        timeElapsed = end - startTime;

        std::vector<libchess::Move> PV = getPV(board, rDepth, prevBestMove);
        std::string pv = "";
        for (auto m: PV) {
            pv += " " + m.to_str();
        }

        if (!incomplete) {
            if (prevBestScore >= 31000) {
                int distance = ((-prevBestScore + 32000) / 2) + (prevBestScore % 2);
                std::cout << "info "
                          << "score mate " << distance
                          << " depth "     << rDepth - 1
                          << " seldepth "  << selDepth
                          << " nodes "     << movesExplored
                          << " nps "       << (int) (getMovesExplored() / (timeElapsed.count() / 1000))
                          << " time "      << (int)timeElapsed.count()
                          << " pv"         << pv << std::endl;
            }
            else if (prevBestScore <= -31000) {
                int distance = -((prevBestScore + 32000) / 2) + -(prevBestScore % 2);
                std::cout << "info "
                          << "score mate " << distance
                          << " depth "     << rDepth - 1
                          << " seldepth "  << selDepth
                          << " nodes "     << movesExplored
                          << " nps "       << (int) (getMovesExplored() / (timeElapsed.count() / 1000))
                          << " time "      << (int)timeElapsed.count()
                          << " pv"         << pv << std::endl;
            }
            else {
                std::cout << "info "
                          << "score cp "   << prevBestScore
                          << " depth "     << rDepth - 1
                          << " seldepth "  << selDepth
                          << " nodes "     << movesExplored
                          << " nps "       << (int) (getMovesExplored() / (timeElapsed.count() / 1000))
                          << " time "      << (int)timeElapsed.count()
                          << " pv"         << pv << std::endl;
            }
        }
        // still give some info on a fail high or low
        else {
            if (prevBestScore >= 31000) {
                int distance = ((-prevBestScore + 32000) / 2) + (prevBestScore % 2);
                std::cout << "info "
                          << "score mate " << distance
                          << " depth "     << rDepth
                          << " seldepth "  << selDepth
                          << (upper ? " upperbound" : (lower ? " lowerbound" : ""))
                          << " nodes "     << movesExplored
                          << " nps "       << (int) (getMovesExplored() / (timeElapsed.count() / 1000))
                          << " time "      << (int)timeElapsed.count()
                          << " pv"         << pv << std::endl;
            }
            else if (prevBestScore <= -31000) {
                int distance = -((prevBestScore + 32000) / 2) + -(prevBestScore % 2);
                std::cout << "info "
                          << "score mate " << distance
                          << " depth "     << rDepth
                          << " seldepth "  << selDepth
                          << (upper ? " upperbound" : (lower ? " lowerbound" : ""))
                          << " nodes "     << movesExplored
                          << " nps "       << (int) (getMovesExplored() / (timeElapsed.count() / 1000))
                          << " time "      << (int)timeElapsed.count()
                          << " pv"         << pv << std::endl;
            }
            else {
                std::cout << "info "
                          << "score cp "   << prevBestScore
                          << " depth "     << rDepth
                          << " seldepth "  << selDepth
                          << (upper ? " upperbound" : (lower ? " lowerbound" : ""))
                          << " nodes "     << movesExplored
                          << " nps "       << (int) (getMovesExplored() / (timeElapsed.count() / 1000))
                          << " time "      << (int)timeElapsed.count()
                          << " pv"         << pv << std::endl;
            }
        }

        int maxHist = 0;
        // age the history table
        for (auto & i : moveHistory) {
            for (int j = 0; j < 64; j++) {
                for (int k = 0; k < 64; k++) {
                    maxHist = std::max(maxHist, i[j][k]);
                    i[j][k] /= rDepth > 10 ? rDepth : 10 * rDepth;
                }
            }
        }
        //std::cout << "info string Max value in History table: " << maxHist << std::endl;
        //std::cout << "info string Attempts at Singular Extensions: " << singularAttempts << std::endl;
        //std::cout << "info string Number of Singular Extensions: " << singularExtensions << std::endl;

        // add the current depth to the branching factor
        // if we searched to depth 1, then we need to do another search
        // to get a branching factor
        if (n1 == 0){
            n1 = movesExplored;
        }
            // we can't calculate a branching factor if we researched because of
            // a window miss, so we first check if the last entry to misses is our current depth
        else {
            branchingFactor = (double) depthNodes / n1;
            avgBranchingFactor = ((avgBranchingFactor * (rDepth - 2)) + branchingFactor) / (rDepth - 1);
            n1 = depthNodes;
            depthNodes = 0;
            //std::cout << "info string Branching factor: " << avgBranchingFactor << std::endl;
        }


        // for debugging
        if (boardFEN != board.fen()) {
            std::cout << "info string Board does not match original at depth: " << rDepth << std::endl;
            std::cout << "info string Bad fen: " << board.fen() << std::endl;
            board.from_fen(boardFEN);
        }

        /*
        std::cout << "Total Quiescence Moves Searched: " << quiesceExplored << std::endl;
        std::cout << "Moves transposed: " << movesTransposed << std::endl;
        std::cout << "Cut Nodes: " << cutNodes << std::endl;
         */

        // reset the variables to prepare for the next loop
        if (!finalDepth) {
            bestScore = -32001;
        }
    }

    setMovesExplored(0);
    cutNodes = 0;
    movesTransposed = 0;
    quiesceExplored = 0;
    stopped = false;

    // tell the GUI what move we want to make
    std::cout << "bestmove " << prevBestMove.to_str() << std::endl;

    board.make_move(prevBestMove);
    incPly();

    //std::cout << board.fen() << std::endl;
}