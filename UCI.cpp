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

#include <cmath>
#include <iostream>
#include <string>
#include <mutex>
#include <thread>

#include "Anduril.h"
#include "libchess/Position.h"
#include "UCI.h"

int libchess::Position::pieceValuesMG[6] = {88, 337, 365, 477, 1025, 0};
int libchess::Position::pieceValuesEG[6] = {138, 281, 297, 512, 936, 0};

// all the UCI protocol stuff is going to be implemented
// with C (C++ dispersed for when I know which C++ item to use over the C implementation) code because the tutorial
// I am following is written in C
namespace UCI {
    // this is the largest input we expect from the GUI
    constexpr int INPUTBUFFER = 400 * 6;

    // FEN for the start position
    const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    void loop(int argc, char* argv[]) {
        // first we turn off the stdin and stdout buffers
        //setbuf(stdin, NULL);
        //setbuf(stdout, NULL);

        char line[INPUTBUFFER];

        // set up the board, engine, book, and game state
        libchess::Position board(StartFEN);
        std::unique_ptr<Anduril> AI = std::make_unique<Anduril>(0);
        AI->quit = false;
        AI->stopped = true;
        AI->searching = false;
        threads = 1;
        Book openingBook = Book(R"(..\book\Performance.bin)");
        bool bookOpen = true;

        // initialize the oversize state array
        for (int i = -7; i < 0; i++) {
            board.continuationHistory(i) = &AI->continuationHistory[0][0][15][0];
        }

        if (argc > 1) {
            std::string in = std::string(argv[1]);
            if (in == "bench") {
                std::vector<libchess::Position> positions;
                positions.reserve(Defaults.size());
                for (auto &i : Defaults) {
                    positions.emplace_back(i);
                }
                AI->limits.timeSet = false;
                AI->limits.depth = 12;
                table.age++;
                AI->stopped = false;
                AI->searching = true;
                for (auto &i : positions) {
                    AI->go(i);
                }
                return;
            }
        }

        // start the search thread and park it until we need it
        std::thread searchThread(&waitForSearch, std::ref(AI), std::ref(board));

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
                table.clear();
                for (auto &i : gondor) {
                    i->pTable = HashTable<PawnEntry, 8>();
                    i->evalTable = HashTable<SimpleNode, 8>();
                    i->resetHistories();
                }
                AI->pTable = HashTable<PawnEntry, 8>();
                AI->evalTable = HashTable<SimpleNode, 8>();
                AI->resetHistories();
                board = *board.from_fen(StartFEN);
            }
            else if (!strncmp(line, "go", 2)) {
                parseGo(line, board, AI, openingBook, bookOpen);
            }
            else if (!strncmp(line, "stop", 4)) {
                for (auto &i : gondor) {
                    i->stopped = true;
                }
                AI->stopped = true;
            }
            else if (!strncmp(line, "quit", 4)) {
                for (auto &i : gondor) {
                    i->stopped = true;
                    i->quit = true;
                }
                AI->stopped = !AI->stopped.load();
                AI->quit = true;
                AI->searching = true;
                AI->cv.notify_one();
                searchThread.join();
                break;
            }
            else if (!strncmp(line, "uci", 3)) {
                std::cout << "id name Anduril" << std::endl;
                std::cout << "id author Krtoonbrat" << std::endl;

                std::cout << "option name ClearHash type button" << std::endl;
                std::cout << "option name Threads type spin default 1 min 1 max 64" << std::endl;
                std::cout << "option name Hash type spin default 256 min 16 max 33554432" << std::endl;
                std::cout << "option name OwnBook type check default true" << std::endl;

                std::cout << "option name kMG type spin default 337 min -40000 max 40000" << std::endl;
                std::cout << "option name kEG type spin default 281 min -40000 max 40000" << std::endl;
                std::cout << "option name oMG type spin default 20  min -1000 max 1000" << std::endl;
                std::cout << "option name oEG type spin default 5   min -1000 max 1000" << std::endl;
                std::cout << "option name tMG type spin default 150 min -1000 max 1000" << std::endl;
                std::cout << "option name tEG type spin default 150 min -1000 max 1000" << std::endl;
                std::cout << "option name bpM type spin default 3   min -1000 max 1000" << std::endl;
                std::cout << "option name bpE type spin default 12  min -1000 max 1000" << std::endl;
                std::cout << "option name spc type spin default 5   min -1000 max 1000" << std::endl;

                std::cout << "option name pMG type spin default 88 min -40000 max 40000" << std::endl;
                std::cout << "option name pEG type spin default 138 min -40000 max 40000" << std::endl;

                std::cout << "option name bMG type spin default 365 min -40000 max 40000" << std::endl;
                std::cout << "option name bEG type spin default 297 min -40000 max 40000" << std::endl;

                std::cout << "option name rMG type spin default 477 min -40000 max 40000" << std::endl;
                std::cout << "option name rEG type spin default 512 min -40000 max 40000" << std::endl;

                std::cout << "option name qMG type spin default 1025 min -40000 max 40000" << std::endl;
                std::cout << "option name qEG type spin default 936 min -40000 max 40000" << std::endl;

                std::cout << "option name QOV type spin default 50000 min 0 max 1000000" << std::endl;
                std::cout << "option name ROV type spin default 25000 min 0 max 1000000" << std::endl;
                std::cout << "option name MOV type spin default 15000 min 0 max 1000000" << std::endl;

                std::cout << "uciok" << std::endl;


            }

