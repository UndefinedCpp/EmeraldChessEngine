#include "eval.h"
#include "weight.h"
#include <array>
#include <cmath>

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

class EvaluatorNet {
private:
    const nnue::Weight& w_;

public:
    EvaluatorNet() : w_(*nnue::weight) {}

    int operator()(const int8_t* __restrict__ x1, const int8_t* __restrict__ x2) const {
        // Set first layer bias
        int32_t accumulator1[32] = {0};
        for (int i = 0; i < 32; ++i) {
            accumulator1[i] = w_.fc1_bias[i]; // auto-vectorizable
        }
        int32_t accumulator2[32] = {0};
        for (int i = 0; i < 32; ++i) {
            accumulator2[i] = w_.fc1_bias[i];
        }
        // Accumulate with weight
        for (int i = 0; i < 768; ++i) {
            for (int j = 0; j < 32; ++j) {
                accumulator1[j] += w_.fc1_weight[i * 32 + j] * x1[i];
            }
        }
        for (int i = 0; i < 768; ++i) {
            for (int j = 0; j < 32; ++j) {
                accumulator2[j] += w_.fc1_weight[i * 32 + j] * x2[i];
            }
        }
        // Clamp to 0 and 32767
        for (int i = 0; i < 32; ++i) {
            accumulator1[i] = std::clamp(accumulator1[i], 0, 32767);
        }
        for (int i = 0; i < 32; ++i) {
            accumulator2[i] = std::clamp(accumulator2[i], 0, 32767);
        }
        // Also compute features with square
        int32_t acc1_sqr[32] = {0};
        for (int i = 0; i < 32; ++i) {
            acc1_sqr[i] = accumulator1[i] * accumulator1[i];
        }
        for (int i = 0; i < 32; ++i) {
            acc1_sqr[i] >>= 15;
        }
        int32_t acc2_sqr[32] = {0};
        for (int i = 0; i < 32; ++i) {
            acc2_sqr[i] = accumulator2[i] * accumulator2[i];
        }
        for (int i = 0; i < 32; ++i) {
            acc2_sqr[i] >>= 15;
        }

        // Values are ready to pass through dense layer.
        int32_t acc  = w_.fc2_bias; // set bias
        int32_t temp = 0;
        for (int i = 0; i < 32; ++i) {
            temp += accumulator1[i] * w_.fc2_weight[i];
        }
        acc += temp / 127;
        temp = 0;
        for (int i = 0; i < 32; ++i) {
            temp += acc1_sqr[i] * w_.fc2_weight[i + 32];
        }
        acc += temp / 127;
        temp = 0;
        for (int i = 0; i < 32; ++i) {
            temp += accumulator2[i] * w_.fc2_weight[i + 64];
        }
        acc += temp / 127;
        temp = 0;
        for (int i = 0; i < 32; ++i) {
            temp += acc2_sqr[i] * w_.fc2_weight[i + 96];
        }
        acc += temp / 127;

        return acc / 152;
    }
};

void getInputRepresentationFor(const Board& pos, int8_t* v1, int8_t* v2) {
    int8_t white[768] = {0};
    int8_t black[768] = {0};

    const bool stm_white = (pos.sideToMove() == WHITE);

    auto scan = [&](Bitboard bb, bool isWhite, int idx) {
        while (bb) {
            int sq            = bb.pop();
            int whiteIndex    = ((int) (!isWhite) * 6 + idx) * 64 + sq;
            white[whiteIndex] = 1;
            int blackIndex    = ((int) (isWhite) * 6 + idx) * 64 + sq;
            black[blackIndex] = 1;
        }
    };

    for (int p_index = 0; p_index < 6; ++p_index) {
        PieceType pt = (PieceType::underlying) p_index;
        scan(pos.pieces(pt, WHITE), true, p_index);
        scan(pos.pieces(pt, BLACK), false, p_index);
    }

    if (stm_white) {
        memcpy(v1, white, 768);
        memcpy(v2, black, 768);
    } else {
        memcpy(v2, white, 768);
        memcpy(v1, black, 768);
    }
}

} // namespace

/**
 * Main evaluation function.
 */
Value evaluate(Position& pos) {
    static EvaluatorNet net;
    int8_t              vec1[768];
    int8_t              vec2[768];
    getInputRepresentationFor(pos, vec1, vec2);
    return net(vec1, vec2);
}
