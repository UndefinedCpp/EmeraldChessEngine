#include "movepick.h"
#include <algorithm>

// clang-format off
constexpr int16_t MVV_LVA_TABLE[7][7] = {
    //         P     N     B     R     Q      K   none
    /* P */{   0,  200,  250,  450,  900,     0,     0},
    /* N */{-200,   10,   50,  250,  700,     0,     0},
    /* B */{-250,  -50,    5,  200,  650,     0,     0},
    /* R */{-450, -250, -200,   15,  450,     0,     0},
    /* Q */{-900, -700, -650, -450,   20,     0,     0},
    /* K */{   0,    0,    0,    0,    0,     0,     0},
    /*   */{   0,    0,    0,    0,    0,     0,     0},
};

constexpr int16_t CHECK_BONUS = 200;
constexpr int16_t PROMOTION_BONUS = 200;
// clang-format on

Move MovePicker::next() {
    switch (stage) {
        case MovePickerStage::TT: {
            // Generate TT move. First check if such move is legal.
            // If not, simply ignore it.
            Move ttMove = Move(ttMoveCode);
            if (ttMove.isValid() && pos.isLegal(ttMove)) {
                stage = MovePickerStage::GEN_NOISY;
                return ttMove;
            }
        }
            [[fallthrough]];

        case MovePickerStage::GEN_NOISY:
            // Generate noisy moves
            generateNoisyMoves();
            stage = MovePickerStage::GOOD_NOISY;
            [[fallthrough]];

        case MovePickerStage::GOOD_NOISY:
            // Pick a good noisy move
            while (!noisyBuffer.empty()) {
                const auto& scoredMove = noisyBuffer.back();
                if (scoredMove.score < 0) { // not a good move anymore
                    break;                  // we are done
                }
                noisyBuffer.pop_back();
                if (scoredMove.moveCode == ttMoveCode) {
                    continue; // do not yield the same move twice
                }
                return scoredMove.move();
            }
            stage = MovePickerStage::KILLER_1;
            [[fallthrough]];

        case MovePickerStage::KILLER_1: {
            // Killer moves are never tactic moves, so they should never appear
            // in the noisy buffer.
            const Move killer1 = history.killerTable[ply].killer1;
            if (killer1.isValid() && pos.isLegal<movegen::MoveGenType::QUIET>(killer1)) {
                stage = MovePickerStage::KILLER_2;
                return killer1;
            }
        }
            [[fallthrough]];

        case MovePickerStage::KILLER_2: {
            const Move killer2 = history.killerTable[ply].killer2;
            if (killer2.isValid() && pos.isLegal<movegen::MoveGenType::QUIET>(killer2)) {
                stage = MovePickerStage::GEN_QUIET;
                return killer2;
            }
        }
            [[fallthrough]];

        case MovePickerStage::GEN_QUIET:
            if (_skipQuiet) { // Maybe used in pruning
                stage = MovePickerStage::END_NORMAL;
            } else {
                generateQuietMoves();
                stage = MovePickerStage::GOOD_QUIET;
            }
            [[fallthrough]];

        case MovePickerStage::GOOD_QUIET:
            while (!quietBuffer.empty()) {
                const auto& scoredMove = quietBuffer.back();
                if (scoredMove.score < 0) { // not a good move anymore
                    break;                  // we are done
                }
                quietBuffer.pop_back();
                if (scoredMove.moveCode == ttMoveCode ||
                    scoredMove.moveCode == history.killerTable[ply].killer1 ||
                    scoredMove.moveCode == history.killerTable[ply].killer2) {
                    continue; // do not yield the same move twice
                }
                return scoredMove.move();
            }
            stage = MovePickerStage::BAD_NOISY;
            [[fallthrough]];

        case MovePickerStage::BAD_NOISY:
            while (!noisyBuffer.empty()) {
                const auto& scoredMove = noisyBuffer.back();
                noisyBuffer.pop_back();
                if (scoredMove.moveCode == ttMoveCode) {
                    continue;
                }
                return scoredMove.move();
            }
            stage = MovePickerStage::BAD_QUIET;
            [[fallthrough]];

        case MovePickerStage::BAD_QUIET:
            while (!quietBuffer.empty()) {
                const auto& scoredMove = quietBuffer.back();
                quietBuffer.pop_back();
                if (scoredMove.moveCode == ttMoveCode ||
                    scoredMove.moveCode == history.killerTable[ply].killer1 ||
                    scoredMove.moveCode == history.killerTable[ply].killer2) {
                    continue;
                }
                return scoredMove.move();
            }
            stage = MovePickerStage::END_NORMAL;
            [[fallthrough]];

        case MovePickerStage::END_NORMAL:
            return Move(Move::NO_MOVE);

        case MovePickerStage::GEN_QSEARCH:
            if (inCheck) {
                generateEvasionMoves();
            } else {
                generateNoisyMoves();
            }
            stage = MovePickerStage::GOOD_QSEARCH;
            [[fallthrough]];

        case MovePickerStage::GOOD_QSEARCH:
            while (!noisyBuffer.empty()) {
                const auto& scoredMove = noisyBuffer.back();
                if (!inCheck && scoredMove.score < 0) {
                    break;
                }
                noisyBuffer.pop_back();
                if (scoredMove.moveCode == ttMoveCode) {
                    continue;
                }
                return scoredMove.move();
            }
            stage = MovePickerStage::END_QSEARCH;
            [[fallthrough]];

        case MovePickerStage::END_QSEARCH:
            return Move(Move::NO_MOVE);

        default:
            return Move(Move::NO_MOVE);
    }
}

