#pragma once

#include "history.h"
#include "position.h"
#include "types.h"
#include <vector>

class MovePicker {
private:
    struct MoveEntry {
        uint16_t code;
        int16_t score;

        bool operator<(const MoveEntry &other) const {
            return score < other.score;
        }
        Move move() const {
            return Move(code);
        }
    };

    Position &pos;
    std::vector<MoveEntry> q;
    size_t m_size;

    int16_t score(const Move m);

public:
    using KillerTbl = SearchHistory::KillerTable;
    using HistTbl = SearchHistory::HistoryTable;

    MovePicker(Position &pos);
    void init(KillerTbl *killerTable = nullptr, HistTbl *historyTable = nullptr,
              Move hashMove = Move::NO_MOVE);
    void initQuiet(HistTbl *historyTable = nullptr);
    Move pick();

    inline int16_t size() const {
        return m_size;
    }
};