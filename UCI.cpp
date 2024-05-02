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

int maxHistoryVal = 20805;
int maxContinuationVal = 23293;
int maxCaptureVal = 10962;

extern int BlockedPawnMG[2];
extern int BlockedPawnEG[2];

extern int threatBySafePawn[2];
extern int threatByPawnPush[2];

extern int sbc;
extern int sbm;
extern int msb;

extern int lbc;

extern int rvc;
extern int rvs;

extern int rfm;

extern int hpv;
extern int hrv;
extern int qte;

extern int sec;
extern int sem;
extern int sed;

namespace NNUE {
    extern char nnue_path[256];
    extern char nnue_library_path[256];
}

ThreadPool gondor;

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
        Book openingBook = Book(R"(..\book\Performance.bin)");
        bool bookOpen = openingBook.getBookOpen();
        gondor.set(board, 1);

        // load the nnue file
        NNUE::LoadNNUE();

        // initialize the oversize state array
        for (int i = -7; i < 0; i++) {
            board.continuationHistory(i) = &AI->continuationHistory[0][0][15][0];
        }

        //TODO: refactor benching after thread change
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
                for (auto &i : positions) {
                    table.newSearch();
                    AI->go(i);
                }
                return;
            }
        }

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
                parseOption(line, board, bookOpen);
            }
            else if (!strncmp(line, "ucinewgame", 10)) {
                if (!openingBook.getBookOpen()) { openingBook.flipBookOpen(); }
                table.clear();
                gondor.clear();
                board = *board.from_fen(StartFEN);
            }
            else if (!strncmp(line, "go", 2)) {
                parseGo(line, board, openingBook, bookOpen);
            }
            else if (!strncmp(line, "stop", 4)) {
                gondor.stop = true;
            }
            else if (!strncmp(line, "quit", 4)) {
                gondor.stop = true;
                break;
            }
            else if (!strncmp(line, "uci", 3)) {
                std::cout << "id name Anduril" << std::endl;
                std::cout << "id author Krtoonbrat" << std::endl;

                std::cout << "option name ClearHash type button" << std::endl;
                std::cout << "option name Threads type spin default 1 min 1 max 64" << std::endl;
                std::cout << "option name Hash type spin default 256 min 16 max 33554432" << std::endl;
                std::cout << "option name OwnBook type check default true" << std::endl;

                std::cout << "option name nnue_path type string default ../egbdll/nets/nn-62ef826d1a6d.nnue" << std::endl;
                std::cout << "option name nnue_library_path type string default ../egbdll/nnueprobe.dll" << std::endl;
                /*
                std::cout << "option name kMG type spin default 446 min -40000 max 40000" << std::endl;
                std::cout << "option name kEG type spin default 490 min -40000 max 40000" << std::endl;
                std::cout << "option name tMG type spin default 150 min -10000 max 10000" << std::endl;
                std::cout << "option name tEG type spin default 150 min -10000 max 10000" << std::endl;
                std::cout << "option name bpM type spin default 10  min -1000  max 1000"  << std::endl;
                std::cout << "option name bpE type spin default 30  min -1000  max 1000"  << std::endl;
                std::cout << "option name spc type spin default 47  min -1000  max 1000"  << std::endl;

                std::cout << "option name bMG type spin default 502 min -40000 max 40000" << std::endl;
                std::cout << "option name bEG type spin default 518 min -40000 max 40000" << std::endl;

                std::cout << "option name rMG type spin default 649 min -40000 max 40000" << std::endl;
                std::cout << "option name rEG type spin default 878 min -40000 max 40000" << std::endl;

                std::cout << "option name QOV type spin default 50000 min 0 max 1000000" << std::endl;
                std::cout << "option name ROV type spin default 25000 min 0 max 1000000" << std::endl;
                std::cout << "option name MOV type spin default 15000 min 0 max 1000000" << std::endl;
                 */

                std::cout << "option name neb type string default -0.565281" << std::endl;
                std::cout << "option name nem type string default 1.87106" << std::endl;

                std::cout << "option name bm5 type string default 20" << std::endl;
                std::cout << "option name bm6 type string default 63" << std::endl;
                std::cout << "option name be5 type string default 13" << std::endl;
                std::cout << "option name be6 type string default 94" << std::endl;

                std::cout << "option name spm type string default 48" << std::endl;
                std::cout << "option name spe type string default 26" << std::endl;
                std::cout << "option name ppm type string default 13" << std::endl;
                std::cout << "option name ppe type string default 11" << std::endl;

                std::cout << "option name sbc type string default 163" << std::endl;
                std::cout << "option name sbm type string default 417" << std::endl;
                std::cout << "option name msb type string default 6413" << std::endl;
                std::cout << "option name lbc type string default 40" << std::endl;
                std::cout << "option name mhv type string default 20805" << std::endl;
                std::cout << "option name mcv type string default 23293" << std::endl;
                std::cout << "option name cpm type string default 10962" << std::endl;
                std::cout << "option name hpv type string default -5928" << std::endl;
                std::cout << "option name hrv type string default 26602" << std::endl;
                std::cout << "option name qte type string default 4500" << std::endl;

                std::cout << "option name rvc type string default 460" << std::endl;
                std::cout << "option name rvs type string default 148" << std::endl;
                std::cout << "option name rfm type string default 200" << std::endl;

                std::cout << "option name sec type string default 22" << std::endl;
                std::cout << "option name sem type string default 18" << std::endl;
                std::cout << "option name sed type string default 20" << std::endl;
/*
                std::cout << "option name pMG type string default 115" << std::endl;
                std::cout << "option name pEG type string default 148" << std::endl;
                std::cout << "option name kMG type string default 446" << std::endl;
                std::cout << "option name kEG type string default 490" << std::endl;
                std::cout << "option name bMG type string default 502" << std::endl;
                std::cout << "option name bEG type string default 518" << std::endl;
                std::cout << "option name rMG type string default 649" << std::endl;
                std::cout << "option name rEG type string default 878" << std::endl;
                std::cout << "option name qMG type string default 1332" << std::endl;
                std::cout << "option name qEG type string default 1749" << std::endl;

                /*
                std::cout << "option name svq type spin default -171 min -10000 max 10000" << std::endl;
                std::cout << "option name rvc type spin default 460 min -10000 max 10000" << std::endl;
                std::cout << "option name rvs type spin default 115 min -10000 max 10000" << std::endl;
                std::cout << "option name pcc type spin default 148 min -10000 max 10000" << std::endl;
                std::cout << "option name pci type spin default 68 min -10000 max 10000" << std::endl;
                std::cout << "option name fpc type spin default 148 min -10000 max 10000" << std::endl;
                std::cout << "option name fpm type spin default 78 min -10000 max 10000" << std::endl;
                std::cout << "option name hpv type spin default -5928 min -50000 max 50000" << std::endl;
                std::cout << "option name smq type spin default -112 min -10000 max 10000" << std::endl;
                std::cout << "option name smt type spin default -48 min -10000 max 10000" << std::endl;
                std::cout << "option name sec type spin default 22 min -10000 max 10000" << std::endl;
                std::cout << "option name sem type spin default 18 min -10000 max 10000" << std::endl;
                std::cout << "option name sed type spin default 20 min -10000 max 10000" << std::endl;
                */

                std::cout << "uciok" << std::endl;


            }
        }
    }

    void parseOption(char* line, libchess::Position &board, bool &bookOpen) {
        char *ptr = NULL;

        // set thread count
        if ((ptr = strstr(line, "Threads"))) {
            gondor.numThreads = atoi(ptr + 13);
            gondor.set(board, gondor.numThreads);
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

        // set nnue path
        if ((ptr = strstr(line, "nnue_path"))) {
            strcpy(NNUE::nnue_path, ptr + 16);
            char *end = strchr(NNUE::nnue_path, '\n');
            if (end) {
                *end = '\0';
            }
            NNUE::LoadNNUE();
        }
        if ((ptr = strstr(line, "nnue_library_path"))) {
            strcpy(NNUE::nnue_library_path, ptr + 24);
            char *end = strchr(NNUE::nnue_library_path, '\n');
            if (end) {
                *end = '\0';
            }
            NNUE::LoadNNUE();
        }

        // setoption name pMG value 100

        /*
        if ((ptr = strstr(line, "pMG"))) {
            libchess::Position::pieceValuesMG[0] = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "pEG"))) {
            libchess::Position::pieceValuesEG[0] = atoi(ptr + 10);
            Anduril::pieceValues[0] = libchess::Position::pieceValuesEG[0];
        }

        if ((ptr = strstr(line, "kMG"))) {
            libchess::Position::pieceValuesMG[1] = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "kEG"))) {
            libchess::Position::pieceValuesEG[1] = atoi(ptr + 10);
            Anduril::pieceValues[1] = libchess::Position::pieceValuesEG[1];
        }

        if ((ptr = strstr(line, "bMG"))) {
            libchess::Position::pieceValuesMG[2] = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "bEG"))) {
            libchess::Position::pieceValuesEG[2] = atoi(ptr + 10);
            Anduril::pieceValues[2] = libchess::Position::pieceValuesEG[2];
        }

        if ((ptr = strstr(line, "rMG"))) {
            libchess::Position::pieceValuesMG[3] = atoi(ptr + 10);

        }

        if ((ptr = strstr(line, "rEG"))) {
            libchess::Position::pieceValuesEG[3] = atoi(ptr + 10);
            Anduril::pieceValues[3] = libchess::Position::pieceValuesEG[3];
        }

        if ((ptr = strstr(line, "qMG"))) {
            libchess::Position::pieceValuesMG[4] = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "qEG"))) {
            libchess::Position::pieceValuesEG[4] = atoi(ptr + 10);
            Anduril::pieceValues[4] = libchess::Position::pieceValuesEG[4];
        }
         */

        if ((ptr = strstr(line, "neb"))) {
            neb = atof(ptr + 10);
            initReductions(nem, neb);
        }

        if ((ptr = strstr(line, "nem"))) {
            nem = atof(ptr + 10);
            initReductions(nem, neb);
        }

        if ((ptr = strstr(line, "bm5"))) {
            BlockedPawnMG[0] = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "bm6"))) {
            BlockedPawnMG[1] = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "be5"))) {
            BlockedPawnEG[0] = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "be6"))) {
            BlockedPawnEG[1] = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "sbc"))) {
            sbc = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "sbm"))) {
            sbm = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "msb"))) {
            msb = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "lbc"))) {
            lbc = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "cpm"))) {
            maxCaptureVal = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "rvc"))) {
            rvc = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "rvs"))) {
            rvs = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "rfm"))) {
            rfm = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "mhv"))) {
            maxHistoryVal = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "mcv"))) {
            maxContinuationVal = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "hpv"))) {
            hpv = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "hrv"))) {
            hrv = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "qte"))) {
            qte = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "spm"))) {
            threatBySafePawn[0] = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "spe"))) {
            threatBySafePawn[1] = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "ppm"))) {
            threatByPawnPush[0] = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "ppe"))) {
            threatByPawnPush[1] = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "sec"))) {
            sec = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "sem"))) {
            sem = atoi(ptr + 10);
        }

        if ((ptr = strstr(line, "sed"))) {
            sed = atoi(ptr + 10);
        }

        /*
        if ((ptr = strstr(line, "svq"))) {
            AI->svq = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->svq = AI->svq;
            }
        }

        if ((ptr = strstr(line, "rvc"))) {
            AI->rvc = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->rvc = AI->rvc;
            }
        }

        if ((ptr = strstr(line, "rvs"))) {
            AI->rvs = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->rvs = AI->rvs;
            }
        }

        if ((ptr = strstr(line, "pcc"))) {
            AI->pcc = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->pcc = AI->pcc;
            }
        }

        if ((ptr = strstr(line, "pci"))) {
            AI->pci = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->pci = AI->pci;
            }
        }

        if ((ptr = strstr(line, "fpc"))) {
            AI->fpc = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->fpc = AI->fpc;
            }
        }

        if ((ptr = strstr(line, "fpm"))) {
            AI->fpm = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->fpm = AI->fpm;
            }
        }

        if ((ptr = strstr(line, "hpv"))) {
            AI->hpv = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->hpv = AI->hpv;
            }
        }

        if ((ptr = strstr(line, "smq"))) {
            AI->smq = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->smq = AI->smq;
            }
        }

        if ((ptr = strstr(line, "smt"))) {
            AI->smt = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->smt = AI->smt;
            }
        }

        if ((ptr = strstr(line, "sec"))) {
            AI->sec = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->sec = AI->sec;
            }
        }

        if ((ptr = strstr(line, "sem"))) {
            AI->sem = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->sem = AI->sem;
            }
        }

        if ((ptr = strstr(line, "sed"))) {
            AI->sed = atoi(ptr + 10);
            for (auto &thread : gondor) {
                thread->sed = AI->sed;
            }
        }
         */
    }

    void parseGo(char* line, libchess::Position &board, Book &openingBook, bool &bookOpen) {
        // reset all the limit
        int depth = -1; int moveTime = -1; int mtg = 35;
        int time = -1;
        int increment = 0;
        gondor.mainThread()->engine->limits.timeSet = false;
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

    if (id == 0) {
        movesExplored = 0;
        cutNodes = 0;
        movesTransposed = 0;
        quiesceExplored = 0;
        //threads = 4;   // for profiling
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