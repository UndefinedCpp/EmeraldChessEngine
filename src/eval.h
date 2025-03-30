#pragma once
#include "position.h"
#include "types.h"
#include <cstring>

/**
 * Checks if the game is over and returns the appropriate score.
 */
std::pair<bool, Value> checkGameStatus(Position &board);

// Override `operator<<` for fast printing of Values and Scores
inline std::ostream &operator<<(std::ostream &os, const Value &s) {
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
inline std::ostream &operator<<(std::ostream &os, const Score &s) {
    os << "S(" << (int)s.mg << ", " << (int)s.eg << ")";
    return os;
}

Value evaluate(Position &pos);
