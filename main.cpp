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
        if (parameter.name() == "SafetyTable0") {
            (*evaluator)->SafetyTable[0] = parameter.value();
        }
        if (parameter.name() == "SafetyTable1") {
            (*evaluator)->SafetyTable[1] = parameter.value();
        }
        if (parameter.name() == "SafetyTable2") {
            (*evaluator)->SafetyTable[2] = parameter.value();
        }
        if (parameter.name() == "SafetyTable3") {
            (*evaluator)->SafetyTable[3] = parameter.value();
        }
        if (parameter.name() == "SafetyTable4") {
            (*evaluator)->SafetyTable[4] = parameter.value();
        }
        if (parameter.name() == "SafetyTable5") {
            (*evaluator)->SafetyTable[5] = parameter.value();
        }
        if (parameter.name() == "SafetyTable6") {
            (*evaluator)->SafetyTable[6] = parameter.value();
        }
        if (parameter.name() == "SafetyTable7") {
            (*evaluator)->SafetyTable[7] = parameter.value();
        }
        if (parameter.name() == "SafetyTable8") {
            (*evaluator)->SafetyTable[8] = parameter.value();
        }
        if (parameter.name() == "SafetyTable9") {
            (*evaluator)->SafetyTable[9] = parameter.value();
        }
        if (parameter.name() == "SafetyTable10") {
            (*evaluator)->SafetyTable[10] = parameter.value();
        }
        if (parameter.name() == "SafetyTable11") {
            (*evaluator)->SafetyTable[11] = parameter.value();
        }
        if (parameter.name() == "SafetyTable12") {
            (*evaluator)->SafetyTable[12] = parameter.value();
        }
        if (parameter.name() == "SafetyTable13") {
            (*evaluator)->SafetyTable[13] = parameter.value();
        }
        if (parameter.name() == "SafetyTable14") {
            (*evaluator)->SafetyTable[14] = parameter.value();
        }
        if (parameter.name() == "SafetyTable15") {
            (*evaluator)->SafetyTable[15] = parameter.value();
        }
        if (parameter.name() == "SafetyTable16") {
            (*evaluator)->SafetyTable[16] = parameter.value();
        }
        if (parameter.name() == "SafetyTable17") {
            (*evaluator)->SafetyTable[17] = parameter.value();
        }
        if (parameter.name() == "SafetyTable18") {
            (*evaluator)->SafetyTable[18] = parameter.value();
        }
        if (parameter.name() == "SafetyTable19") {
            (*evaluator)->SafetyTable[19] = parameter.value();
        }
        if (parameter.name() == "SafetyTable20") {
            (*evaluator)->SafetyTable[20] = parameter.value();
        }
        if (parameter.name() == "SafetyTable21") {
            (*evaluator)->SafetyTable[21] = parameter.value();
        }
        if (parameter.name() == "SafetyTable22") {
            (*evaluator)->SafetyTable[22] = parameter.value();
        }
        if (parameter.name() == "SafetyTable23") {
            (*evaluator)->SafetyTable[23] = parameter.value();
        }
        if (parameter.name() == "SafetyTable24") {
            (*evaluator)->SafetyTable[24] = parameter.value();
        }
        if (parameter.name() == "SafetyTable25") {
            (*evaluator)->SafetyTable[25] = parameter.value();
        }
        if (parameter.name() == "SafetyTable26") {
            (*evaluator)->SafetyTable[26] = parameter.value();
        }
        if (parameter.name() == "SafetyTable27") {
            (*evaluator)->SafetyTable[27] = parameter.value();
        }
        if (parameter.name() == "SafetyTable28") {
            (*evaluator)->SafetyTable[28] = parameter.value();
        }
        if (parameter.name() == "SafetyTable29") {
            (*evaluator)->SafetyTable[29] = parameter.value();
        }
        if (parameter.name() == "SafetyTable30") {
            (*evaluator)->SafetyTable[30] = parameter.value();
        }
        if (parameter.name() == "SafetyTable31") {
            (*evaluator)->SafetyTable[31] = parameter.value();
        }
        if (parameter.name() == "SafetyTable32") {
            (*evaluator)->SafetyTable[32] = parameter.value();
        }
        if (parameter.name() == "SafetyTable33") {
            (*evaluator)->SafetyTable[33] = parameter.value();
        }
        if (parameter.name() == "SafetyTable34") {
            (*evaluator)->SafetyTable[34] = parameter.value();
        }
        if (parameter.name() == "SafetyTable35") {
            (*evaluator)->SafetyTable[35] = parameter.value();
        }
        if (parameter.name() == "SafetyTable36") {
            (*evaluator)->SafetyTable[36] = parameter.value();
        }
        if (parameter.name() == "SafetyTable37") {
            (*evaluator)->SafetyTable[37] = parameter.value();
        }
        if (parameter.name() == "SafetyTable38") {
            (*evaluator)->SafetyTable[38] = parameter.value();
        }
        if (parameter.name() == "SafetyTable39") {
            (*evaluator)->SafetyTable[39] = parameter.value();
        }
        if (parameter.name() == "SafetyTable40") {
            (*evaluator)->SafetyTable[40] = parameter.value();
        }
        if (parameter.name() == "SafetyTable41") {
            (*evaluator)->SafetyTable[41] = parameter.value();
        }
        if (parameter.name() == "SafetyTable42") {
            (*evaluator)->SafetyTable[42] = parameter.value();
        }
        if (parameter.name() == "SafetyTable43") {
            (*evaluator)->SafetyTable[43] = parameter.value();
        }
        if (parameter.name() == "SafetyTable44") {
            (*evaluator)->SafetyTable[44] = parameter.value();
        }
        if (parameter.name() == "SafetyTable45") {
            (*evaluator)->SafetyTable[45] = parameter.value();
        }
        if (parameter.name() == "SafetyTable46") {
            (*evaluator)->SafetyTable[46] = parameter.value();
        }
        if (parameter.name() == "SafetyTable47") {
            (*evaluator)->SafetyTable[47] = parameter.value();
        }
        if (parameter.name() == "SafetyTable48") {
            (*evaluator)->SafetyTable[48] = parameter.value();
        }
        if (parameter.name() == "SafetyTable49") {
            (*evaluator)->SafetyTable[49] = parameter.value();
        }
        if (parameter.name() == "SafetyTable50") {
            (*evaluator)->SafetyTable[50] = parameter.value();
        }
        if (parameter.name() == "SafetyTable51") {
            (*evaluator)->SafetyTable[51] = parameter.value();
        }
        if (parameter.name() == "SafetyTable52") {
            (*evaluator)->SafetyTable[52] = parameter.value();
        }
        if (parameter.name() == "SafetyTable53") {
            (*evaluator)->SafetyTable[53] = parameter.value();
        }
        if (parameter.name() == "SafetyTable54") {
            (*evaluator)->SafetyTable[54] = parameter.value();
        }
        if (parameter.name() == "SafetyTable55") {
            (*evaluator)->SafetyTable[55] = parameter.value();
        }
        if (parameter.name() == "SafetyTable56") {
            (*evaluator)->SafetyTable[56] = parameter.value();
        }
        if (parameter.name() == "SafetyTable57") {
            (*evaluator)->SafetyTable[57] = parameter.value();
        }
        if (parameter.name() == "SafetyTable58") {
            (*evaluator)->SafetyTable[58] = parameter.value();
        }
        if (parameter.name() == "SafetyTable59") {
            (*evaluator)->SafetyTable[59] = parameter.value();
        }
        if (parameter.name() == "SafetyTable60") {
            (*evaluator)->SafetyTable[60] = parameter.value();
        }
        if (parameter.name() == "SafetyTable61") {
            (*evaluator)->SafetyTable[61] = parameter.value();
        }
        if (parameter.name() == "SafetyTable62") {
            (*evaluator)->SafetyTable[62] = parameter.value();
        }
        if (parameter.name() == "SafetyTable63") {
            (*evaluator)->SafetyTable[63] = parameter.value();
        }
        if (parameter.name() == "SafetyTable64") {
            (*evaluator)->SafetyTable[64] = parameter.value();
        }
        if (parameter.name() == "SafetyTable65") {
            (*evaluator)->SafetyTable[65] = parameter.value();
        }
        if (parameter.name() == "SafetyTable66") {
            (*evaluator)->SafetyTable[66] = parameter.value();
        }
        if (parameter.name() == "SafetyTable67") {
            (*evaluator)->SafetyTable[67] = parameter.value();
        }
        if (parameter.name() == "SafetyTable68") {
            (*evaluator)->SafetyTable[68] = parameter.value();
        }
        if (parameter.name() == "SafetyTable69") {
            (*evaluator)->SafetyTable[69] = parameter.value();
        }
        if (parameter.name() == "SafetyTable70") {
            (*evaluator)->SafetyTable[70] = parameter.value();
        }
        if (parameter.name() == "SafetyTable71") {
            (*evaluator)->SafetyTable[71] = parameter.value();
        }
        if (parameter.name() == "SafetyTable72") {
            (*evaluator)->SafetyTable[72] = parameter.value();
        }
        if (parameter.name() == "SafetyTable73") {
            (*evaluator)->SafetyTable[73] = parameter.value();
        }
        if (parameter.name() == "SafetyTable74") {
            (*evaluator)->SafetyTable[74] = parameter.value();
        }
        if (parameter.name() == "SafetyTable75") {
            (*evaluator)->SafetyTable[75] = parameter.value();
        }
        if (parameter.name() == "SafetyTable76") {
            (*evaluator)->SafetyTable[76] = parameter.value();
        }
        if (parameter.name() == "SafetyTable77") {
            (*evaluator)->SafetyTable[77] = parameter.value();
        }
        if (parameter.name() == "SafetyTable78") {
            (*evaluator)->SafetyTable[78] = parameter.value();
        }
        if (parameter.name() == "SafetyTable79") {
            (*evaluator)->SafetyTable[79] = parameter.value();
        }
        if (parameter.name() == "SafetyTable80") {
            (*evaluator)->SafetyTable[80] = parameter.value();
        }
        if (parameter.name() == "SafetyTable81") {
            (*evaluator)->SafetyTable[81] = parameter.value();
        }
        if (parameter.name() == "SafetyTable82") {
            (*evaluator)->SafetyTable[82] = parameter.value();
        }
        if (parameter.name() == "SafetyTable83") {
            (*evaluator)->SafetyTable[83] = parameter.value();
        }
        if (parameter.name() == "SafetyTable84") {
            (*evaluator)->SafetyTable[84] = parameter.value();
        }
        if (parameter.name() == "SafetyTable85") {
            (*evaluator)->SafetyTable[85] = parameter.value();
        }
        if (parameter.name() == "SafetyTable86") {
            (*evaluator)->SafetyTable[86] = parameter.value();
        }
        if (parameter.name() == "SafetyTable87") {
            (*evaluator)->SafetyTable[87] = parameter.value();
        }
        if (parameter.name() == "SafetyTable88") {
            (*evaluator)->SafetyTable[88] = parameter.value();
        }
        if (parameter.name() == "SafetyTable89") {
            (*evaluator)->SafetyTable[89] = parameter.value();
        }
        if (parameter.name() == "SafetyTable90") {
            (*evaluator)->SafetyTable[90] = parameter.value();
        }
        if (parameter.name() == "SafetyTable91") {
            (*evaluator)->SafetyTable[91] = parameter.value();
        }
        if (parameter.name() == "SafetyTable92") {
            (*evaluator)->SafetyTable[92] = parameter.value();
        }
        if (parameter.name() == "SafetyTable93") {
            (*evaluator)->SafetyTable[93] = parameter.value();
        }
        if (parameter.name() == "SafetyTable94") {
            (*evaluator)->SafetyTable[94] = parameter.value();
        }
        if (parameter.name() == "SafetyTable95") {
            (*evaluator)->SafetyTable[95] = parameter.value();
        }
        if (parameter.name() == "SafetyTable96") {
            (*evaluator)->SafetyTable[96] = parameter.value();
        }
        if (parameter.name() == "SafetyTable97") {
            (*evaluator)->SafetyTable[97] = parameter.value();
        }
        if (parameter.name() == "SafetyTable98") {
            (*evaluator)->SafetyTable[98] = parameter.value();
        }
        if (parameter.name() == "SafetyTable99") {
            (*evaluator)->SafetyTable[99] = parameter.value();
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

    for (int i = 0; i < 50; i++) {
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
    parameters.emplace_back("SafetyTable0", 0);
    parameters.emplace_back("SafetyTable1", 0);
    parameters.emplace_back("SafetyTable2", 1);
    parameters.emplace_back("SafetyTable3", 3);
    parameters.emplace_back("SafetyTable4", 3);
    parameters.emplace_back("SafetyTable5", 5);
    parameters.emplace_back("SafetyTable6", 7);
    parameters.emplace_back("SafetyTable7", 9);
    parameters.emplace_back("SafetyTable8", 12);
    parameters.emplace_back("SafetyTable9", 15);
    parameters.emplace_back("SafetyTable10", 18);
    parameters.emplace_back("SafetyTable11", 22);
    parameters.emplace_back("SafetyTable12", 26);
    parameters.emplace_back("SafetyTable13", 30);
    parameters.emplace_back("SafetyTable14", 35);
    parameters.emplace_back("SafetyTable15", 39);
    parameters.emplace_back("SafetyTable16", 44);
    parameters.emplace_back("SafetyTable17", 50);
    parameters.emplace_back("SafetyTable18", 56);
    parameters.emplace_back("SafetyTable19", 62);
    parameters.emplace_back("SafetyTable20", 68);
    parameters.emplace_back("SafetyTable21", 75);
    parameters.emplace_back("SafetyTable22", 82);
    parameters.emplace_back("SafetyTable23", 85);
    parameters.emplace_back("SafetyTable24", 89);
    parameters.emplace_back("SafetyTable25", 97);
    parameters.emplace_back("SafetyTable26", 105);
    parameters.emplace_back("SafetyTable27", 113);
    parameters.emplace_back("SafetyTable28", 122);
    parameters.emplace_back("SafetyTable29", 131);
    parameters.emplace_back("SafetyTable30", 140);
    parameters.emplace_back("SafetyTable31", 150);
    parameters.emplace_back("SafetyTable32", 169);
    parameters.emplace_back("SafetyTable33", 180);
    parameters.emplace_back("SafetyTable34", 191);
    parameters.emplace_back("SafetyTable35", 202);
    parameters.emplace_back("SafetyTable36", 213);
    parameters.emplace_back("SafetyTable37", 225);
    parameters.emplace_back("SafetyTable38", 237);
    parameters.emplace_back("SafetyTable39", 248);
    parameters.emplace_back("SafetyTable40", 260);
    parameters.emplace_back("SafetyTable41", 272);
    parameters.emplace_back("SafetyTable42", 283);
    parameters.emplace_back("SafetyTable43", 295);
    parameters.emplace_back("SafetyTable44", 307);
    parameters.emplace_back("SafetyTable45", 319);
    parameters.emplace_back("SafetyTable46", 330);
    parameters.emplace_back("SafetyTable47", 342);
    parameters.emplace_back("SafetyTable48", 354);
    parameters.emplace_back("SafetyTable49", 366);
    parameters.emplace_back("SafetyTable50", 377);
    parameters.emplace_back("SafetyTable51", 389);
    parameters.emplace_back("SafetyTable52", 401);
    parameters.emplace_back("SafetyTable53", 412);
    parameters.emplace_back("SafetyTable54", 424);
    parameters.emplace_back("SafetyTable55", 436);
    parameters.emplace_back("SafetyTable56", 448);
    parameters.emplace_back("SafetyTable57", 459);
    parameters.emplace_back("SafetyTable58", 471);
    parameters.emplace_back("SafetyTable59", 483);
    parameters.emplace_back("SafetyTable60", 494);
    parameters.emplace_back("SafetyTable61", 500);
    parameters.emplace_back("SafetyTable62", 500);
    parameters.emplace_back("SafetyTable63", 500);
    parameters.emplace_back("SafetyTable64", 500);
    parameters.emplace_back("SafetyTable65", 500);
    parameters.emplace_back("SafetyTable66", 500);
    parameters.emplace_back("SafetyTable67", 500);
    parameters.emplace_back("SafetyTable68", 500);
    parameters.emplace_back("SafetyTable69", 500);
    parameters.emplace_back("SafetyTable70", 500);
    parameters.emplace_back("SafetyTable71", 500);
    parameters.emplace_back("SafetyTable72", 500);
    parameters.emplace_back("SafetyTable73", 500);
    parameters.emplace_back("SafetyTable74", 500);
    parameters.emplace_back("SafetyTable75", 500);
    parameters.emplace_back("SafetyTable76", 500);
    parameters.emplace_back("SafetyTable77", 500);
    parameters.emplace_back("SafetyTable78", 500);
    parameters.emplace_back("SafetyTable79", 500);
    parameters.emplace_back("SafetyTable80", 500);
    parameters.emplace_back("SafetyTable81", 500);
    parameters.emplace_back("SafetyTable82", 500);
    parameters.emplace_back("SafetyTable83", 500);
    parameters.emplace_back("SafetyTable84", 500);
    parameters.emplace_back("SafetyTable85", 500);
    parameters.emplace_back("SafetyTable86", 500);
    parameters.emplace_back("SafetyTable87", 500);
    parameters.emplace_back("SafetyTable88", 500);
    parameters.emplace_back("SafetyTable89", 500);
    parameters.emplace_back("SafetyTable90", 500);
    parameters.emplace_back("SafetyTable91", 500);
    parameters.emplace_back("SafetyTable92", 500);
    parameters.emplace_back("SafetyTable93", 500);
    parameters.emplace_back("SafetyTable94", 500);
    parameters.emplace_back("SafetyTable95", 500);
    parameters.emplace_back("SafetyTable96", 500);
    parameters.emplace_back("SafetyTable97", 500);
    parameters.emplace_back("SafetyTable98", 500);
    parameters.emplace_back("SafetyTable99", 500);

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
