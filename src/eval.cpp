#include "eval.h"
#include "evalconstants.h"
#include <cmath>

std::pair<bool, Value> checkGameStatus(Position &board) {
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

// ============================================================================
// EVALUATOR CLASS
// ============================================================================

void Evaluator::_initialize() {
    Bitboard bb;
    /**
     * Initialize pawn attack bitboards.
     */
    bb = pos.pieces(TYPE_PAWN, WHITE);
    w_pawn_atk_bb = Bitboard(0ull);
    while (bb) {
        Square sq = Square(bb.pop());
        Bitboard a = chess::attacks::pawn(WHITE, sq);
        w_pawn_atk_bb |= a;
    }

    bb = pos.pieces(TYPE_PAWN, BLACK);
    b_pawn_atk_bb = Bitboard(0ull);
    while (bb) {
        Square sq = Square(bb.pop());
        Bitboard a = chess::attacks::pawn(BLACK, sq);
        b_pawn_atk_bb |= a;
    }
    /**
     * Initialize mobility area.
     */
    w_mobility_area = (~b_pawn_atk_bb) & (~pos.pieces(TYPE_PAWN, WHITE)) &
                      (~pos.pieces(TYPE_QUEEN, WHITE)) &
                      (~pos.pieces(TYPE_KING, WHITE));
    b_mobility_area = (~w_pawn_atk_bb) & (~pos.pieces(TYPE_PAWN, BLACK)) &
                      (~pos.pieces(TYPE_QUEEN, BLACK)) &
                      (~pos.pieces(TYPE_KING, BLACK));
    /**
     * Initialize king ring. Retrieve pre-computed bitboards and remove squares
     * that are double defended by pawns.
     */
    w_king_ring = KING_RING_BB[pos.kingSq(WHITE).index()];
    b_king_ring = KING_RING_BB[pos.kingSq(BLACK).index()];
    bb = w_king_ring;
    while (bb) {
        // We treat the square as if there is an opponent pawn on it, and see
        // if it attacks our pawns
        Square sq = Square(bb.pop());
        Bitboard atk = chess::attacks::pawn(BLACK, sq);
        int defenders = (atk & pos.pieces(TYPE_PAWN, WHITE)).count();
        if (defenders >= 2) {
            w_king_ring &= ~Bitboard::fromSquare(sq);
        }
    }
    bb = b_king_ring;
    while (bb) {
        Square sq = Square(bb.pop());
        Bitboard atk = chess::attacks::pawn(WHITE, sq);
        int defenders = (atk & pos.pieces(TYPE_PAWN, BLACK)).count();
        if (defenders >= 2) {
            w_king_ring &= ~Bitboard::fromSquare(sq);
        }
    }
    /**
     * Initialize passed pawns
     */
    bb = pos.pieces(TYPE_PAWN, WHITE);
    w_passed_pawns = 0;
    while (bb) {
        Square sq = Square(bb.pop());
        if ((PASSED_PAWN_DETECT_MASK[0][sq.index()] &
             pos.pieces(TYPE_PAWN, BLACK)) == 0) {
            w_passed_pawns |= Bitboard::fromSquare(sq);
        }
    }
    bb = pos.pieces(TYPE_PAWN, BLACK);
    b_passed_pawns = 0;
    while (bb) {
        Square sq = Square(bb.pop());
        if ((PASSED_PAWN_DETECT_MASK[1][sq.index()] &
             pos.pieces(TYPE_PAWN, WHITE)) == 0) {
            b_passed_pawns |= Bitboard::fromSquare(sq);
        }
    }
}

/**
 * `phase` determines the interpolation factor between middle and end games.
 */
int Evaluator::phase() {
    // Count non-pawn materials
    Value pts = PIECE_VALUE[1].mg * pos.countPieces(TYPE_PAWN) +
                PIECE_VALUE[2].mg * pos.countPieces(TYPE_KNIGHT) +
                PIECE_VALUE[3].mg * pos.countPieces(TYPE_BISHOP) +
                PIECE_VALUE[4].mg * pos.countPieces(TYPE_ROOK) +
                PIECE_VALUE[5].mg * pos.countPieces(TYPE_QUEEN);
    constexpr int MIDGAME_LIMIT = 10000;
    constexpr int ENDGAME_LIMIT = 1700;
    return (pts.value() - ENDGAME_LIMIT) * ALL_GAME_PHASES /
           (MIDGAME_LIMIT - ENDGAME_LIMIT);
}

/**
 * Scale endgame evaluation.
 */
int Evaluator::endgame_scale_factor() {
    return 1;
}

Value Evaluator::operator()() {
    Score total;
    total =
        // Pieces
        eval_component(_piece_value) + eval_component(_psqt) +
        // Mobility
        eval_component(_mobility) +
        // Space
        eval_component(_space) +
        // Pawns
        eval_component(_isolated_pawns) + eval_component(_doubled_pawns) +
        eval_component(_supported_pawns) + eval_component(_passed_pawns) +
        // Pieces
        eval_component(_bishop_pair) +
        // King
        eval_component(_king_attackers) + eval_component(_weak_king_squares);

    // Value is always from the side-to-move perspective
    if (pos.sideToMove() == BLACK) {
        total = ~total;
    }

    // Fuse the value
    int phase = this->phase();
    Value value = total.fuse(phase) + TEMPO_BONUS;
    // Adjust value with respect to 50 move rule
    int fiftyMoveRuleScaler = 100 - pos.halfMoveClock();
    int scaledValue = static_cast<int>(value) * fiftyMoveRuleScaler / 100;
    return Value(scaledValue);
}

/**
 * Evaluate piece value.
 */
__subeval Score Evaluator::_piece_value() const {
    return PIECE_VALUE[0] * pos.countPieces(TYPE_PAWN, __color) +
           PIECE_VALUE[1] * pos.countPieces(TYPE_KNIGHT, __color) +
           PIECE_VALUE[2] * pos.countPieces(TYPE_BISHOP, __color) +
           PIECE_VALUE[3] * pos.countPieces(TYPE_ROOK, __color) +
           PIECE_VALUE[4] * pos.countPieces(TYPE_QUEEN, __color);
}

__subeval Score Evaluator::_psqt() const {
    Score total;
    for (chess::PieceType pt : {TYPE_PAWN, TYPE_KNIGHT, TYPE_BISHOP, TYPE_ROOK,
                                TYPE_QUEEN, TYPE_KING}) {
        Bitboard bb = pos.pieces(pt, __color);
        while (bb) {
            Square sq = bb.pop();
            int index;
            white_specific index = sq.index();
            black_specific index = sq.flip().index(); // mirror value
            total = total + PIECE_SQUARE_TABLES[(int)pt][index];
        }
    }
    return total;
}

__subeval Score Evaluator::_mobility() const {
    const Bitboard candidate_mob_area =
        side ? w_mobility_area : b_mobility_area;
    Score total;
    for (chess::PieceType pt :
         {TYPE_KNIGHT, TYPE_BISHOP, TYPE_ROOK, TYPE_QUEEN}) {
        Bitboard bb = pos.pieces(pt, __color);
        while (bb) {
            Square sq = Square(bb.pop());
            Bitboard piece_mob_area = pos.getMotionMap(sq) & candidate_mob_area;
            int mobility_score = piece_mob_area.count();
            total = total + MOBILITY_BONUS[(int)pt][mobility_score];
        }
    }
    return total;
}

__subeval Score Evaluator::_space() const {
    // Candidate squares
    constexpr Bitboard SPACE_MASK =
        side ? 0x003c3c3c00000000ull : 0x000000003c3c3c00ull;
    // Drop bad squares
    Bitboard space;
    white_specific space =
        SPACE_MASK & (~b_pawn_atk_bb) & (~pos.pieces(TYPE_PAWN, WHITE));
    black_specific space =
        SPACE_MASK & (~w_pawn_atk_bb) & (~pos.pieces(TYPE_PAWN, BLACK));
    // Apply bonus
    Score total = SPACE_BONUS * space.count();
    return total;
}

__subeval Score Evaluator::_isolated_pawns() const {
    // Isolated pawns are pawns that have no neighbors. Get all pawns
    // first.
    const Bitboard pawns = pos.pieces(TYPE_PAWN, __color);
    Bitboard bb = pawns;

    Score total;
    while (bb) {
        Square sq = Square(bb.pop());
        const Bitboard mask = PAWN_NEIGHBORING_FILES[(int)sq.file()];
        if ((pawns & mask) == 0) { // No neighbors
            total = total + ISOLATED_PAWN_PENALTY;
        }
    }
    return total;
}

__subeval Score Evaluator::_doubled_pawns() const {
    // A pawn is considered a doubled pawn only if it is right in front of
    // another friendly pawn.
    const Bitboard pawns = pos.pieces(TYPE_PAWN, __color);
    // Loop through all pawns and check if there is another pawn in front
    Bitboard bb = pawns;
    Score total;
    while (bb) {
        Square sq = Square(bb.pop());
        Square frontSq;
        white_specific frontSq = sq + chess::Direction::NORTH;
        black_specific frontSq = sq + chess::Direction::SOUTH;
        if (pawns & Bitboard::fromSquare(frontSq)) {
            total = total + DOUBLED_PAWN_PENALTY;
        }
    }
    return total;
}

__subeval Score Evaluator::_supported_pawns() const {
    const Bitboard pawns = pos.pieces(TYPE_PAWN, __color);
    Bitboard bb = pawns;
    Score total;
    while (bb) {
        Square sq = Square(bb.pop());
        Bitboard supporting = (chess::attacks::pawn(__color, sq) & pawns);
        total = SUPPORTED_PAWN_BONUS * supporting.count();
    }
    return total;
}

__subeval Score Evaluator::_passed_pawns() const {
    Score total;
    Bitboard bb = side ? w_passed_pawns : b_passed_pawns;
    while (bb) {
        Square sq = Square(bb.pop());
        int distance = side ? (7 - sq.rank()) : ((int)sq.rank());
        total = total + PASSED_PAWN_BONUS[distance];
    }
    return total;
}

__subeval Score Evaluator::_passed_unblocked() const {
    Score total;
    Bitboard bb = side ? w_passed_pawns : b_passed_pawns;
    const Bitboard blockers = side ? pos.us(BLACK) : pos.us(WHITE);

    while (bb) {
        Square sq = Square(bb.pop());
        // Additional check for any blockers
        const Bitboard mask =
            PASSED_PAWN_DETECT_MASK[(int)__color][sq.index()] &
            Bitboard(sq.file());
        if ((mask & blockers) == 0) {
            int distance = side ? (7 - sq.rank()) : ((int)sq.rank());
            total = total + UNBLOCKED_PASSED_PAWN_BONUS[distance];
        }
    }
    return total;
}

__subeval Score Evaluator::_bishop_pair() const {
    return pos.pieces(TYPE_BISHOP, __color).count() >= 2 ? BISHOP_PAIR_BONUS
                                                         : S(0, 0);
}

__subeval Score Evaluator::_king_attackers() const {
    Bitboard kingRing = side ? w_king_ring : b_king_ring;
    Value total = 0;
    while (kingRing) {
        Square sq = Square(kingRing.pop());
        Bitboard attackersBB = chess::attacks::attackers(pos, ~__color, sq);
        total = total +
                (attackersBB & pos.pieces(TYPE_KNIGHT, ~__color)).count() *
                    KING_ATTACKER_WEIGHT[1] +
                (attackersBB & pos.pieces(TYPE_BISHOP, ~__color)).count() *
                    KING_ATTACKER_WEIGHT[2] +
                (attackersBB & pos.pieces(TYPE_ROOK, ~__color)).count() *
                    KING_ATTACKER_WEIGHT[3] +
                (attackersBB & pos.pieces(TYPE_QUEEN, ~__color)).count() *
                    KING_ATTACKER_WEIGHT[4];
    }

    return S(-total, -total / 4); // penalty
}

__subeval Score Evaluator::_weak_king_squares() const {
    Bitboard kingRing = side ? w_king_ring : b_king_ring;
    Value total = 0;
    while (kingRing) {
        Square sq = Square(kingRing.pop());
        Bitboard defenders =
            (pos.pieces(TYPE_QUEEN, __color) | pos.pieces(TYPE_KING, __color)) &
            chess::attacks::king(sq);
        if (defenders.count() <= 1) {
            total = total - WEAK_KING_SQUARE_PENALTY;
        }
    }
    return S(total, total / 4);
}