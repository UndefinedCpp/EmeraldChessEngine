#pragma once
#include "position.h"
#include "types.h"
#include <cstring>

constexpr Value PIECE_VALUE[7] = {100, 300, 330, 550, 900, 10000, 0};

/**
 * Checks if the game is over and returns the appropriate score.
 */
std::pair<bool, Value> checkGameStatus(Position& board);

// Override `operator<<` for fast printing of Values and Scores
inline std::ostream& operator<<(std::ostream& os, const Value& s) {
    if (!s.isValid()) {
        os << "(invalid score)";
    } else {
        if (s.isMate()) {
            os << "mate " << s.mate();
        } else {
            os << "cp " << s.value();
        }
    }
    return os;
}
// This is mostly for debugging purposes
inline std::ostream& operator<<(std::ostream& os, const Score& s) {
    os << "S(" << (int) s.mg << ", " << (int) s.eg << ")";
    return os;
}

Value evaluate(Position& pos);

/**
 * Update evaluator network state.
 */
void updateEvaluatorState(const Position& pos, const Move& move);
void updateEvaluatorState(const Position& pos);
void updateEvaluatorState();