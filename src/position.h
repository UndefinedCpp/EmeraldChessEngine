#pragma once

#include "chess.hpp"
#include "types.h"
#include <cmath>
#include <stack>

/**
 * Simple extension to Board with extra helper functions.
 */
class Position : public Board {
  public:
    using Board::Board; // Inherit the constructor

    /**
     * @brief Check if a move puts the other side in check
     */
    bool isCheckMove(const Move move) {
        makeMove(move);
        bool check = inCheck();
        unmakeMove(move);
        return check;
    }

    /**
     * Get the legal moves in the current position.
     */
    const Movelist legalMoves() {
        Movelist moves;
        movegen::legalmoves(moves, *this);
        return moves;
    }

    const Movelist generateCaptureMoves() {
        Movelist moves;
        movegen::legalmoves<movegen::MoveGenType::CAPTURE>(moves, *this);
        return moves;
    }

    /**
     * Get the legal moves for the opponent in the current position.
     */
    const Movelist getOpponentMoves() const {
        Movelist moves;
        const Color opponent = ~sideToMove();
        if (opponent == WHITE) {
            movegen::legalmoves<BLACK, MOVE_GEN_ALL>(moves, *this, 63);
        } else {
            movegen::legalmoves<WHITE, MOVE_GEN_ALL>(moves, *this, 63);
        }
        return moves;
    }

    /**
     * Gets the bitboard of attacked squares from a given square.
     */
    Bitboard getAttackMap(const Square square) const {
        Piece piece = at(square);
        Color color = piece.color();
        int type = static_cast<int>(piece.type());
        switch (type) {
        case (int)TYPE_PAWN:
            return chess::attacks::pawn(color, square);
        case (int)TYPE_KNIGHT:
            return chess::attacks::knight(square);
        case (int)TYPE_BISHOP:
            return chess::attacks::bishop(square, occ());
        case (int)TYPE_ROOK:
            return chess::attacks::rook(square, occ());
        case (int)TYPE_QUEEN:
            return chess::attacks::bishop(square, occ()) |
                   chess::attacks::rook(square, occ());
        case (int)TYPE_KING:
            return chess::attacks::king(square);
        default:
            return Bitboard(0ull);
        }
    }

    /**
     * Similar to `getAttackMap`, but not including friendly occupied squares.
     */
    Bitboard getMotionMap(const Square square) const {
        Color ally = at(square).color();
        Bitboard allyOccupied = us(ally);
        Bitboard attackMap = getAttackMap(square);
        return attackMap & ~allyOccupied;
    }

    int countPieces(const chess::PieceType type) const {
        return pieces(type).count();
    }
    int countPieces(const chess::PieceType type, const Color color) const {
        return pieces(type, color).count();
    }

  public:
    /**
     * Yet another way to quick access the board
     */
    Piece operator[](const Square square) const {
        assert(square.index() < 64 && square.index() >= 0);
        return board_[square.index()];
    }
};

namespace chess {
// Some utility functions also included here

// Implements various distance calculations
namespace dist {
/**
 * Manhattan distance between two squares, i.e. the number
 * of orthogonal king steps
 */
inline int manhattan(const Square a, const Square b) {
    return std::abs(a.file() - b.file()) + std::abs(a.rank() - b.rank());
}

/**
 * Chebyshev distance between two squares, i.e. the number
 * of king steps
 */
inline int chebyshev(const Square a, const Square b) {
    return std::max(std::abs(a.file() - b.file()),
                    std::abs(a.rank() - b.rank()));
}

/**
 * Knight distance between two squares, i.e. the number of
 * knight steps.
 */
inline int knight(const Square a, const Square b) {
    int dx = std::abs(a.file() - b.file());
    int dy = std::abs(a.rank() - b.rank());
    if (dx + dy == 1) {
        return 3;
    } else if (dx == 2 && dy == 2) {
        return 4;
    } else if (dx == 1 && dy == 1) {
        // Special case for corners
        bool aIsCorner = (a == Square::underlying::SQ_A1) ||
                         (a == Square::underlying::SQ_H1) ||
                         (a == Square::underlying::SQ_A8) ||
                         (a == Square::underlying::SQ_H8);
        bool bIsCorner = (b == Square::underlying::SQ_A1) ||
                         (b == Square::underlying::SQ_H1) ||
                         (b == Square::underlying::SQ_A8) ||
                         (b == Square::underlying::SQ_H8);
        if (aIsCorner || bIsCorner) {
            return 4;
        }
    }
    int m = std::ceil(std::max(std::max(dx / 2.0, dy / 2.0), (dx + dy) / 3.0));
    return m + ((m + dx + dy) % 2);
}
} // namespace dist

} // namespace chess