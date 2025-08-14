#pragma once

#include "position.h"
#include "types.h"
#include <cstring>
#include <vector>

constexpr short MAX_HISTORY_SCORE = 5000;
constexpr short MIN_HISTORY_SCORE = -5000;

/**
 * This class keeps track of the search history.
 */
class SearchHistory {
public:
    /**
     * Killer move heuristic. The idea is that very often the same move will
     * cause a beta cutoff in many different branches. By keeping track of
     * the killer moves, we can improve the move ordering and thus speed up
     * the search.
     */
    struct KillerTable {
        uint16_t killer1 = 0;
        uint16_t killer2 = 0;

        bool has(const Move m) const { return m.move() == killer1 || m.move() == killer2; }
        void add(const Move m) {
            // This essentially works like a stack
            uint16_t code = m.move();
            if (killer1 == 0) { // Empty slot
                killer1 = code;
            } else if (code != killer1 && code != killer2) { // A new killer move seen
                killer2 = killer1; // Move the old killer to the second slot
                killer1 = code;
            }
        }
        void clear() {
            killer1 = 0;
            killer2 = 0;
        }
    };

public:
    KillerTable killerTable;

    void clear() { killerTable.clear(); }
};