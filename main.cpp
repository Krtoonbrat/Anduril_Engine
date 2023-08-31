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

int tuneEval(libchess::Position &board, const std::vector<libchess::TunableParameter> &parameters) {
    std::unique_ptr<Anduril> evaluator = std::make_unique<Anduril>(0);

    for (int i = -7; i < 0; i++) {
        board.continuationHistory(i) = &evaluator->continuationHistory[0][0][15][0];
    }
    for (auto &parameter : parameters) {
        if (parameter.name() == "BishopPairMiddlegame") {
            evaluator->bpM = parameter.value();
        }
        if (parameter.name() == "BishopPairEndgame") {
            evaluator->bpE = parameter.value();
        }
        if (parameter.name() == "Space") {
            evaluator->spc = parameter.value();
        }
    }
    return evaluator->quiescence<Anduril::PV>(board, -32000, 32000, 0);
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

    std::vector<libchess::TunableParameter> parameters;
    parameters.emplace_back("BishopPairMiddlegame", 3);
    parameters.emplace_back("BishopPairEndgame", 12);
    parameters.emplace_back("Space", 5);

    std::function<int(libchess::Position &, const std::vector<libchess::TunableParameter> &)> eval = tuneEval;
    std::function<libchess::Position(const std::string&)> parseFen = [&](const std::string& fen) { return libchess::Position(fen); };
    std::vector<libchess::NormalizedResult<libchess::Position>> epdResults = libchess::NormalizedResult<libchess::Position>::parse_epd(R"(C:\Users\kjhughes4\Desktop\cutechess-1.3.1-win64\TuningData\tune.epd)", parseFen);

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
