#pragma once
#include "position.h"
#include "timecontrol.h"
#include "types.h"
#include <atomic>

extern std::atomic<bool> g_stopRequested;
extern TimeControl       g_timeControl;

void think(SearchParams params, const Position pos);
void stopThinking();
