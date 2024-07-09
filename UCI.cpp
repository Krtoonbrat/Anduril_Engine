//
// Created by 80hugkev on 7/6/2022.
//

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <mutex>
#include <thread>

#include "Anduril.h"
#include "misc.h"
#include "libchess/Position.h"
#include "Thread.h"
#include "UCI.h"

int libchess::Position::pieceValuesMG[6] = {115, 446, 502, 649, 1332, 0};
int libchess::Position::pieceValuesEG[6] = {148, 490, 518, 878, 1749, 0};
int Anduril::pieceValues[16] = { 148,  490,  518,  878,  1749, 0, 0, 0,
                                 148,  490,  518,  878,  1749, 0, 0, 0};

extern int maxHistoryVal;
extern int maxContinuationVal;
extern int maxCaptureVal;

extern int sbc;
extern int sbm;
extern int msb;
extern int lsb;

extern int lbc;

extern int rvc;
extern int rvs;

extern int rfm;

extern int hpv;
extern int hrv;
extern int qte;

extern int sec;
extern int sem;

extern int fth;
extern int svq;
extern int pcc;
extern int pci;
extern int fpc;
extern int fpm;
extern int smq;
extern int smt;

int baseNullReduction = 4;
int reductionDepthDividend = 5;
int reductionEvalModifierMin = 3;
int reductionEvalModifierDividend = 50;
int verificationMultiplier = 3;
int verificationDividend = 4;
int minVerificationDepth = 12;

namespace NNUE {
    extern char nnue_path[256];
}

ThreadPool gondor;

// all the UCI protocol stuff is going to be implemented
// with C (C++ dispersed for when I know which C++ item to use over the C implementation) code because the tutorial
// I am following is written in C
namespace UCI {

    // FEN for the start position
    const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    void loop(int argc, char* argv[]) {
        // strings for parsing messages from the GUI
        std::string line, token;

        // set up the board, engine, book, and game state
        libchess::Position board(StartFEN);
        Book openingBook = Book(R"(..\book\Performance.bin)");
        openingBook.closeBook();
        bool bookOpen = openingBook.getBookOpen();
        gondor.set(board, 1);

        // load the nnue file
        NNUE::LoadNNUE();

        if (argc > 1) {
            std::string in = std::string(argv[1]);
            if (in == "bench") {
                // benchmark will just use the already created engine and board, run for depth 20, and report node count and speed.  Program exits when this is finished if the bench command was given as an argument
                gondor.mainThread()->engine->bench(board);
                return;
            }
        }

        while (true) {
            // grab the line
            // continue if we don't receive anything
            if (!std::getline(std::cin, line)) { continue; }

            // continue if all we get is a new line
            if (line == "\n") { continue; }

            // string stream for easy parsing
            std::stringstream stream(line);

            // clear the string and extract the first token
            token.clear();
            stream >> std::skipws >> token;

            // these are the commands we can get from the GUI
            if (token == "isready") {
                std::cout << "readyok" << std::endl;
            }
            else if (token == "position") {
                parsePosition(stream, board);
            }
            else if (token == "setoption") {
                parseOption(stream, board, bookOpen);
            }
            else if (token == "ucinewgame") {
                if (!openingBook.getBookOpen()) { openingBook.flipBookOpen(); }
                table.clear();
                gondor.clear();
                board = *libchess::Position::from_fen(StartFEN);
            }
            else if (token == "go") {
                parseGo(stream, board, openingBook, bookOpen);
            }
            else if (token == "perft") {
                int d;
                stream >> d;
                gondor.mainThread()->engine->perft(board, d);
            }
            else if (token == "stop") {
                gondor.stop = true;
            }
            else if (token == "quit") {
                gondor.stop = true;
                break;
            }
            else if (token == "uci") {
                std::cout << "id name Anduril" << std::endl;
                std::cout << "id author Krtoonbrat" << std::endl;

                std::cout << "option name ClearHash type button" << std::endl;
                std::cout << "option name Threads type spin default 1 min 1 max 64" << std::endl;
                std::cout << "option name Hash type spin default 256 min 16 max 33554432" << std::endl;
                std::cout << "option name OwnBook type check default false" << std::endl;

                std::cout << "option name nnue_path type string default " << NNUE::nnue_path << std::endl;

                std::cout << "option name neb type string default -0.5713" << std::endl;
                std::cout << "option name nem type string default 3.3186" << std::endl;

                std::cout << "option name sbc type string default 159" << std::endl;
                std::cout << "option name sbm type string default 386" << std::endl;
                std::cout << "option name msb type string default 6664" << std::endl;
                std::cout << "option name lsb type string default 199" << std::endl;
                std::cout << "option name lbc type string default 52" << std::endl;
                std::cout << "option name mhv type string default 8171" << std::endl;
                std::cout << "option name mcv type string default 29871" << std::endl;
                std::cout << "option name cpm type string default 7324" << std::endl;
                std::cout << "option name hpv type string default -14868" << std::endl;
                std::cout << "option name hrv type string default 22690" << std::endl;
                std::cout << "option name qte type string default 4475" << std::endl;

                std::cout << "option name rvc type string default 582" << std::endl;
                std::cout << "option name rvs type string default 162" << std::endl;
                std::cout << "option name rfm type string default 243" << std::endl;

                std::cout << "option name sec type string default 162" << std::endl;
                std::cout << "option name sem type string default 28" << std::endl;

                std::cout << "option name fth type string default 230" << std::endl;
                std::cout << "option name svq type string default -113" << std::endl;
                std::cout << "option name pcc type string default 269" << std::endl;
                std::cout << "option name pci type string default 152" << std::endl;
                std::cout << "option name fpc type string default 294" << std::endl;
                std::cout << "option name fpm type string default 70" << std::endl;
                std::cout << "option name smq type string default -52" << std::endl;
                std::cout << "option name smt type string default -21" << std::endl;

                std::cout << "option name baseNullReduction type string default 4" << std::endl;
                std::cout << "option name reductionDepthDividend type string default 5" << std::endl;
                std::cout << "option name reductionEvalModifierMin type string default 3" << std::endl;
                std::cout << "option name reductionEvalModifierDividend type string default 50" << std::endl;
                std::cout << "option name verificationMultiplier type string default 3" << std::endl;
                std::cout << "option name verificaitonDividend type string default 4" << std::endl;
                std::cout << "option name minVerificationDepth type string default 12" << std::endl;

                std::cout << "uciok" << std::endl;


            }
        }
    }

