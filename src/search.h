#pragma once
#include "position.h"
#include <atomic>

struct SearchParams {
    bool     infinite = false;
    bool     ponder   = false;
    uint32_t wtime = 0, btime = 0;
    uint32_t winc = 0, binc = 0;
    uint32_t movestogo = 0;
    uint32_t depth     = 0;
    uint32_t nodes     = 0;
    uint32_t mate      = 0;
    uint32_t movetime  = 0;
};

extern std::atomic<bool> g_stopRequested;

void think(SearchParams params, const Position pos);
void stopThinking();