            // break the loop if quit is received
            if (AI->quit.load()) { break; }
        }
    }

    void parseOption(char* line, std::unique_ptr<Anduril> &AI, bool &bookOpen) {
        char *ptr = NULL;

        // set thread count
        // this is probably risky cuz a thread could be executing on an object we are deleting, YOLO
        if ((ptr = strstr(line, "Threads"))) {
            threads = atoi(ptr + 13);
            while (threads != gondor.size() + 1) {
                if (threads > gondor.size() + 1) {
                    gondor.emplace_back(std::make_unique<Anduril>(gondor.size() + 1));
                }
                else {
                    gondor.pop_back();
                }
            }
        }

        // set hash size
        if ((ptr = strstr(line, "ClearHash"))) {
            table.clear();
            std::cout << "info string Hash table cleared" << std::endl;
        }
        else if ((ptr = strstr(line, "Hash"))) {
            int hashSize = atoi(ptr + 10);
            if (hashSize == table.sizeMB) {
                std::cout << "info string Hash size already set to " << hashSize << " MB" << std::endl;
            }
            else {
                table.resize(hashSize >= 16 ? hashSize : 16);
            }
        }

        // set book open or closed
        if ((ptr = strstr(line, "OwnBook"))) {
            if ((ptr = strstr(line, "true"))) {
                bookOpen = true;
            }
            else {
                bookOpen = false;
            }
        }

        // setoption name pMG value 100
        if ((ptr = strstr(line, "kMG"))) {
            AI->kMG = atoi(ptr + 10);
            libchess::Position::pieceValuesMG[1] = AI->kMG;
            for (auto &thread : gondor) {
                thread->kMG = AI->kMG;
            }
        }

        if ((ptr = strstr(line, "kEG"))) {
            AI->kEG = atoi(ptr + 10);
            libchess::Position::pieceValuesEG[1] = AI->kEG;
            for (auto &thread : gondor) {
                thread->kEG = AI->kEG;
            }
        }

        if ((ptr = strstr(line, "bMG"))) {
            AI->bMG = atoi(ptr + 10);
            libchess::Position::pieceValuesMG[2] = AI->bMG;
            for (auto &thread : gondor) {
                thread->bMG = AI->bMG;
            }
        }

        if ((ptr = strstr(line, "bEG"))) {
            AI->bEG = atoi(ptr + 10);
            libchess::Position::pieceValuesEG[2] = AI->bEG;
            for (auto &thread : gondor) {
                thread->bEG = AI->bEG;
            }
        }

        if ((ptr = strstr(line, "rMG"))) {
            AI->rMG = atoi(ptr + 10);
            libchess::Position::pieceValuesMG[3] = AI->rMG;
            for (auto &thread : gondor) {
                thread->rMG = AI->rMG;
            }
        }

        if ((ptr = strstr(line, "rEG"))) {
            AI->rEG = atoi(ptr + 10);
            libchess::Position::pieceValuesEG[3] = AI->rEG;
            for (auto &thread : gondor) {
                thread->rEG = AI->rEG;
            }
        }

        if ((ptr = strstr(line, "qMG"))) {
            AI->qMG = atoi(ptr + 10);
            libchess::Position::pieceValuesMG[4] = AI->qMG;
            for (auto &thread : gondor) {
                thread->qMG = AI->qMG;
            }
        }

        if ((ptr = strstr(line, "qEG"))) {
            AI->qEG = atoi(ptr + 10);
            libchess::Position::pieceValuesEG[4] = AI->qEG;
            for (auto &thread : gondor) {
                thread->qEG = AI->qEG;
            }
        }

        if ((ptr = strstr(line, "oMG"))) {
            AI->oMG = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->oMG = AI->oMG;
            }
        }

        if ((ptr = strstr(line, "oEG"))) {
            AI->oEG = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->oEG = AI->oEG;
            }
        }

        if ((ptr = strstr(line, "tMG"))) {
            AI->tMG = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->tMG = AI->tMG;
            }
        }

        if ((ptr = strstr(line, "tEG"))) {
            AI->tEG = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->tEG = AI->tEG;
            }
        }

        if ((ptr = strstr(line, "pMG"))) {
            AI->pMG = atoi(ptr + 10);
            libchess::Position::pieceValuesMG[0] = AI->pMG;
            for (auto &thread : gondor) {
                thread->pMG = AI->pMG;
            }
        }

        if ((ptr = strstr(line, "pEG"))) {
            AI->pEG = atoi(ptr + 10);
            libchess::Position::pieceValuesEG[0] = AI->pEG;
            for (auto &thread : gondor) {
                thread->pEG = AI->pEG;
            }
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

        if ((ptr = strstr(line, "bpM"))) {
            AI->bpM = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->bpM = AI->bpM;
            }
        }

        if ((ptr = strstr(line, "bpE"))) {
            AI->bpE = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->bpE = AI->bpE;
            }
        }

        if ((ptr = strstr(line, "spc"))) {
            AI->spc = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->spc = AI->spc;
            }
        }
    }

    void parseGo(char* line, libchess::Position &board, std::unique_ptr<Anduril> &AI, Book &openingBook, bool &bookOpen) {
        // reset all the limit
        int depth = -1; int moveTime = -1; int mtg = 35;
        int time = -1;
        int increment = 0;
        AI->limits.timeSet = false;
        char *ptr = NULL;

        // this makes sure that the opening book is set to the correct state
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

        AI->startTime = std::chrono::steady_clock::now();
        AI->limits.depth = depth;

        if (time != -1) {
            AI->limits.timeSet = true;
            time /= mtg;
            time -= 50;
            std::chrono::milliseconds searchTime(time + increment);
            AI->stopTime = std::chrono::steady_clock::now();
            AI->stopTime += searchTime;
        }

        if (depth == -1) {
            // we won't ever hit a depth of 100, so it stands in as a "max" or "infinite" depth
            AI->limits.depth = 100;
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
                for (auto &i : gondor) {
                    i->stopped = false;
                    i->searching = true;
                }
                table.age++;
                AI->stopped = false;
                AI->searching = true;
                AI->cv.notify_one();
                //AI.go(board);
            }
        }
        else {
            for (auto &i : gondor) {
                i->stopped = false;
                i->searching = true;
            }
            table.age++;
            AI->stopped = false;
            AI->searching = true;
            AI->cv.notify_one();
            //AI.go(board);
        }
    }

    void waitForSearch(std::unique_ptr<Anduril> &AI, libchess::Position &board) {
        while (true) {
            std::unique_lock<std::mutex> lock(AI->mutex);
            AI->stopped = true;
            AI->searching = false;
            AI->cv.wait(lock, [&AI] { return AI->searching.load(); });

            if (AI->quit.load()) {
                return;
            }

            lock.unlock();
            AI->go(board);

            // we have to check for a quit on both ends in case the quit command comes during a search
            // if that happens, stopped will be set to true, and we would wait at the top, but we actually want to exit
            if (AI->quit.load()) {
                return;
            }
        }
    }

    void parsePosition(char* line, libchess::Position &board, std::unique_ptr<Anduril> &AI) {
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
        AI->resetPly();

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
                AI->incPly();
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

}  // namespace UCI