    void parseOption(std::stringstream &stream, libchess::Position &board, bool &bookOpen) {
        // format:
        // setoption name pMG value 100

        std::string token, value;

        // must send token "name" with a setoption command
        stream >> token;
        if (token != "name" && token != "Name") {
            return;
        }

        // actually grab the token we want this time
        stream >> token;

        // we check "ClearHash" here because it does not need a "value" token
        if (token == "ClearHash") {
            table.clear();
            std::cout << "info string Hash table cleared" << std::endl;
        }

        // now we check for the required "value" token
        stream >> value;
        if (value != "value" && value != "Value") {
            return;
        }

        // set thread count
        if (token == "Threads") {
            stream >> gondor.numThreads;
            gondor.set(board, gondor.numThreads);
        }

        else if (token == "Hash") {
            int hashSize;
            stream >> hashSize;
            if (hashSize == table.sizeMB) {
                std::cout << "info string Hash size already set to " << hashSize << " MB" << std::endl;
            }
            else {
                table.resize(hashSize >= 16 ? hashSize : 16);
            }
        }

        // set book open or closed
        else if (token == "OwnBook") {
            stream >> token;
            if (token == "true") {
                bookOpen = true;
            }
            else {
                bookOpen = false;
            }
        }

        // set nnue path
        else if (token == "nnue_path") {
            stream >> NNUE::nnue_path;
            char *end = strchr(NNUE::nnue_path, '\n');
            if (end) {
                *end = '\0';
            }
            NNUE::LoadNNUE();
        }

        else if (token == "neb") {
            stream >> neb;
            initReductions(nem, neb);
        }

        else if (token == "nem") {
            stream >> nem;
            initReductions(nem, neb);
        }

        else if (token == "sbc") {
            stream >> sbc;
        }

        else if (token == "sbm") {
            stream >> sbm;
        }

        else if (token == "msb") {
            stream >> msb;
        }

        else if (token == "lsb") {
            stream >> lsb;
        }

        else if (token == "lbc") {
            stream >> lbc;
        }

        else if (token == "cpm") {
            stream >> maxCaptureVal;
        }

        else if (token == "rvc") {
            stream >> rvc;
        }

        else if (token == "rvs") {
            stream >> rvs;
        }

        else if (token == "rfm") {
            stream >> rfm;
        }

        else if (token == "mhv") {
            stream >> maxHistoryVal;
        }

        else if (token == "mcv") {
            stream >> maxContinuationVal;
        }

        else if (token == "hpv") {
            stream >> hpv;
        }

        else if (token == "hrv") {
            stream >> hrv;
        }

        else if (token == "qte") {
            stream >> qte;
        }

        else if (token == "sec") {
            stream >> sec;
        }

        else if (token == "sem") {
            stream >> sem;
        }

        else if (token == "fth") {
            stream >> fth;
        }

        else if (token == "svq") {
            stream >> svq;
        }

        else if (token == "pcc") {
            stream >> pcc;
        }

        else if (token == "pci") {
            stream >> pci;
        }

        else if (token == "fpc") {
            stream >> fpc;
        }

        else if (token == "fpm") {
            stream >> fpm;
        }

        else if (token == "smq") {
            stream >> smq;
        }

        else if (token == "smt") {
            stream >> smt;
        }

        else if (token == "baseNullReduction") {
            stream >> baseNullReduction;
        }

        else if (token == "reductionDepthDividend") {
            stream >> reductionDepthDividend;
        }

        else if (token == "reductionEvalModifierMin") {
            stream >> reductionEvalModifierMin;
        }

        else if (token == "reductionEvalMoifierDividend") {
            stream >> reductionEvalModifierDividend;
        }

        else if (token == "verificationMultiplier") {
            stream >> verificationMultiplier;
        }

        else if (token == "verificationDividend") {
            stream >> verificationDividend;
        }

        else if (token == "minVerificationDepth") {
            stream >> minVerificationDepth;
        }

    }

