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
#include "Pyrrhic/tbprobe.h"
#include "Thread.h"
#include "UCI.h"

int libchess::Position::pieceValuesMG[6] = {108, 445, 498, 644, 1423, 0};
int libchess::Position::pieceValuesEG[6] = {152, 503, 523, 875, 1768, 0};
int Anduril::pieceValues[16] = { 152,  503,  523,  875,  1768, 0, 0, 0,
                                 152,  503,  523,  875,  1768, 0, 0, 0};

extern int maxHistoryVal;
extern int maxContinuationVal;
extern int maxCaptureVal;

extern bool use_nnue;

extern int bishopPair[2];
extern int outpost[2];
extern int trappedKnight[2];
extern int fianchetto[2];
extern int spaceDivisor;
extern int BlockedPawnMG[2];
extern int BlockedPawnEG[2];
extern int Connected[7];
extern int passedBonusMG[7];
extern int passedBonusEG[7];
extern int rookPawnBonus[9];
extern int knightPawnBonus[9];
extern int trappedRook[2];
extern int doubledPawn[2];
extern int isolated[2];
extern int weakUnopposed[2];
extern int backwardPawn[2];
extern int weakLever[2];


namespace NNUE {
    extern char nnue_path[256];
}

char syzygy_path[256] = "<empty>";
int syzygyProbeDepth = 1;
bool syzygy50MoveRule = true;
int syzygyProbeLimit = 7;

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

        // initialize tablebase
        tb_free();
        tb_init(syzygy_path);

        // load the nnue file
        NNUE::LoadNNUE();

        if (argc > 1) {
            std::string in = std::string(argv[1]);
            // benchmark will just use the already created engine and board, run for depth 20, and report node count and speed.  Program exits when this is finished if the bench command was given as an argument
            if (in == "bench") {
                // set transposition table to min size for more accurate measurement
                table.resize(16);
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

                std::cout << "option name SyzygyPath type string default " << syzygy_path << std::endl;
                std::cout << "option name SyzygyProbeDepth type spin default 1 min 1 max 100" << std::endl;
                std::cout << "option name Syzygy50MoveRule type check default true" << std::endl;
                std::cout << "option name SyzygyProbeLimit type spin default 7 min 0 max 7" << std::endl;

                std::cout << "option name UseNNUE type check default false" << std::endl;

                std::cout << "option name bishopPair1 type string default " << bishopPair[0] << std::endl;
                std::cout << "option name bishopPair2 type string default " << bishopPair[1] << std::endl;

                std::cout << "option name outpost1 type string default " << outpost[0] << std::endl;
                std::cout << "option name outpost2 type string default " << outpost[1] << std::endl;

                std::cout << "option name trappedKnight1 type string default " << trappedKnight[0] << std::endl;
                std::cout << "option name trappedKnight2 type string default " << trappedKnight[1] << std::endl;

                std::cout << "option name fianchetto1 type string default " << fianchetto[0] << std::endl;
                std::cout << "option name fianchetto2 type string default " << fianchetto[1] << std::endl;

                std::cout << "option name spaceDivisor type string default " << spaceDivisor << std::endl;

                std::cout << "option name BlockedPawnMG1 type string default " << BlockedPawnMG[0] << std::endl;
                std::cout << "option name BlockedPawnMG2 type string default " << BlockedPawnMG[1] << std::endl;\
                std::cout << "option name BlockedPawnEG1 type string default " << BlockedPawnEG[0] << std::endl;
                std::cout << "option name BlockedPawnEG2 type string default " << BlockedPawnEG[1] << std::endl;

                std::cout << "option name Connected1 type string default " << Connected[0] << std::endl;
                std::cout << "option name Connected2 type string default " << Connected[1] << std::endl;
                std::cout << "option name Connected3 type string default " << Connected[2] << std::endl;
                std::cout << "option name Connected4 type string default " << Connected[3] << std::endl;
                std::cout << "option name Connected5 type string default " << Connected[4] << std::endl;
                std::cout << "option name Connected6 type string default " << Connected[5] << std::endl;
                std::cout << "option name Connected7 type string default " << Connected[6] << std::endl;

                std::cout << "option name passedBonusMG1 type string default " << passedBonusMG[0] << std::endl;
                std::cout << "option name passedBonusMG2 type string default " << passedBonusMG[1] << std::endl;
                std::cout << "option name passedBonusMG3 type string default " << passedBonusMG[2] << std::endl;
                std::cout << "option name passedBonusMG4 type string default " << passedBonusMG[3] << std::endl;
                std::cout << "option name passedBonusMG5 type string default " << passedBonusMG[4] << std::endl;
                std::cout << "option name passedBonusMG6 type string default " << passedBonusMG[5] << std::endl;
                std::cout << "option name passedBonusMG7 type string default " << passedBonusMG[6] << std::endl;
                std::cout << "option name passedBonusEG1 type string default " << passedBonusEG[0] << std::endl;
                std::cout << "option name passedBonusEG2 type string default " << passedBonusEG[1] << std::endl;
                std::cout << "option name passedBonusEG3 type string default " << passedBonusEG[2] << std::endl;
                std::cout << "option name passedBonusEG4 type string default " << passedBonusEG[3] << std::endl;
                std::cout << "option name passedBonusEG5 type string default " << passedBonusEG[4] << std::endl;
                std::cout << "option name passedBonusEG6 type string default " << passedBonusEG[5] << std::endl;
                std::cout << "option name passedBonusEG7 type string default " << passedBonusEG[6] << std::endl;

                std::cout << "option name rookPawnBonus1 type string default " << rookPawnBonus[0] << std::endl;
                std::cout << "option name rookPawnBonus2 type string default " << rookPawnBonus[1] << std::endl;
                std::cout << "option name rookPawnBonus3 type string default " << rookPawnBonus[2] << std::endl;
                std::cout << "option name rookPawnBonus4 type string default " << rookPawnBonus[3] << std::endl;
                std::cout << "option name rookPawnBonus5 type string default " << rookPawnBonus[4] << std::endl;
                std::cout << "option name rookPawnBonus6 type string default " << rookPawnBonus[5] << std::endl;
                std::cout << "option name rookPawnBonus7 type string default " << rookPawnBonus[6] << std::endl;
                std::cout << "option name rookPawnBonus8 type string default " << rookPawnBonus[7] << std::endl;
                std::cout << "option name rookPawnBonus9 type string default " << rookPawnBonus[8] << std::endl;

                std::cout << "option name knightPawnBonus1 type string default " << knightPawnBonus[0] << std::endl;
                std::cout << "option name knightPawnBonus2 type string default " << knightPawnBonus[1] << std::endl;
                std::cout << "option name knightPawnBonus3 type string default " << knightPawnBonus[2] << std::endl;
                std::cout << "option name knightPawnBonus4 type string default " << knightPawnBonus[3] << std::endl;
                std::cout << "option name knightPawnBonus5 type string default " << knightPawnBonus[4] << std::endl;
                std::cout << "option name knightPawnBonus6 type string default " << knightPawnBonus[5] << std::endl;
                std::cout << "option name knightPawnBonus7 type string default " << knightPawnBonus[6] << std::endl;
                std::cout << "option name knightPawnBonus8 type string default " << knightPawnBonus[7] << std::endl;
                std::cout << "option name knightPawnBonus9 type string default " << knightPawnBonus[8] << std::endl;

                std::cout << "option name trappedRook1 type string default " << trappedRook[0] << std::endl;
                std::cout << "option name trappedRook2 type string default " << trappedRook[1] << std::endl;

                std::cout << "option name doubledPawn1 type string default " << doubledPawn[0] << std::endl;
                std::cout << "option name doubledPawn2 type string default " << doubledPawn[1] << std::endl;

                std::cout << "option name isolated1 type string default " << isolated[0] << std::endl;
                std::cout << "option name isolated2 type string default " << isolated[1] << std::endl;

                std::cout << "option name weakUnopposed1 type string default " << weakUnopposed[0] << std::endl;
                std::cout << "option name weakUnopposed2 type string default " << weakUnopposed[1] << std::endl;

                std::cout << "option name backwardPawn1 type string default " << backwardPawn[0] << std::endl;
                std::cout << "option name backwardPawn2 type string default " << backwardPawn[1] << std::endl;

                std::cout << "option name weakLever1 type string default " << weakLever[0] << std::endl;
                std::cout << "option name weakLever2 type string default " << weakLever[1] << std::endl;

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

        // use NNUE or HCE
        else if (token == "UseNNUE") {
            stream >> token;
            if (token == "true") {
                use_nnue = true;
            }
            else {
                use_nnue = false;
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

        else if (token == "SyzygyPath") {
            stream >> syzygy_path;
            char *end = strchr(syzygy_path, '\n');
            if (end) {
                *end = '\0';
            }
            tb_free();
            tb_init(syzygy_path);

            // give them some info
            if (TB_LARGEST != 0) {
                std::cout << "info string up to " << TB_LARGEST << "-piece Syzygy tablebases loaded" << std::endl;
                std::cout << "info string loaded " << TB_NUM_WDL << " WDL; " << TB_NUM_DTZ << " DTZ; " << TB_NUM_DTM << " DTM" << std::endl;
            }

        }

        else if (token == "SyzygyProbeDepth") {
            stream >> syzygyProbeDepth;
        }

        else if (token == "Syzygy50MoveRule") {
            stream >> token;
            if (token == "true") {
                syzygy50MoveRule = true;
            }
            else {
                syzygy50MoveRule = false;
            }
        }

        else if (token == "SyzygyProbeLimit") {
            stream >> syzygyProbeLimit;
        }


        else if (token == "bishopPair1") {
            stream >> bishopPair[0];
        }
        else if (token == "bishopPair2") {
            stream >> bishopPair[1];
        }
        else if (token == "outpost1") {
            stream >> outpost[0];
        }
        else if (token == "outpost2") {
            stream >> outpost[1];
        }
        else if (token == "trappedKnight1") {
            stream >> trappedKnight[0];
        }
        else if (token == "trappedKnight2") {
            stream >> trappedKnight[1];
        }
        else if (token == "fianchetto1") {
            stream >> fianchetto[0];
        }
        else if (token == "fianchetto2") {
            stream >> fianchetto[1];
        }
        else if (token == "spaceDivisor") {
            stream >> spaceDivisor;
        }
        else if (token == "BlockedPawnMG1") {
            stream >> BlockedPawnMG[0];
        }
        else if (token == "BlockedPawnMG2") {
            stream >> BlockedPawnMG[1];
        }
        else if (token == "BlockedPawnEG1") {
            stream >> BlockedPawnEG[0];
        }
        else if (token == "BlockedPawnEG2") {
            stream >> BlockedPawnEG[1];
        }
        else if (token == "Connected1") {
            stream >> Connected[0];
        }
        else if (token == "Connected2") {
            stream >> Connected[1];
        }
        else if (token == "Connected3") {
            stream >> Connected[2];
        }
        else if (token == "Connected4") {
            stream >> Connected[3];
        }
        else if (token == "Connected5") {
            stream >> Connected[4];
        }
        else if (token == "Connected6") {
            stream >> Connected[5];
        }
        else if (token == "Connected7") {
            stream >> Connected[6];
        }
        else if (token == "passedBonusMG1") {
            stream >> passedBonusMG[0];
        }
        else if (token == "passedBonusMG2") {
            stream >> passedBonusMG[1];
        }
        else if (token == "passedBonusMG3") {
            stream >> passedBonusMG[2];
        }
        else if (token == "passedBonusMG4") {
            stream >> passedBonusMG[3];
        }
        else if (token == "passedBonusMG5") {
            stream >> passedBonusMG[4];
        }
        else if (token == "passedBonusMG6") {
            stream >> passedBonusMG[5];
        }
        else if (token == "passedBonusMG7") {
            stream >> passedBonusMG[6];
        }
        else if (token == "passedBonusEG1") {
            stream >> passedBonusEG[0];
        }
        else if (token == "passedBonusEG2") {
            stream >> passedBonusEG[1];
        }
        else if (token == "passedBonusEG3") {
            stream >> passedBonusEG[2];
        }
        else if (token == "passedBonusEG4") {
            stream >> passedBonusEG[3];
        }
        else if (token == "passedBonusEG5") {
            stream >> passedBonusEG[4];
        }
        else if (token == "passedBonusEG6") {
            stream >> passedBonusEG[5];
        }
        else if (token == "passedBonusEG7") {
            stream >> passedBonusEG[6];
        }
        else if (token == "rookPawnBonus1") {
            stream >> rookPawnBonus[0];
        }
        else if (token == "rookPawnBonus2") {
            stream >> rookPawnBonus[1];
        }
        else if (token == "rookPawnBonus3") {
            stream >> rookPawnBonus[2];
        }
        else if (token == "rookPawnBonus4") {
            stream >> rookPawnBonus[3];
        }
        else if (token == "rookPawnBonus5") {
            stream >> rookPawnBonus[4];
        }
        else if (token == "rookPawnBonus6") {
            stream >> rookPawnBonus[5];
        }
        else if (token == "rookPawnBonus7") {
            stream >> rookPawnBonus[6];
        }
        else if (token == "rookPawnBonus8") {
            stream >> rookPawnBonus[7];
        }
        else if (token == "rookPawnBonus9") {
            stream >> rookPawnBonus[8];
        }
        else if (token == "knightPawnBonus1") {
            stream >> knightPawnBonus[0];
        }
        else if (token == "knightPawnBonus2") {
            stream >> knightPawnBonus[1];
        }
        else if (token == "knightPawnBonus3") {
            stream >> knightPawnBonus[2];
        }
        else if (token == "knightPawnBonus4") {
            stream >> knightPawnBonus[3];
        }
        else if (token == "knightPawnBonus5") {
            stream >> knightPawnBonus[4];
        }
        else if (token == "knightPawnBonus6") {
            stream >> knightPawnBonus[5];
        }
        else if (token == "knightPawnBonus7") {
            stream >> knightPawnBonus[6];
        }
        else if (token == "knightPawnBonus8") {
            stream >> knightPawnBonus[7];
        }
        else if (token == "knightPawnBonus9") {
            stream >> knightPawnBonus[8];
        }
        else if (token == "trappedRook1") {
            stream >> trappedRook[0];
        }
        else if (token == "trappedRook2") {
            stream >> trappedRook[1];
        }
        else if (token == "doubledPawn1") {
            stream >> doubledPawn[0];
        }
        else if (token == "doubledPawn2") {
            stream >> doubledPawn[1];
        }
        else if (token == "isolated1") {
            stream >> isolated[0];
        }
        else if (token == "isolated2") {
            stream >> isolated[1];
        }
        else if (token == "weakUnopposed1") {
            stream >> weakUnopposed[0];
        }
        else if (token == "weakUnopposed2") {
            stream >> weakUnopposed[1];
        }
        else if (token == "backwardPawn1") {
            stream >> backwardPawn[0];
        }
        else if (token == "backwardPawn2") {
            stream >> backwardPawn[1];
        }
        else if (token == "weakLever1") {
            stream >> weakLever[0];
        }
        else if (token == "weakLever2") {
            stream >> weakLever[1];
        }
    }

    void parseGo(std::stringstream &stream, libchess::Position &board, Book &openingBook, bool &bookOpen) {
        // reset all the limit
        int depth = -1; int moveTime = -1; int mtg = 35;
        int time = -1;
        int increment = 0;
        int nodes = -1;
        gondor.mainThread()->engine->limits.timeSet = false;

        std::string token;

        // start the clock as early as possible to help avoid time loss on with extremely low clock
        gondor.mainThread()->engine->startTime = std::chrono::steady_clock::now();

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

            else if (token == "nodes") {
                stream >> nodes;
            }
        }

        if (moveTime != -1) {
            time = moveTime;
            mtg = 1;
        }

        gondor.mainThread()->engine->limits.depth = depth;

        // nodes will really only work if we are searching with one thread
        // the engine will most likely search a few more nodes than this if we are using multiple threads
        gondor.mainThread()->engine->limits.nodes = nodes;

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
    int delta = 14;

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
        if (rDepth >= 6) {
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
                delta = 14;
                alpha = std::max(bestScore - delta, -32001);
                beta = std::min(bestScore + delta, 32001);
            }
        }
        // for depths less than 5
        else {
            delta = 14;
            rDepth++;
            rDepth = std::clamp(rDepth, 1, 100);
            sDepth = rDepth;
        }

        // expand search window in case we miss (will be reset anyway if we didn't)
        // we can do it here because if we did not miss, we set alpha and beta for the next search above,
        // if we did miss, delta was already modified before we searched, meaning the alpha and beta windows were expanded
        // 3 / 4
        delta += delta * 29 / 40;

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
                              << " tbhits " << getTbHits()
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
                              << " tbhits " << getTbHits()
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
                              << " tbhits " << getTbHits()
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
                              << " tbhits " << getTbHits()
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
                              << " tbhits " << getTbHits()
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
                              << " tbhits " << getTbHits()
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
            thread->engine->setTbHits(0);
        }

        // tell the GUI what move we want to make
        std::cout << "bestmove " << bestMove.to_str() << std::endl;
    }

    //std::cout << board.fen() << std::endl;
}