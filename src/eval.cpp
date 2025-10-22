#include "eval.h"
#include "weight.h"
#include <array>
#include <cmath>

#define INPUT_SIZE 768
#define FEATURE_SIZE 32
#define LAYER1_SIZE 128

std::pair<bool, Value> checkGameStatus(Position& board) {
    // Generate legal moves to validate checkmate or stalemate
    Movelist moves = board.legalMoves();
    if (moves.empty()) {
        if (board.inCheck()) {
            return {true, MATED_VALUE}; // checkmate
        }
        return {true, DRAW_VALUE}; // stalemate
    }
    // Check for extra rules
    if (board.isHalfMoveDraw()            // 50 moves rule
        || board.isInsufficientMaterial() // insufficient material by FIDE rules
    ) {
        return {true, DRAW_VALUE};
    }
    if (board.isRepetition()) {
        return {true, DRAW_VALUE};
    }
    // Game is not over
    return {false, 0};
}

namespace {

struct alignas(32) Accumulator {
    int white[FEATURE_SIZE];
    int black[FEATURE_SIZE];
};

std::pair<size_t, size_t> getFeatureIndices(const Color color, const PieceType pt, Square sq) {
    const bool   isWhite    = color == WHITE;
    const size_t whiteIndex = ((int) (!isWhite) * 6 + (int) pt) * 64 + sq.index();
    const size_t blackIndex = ((int) (isWhite) * 6 + (int) pt) * 64 + sq.flip().index();
    return {whiteIndex, blackIndex};
}

class NNUEState {
private:
    std::vector<Accumulator> accumulatorStack;
    const nnue::Weight&      w;

public:
    inline Accumulator& curr() { return accumulatorStack.back(); }

public:
    NNUEState() : w(*nnue::weight) {}

    void push() {
        Accumulator copy = curr();
        accumulatorStack.push_back(copy);
    }
    void pop() { accumulatorStack.pop_back(); }

    void reset(const Position& pos) {
        // Create a new accumulator
        Accumulator accum;
        // Initialize with bias
        for (int i = 0; i < FEATURE_SIZE; ++i) {
            accum.white[i] = w.fc1_bias[i];
            accum.black[i] = w.fc1_bias[i];
        }
        // Clear the stack and push the accumulator
        accumulatorStack.clear();
        accumulatorStack.push_back(std::move(accum));
        // Call the update functions
        Bitboard occ = pos.occ();
        while (occ) {
            Square sq = occ.pop();
            Piece  p  = pos.at(sq);
            update<true>(p, sq);
        }
    }

    template <bool activate>
    void update(const Piece piece, const Square square) {
        update<activate>(piece.color(), piece.type(), square);
    }

    template <bool activate>
    void update(const Color color, const PieceType pt, const Square sq) {
        const auto [wi, bi]      = getFeatureIndices(color, pt, sq);
        constexpr int multiplier = (activate ? 1 : -1);
        for (int i = 0; i < FEATURE_SIZE; ++i) {
            curr().white[i] += w.fc1_weight[wi * FEATURE_SIZE + i] * multiplier;
        }
        for (int i = 0; i < FEATURE_SIZE; ++i) {
            curr().black[i] += w.fc1_weight[bi * FEATURE_SIZE + i] * multiplier;
        }
    }

    int evaluate(const Color color) {
        const int* input1 = ((color == WHITE) ? curr().white : curr().black);
        const int* input2 = ((color == WHITE) ? curr().black : curr().white);
        // Clipped ReLU activation
        int v1[FEATURE_SIZE], v2[FEATURE_SIZE];
        for (int i = 0; i < 32; ++i) {
            v1[i] = std::clamp(input1[i], 0, 32767);
            v2[i] = std::clamp(input2[i], 0, 32767);
        }
        // Clipped square activation
        int v1s[FEATURE_SIZE], v2s[FEATURE_SIZE];
        for (int i = 0; i < 32; ++i) {
            v1s[i] = v1[i] * v1[i];
            v2s[i] = v2[i] * v2[i];
        }
        for (int i = 0; i < 32; ++i) {
            v1s[i] >>= 15;
            v2s[i] >>= 15;
        }
        // Pass through second layer
        int temp[4] = {0};
        for (int i = 0; i < 32; ++i) {
            temp[0] += v1[i] * w.fc2_weight[i];
        }
        for (int i = 0; i < 32; ++i) {
            temp[1] += v1s[i] * w.fc2_weight[i + FEATURE_SIZE];
        }
        for (int i = 0; i < 32; ++i) {
            temp[2] += v2[i] * w.fc2_weight[i + FEATURE_SIZE * 2];
        }
        for (int i = 0; i < 32; ++i) {
            temp[3] += v2s[i] * w.fc2_weight[i + FEATURE_SIZE * 3];
        }
        // Accumulate
        int y = w.fc2_bias + temp[0] / 127 + temp[1] / 127 + temp[2] / 127 + temp[3] / 127;
        y     = y / 170;
        return y;
    }
};

// global instance
NNUEState gNNUE;

} // namespace

/**
 * Main evaluation function.
 */
Value evaluate(Position& pos) {
    gNNUE.reset(pos);
    int       rawScore         = gNNUE.evaluate(pos.sideToMove());
    const int fiftyMoveCounter = pos.halfMoveClock();
    // Decay score with respect to fifty-move rule linearly
    rawScore = rawScore * (100 - fiftyMoveCounter) / 100;
    return Value(rawScore);
}

/**
 * Update evaluator state. This tells the net to incrementally
 * update since you make some move.
 */
void updateEvaluatorState(const Position& pos, const Move& move) {
    // const Piece pFrom = pos.at(move.from());
    // const Square sFrom = move.from();
    // const Piece pTo = pos.at(move.to());
    // const Square sTo = move.to();
}

/**
 * This tells the net to refresh all accumulators.
 */
void updateEvaluatorState(const Position& pos) {
    // gNNUE.reset(pos);
}

/**
 * This tells the net that you have undone a move.
 */
void updateEvaluatorState() {
    // gNNUE.pop();
}
