#pragma once

#include "chess.hpp"
#include <chrono>

#define WHITE chess::Color::WHITE
#define BLACK chess::Color::BLACK
#define TYPE_PAWN chess::PieceType::PAWN
#define TYPE_KNIGHT chess::PieceType::KNIGHT
#define TYPE_BISHOP chess::PieceType::BISHOP
#define TYPE_ROOK chess::PieceType::ROOK
#define TYPE_QUEEN chess::PieceType::QUEEN
#define TYPE_KING chess::PieceType::KING
#define MOVE_GEN_ALL chess::movegen::MoveGenType::ALL
#define MOVE_GEN_CAPTURE chess::movegen::MoveGenType::CAPTURE

using chess::attacks;
using chess::Bitboard;
using chess::Board;
using chess::Color;
using chess::Move;
using chess::movegen;
using chess::Movelist;
using chess::Piece;
using chess::PieceType;
using chess::Square;

#define MATE_VALUE_THRESHOLD 32000

class Value {
private:
    short m_value;

public:
    constexpr Value() : m_value(0) {
    }
    constexpr Value(int value) : m_value((short)value) {
    }

    inline bool isValid() const {
        return m_value != -32768;
    }
    inline bool isMate() const {
        return isValid() && (m_value >= MATE_VALUE_THRESHOLD ||
                             m_value <= -MATE_VALUE_THRESHOLD);
    }

    constexpr static Value mateIn(int ply) {
        return Value(32767 - ply);
    }

    constexpr static Value matedIn(int ply) {
        return Value(-32767 + ply);
    }
    constexpr static Value none() {
        return Value(-32768);
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
    inline Value &operator+=(const Value &rhs) {
        m_value += rhs.m_value;
        return *this;
    }
    inline Value &operator-=(const Value &rhs) {
        m_value -= rhs.m_value;
        return *this;
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
    inline Score &operator+=(const Score &rhs) {
        mg += rhs.mg;
        eg += rhs.eg;
        return *this;
    }
    inline Score &operator-=(const Score &rhs) {
        mg -= rhs.mg;
        eg -= rhs.eg;
        return *this;
    }
};

constexpr Value MATE_VALUE = Value::mateIn(0);
constexpr Value MATED_VALUE = Value::matedIn(0);
constexpr Value VALUE_NONE = Value::none();
constexpr Value DRAW_VALUE = Value(0);

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

constexpr int MAX_PLY = 128;