    void parseGo(std::stringstream &stream, libchess::Position &board, Book &openingBook, bool &bookOpen) {
        // reset all the limit
        int depth = -1; int moveTime = -1; int mtg = 35;
        int time = -1;
        int increment = 0;
        gondor.mainThread()->engine->limits.timeSet = false;

        std::string token;

        // this makes sure that the opening book is set to the correct state
        if (!bookOpen) {
            openingBook.closeBook();
        }

        // consume the tokens
        while (stream >> token) {
            // commands
            if (token == "infinite") {
                openingBook.closeBook();
            }

            else if (token == "btime" && board.side_to_move()) {
                stream >> time;
            }

            else if (token ==  "wtime" && !board.side_to_move()) {
                stream >> time;
            }

            else if (token == "binc" && board.side_to_move()) {
                stream >> increment;
            }

            else if (token == "winc" && !board.side_to_move()) {
                stream >> increment;
            }

            else if (token == "movestogo") {
                stream >> mtg;
            }

            else if (token == "movetime") {
                stream >> moveTime;
            }

            else if (token == "depth") {
                stream >> depth;
            }
        }

        if (moveTime != -1) {
            time = moveTime;
            mtg = 1;
        }

        gondor.mainThread()->engine->startTime = std::chrono::steady_clock::now();
        gondor.mainThread()->engine->limits.depth = depth;

        if (time != -1) {
            gondor.mainThread()->engine->limits.timeSet = true;
            time /= mtg;
            time -= 50 * (moveTime == -1);
            std::chrono::milliseconds searchTime(time + increment);
            gondor.mainThread()->engine->stopTime = std::chrono::steady_clock::now();
            gondor.mainThread()->engine->stopTime += searchTime;
        }

        if (depth == -1) {
            // we won't ever hit a depth of 100, so it stands in as a "max" or "infinite" depth
            gondor.mainThread()->engine->limits.depth = 100;
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
                openingBook.flipBookOpen();
                table.newSearch();
                gondor.startSearch();
            }
        }
        else {
            table.newSearch();
            gondor.startSearch();
        }
    }

    void parsePosition(std::stringstream &stream, libchess::Position &board) {
        std::string token, fen;

        // grab the first part of the command
        stream >> token;

        // instructions for different commands we could receive
        if (token == "startpos") {
            board = *libchess::Position::from_fen(StartFEN);
            // consume the "moves" token
            stream >> token;
        }
        else if (token == "fen") {
            // grab the fen string
            while (stream >> token && token != "moves") {
                fen += token + " ";
            }
            board = *libchess::Position::from_fen(fen);
        }
        else {
            return;
        }

        // set up the variable for parsing the moves
        std::vector<std::string> moves;
        while (stream >> token) {
            moves.push_back(token);
        }

        // set up the board
        for (auto &i : moves) {
            libchess::Move move = *libchess::Move::from(i);
            board.make_move(move);
        }
    }

}  // namespace UCI

