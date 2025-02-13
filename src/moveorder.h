#pragma once

#include "position.h"
#include "types.h"
#include <vector>

enum class OrderMode { DEFAULT, QUIET };

/**
 * MVV-LVA table, indexed by [aggressor][victim]. This table is
 * adapted from https://open-chess.org/viewtopic.php?t=3058.
 */
// clang-format off
constexpr int16_t MVV_LVA_TABLE[7][7] = {
    //   P     N     B     R     Q      K none   
    {    2,  225,  250,  400,  800,  900,    0},  // P
    { -125,    4,   25,  175,  575,  675,    0},  // N
    { -250,  -25,    6,  150,  550,  650,    0},  // B
    { -400, -175, -150,    8,  400,  500,    0},  // R
    { -800, -575, -550, -400,   10,  100,    0},  // Q
    { -900, -880, -880, -860, -840,    0,    0},  // K
    {    0,    0,    0,    0,    0,    0,    0},  // none
};
// clang-format on
constexpr int16_t MOVE_ORDERING_CHECK_BONUS = 100;
constexpr int16_t MOVE_ORDERING_PROMOTION_BONUS = 500;
constexpr int16_t MOVE_ORDERING_BAD_SQUARE_PENALTY = 300;
constexpr int16_t MOVE_ORDERING_KILLER_BONUS = 150;
constexpr int16_t MOVE_ORDERING_PV_BONUS = 2000;

/**
 * `MoveOrderer` orders moves to search for a position.
 */
template <OrderMode mode>
class MoveOrderer {
  private:
    std::vector<Move> buffer;
    KillerHeuristics _killers;
    Move _pvMoveFromIteration;
    size_t pointer;
    OrderMode type;

    void scoreMoves(Position &pos) {
        for (Move &m : buffer) {
            int16_t score = 0;
            /**
             * Most Valuable Victim, Least Valuable Aggressor (MVV/LVA)
             *
             * Prioritize captures that take high-value pieces over low-value
             * pieces, eg. prefer searching PxR over PxB.
             *
             * Reference: https://www.chessprogramming.org/MVV-LVA
             */
            // Implementation specific: we do not check if this move really
            // is a capture here, as the table will apply no penalty/bonus
            // if there is nothing to capture.
            PieceType victimType = pos.at<PieceType>(m.to());
            PieceType aggressorType = pos.at<PieceType>(m.from());
            score += MVV_LVA_TABLE[(int)aggressorType][(int)victimType];
            // Bonus for checks
            bool isCheckMove = pos.isCheckMove(m);
            if (isCheckMove) {
                score += MOVE_ORDERING_CHECK_BONUS;
            }
            // Bonus for queen promotions and knight promotions with check
            if ((m.typeOf() == Move::PROMOTION) &&
                ((m.promotionType() == TYPE_QUEEN) ||
                 (isCheckMove && (m.promotionType() == TYPE_KNIGHT)))) {
                score += MOVE_ORDERING_PROMOTION_BONUS;
            }
            // Penalty for non-pawns moving to squares protected by enemy pawn.
            Square sq = m.to();
            if (aggressorType != TYPE_PAWN &&
                (chess::attacks::pawn(pos.sideToMove(), sq) &
                 pos.pieces(TYPE_PAWN, ~pos.sideToMove()))) {
                score -= MOVE_ORDERING_BAD_SQUARE_PENALTY;
            }
            // Killer move heuristic and history heuristic
            if (!pos.isCapture(m)) {
                if (_killers.has(m)) {
                    score += MOVE_ORDERING_KILLER_BONUS;
                }
            }
            // Bonus for PV move from last iteration
            if (_pvMoveFromIteration.isValid() && _pvMoveFromIteration == m) {
                score += MOVE_ORDERING_PV_BONUS;
            }
            m.setScore(score);
        }
    }
    void sortMoves() {
        // Sort the moves in descending order of score
        std::sort(buffer.begin(), buffer.end(),
                  [&](const Move &a, const Move &b) {
                      return a.score() > b.score();
                  });
    }

  public:
    MoveOrderer() {
        // Set generation type
        this->type = mode;
        pointer = 0;
    }

    inline void add(Move move) {
        buffer.push_back(move);
    }

    inline void init(Position &pos, const KillerHeuristics *killers = nullptr,
                     const Move *pvMoveFromIteration = nullptr) {
        // Generate legal moves
        Movelist legalmoves = pos.legalMoves();
        buffer.reserve(buffer.size() + legalmoves.size());
        for (int i = 0; i < legalmoves.size(); i++) {
            bool keep = false;

            // Additional check if needed for quiescence use. In this case,
            // only keep captures and promotions.
            if constexpr (mode == OrderMode::QUIET) {
                keep =
                    pos.inCheck() || // Always keep check evasions
                    (legalmoves[i].typeOf() == Move::PROMOTION || // Promotions
                     pos.isCapture(legalmoves[i])); // Capture moves
            } else {
                keep = true; // Normal move ordering
            }

            if (keep)
                buffer.push_back(legalmoves[i]);
        }
        // Add killers
        if (killers) {
            _killers = *killers;
        }
        // Add pv move
        if (pvMoveFromIteration) {
            _pvMoveFromIteration = *pvMoveFromIteration;
        } else {
            _pvMoveFromIteration = Move::NO_MOVE;
        }
        scoreMoves(pos);
        sortMoves();
    }

    inline Move get() {
        return pointer < buffer.size() ? buffer[pointer++]
                                       : Move(Move::NO_MOVE);
    }
    inline size_t size() const {
        return buffer.size();
    }
};