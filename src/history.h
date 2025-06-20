#pragma once

#include "position.h"
#include "types.h"
#include <cstring>
#include <vector>

constexpr short MAX_HISTORY_SCORE = 5000;
constexpr short MIN_HISTORY_SCORE = -5000;
constexpr int   STABILITY_MARGIN  = 30;

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

    /**
     * History heuristic.
     */
    struct HistoryTable {
        int16_t db[2][6][64]; // indexed by [color][piece][square]

        HistoryTable() { std::memset(db, 0, sizeof(db)); }

        int16_t get(const Move m, const PieceType p, bool isWhite) const {
            return db[isWhite][(int) p][m.to().index()];
        }
        void update(const Move m, const PieceType p, int depth, bool isWhite, bool good) {
            int16_t bonus = depth * depth;
            if (!good) { bonus = -bonus; }
            int16_t current = get(m, p, isWhite);
            int16_t update  = gravity(current, bonus);
            db[isWhite][(int) p][m.to().index()] += update;
        }

        /**
         * Gravity formula. Scales up the score when the cutoff is unexpected.
         * See https://www.chessprogramming.org/History_Heuristic#Update.
         */
        int16_t gravity(int16_t now, int16_t bonus) {
            bonus = std::clamp(bonus, MIN_HISTORY_SCORE, MAX_HISTORY_SCORE);
            return bonus - now * std::abs(bonus) / MAX_HISTORY_SCORE;
        }
        void clear() { std::memset(db, 0, sizeof(db)); }
    };

public:
    KillerTable  killerTable[MAX_PLY];
    HistoryTable historyTable;
    int          evalStability;

    void clear() {
        historyTable.clear();
        for (int i = 0; i < MAX_PLY; i++) { killerTable[i].clear(); }
        evalStability = 0;
    }

public:
    SearchHistory() { clear(); }
    void update(
        const Position&              pos,
        const Move                   bestMove,
        const std::vector<uint16_t>& quietMoves,
        const std::vector<uint16_t>& captureMoves,
        bool                         isWhite,
        int                          depth,
        int                          ply) {
        if (!pos.isCapture(bestMove)) {
            // Update killer
            killerTable[ply].add(bestMove);

            // Update history
            for (const uint16_t moveCode : quietMoves) {
                const Move      move = Move(moveCode);
                const PieceType pt   = pos.at(move.to()).type();
                const bool      good = moveCode == bestMove.move();
                historyTable.update(Move(moveCode), pt, depth, isWhite, good);
            }
        }
    }
    void updateStability(const Value prev, const Value curr) {
        if (std::abs(prev - curr) <= STABILITY_MARGIN) {
            evalStability++;
        } else {
            evalStability = 0;
        }
    }
};