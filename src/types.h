#pragma once

#include "position.h"

/**
 * Implements data structure for killer move heuristics. We keep
 * track of the last two killer moves for each ply.
 */
struct KillerHeuristics {
    uint16_t killer1;
    uint16_t killer2;

    KillerHeuristics() : killer1(0), killer2(0) {
    }
    void add(Move move) {
        if (move.move() != killer1) {
            killer2 = killer1;
            killer1 = move.move();
        }
    }
    bool has(Move move) {
        return move.move() == killer1 || move.move() == killer2;
    }
};
