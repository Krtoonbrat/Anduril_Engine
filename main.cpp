#include <bitset>
#include "libchess/Position.h"
#include "libchess/Tuner.h"
#include "UCI.h"

std::array<libchess::Bitboard, 64> libchess::lookups::SQUARES;

std::array<libchess::Bitboard, 64> libchess::lookups::NORTH;
std::array<libchess::Bitboard, 64> libchess::lookups::SOUTH;
std::array<libchess::Bitboard, 64> libchess::lookups::EAST;
std::array<libchess::Bitboard, 64> libchess::lookups::WEST;
std::array<libchess::Bitboard, 64> libchess::lookups::NORTHWEST;
std::array<libchess::Bitboard, 64> libchess::lookups::SOUTHWEST;
std::array<libchess::Bitboard, 64> libchess::lookups::NORTHEAST;
std::array<libchess::Bitboard, 64> libchess::lookups::SOUTHEAST;
std::array<std::array<libchess::Bitboard, 64>, 64> libchess::lookups::INTERVENING;

std::array<std::array<libchess::Bitboard, 64>, 2> libchess::lookups::PAWN_ATTACKS;
std::array<libchess::Bitboard, 64> libchess::lookups::KNIGHT_ATTACKS;
std::array<libchess::Bitboard, 64> libchess::lookups::KING_ATTACKS;
std::array<libchess::Bitboard, 64> libchess::lookups::BISHOP_ATTACKS;
std::array<libchess::Bitboard, 64> libchess::lookups::ROOK_ATTACKS;
std::array<libchess::Bitboard, 64> libchess::lookups::QUEEN_ATTACKS;

std::array<std::array<libchess::Bitboard, 64>, 64> libchess::lookups::FULL_RAY;

libchess::lookups::Magic libchess::lookups::rook_magics[64] = {Magic()};
libchess::lookups::Magic libchess::lookups::bishop_magics[64] = {Magic()};
libchess::Bitboard libchess::lookups::rook_table[0x19000] = {Bitboard()};
libchess::Bitboard libchess::lookups::bishop_table[0x1480] = {Bitboard()};

int threads;

std::vector<std::unique_ptr<Anduril>> gondor;

int tuneEval(libchess::Position &board, const std::vector<libchess::TunableParameter> &parameters) {
    std::unique_ptr<Anduril> *evaluator = &gondor[0];
    for (int i = 0; i < gondor.size() && !(*evaluator)->searching; i++) {
        if (!(*evaluator)->searching) {
            evaluator = &gondor[i];
            (*evaluator)->searching = true;
        }
    }

    for (int i = -7; i < 0; i++) {
        board.continuationHistory(i) = &(*evaluator)->continuationHistory[0][0][15][0];
    }

    for (auto &parameter : parameters) {
        if (parameter.name() == "BishopPairMiddlegame") {
            (*evaluator)->bpM = parameter.value();
        }
        if (parameter.name() == "BishopPairEndgame") {
            (*evaluator)->bpE = parameter.value();
        }
        if (parameter.name() == "Space") {
            (*evaluator)->spc = parameter.value();
        }
        if (parameter.name() == "OutpostMiddlegame") {
            (*evaluator)->oMG = parameter.value();
        }
        if (parameter.name() == "OutpostEndgame") {
            (*evaluator)->oEG = parameter.value();
        }
        if (parameter.name() == "TrappedKnightMiddlegame") {
            (*evaluator)->tMG = parameter.value();
        }
        if (parameter.name() == "TrappedKnightEndgame") {
            (*evaluator)->tEG = parameter.value();
        }
        if (parameter.name() == "PawnMiddlegame") {
            (*evaluator)->pMG = parameter.value();
            (*evaluator)->seeValues[0] = parameter.value();
            board.pieceValuesMG[0] = parameter.value();
        }
        if (parameter.name() == "PawnEndgame") {
            (*evaluator)->pEG = parameter.value();
            (*evaluator)->pieceValues[0] = parameter.value();
            (*evaluator)->pieceValues[8] = parameter.value();
            board.pieceValuesEG[0] = parameter.value();
        }
        if (parameter.name() == "KnightMiddlegame") {
            (*evaluator)->kMG = parameter.value();
            (*evaluator)->seeValues[1] = parameter.value();
            board.pieceValuesMG[1] = parameter.value();
        }
        if (parameter.name() == "KnightEndgame") {
            (*evaluator)->kEG = parameter.value();
            (*evaluator)->pieceValues[1] = parameter.value();
            (*evaluator)->pieceValues[9] = parameter.value();
            board.pieceValuesEG[1] = parameter.value();
        }
        if (parameter.name() == "BishopMiddlegame") {
            (*evaluator)->bMG = parameter.value();
            (*evaluator)->seeValues[2] = parameter.value();
            board.pieceValuesMG[2] = parameter.value();
        }
        if (parameter.name() == "BishopEndgame") {
            (*evaluator)->bEG = parameter.value();
            (*evaluator)->pieceValues[2] = parameter.value();
            (*evaluator)->pieceValues[10] = parameter.value();
            board.pieceValuesEG[2] = parameter.value();
        }
        if (parameter.name() == "RookMiddlegame") {
            (*evaluator)->rMG = parameter.value();
            (*evaluator)->seeValues[3] = parameter.value();
            board.pieceValuesMG[3] = parameter.value();
        }
        if (parameter.name() == "RookEndgame") {
            (*evaluator)->rEG = parameter.value();
            (*evaluator)->pieceValues[3] = parameter.value();
            (*evaluator)->pieceValues[11] = parameter.value();
            board.pieceValuesEG[3] = parameter.value();
        }
        if (parameter.name() == "QueenMiddlegame") {
            (*evaluator)->qMG = parameter.value();
            (*evaluator)->seeValues[4] = parameter.value();
            board.pieceValuesMG[4] = parameter.value();
        }
        if (parameter.name() == "QueenEndgame") {
            (*evaluator)->qEG = parameter.value();
            (*evaluator)->pieceValues[4] = parameter.value();
            (*evaluator)->pieceValues[12] = parameter.value();
            board.pieceValuesEG[4] = parameter.value();
        }
    }
    board.setPSQTBoth();

    int score = (*evaluator)->quiescence<Anduril::PV>(board, -32001, 32001);
    (*evaluator)->searching = false;

    return board.side_to_move() == libchess::constants::WHITE ? score : -score;
}