void MovePicker::skipQuiet() {
    _skipQuiet = true;
}

void MovePicker::generateNoisyMoves() {
    Movelist noisyMoves;
    movegen::legalmoves<movegen::MoveGenType::CAPTURE>(noisyMoves, pos);
    // Assign scores to moves based on MVV/LVA & SEE
    for (const Move& move : noisyMoves) {
        int16_t         score    = 0;
        const auto      fromSq   = move.from();
        const auto      toSq     = move.to();
        const PieceType attacker = pos.at(fromSq).type();
        const PieceType victim =
            (move.typeOf() == Move::ENPASSANT) ? PieceType::PAWN : pos.at(toSq).type();
        const int16_t mvvlva = MVV_LVA_TABLE[(int) attacker][(int) victim];
        if (pos.see(move, 0)) { // static exchange evaluation indicates an acceptable capture
            score = mvvlva;
            // Additional bonus for checks
            if (pos.isCheckMove(move)) {
                score += CHECK_BONUS;
            }
        } else { // a losing capture
            score = mvvlva - 1000;
        }

        noisyBuffer.emplace_back(ScoredMove {move.move(), score});
    }
    // Sort moves by score in ascending order
    std::sort(noisyBuffer.begin(), noisyBuffer.end());
}

void MovePicker::generateQuietMoves() {
    Movelist quietMoves;
    movegen::legalmoves<movegen::MoveGenType::QUIET>(quietMoves, pos);

    for (const Move& move : quietMoves) {
        int16_t score = 0;
        // Bonus for checks
        if (pos.isCheckMove(move)) {
            score += CHECK_BONUS;
        }
        // Promotion bonus
        if (move.typeOf() == Move::PROMOTION && move.promotionType() == PieceType::QUEEN) {
            score += PROMOTION_BONUS;
        }
        // Penalty for moving into squares controlled by opponent pawns
        if (pos.at(move.from()).type() != PieceType::PAWN &&
            (attacks::pawn(pos.sideToMove(), move.to()) &
             pos.pieces(PieceType::PAWN, ~pos.sideToMove()))) {
            score -= 200;
        }

        quietBuffer.emplace_back(ScoredMove {move.move(), score});
    }
    std::sort(quietBuffer.begin(), quietBuffer.end());
}

void MovePicker::generateEvasionMoves() {
    Movelist moves;
    movegen::legalmoves(moves, pos);
    for (const Move& move : moves) {
        noisyBuffer.emplace_back(ScoredMove {move.move(), 0});
    }
}
