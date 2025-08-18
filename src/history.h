#pragma once

#include "position.h"
#include "types.h"
#include <cstring>
#include <vector>

constexpr short MAX_HISTORY_SCORE = 10000;
constexpr short MIN_HISTORY_SCORE = -10000;

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

    struct QuietHistoryTable {
        int16_t data[2][64][64];

        void clear() { std::memset(data, 0, sizeof(data)); }

        inline int16_t get(const Color stm, const Move& move) {
            return data[(int) stm][move.from().index()][move.to().index()];
        }
        inline void update(const Color stm, const Move& move, int16_t bonus) {
            int16_t& ref  = data[(int) stm][move.from().index()][move.to().index()];
            int      diff = bonus - ref * std::abs(bonus) / MAX_HISTORY_SCORE;

            ref = std::clamp(ref + diff, (int) MIN_HISTORY_SCORE, (int) MAX_HISTORY_SCORE);
        }
    };

    struct CaptureHistoryTable {
        int16_t data[2][6][64][6];

        void clear() { std::memset(data, 0, sizeof(data)); }

        inline int16_t get(const Color stm, const Move& move, const Position& pos) {
            const Square to        = move.to();
            const auto   aggressor = pos.at(move.from()).type();
            const auto   victim    = pos.at(to).type();
            assert(aggressor != PieceType::NONE);
            if (victim == PieceType::NONE) { // en passant
                return 0;
            }
            return data[(int) stm][(int) aggressor][to.index()][(int) victim];
        }

        inline void update(const Color stm, const Move& move, const Position& pos, int16_t bonus) {
            const Square to        = move.to();
            const auto   aggressor = pos.at(move.from()).type();
            const auto   victim    = pos.at(to).type();
            assert(aggressor != PieceType::NONE);
            if (victim == PieceType::NONE) { // en passant
                return;
            }
            int16_t& ref  = data[(int) stm][(int) aggressor][to.index()][(int) victim];
            int      diff = bonus - ref * std::abs(bonus) / MAX_HISTORY_SCORE;

            ref = std::clamp(ref + diff, (int) MIN_HISTORY_SCORE, (int) MAX_HISTORY_SCORE);
        }
    };

    KillerTable         killerTable[MAX_PLY];
    QuietHistoryTable   qHistoryTable;
    CaptureHistoryTable capHistoryTable;

    void clear() {
        for (int i = 0; i < MAX_PLY; i++) {
            killerTable[i].clear();
        }
        qHistoryTable.clear();
        capHistoryTable.clear();
    }
};