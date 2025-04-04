#pragma once

#include "position.h"

struct SearchParams {
    bool infinite = false; // Infinite search
    bool ponder = false;
    uint32_t wtime = 0;
    uint32_t btime = 0;
    uint32_t winc = 0;
    uint32_t binc = 0;
    uint32_t movestogo = 0;
    uint32_t depth = 0;
    uint32_t nodes = 0;
    uint32_t mate = 0;
    uint32_t movetime = 0;
};

void think(SearchParams params, const Position pos);
void stopThinking();
