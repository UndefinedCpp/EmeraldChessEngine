#pragma once

#include "history.h"
#include "position.h"
#include "types.h"
#include <vector>

class MovePicker {
private:
    struct MoveEntry {
        uint16_t id;
        int16_t  score;

        MoveEntry(uint16_t _id, int16_t _s) : id(_id), score(_s) {}
        inline Move object() const { return Move(id); }
        inline bool operator<(const MoveEntry& other) { return score < other.score; }
        inline bool operator>(const MoveEntry& other) { return score > other.score; }
    };

    std::vector<MoveEntry> moveBuffer;

public:
    MovePicker(Position& pos, const SearchHistory& history, const Move& ttMove);
    ~MovePicker();

    void skipQuiet();

    Move next();
};