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

class EvalNet {
private:
    const nnue::Weight* w_;
    int16_t             fc1_w[16 * 768];
    int16_t             fc2_w[16 * 16];

public:
    EvalNet(const nnue::Weight* weight) : w_(weight) { init(); }

    void init() {
        for (size_t i = 0; i < 16 * 768; ++i) {
            fc1_w[i] = w_->fc1_weight[i] - 128;
        }
        for (size_t i = 0; i < 16 * 16; ++i) {
            fc2_w[i] = w_->fc2_weight[i] - 128;
        }
    }

    int operator()(const InputVector& x) const {
        // --- Layer 1 ---
        int8_t a1[16];
        for (int i = 0; i < 16; ++i) {
            int acc = ((int) (w_->fc1_bias[i]) - 128);
            for (int j = 0; j < 768; ++j) {
                acc += fc1_w[i * 768 + j] * x[j]; // x[j]=0 or 1
            }
            acc   = std::max(0, std::min(acc, 127));
            a1[i] = acc;
        }

        // --- Layer 2 ---
        int8_t a2[16];
        for (int i = 0; i < 16; ++i) {
            int32_t acc = (int32_t(w_->fc2_bias[i]) - 128) << 7;
            for (int j = 0; j < 16; ++j) {
                acc += fc2_w[i * 16 + j] * (int) (a1[j]);
            }
            acc   = std::max(0, std::min(acc, 16256));
            a2[i] = (acc >> 7);
        }

        // --- Layer 3 (final) ---
        int32_t acc = (int32_t(w_->fc3_bias[0]) - 128) << 7;
        for (int j = 0; j < 16; ++j) {
            int16_t wq = int16_t(w_->fc3_weight[j]) - 128;
            acc += wq * (int) (a2[j]);
        }

        return acc >> 7;
    }
};

InputVector getInputVectorFor(const Board& pos) {
    InputVector vec;
    vec.fill(0);

    const bool stm_white = (pos.sideToMove() == WHITE);

    auto scan = [&](Bitboard bb, bool flip, int idx) {
        while (bb) {
            int sq = bb.pop();
            if (flip)
                sq ^= 56;
            vec[sq * 12 + idx * 2 + (flip ? 1 : 0)] = 1;
        }
    };

    Bitboard wP = pos.pieces(TYPE_PAWN, WHITE);
    Bitboard wN = pos.pieces(TYPE_KNIGHT, WHITE);
    Bitboard wB = pos.pieces(TYPE_BISHOP, WHITE);
    Bitboard wR = pos.pieces(TYPE_ROOK, WHITE);
    Bitboard wQ = pos.pieces(TYPE_QUEEN, WHITE);
    Bitboard wK = pos.pieces(TYPE_KING, WHITE);

    Bitboard bP = pos.pieces(TYPE_PAWN, BLACK);
    Bitboard bN = pos.pieces(TYPE_KNIGHT, BLACK);
    Bitboard bB = pos.pieces(TYPE_BISHOP, BLACK);
    Bitboard bR = pos.pieces(TYPE_ROOK, BLACK);
    Bitboard bQ = pos.pieces(TYPE_QUEEN, BLACK);
    Bitboard bK = pos.pieces(TYPE_KING, BLACK);

    scan(wP, !stm_white, 0);
    scan(wN, !stm_white, 1);
    scan(wB, !stm_white, 2);
    scan(wR, !stm_white, 3);
    scan(wQ, !stm_white, 4);
    scan(wK, !stm_white, 5);

    scan(bP, stm_white, 0);
    scan(bN, stm_white, 1);
    scan(bB, stm_white, 2);
    scan(bR, stm_white, 3);
    scan(bQ, stm_white, 4);
    scan(bK, stm_white, 5);

    return vec;
}

} // namespace

/**
 * Main evaluation function.
 */
Value evaluate(Position& pos) {
    static EvalNet net(nnue::weight);
    return net(getInputVectorFor(pos));
}
