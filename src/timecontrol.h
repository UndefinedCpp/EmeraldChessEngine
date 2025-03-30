#pragma once

#include "search.h"
#include <chrono>

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

/**
 * The amount of time the engine decides to use is splitted into two parts:
 * hard time limit and soft time limit. Hard time limit is constantly checked
 * during the search to make sure the engine does not exceed the time limit.
 * Soft time limit is served as a hint to the engine that it is spending quite
 * much time and it should stop as soon as possible. Such limit is checked only
 * at the beginning of an iteration. The idea is that if the engine isn't likely
 * to finish the iteration in time, it should stop the search immediately to
 * save time.
 */
struct TimeControl {
    uint32_t softTimeWall = 0;
    uint32_t hardTimeWall = 0;
    uint32_t maxDepth = 0;
    uint32_t softNodesWall = 0;
    TimePoint startTime;
    bool competitionMode = false;

    TimeControl() = default;
    TimeControl(const Color stm, const SearchParams &params, TimePoint now) {
        uint32_t time, inc;
        startTime = now;

        if (params.movetime > 0) { // specify move time
            softTimeWall = hardTimeWall = params.movetime;
            return;
        } else if (params.depth > 0) { // specify depth
            maxDepth = params.depth;
            return;
        } else if (params.nodes > 0) // specify nodes
        {
            softNodesWall = params.nodes;
            return;
        } else { // specify remaining time and increment
            time = stm == WHITE ? params.wtime : params.btime;
            inc = stm == WHITE ? params.winc : params.binc;
            competitionMode = true;
        }

        float baseTime = time * 0.05f + inc * 0.75f;
        softTimeWall = (uint32_t)(baseTime * 0.6f);
        hardTimeWall = (uint32_t)(std::min(baseTime * 1.5f, time * 0.9f));
    }

    static TimePoint now() {
        using namespace std::chrono;
        return steady_clock::now();
    }

    int _elapsed() const {
        using namespace std::chrono;
        TimePoint now = TimeControl::now();
        return duration_cast<milliseconds>(now - startTime).count();
    }

    /**
     * Check if we have hit the hard time limit.
     */
    bool hitHardLimit(int depth, int nodes) const {
        if (softNodesWall > 0 && nodes >= softNodesWall)
            return true;
        if (maxDepth > 0) {
            return depth >= (int)maxDepth;
        }
        return (uint32_t)_elapsed() >= hardTimeWall;
    }

    /**
     * Check if we have hit the soft time limit.
     * @param stability The evaluation stability. The higher, the more likely
     * the evaluation is stable.
     */
    bool hitSoftLimit(int depth, int nodes, int stability) const {
        if (softNodesWall > 0 && nodes >= softNodesWall)
            return true;
        if (maxDepth > 0) {
            return depth >= (int)maxDepth;
        }
        // Dynamically adjust the time limit based on the stability.
        float scaleFactor = 1.0f;
        if (depth >= 5) { // Only scale after 5 plies are searched
            scaleFactor += 0.5 - std::min(stability, 5) / 10.0f;
        }
        uint32_t limit = (uint32_t)(softTimeWall * scaleFactor);

        return _elapsed() >= limit;
    }
};