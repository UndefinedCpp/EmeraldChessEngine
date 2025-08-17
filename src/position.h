#pragma once

#include "chess.hpp"
#include "types.h"
#include <cmath>

constexpr int SEE_PIECE_VALUE[] = {100, 300, 320, 550, 1000, 99999, 0};

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
    const Movelist legalMoves() const {
        Movelist moves;
        movegen::legalmoves(moves, *this);
        return moves; // Let RVO take care of this, no need for std::move
    }

    const Movelist generateCaptureMoves() const {
        Movelist moves;
        movegen::legalmoves<movegen::MoveGenType::CAPTURE>(moves, *this);
        return moves;
    }

    /**
     * Get the legal moves for the opponent in the current position.
     */
    const Movelist getOpponentMoves() const {
        Movelist    moves;
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
        int   type  = static_cast<int>(piece.type());
        switch (type) {
            case (int) TYPE_PAWN:
                return chess::attacks::pawn(color, square);
            case (int) TYPE_KNIGHT:
                return chess::attacks::knight(square);
            case (int) TYPE_BISHOP:
                return chess::attacks::bishop(square, occ());
            case (int) TYPE_ROOK:
                return chess::attacks::rook(square, occ());
            case (int) TYPE_QUEEN:
                return chess::attacks::bishop(square, occ()) | chess::attacks::rook(square, occ());
            case (int) TYPE_KING:
                return chess::attacks::king(square);
            default:
                return Bitboard(0ull);
        }
    }

    /**
     * Similar to `getAttackMap`, but not including friendly occupied squares.
     */
    Bitboard getMotionMap(const Square square) const {
        Color    ally         = at(square).color();
        Bitboard allyOccupied = us(ally);
        Bitboard attackMap    = getAttackMap(square);
        return attackMap & ~allyOccupied;
    }

    int countPieces(const chess::PieceType type) const { return pieces(type).count(); }
    int countPieces(const chess::PieceType type, const Color color) const {
        return pieces(type, color).count();
    }

    bool hasNonPawnMaterial(const Color color) const {
        const Bitboard occupied = us(color);
        const Bitboard pawns    = pieces(TYPE_PAWN, color);
        return (occupied & ~pawns) != 0;
    }
    bool hasNonPawnMaterial() const {
        return hasNonPawnMaterial(WHITE) && hasNonPawnMaterial(BLACK);
    }

    /**
     * Static Exchange Evaluation.
     *
     * This is useful to check if a series of captures is good without
     * explicitly playing the moves. This helps improve move ordering and
     * skipping certain moves in search.
     *
     * @returns true if the move wins material after exchange sequence
     */
    bool see(const Move move, int threshold) {
        const Square    from       = move.from();
        const Square    to         = move.to();
        const auto      moveType   = move.typeOf();
        const PieceType fromType   = at<PieceType>(from);
        const PieceType toType     = moveType == Move::ENPASSANT ? TYPE_PAWN : at<PieceType>(to);
        PieceType       nextVictim = moveType == Move::PROMOTION ? move.promotionType() : fromType;

        int balance = -threshold;
        balance += toType != PieceType::NONE ? SEE_PIECE_VALUE[(int) toType] : 0;
        if (moveType == Move::PROMOTION) {
            balance += SEE_PIECE_VALUE[(int) move.promotionType()] - SEE_PIECE_VALUE[0];
        }

        // Best case fails to beat threshold
        if (balance < 0) {
            return false;
        }

        balance -= SEE_PIECE_VALUE[(int) nextVictim];
        if (balance >= 0) { // Guaranteed to beat the threshold if the balance
                            // is still positive even after the exchange
            return true;
        }

        Bitboard diagPieces = pieces(TYPE_BISHOP) | pieces(TYPE_QUEEN);
        Bitboard orthPieces = pieces(TYPE_ROOK) | pieces(TYPE_QUEEN);
        Bitboard occupied   = occ();

        // Suppose that move was actually made
        occupied = occupied ^ Bitboard::fromSquare(from) ^ Bitboard::fromSquare(to);
        if (moveType == Move::ENPASSANT) {
            occupied &= ~Bitboard::fromSquare(enpassantSq());
        }

        // Get all attackers to that square
        Bitboard attackers = attacks::attackers(*this, WHITE, to, occupied) |
                             attacks::attackers(*this, BLACK, to, occupied);
        attackers &= occupied;

        Color color = ~this->sideToMove();

        while (true) {
            // If we have no more attackers, we lose material
            Bitboard myAttackers = attackers & us(color);
            if (myAttackers.empty()) {
                break;
            }

            // Find least valuable attacker
            for (PieceType pt :
                 {TYPE_PAWN, TYPE_KNIGHT, TYPE_BISHOP, TYPE_ROOK, TYPE_QUEEN, TYPE_KING}) {
                nextVictim = pt;
                if (attackers & pieces(pt, color)) {
                    break;
                }
            }

            // Remove this attacker from the occupied bitboard
            occupied &=
                ~Bitboard::fromSquare(Square((myAttackers & pieces(nextVictim, color)).lsb()));

            if (nextVictim == TYPE_PAWN || nextVictim == TYPE_BISHOP || nextVictim == TYPE_QUEEN) {
                attackers |= (attacks::bishop(to, occupied) & diagPieces);
            }
            if (nextVictim == TYPE_ROOK || nextVictim == TYPE_QUEEN) {
                attackers |= (attacks::rook(to, occupied) & orthPieces);
            }

            attackers &= occupied;
            color   = ~color;
            balance = -balance - 1 - SEE_PIECE_VALUE[(int) nextVictim];

            if (balance >= 0) { // We win material if the balance is
                                // non-negative after the exchanges
                if (nextVictim == TYPE_KING && (attackers & us(color))) {
                    // If we are attacking king and we still have attackers, we
                    // still win
                    color = ~color;
                }
                break;
            }
        }

        return sideToMove() != color;
    }

    /**
     * Check for draw by repetition, insufficient material, or fifty-move rule.
     */
    bool isDraw() const { //
        return isHalfMoveDraw() || isInsufficientMaterial() || isRepetition();
    }

    /**
     * Check if a move is actually legal. So far we have no good solutions
     * except for checking if the move is in the legal move list.
     */
    template <movegen::MoveGenType mt = movegen::MoveGenType::ALL>
    bool isLegal(const Move& move) const {
        const auto pt = at(move.from()).type();
        Movelist   moves;
        movegen::legalmoves<mt>(moves, *this, 1 << (int) pt);
        return std::find(moves.begin(), moves.end(), move) != moves.end();
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
    return std::max(std::abs(a.file() - b.file()), std::abs(a.rank() - b.rank()));
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
        bool aIsCorner = (a == Square::underlying::SQ_A1) || (a == Square::underlying::SQ_H1) ||
                         (a == Square::underlying::SQ_A8) || (a == Square::underlying::SQ_H8);
        bool bIsCorner = (b == Square::underlying::SQ_A1) || (b == Square::underlying::SQ_H1) ||
                         (b == Square::underlying::SQ_A8) || (b == Square::underlying::SQ_H8);
        if (aIsCorner || bIsCorner) {
            return 4;
        }
    }
    int m = std::ceil(std::max(std::max(dx / 2.0, dy / 2.0), (dx + dy) / 3.0));
    return m + ((m + dx + dy) % 2);
}
} // namespace dist

} // namespace chess