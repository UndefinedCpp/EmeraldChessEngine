#pragma once
#include "position.h"
#include <cstring>
#define MATE_VALUE_THRESHOLD 32000

class Value {
  private:
    short m_value;

  public:
    constexpr Value() : m_value(0) {
    }
    constexpr Value(int value) : m_value((short)value) {
    }

    bool isMate() const {
        return m_value >= MATE_VALUE_THRESHOLD ||
               m_value <= -MATE_VALUE_THRESHOLD;
    }

    int value() const {
        return m_value;
    }
    int mate() const {
        if (m_value > 0) {
            return (32767 /* short limit */ - m_value + 1) / 2;
        } else {
            return (-32767 - m_value - 1) / 2;
        }
    }

    /**
     * Adjust the mate-related evaluation values based on the
     * number of plies.
     */
    inline Value addPly(int ply = 1) const {
        if (m_value > MATE_VALUE_THRESHOLD) {
            return Value(m_value - ply);
        } else if (m_value < -MATE_VALUE_THRESHOLD) {
            return Value(m_value + ply);
        } else {
            return Value(m_value);
        }
    }

    inline Value operator+(const Value &rhs) const {
        return Value(m_value + rhs.m_value);
    }
    inline Value operator+(const int rhs) const {
        return Value(m_value + rhs);
    }
    inline Value operator-(const Value &rhs) const {
        return Value(m_value - rhs.m_value);
    }
    inline Value operator-(const int rhs) const {
        return Value(m_value - rhs);
    }
    inline Value operator*(const Value &rhs) const {
        return Value(m_value * rhs.m_value);
    }
    inline Value operator*(const float rhs) const {
        return Value(static_cast<int>(m_value * rhs));
    }
    inline Value operator*(const int rhs) const {
        return Value(m_value * rhs);
    }
    inline Value operator/(const int rhs) const {
        return Value(m_value / rhs);
    }
    inline Value operator~() const {
        return Value(-m_value);
    }
    inline operator int() const {
        return m_value;
    }

    inline bool operator>(const Value &rhs) const {
        return m_value > rhs.m_value;
    }
    inline bool operator>=(const Value &rhs) const {
        return m_value >= rhs.m_value;
    }
    inline bool operator<(const Value &rhs) const {
        return m_value < rhs.m_value;
    }
    inline bool operator<=(const Value &rhs) const {
        return m_value <= rhs.m_value;
    }
    inline bool operator==(const Value &rhs) const {
        return m_value == rhs.m_value;
    }
    inline bool operator!=(const Value &rhs) const {
        return m_value != rhs.m_value;
    }
};

#define ALL_GAME_PHASES 32

class Score {
  public:
    Value mg;
    Value eg;

    constexpr Score() : mg(0), eg(0) {
    }
    constexpr Score(int v) : mg(v), eg(v) {
    }
    Score(Value mg, Value eg) : mg(mg), eg(eg) {
    }
    constexpr Score(int mg, int eg) : mg(mg), eg(eg) {
    }

    Value fuse(int k, int N = ALL_GAME_PHASES) const {
        // warning: may overflow, always check the range
        int mg = static_cast<int>(this->mg) * k / N;
        int eg = static_cast<int>(this->eg) * (N - k) / N;
        short v = std::max(-MATE_VALUE_THRESHOLD,
                           std::min(MATE_VALUE_THRESHOLD, mg + eg));
        return Value(v);
    }

    inline Score operator+(const Score &rhs) const {
        return Score(mg + rhs.mg, eg + rhs.eg);
    }
    inline Score operator-(const Score &rhs) const {
        return Score(mg - rhs.mg, eg - rhs.eg);
    }
    inline Score operator*(const int rhs) const {
        return Score(mg * rhs, eg * rhs);
    }
    inline Score operator/(const int rhs) const {
        return Score(mg / rhs, eg / rhs);
    }
    inline Score operator~() const {
        return Score(~mg, ~eg);
    }
};

constexpr Value MATE_VALUE = Value(32767);
constexpr Value MATE_GIVEN = Value(32767);
constexpr Value MATED_VALUE = Value(-32767);
constexpr Value DRAW_VALUE = Value(0);

/**
 * Checks if the game is over and returns the appropriate score.
 */
std::pair<bool, Value> checkGameStatus(Position &board);

// Override `operator<<` for fast printing of Values and Scores
inline std::ostream &operator<<(std::ostream &os, const Value &s) {
    assert(s > MATED_VALUE && s < MATE_VALUE);
    if (s.isMate()) {
        os << "mate " << s.mate();
    } else {
        os << "cp " << s.value();
    }
    return os;
}
// This is mostly for debugging purposes
inline std::ostream &operator<<(std::ostream &os, const Score &s) {
    os << "S(" << (int)s.mg << ", " << (int)s.eg << ")";
    return os;
}

#define __subeval template <bool side>
#define white_specific if constexpr (side)
#define black_specific if constexpr (!side)
#define eval_component(name) (name<true>() - name<false>())
#define __color (side ? WHITE : BLACK)

class Evaluator {
  protected:
    Position &pos;

    // Pawn attack bitboard
    Bitboard w_pawn_atk_bb;
    Bitboard b_pawn_atk_bb;
    // Mobility area
    Bitboard w_mobility_area;
    Bitboard b_mobility_area;
    // King ring
    Bitboard w_king_ring;
    Bitboard b_king_ring;
    // Passed pawns
    Bitboard w_passed_pawns;
    Bitboard b_passed_pawns;

  public:
    Evaluator(Position &pos) : pos(pos) {
        _initialize();
    }

  public:
    Value operator()();

  private:
    void _initialize();

    int phase();
    int endgame_scale_factor();

    // Material =====================================================
    __subeval Score _piece_value() const;
    __subeval Score _psqt() const;

    // Mobility =====================================================
    __subeval Score _mobility() const;

    // Space ========================================================
    __subeval Score _space() const;

    // Pawn =========================================================
    __subeval Score _isolated_pawns() const;
    __subeval Score _doubled_pawns() const;
    __subeval Score _supported_pawns() const;
    __subeval Score _passed_pawns() const;
    __subeval Score _passed_unblocked() const;

    // Piece ========================================================
    __subeval Score _bishop_pair() const;

    // King safety ==================================================
    __subeval Score _king_attackers() const;
    __subeval Score _weak_king_squares() const;
};