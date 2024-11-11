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

int libchess::Position::pieceValuesMG[6] = {118, 453, 487, 671, 1464, 0};
int libchess::Position::pieceValuesEG[6] = {146, 489, 516, 917, 1785, 0};
int Anduril::pieceValues[16] = { 146,  489,  516,  917,  1785, 0, 0, 0,
                                 146,  489,  516,  917,  1785, 0, 0, 0};

extern int maxHistoryVal;
extern int maxContinuationVal;
extern int maxCaptureVal;

extern int queenOrderVal;
extern int rookOrderVal;
extern int minorOrderVal;

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

int singleDepthDividend = 25;
int singleDepthMultiplier = 16;

int dta = 18;
int dtn = 300;
int dtd = 400;

namespace NNUE {
    extern char nnue_path[256];
}

ThreadPool gondor;

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

                std::cout << "option name nem type string default " << UCI::nem << std::endl;
                std::cout << "option name neb type string default " << UCI::neb << std::endl;
                std::cout << "option name tem type string default " << UCI::tem << std::endl;
                std::cout << "option name teb type string default " << UCI::teb << std::endl;

                std::cout << "option name sbc type string default " << sbc << std::endl;
                std::cout << "option name sbm type string default " << sbm << std::endl;
                std::cout << "option name msb type string default " << msb << std::endl;
                std::cout << "option name lsb type string default " << lsb << std::endl;

                std::cout << "option name lbc type string default " << lbc << std::endl;
                std::cout << "option name rvc type string default " << rvc << std::endl;
                std::cout << "option name rvs type string default " << rvs << std::endl;
                std::cout << "option name rfm type string default " << rfm << std::endl;
                std::cout << "option name hpv type string default " << hpv << std::endl;
                std::cout << "option name hrv type string default " << hrv << std::endl;
                std::cout << "option name qte type string default " << qte << std::endl;
                std::cout << "option name sec type string default " << sec << std::endl;
                std::cout << "option name sem type string default " << sem << std::endl;
                std::cout << "option name fth type string default " << fth << std::endl;
                std::cout << "option name svq type string default " << svq << std::endl;
                std::cout << "option name pcc type string default " << pcc << std::endl;
                std::cout << "option name pci type string default " << pci << std::endl;
                std::cout << "option name fpc type string default " << fpc << std::endl;
                std::cout << "option name fpm type string default " << fpm << std::endl;
                std::cout << "option name smq type string default " << smq << std::endl;
                std::cout << "option name smt type string default " << smt << std::endl;
                std::cout << "option name pMG type string default " << libchess::Position::pieceValuesMG[0] << std::endl;
                std::cout << "option name kMG type string default " << libchess::Position::pieceValuesMG[1] << std::endl;
                std::cout << "option name bMG type string default " << libchess::Position::pieceValuesMG[2] << std::endl;
                std::cout << "option name rMG type string default " << libchess::Position::pieceValuesMG[3] << std::endl;
                std::cout << "option name qMG type string default " << libchess::Position::pieceValuesMG[4] << std::endl;
                std::cout << "option name pEG type string default " << libchess::Position::pieceValuesEG[0] << std::endl;
                std::cout << "option name kEG type string default " << libchess::Position::pieceValuesEG[1] << std::endl;
                std::cout << "option name bEG type string default " << libchess::Position::pieceValuesEG[2] << std::endl;
                std::cout << "option name rEG type string default " << libchess::Position::pieceValuesEG[3] << std::endl;
                std::cout << "option name qEG type string default " << libchess::Position::pieceValuesEG[4] << std::endl;
                std::cout << "option name mhv type string default " << maxHistoryVal << std::endl;
                std::cout << "option name mcv type string default " << maxContinuationVal << std::endl;
                std::cout << "option name cpm type string default " << maxCaptureVal << std::endl;
                std::cout << "option name dta type string default " << dta << std::endl;
                std::cout << "option name dtn type string default " << dtn << std::endl;
                std::cout << "option name dtd type string default " << dtd << std::endl;
                std::cout << "option name singleDepthDividend type string default " << singleDepthDividend << std::endl;
                std::cout << "option name singleDepthMultiplier type string default " << singleDepthMultiplier << std::endl;
                std::cout << "option name queenOrderVal type string default " << queenOrderVal << std::endl;
                std::cout << "option name rookOrderVal type string default " << rookOrderVal << std::endl;
                std::cout << "option name minorOrderVal type string default " << minorOrderVal << std::endl;


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

        // set nem
        else if (token == "nem") {
            stream >> UCI::nem;
            initReductions(UCI::nem, UCI::neb, UCI::tem, UCI::teb);
        }

        // set neb
        else if (token == "neb") {
            stream >> UCI::neb;
            initReductions(UCI::nem, UCI::neb, UCI::tem, UCI::teb);
        }

        // set tem
        else if (token == "tem") {
            stream >> UCI::tem;
            initReductions(UCI::nem, UCI::neb, UCI::tem, UCI::teb);
        }

        // set teb
        else if (token == "teb") {
            stream >> UCI::teb;
            initReductions(UCI::nem, UCI::neb, UCI::tem, UCI::teb);
        }

        // set sbc
        else if (token == "sbc") {
            stream >> sbc;
        }

        // set sbm
        else if (token == "sbm") {
            stream >> sbm;
        }

        // set msb
        else if (token == "msb") {
            stream >> msb;
        }

        // set lsb
        else if (token == "lsb") {
            stream >> lsb;
        }

        // set lbc
        else if (token == "lbc") {
            stream >> lbc;
        }

        // set rvc
        else if (token == "rvc") {
            stream >> rvc;
        }

        // set rvs
        else if (token == "rvs") {
            stream >> rvs;
        }

        // set rfm
        else if (token == "rfm") {
            stream >> rfm;
        }

        // set hpv
        else if (token == "hpv") {
            stream >> hpv;
        }

        // set hrv
        else if (token == "hrv") {
            stream >> hrv;
        }

        // set qte
        else if (token == "qte") {
            stream >> qte;
        }

        // set sec
        else if (token == "sec") {
            stream >> sec;
        }

        // set sem
        else if (token == "sem") {
            stream >> sem;
        }

        // set fth
        else if (token == "fth") {
            stream >> fth;
        }

        // set svq
        else if (token == "svq") {
            stream >> svq;
        }

        // set pcc
        else if (token == "pcc") {
            stream >> pcc;
        }

        // set pci
        else if (token == "pci") {
            stream >> pci;
        }

        // set fpc
        else if (token == "fpc") {
            stream >> fpc;
        }

        // set fpm
        else if (token == "fpm") {
            stream >> fpm;
        }

        // set smq
        else if (token == "smq") {
            stream >> smq;
        }

        // set smt
        else if (token == "smt") {
            stream >> smt;
        }

        // set pMG
        else if (token == "pMG") {
            stream >> libchess::Position::pieceValuesMG[0];
        }

        // set kMG
        else if (token == "kMG") {
            stream >> libchess::Position::pieceValuesMG[1];
        }

        // set bMG
        else if (token == "bMG") {
            stream >> libchess::Position::pieceValuesMG[2];
        }

        // set rMG
        else if (token == "rMG") {
            stream >> libchess::Position::pieceValuesMG[3];
        }

        // set qMG
        else if (token == "qMG") {
            stream >> libchess::Position::pieceValuesMG[4];
        }

        // set pEG
        else if (token == "pEG") {
            int val;
            stream >> val;
            libchess::Position::pieceValuesEG[0] = val;
            Anduril::pieceValues[0] = val;
            Anduril::pieceValues[8] = val;
        }

        // set kEG
        else if (token == "kEG") {
            int val;
            stream >> val;
            libchess::Position::pieceValuesEG[1] = val;
            Anduril::pieceValues[1] = val;
            Anduril::pieceValues[9] = val;
        }

        // set bEG
        else if (token == "bEG") {
            int val;
            stream >> val;
            libchess::Position::pieceValuesEG[2] = val;
            Anduril::pieceValues[2] = val;
            Anduril::pieceValues[10] = val;
        }

        // set rEG
        else if (token == "rEG") {
            int val;
            stream >> val;
            libchess::Position::pieceValuesEG[3] = val;
            Anduril::pieceValues[3] = val;
            Anduril::pieceValues[11] = val;
        }

        // set qEG
        else if (token == "qEG") {
            int val;
            stream >> val;
            libchess::Position::pieceValuesEG[4] = val;
            Anduril::pieceValues[4] = val;
            Anduril::pieceValues[12] = val;
        }

        // set mhv
        else if (token == "mhv") {
            stream >> maxHistoryVal;
        }

        // set mcv
        else if (token == "mcv") {
            stream >> maxContinuationVal;
        }

        // set cpm
        else if (token == "cpm") {
            stream >> maxCaptureVal;
        }

        // set singleDepthDividend
        else if (token == "singleDepthDividend") {
            stream >> singleDepthDividend;
        }

        // set singleDepthMultiplier
        else if (token == "singleDepthMultiplier") {
            stream >> singleDepthMultiplier;
        }

        // set queenOrderVal
        else if (token == "queenOrderVal") {
            stream >> queenOrderVal;
        }

        // set rookOrderVal
        else if (token == "rookOrderVal") {
            stream >> rookOrderVal;
        }

        // set minorOrderVal
        else if (token == "minorOrderVal") {
            stream >> minorOrderVal;
        }

        // set dta
        else if (token == "dta") {
            stream >> dta;
        }

        // set dtn
        else if (token == "dtn") {
            stream >> dtn;
        }

        // set dtd
        else if (token == "dtd") {
            stream >> dtd;
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
    int delta = dta;

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
                delta = std::min(dta + bestScore * bestScore / 10000, 100);
                alpha = std::max(bestScore - delta, -32001);
                beta = std::min(bestScore + delta, 32001);
            }
        }
        // for depths less than 5
        else {
            delta = std::min(dta + bestScore * bestScore / 10000, 100);
            rDepth++;
            rDepth = std::clamp(rDepth, 1, 100);
            sDepth = rDepth;
        }

        // expand search window in case we miss (will be reset anyway if we didn't)
        // we can do it here because if we did not miss, we set alpha and beta for the next search above,
        // if we did miss, delta was already modified before we searched, meaning the alpha and beta windows were expanded
        // 3 / 4
        delta += delta * dtn / dtd;

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