//
// Created by Kevin on 12/5/2024.
//

#ifndef SYZYGY_H
#define SYZYGY_H

#include "libchess/Position.h"

namespace Tablebase {

// Probe the Syzygy tablebases for a WDL value
unsigned probeTablebaseWDL(libchess::Position &board, int depth, int ply);

}

#endif //SYZYGY_H