// calls negamax and keeps track of the best move
// this version will also interact with UCI
void Anduril::go(libchess::Position board) {
    //std::cout << board.fen() << std::endl;
    libchess::Move bestMove(0);

    if (id == 0) {
        movesExplored = 0;
        cutNodes = 0;
        movesTransposed = 0;
        quiesceExplored = 0;
        gondor.wakeThreads();
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
            sDepth = std::clamp(rDepth, 1, 100);
        }

        incomplete = false;

        sDepth = std::clamp(sDepth < rDepth - 3 ? rDepth - 3 : sDepth, 1, 100);

        // search for the best score
        bestScore = negamax<Root>(board, sDepth, alpha, beta, false);

        // if we didn't find a node before, try again now that we have searched
        if (!found) {
            node = table.probe(hash, found);
        }

        // was the search stopped?
        // stop the search if time is up
        if (gondor.stop || (limits.timeSet && stopTime - startTime <= std::chrono::steady_clock::now() - startTime)) {
            incomplete = true;
            finalDepth = true;
        }

        // this is the depth we just searched to, we save it here because sDepth might change, but we want to report the value before the change to the GUI
        completedDepth = sDepth;

        // set the aspiration window
        if (rDepth >= 8) {
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
                rDepth = std::clamp(rDepth, 1, 100);
                upper = lower = false;
                delta = std::min(18 + bestScore * bestScore / 10000, 100);
                alpha = std::max(bestScore - delta, -32001);
                beta = std::min(bestScore + delta, 32001);
            }
        }
        // for depths less than 5
        else {
            delta = std::min(18 + bestScore * bestScore / 10000, 100);
            rDepth++;
            rDepth = std::clamp(rDepth, 1, 100);
            sDepth = rDepth;
        }

        // expand search window in case we miss (will be reset anyway if we didn't)
        // we can do it here because if we did not miss, we set alpha and beta for the next search above,
        // if we did miss, delta was already modified before we searched, meaning the alpha and beta windows were expanded
        delta += delta * 3 / 4;

        if (!incomplete && found) {
            bestMove = board.from_table(node->bestMove);
            prevBestScore = bestScore;
            //std::cout << "Total low misses: " << aspMissesL << std::endl;
            //std::cout << "Total high misses: " << aspMissesH << std::endl;
        }

        if (id == 0) {
            // send info to the GUI
            end = std::chrono::steady_clock::now();
            timeElapsed = end - startTime;

            std::vector<libchess::Move> PV = getPV(board, rDepth, bestMove);
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
                              << "score cp " << (prevBestScore * 100 / 208) // this is the centipawn conversion stockfish used in the version the default network file was trained on
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
                              << "score cp " << (prevBestScore * 100 / 208) // this is the centipawn conversion stockfish used in the version the default network file was trained on
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
        gondor.stop = true;
        cutNodes = 0;
        movesTransposed = 0;
        quiesceExplored = 0;

        // stop the other threads
        gondor.waitForSearchFinish();

        // reset the node count for each thread
        for (auto &thread : gondor) {
            thread->engine->setMovesExplored(0);
        }

        // tell the GUI what move we want to make
        std::cout << "bestmove " << bestMove.to_str() << std::endl;
    }

    //std::cout << board.fen() << std::endl;
}