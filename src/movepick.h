#pragma once

#include "history.h"
#include "position.h"
#include "types.h"
#include <vector>

namespace {
enum class MovePickerStage {
    TT,
    GEN_NOISY,
    GOOD_NOISY,
    KILLER_1,
    KILLER_2,
    GEN_QUIET,
    GOOD_QUIET,
    BAD_NOISY,
    BAD_QUIET,
    END_NORMAL,

    GEN_QSEARCH,
    GOOD_QSEARCH,
    END_QSEARCH
};
}

class MovePicker {
private:
    struct ScoredMove {
        uint16_t moveCode;
        int16_t  score;

        bool operator<(const ScoredMove& other) const { return score < other.score; }
        bool operator>(const ScoredMove& other) const { return score > other.score; }
        Move move() const { return Move(moveCode); }
    };

    Position&      pos;
    SearchHistory& history;
    uint16_t       ttMoveCode;
    bool           _skipQuiet; // when enabled, skips everything after GOOD_NOISY
    bool           inCheck;
    int            ply;

    std::vector<ScoredMove> quietBuffer;
    std::vector<ScoredMove> noisyBuffer;
    MovePickerStage         stage;

private:
    void generateNoisyMoves();
    void generateQuietMoves();
    void generateEvasionMoves();

public:
    MovePicker(
        Position& pos, SearchHistory& history, int ply, uint16_t ttMoveCode, bool isQsearch) :
        pos(pos), history(history), ply(ply), ttMoveCode(ttMoveCode), _skipQuiet(false) {
        inCheck = pos.inCheck();
        stage   = isQsearch ? MovePickerStage::GEN_QSEARCH : MovePickerStage::TT;
    }

    Move next();
    void skipQuiet();

    const MovePickerStage& getStage() const { return stage; }
};