int main() {
    libchess::lookups::SQUARES = libchess::lookups::init::squares();

    libchess::lookups::NORTH = libchess::lookups::init::north();
    libchess::lookups::SOUTH = libchess::lookups::init::south();
    libchess::lookups::EAST = libchess::lookups::init::east();
    libchess::lookups::WEST = libchess::lookups::init::west();
    libchess::lookups::NORTHWEST = libchess::lookups::init::northwest();
    libchess::lookups::SOUTHWEST = libchess::lookups::init::southwest();
    libchess::lookups::NORTHEAST = libchess::lookups::init::northeast();
    libchess::lookups::SOUTHEAST = libchess::lookups::init::southeast();
    libchess::lookups::INTERVENING = libchess::lookups::init::intervening();

    libchess::lookups::PAWN_ATTACKS = libchess::lookups::init::pawn_attacks();
    libchess::lookups::KNIGHT_ATTACKS = libchess::lookups::init::knight_attacks();
    libchess::lookups::KING_ATTACKS = libchess::lookups::init::king_attacks();
    libchess::lookups::BISHOP_ATTACKS = libchess::lookups::init::bishop_attacks();
    libchess::lookups::ROOK_ATTACKS = libchess::lookups::init::rook_attacks();
    libchess::lookups::QUEEN_ATTACKS = libchess::lookups::init::queen_attacks();

    libchess::lookups::FULL_RAY = libchess::lookups::init::full_ray();

    libchess::lookups::init::init_magics(libchess::constants::ROOK, libchess::lookups::rook_table, libchess::lookups::rook_magics);
    libchess::lookups::init::init_magics(libchess::constants::BISHOP, libchess::lookups::bishop_table, libchess::lookups::bishop_magics);

    for (int i = 0; i < 100; i++) {
        gondor.emplace_back(std::make_unique<Anduril>(i));
    }

    std::vector<libchess::TunableParameter> parameters;
    parameters.emplace_back("BishopPairMiddlegame", 3);
    parameters.emplace_back("BishopPairEndgame", 12);
    parameters.emplace_back("Space", 5);
    parameters.emplace_back("OutpostMiddlegame", 20);
    parameters.emplace_back("OutpostEndgame", 5);
    parameters.emplace_back("TrappedKnightMiddlegame", 150);
    parameters.emplace_back("TrappedKnightEndgame", 150);
    parameters.emplace_back("PawnMiddlegame", 88);
    parameters.emplace_back("PawnEndgame", 138);
    parameters.emplace_back("KnightMiddlegame", 337);
    parameters.emplace_back("KnightEndgame", 281);
    parameters.emplace_back("BishopMiddlegame", 365);
    parameters.emplace_back("BishopEndgame", 297);
    parameters.emplace_back("RookMiddlegame", 477);
    parameters.emplace_back("RookEndgame", 512);
    parameters.emplace_back("QueenMiddlegame", 1025);
    parameters.emplace_back("QueenEndgame", 936);

    std::function<int(libchess::Position &, const std::vector<libchess::TunableParameter> &)> eval = tuneEval;
    std::function<libchess::Position(const std::string&)> parseFen = [&](const std::string& fen) { return libchess::Position(fen); };

    std::cout << "size of normalized results: " << sizeof(libchess::NormalizedResult<libchess::Position>) << std::endl;
    std::cout << "size of epd structure(ish): " << (sizeof(libchess::NormalizedResult<libchess::Position>) * 8117592) / 1024 / 1024 / 1024 << std::endl;

    std::cout << "Parsing EPD..." << std::endl;
    std::vector<libchess::NormalizedResult<libchess::Position>> epdResults = libchess::NormalizedResult<libchess::Position>::parse_epd(R"(tune.epd)", parseFen);
    std::cout << "Done parsing EPD." << std::endl;

    std::cout << "Starting tuner..." << std::endl;
    libchess::Tuner<libchess::Position> tuner(epdResults, parameters, eval);
    tuner.tune();

    //threads = 1;
    //table.resize(256);

    // for profiling
    /*
    threads = 4;
    gondor.clear();
    std::unique_ptr<Anduril> AI = std::make_unique<Anduril>(0);
    AI->limits.depth = 20;
    AI->stopped = false;
    AI->searching = true;
    libchess::Position board("r1bn1rk1/pp2ppbp/6p1/3P4/4P3/5N2/q2BBPPP/1R1Q1RK1 w - - 1 14");
    AI->startTime = std::chrono::steady_clock::now();
    AI->go(board);
    */
    
    //UCI::loop();

    return 0;
}