// calls negamax and keeps track of the best move
// this version will also interact with UCI
void Anduril::go(libchess::Position board) {
    //std::cout << board.fen() << std::endl;
    libchess::Move bestMove(0);
    libchess::Move prevBestMove = bestMove;

    if (id == 0) {
        movesExplored = 0;
        cutNodes = 0;
        movesTransposed = 0;
        quiesceExplored = 0;
        //threads = 4;   // for profiling
        for (int i = 1; i < threads; i++) {
            // more profiling stuff
            /*
            gondor.emplace_back(std::make_unique<Anduril>(i));
            gondor[i - 1]->limits.depth = 20;
            gondor[i - 1]->stopped = false;
            gondor[i - 1]->searching = true;
            gondor[i - 1]->startTime = std::chrono::steady_clock::now();
            */
            std::thread(&Anduril::go, gondor[i - 1].get(), board).detach();
        }
    }

    // this is for debugging
    std::string boardFENs = board.fen();
    char *boardFEN = &boardFENs[0];

    int alpha = -32001;
    int beta = 32001;
    int bestScore = -32001;
    int prevBestScore = bestScore;
    int delta = 18;

    // set the killer vector to have the correct number of slots
    // the vector is padded a little at the end in case of the search being extended
    for (auto i : killers) {
        i[0] = libchess::Move(0);
        i[1] = libchess::Move(0);
    }

    ply = board.ply();
    rootPly = ply;

    // initialize the oversize state array
    for (int i = 7; i > 0; i--) {
        board.continuationHistory(ply - i) = &continuationHistory[0][0][15][0];
    }

    // these variables are for debugging
    int aspMissesL = 0, aspMissesH = 0;
    std::vector<int> misses;

    rDepth = 1;
    int sDepth = rDepth;
    int completedDepth = 0;
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

        if (rDepth == limits.depth){
            finalDepth = true;
        }

        // reset selDepth
        if (!incomplete) {
            selDepth = 0;
            sDepth = rDepth;
        }

        incomplete = false;

        sDepth = sDepth < rDepth - 3 ? rDepth - 3 : sDepth;

        // search for the best score
        bestScore = negamax<Root>(board, sDepth, alpha, beta, false);

        // if we didn't find a node before, try again now that we have searched
        if (!found) {
            node = table.probe(hash, found);
        }

        // was the search stopped?
        // stop the search if time is up
        if (stopped.load() || (limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime)) {
            incomplete = true;
            finalDepth = true;
        }

        // this is the depth we just searched to, we save it here because sDepth might change, but we want to report the value before the change to the GUI
        completedDepth = sDepth;

        // set the aspiration window
        if (rDepth >= 5) {
            // search was outside the window, need to redo the search
            // fail low
            if (bestScore <= alpha) {
                //std::cout << "Low miss at: " << rDepth << std::endl;
                if (!limits.timeSet && limits.depth != 100) { finalDepth = false; }
                misses.push_back(rDepth);
                aspMissesL++;
                beta = (alpha + beta) / 2;
                alpha = std::max(bestScore - delta, -32001);
                incomplete = true;
                upper = true;
            }
            // fail high
            else if (bestScore >= beta) {
                if (!limits.timeSet && limits.depth != 100) { finalDepth = false; }
                //std::cout << "High miss at: " << rDepth << std::endl;
                misses.push_back(rDepth);
                aspMissesH++;
                beta = std::min(bestScore + delta, 32001);
                sDepth--;
                incomplete = true;
                lower = true;
            }
            // the search didn't fall outside the window, we can move to the next depth
            else {
                rDepth++;
                upper = lower = false;
                delta = 18 + bestScore * bestScore / 10000;
                alpha = std::max(bestScore - delta, -32001);
                beta = std::min(bestScore + delta, 32001);
            }
        }
        // for depths less than 5
        else {
            delta = 18 + bestScore * bestScore / 10000;
            rDepth++;
            sDepth = rDepth;
        }

        // expand search window in case we miss (will be reset anyway if we didn't)
        // we can do it here because if we did not miss, we set alpha and beta for the next search above,
        // if we did miss, delta was already modified before we searched, meaning the alpha and beta windows were expanded
        delta += delta * 3 / 4;

        if (!incomplete && found) {
            prevBestMove = libchess::Move(node->bestMove);
            prevBestScore = bestScore;
            //std::cout << "Total low misses: " << aspMissesL << std::endl;
            //std::cout << "Total high misses: " << aspMissesH << std::endl;
        }

        if (id == 0) {

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
                              << " depth " << completedDepth
                              << " seldepth " << selDepth
                              << " nodes " << getMovesExplored()
                              << " nps " << (uint64_t) (getMovesExplored() / (timeElapsed.count() / 1000))
                              << " hashfull " << table.hashFull()
                              << " time " << (uint64_t) timeElapsed.count()
                              << " pv" << pv << std::endl;
                } else if (prevBestScore <= -31000) {
                    int distance = -((prevBestScore + 32000) / 2) + -(prevBestScore % 2);
                    std::cout << "info "
                              << "score mate " << distance
                              << " depth " << completedDepth
                              << " seldepth " << selDepth
                              << " nodes " << getMovesExplored()
                              << " nps " << (uint64_t) (getMovesExplored() / (timeElapsed.count() / 1000))
                              << " hashfull " << table.hashFull()
                              << " time " << (uint64_t) timeElapsed.count()
                              << " pv" << pv << std::endl;
                } else {
                    std::cout << "info "
                              << "score cp " << prevBestScore
                              << " depth " << completedDepth
                              << " seldepth " << selDepth
                              << " nodes " << getMovesExplored()
                              << " nps " << (uint64_t) (getMovesExplored() / (timeElapsed.count() / 1000))
                              << " hashfull " << table.hashFull()
                              << " time " << (uint64_t) timeElapsed.count()
                              << " pv" << pv << std::endl;
                }
            }
                // still give some info on a fail high or low
            else {
                if (prevBestScore >= 31000) {
                    int distance = ((-prevBestScore + 32000) / 2) + (prevBestScore % 2);
                    std::cout << "info "
                              << "score mate " << distance
                              << " depth " << completedDepth
                              << " seldepth " << selDepth
                              << (upper ? " upperbound" : (lower ? " lowerbound" : ""))
                              << " nodes " << getMovesExplored()
                              << " nps " << (uint64_t) (getMovesExplored() / (timeElapsed.count() / 1000))
                              << " hashfull " << table.hashFull()
                              << " time " << (uint64_t) timeElapsed.count()
                              << " pv" << pv << std::endl;
                } else if (prevBestScore <= -31000) {
                    int distance = -((prevBestScore + 32000) / 2) + -(prevBestScore % 2);
                    std::cout << "info "
                              << "score mate " << distance
                              << " depth " << completedDepth
                              << " seldepth " << selDepth
                              << (upper ? " upperbound" : (lower ? " lowerbound" : ""))
                              << " nodes " << getMovesExplored()
                              << " nps " << (uint64_t) (getMovesExplored() / (timeElapsed.count() / 1000))
                              << " hashfull " << table.hashFull()
                              << " time " << (uint64_t) timeElapsed.count()
                              << " pv" << pv << std::endl;
                } else {
                    std::cout << "info "
                              << "score cp " << prevBestScore
                              << " depth " << completedDepth
                              << " seldepth " << selDepth
                              << (upper ? " upperbound" : (lower ? " lowerbound" : ""))
                              << " nodes " << getMovesExplored()
                              << " nps " << (uint64_t) (getMovesExplored() / (timeElapsed.count() / 1000))
                              << " hashfull " << table.hashFull()
                              << " time " << (uint64_t) timeElapsed.count()
                              << " pv" << pv << std::endl;
                }
            }
            //std::cout << "info string Attempts at Singular Extensions: " << singularAttempts << std::endl;
            //std::cout << "info string Number of Singular Extensions: " << singularExtensions << std::endl;

            // calculate branching factor
            //std::cout << "info string Branching factor (the stockfish way):" << std::pow((double) getMovesExplored(), (1.0 / (rDepth - 1))) << std::endl;


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
        }

        // reset the variables to prepare for the next loop
        if (!finalDepth) {
            bestScore = -32001;
        }
    }

    if (id == 0) {
        // tell the GUI what move we want to make
        std::cout << "bestmove " << prevBestMove.to_str() << std::endl;

        setMovesExplored(0);
        cutNodes = 0;
        movesTransposed = 0;
        quiesceExplored = 0;

        // stop the other threads
        for (auto &i : gondor) {
            i->stopped = true;
            i->searching = false;
            i->setMovesExplored(0);
        }
    }

    //std::cout << board.fen() << std::endl;
}