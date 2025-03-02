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

namespace {

enum EvalColor { White, Black, ColorNum = 2 };
enum EvalPieceType {
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King,
    PieceTypeNum = 6,
    AllPieceTypes = 6,
};
constexpr Color COLOR_MAPPING[ColorNum] = {WHITE, BLACK};
constexpr PieceType PIECE_TYPE_MAPPING[PieceTypeNum] = {
    TYPE_PAWN, TYPE_KNIGHT, TYPE_BISHOP, TYPE_ROOK, TYPE_QUEEN, TYPE_KING};

// Shortcut for opponent color
#define _C (C == White ? Black : White)

/**
 * Evaluator class, implements various evaluation aspects.
 */
class Evaluator {
  public:
    Evaluator(Position &pos) : pos(pos) {
    }

  private:
    Position &pos;

    struct {
        Score nonPawnMaterial[ColorNum];
        Score psqt[ColorNum];
        Bitboard attackedBy[ColorNum][PieceTypeNum + 1];
        int kingAttackersCount[ColorNum];
        int kingAttackersWeight[ColorNum];
        int kingAttackSquares[ColorNum];
        Bitboard kingRing[ColorNum];
        Bitboard mobilityArea[ColorNum];
        Bitboard passedPawns[ColorNum];
    } cache;

    /**
     * Count material. This initializes the basic material values in
     * cache, and should always be called before proceeding to any other
     * evaluation.
     */
    template <EvalColor C>
    void _countMaterial() {
        for (PieceType pt : {TYPE_PAWN, TYPE_KNIGHT, TYPE_BISHOP, TYPE_ROOK,
                             TYPE_QUEEN, TYPE_KING}) {
            Bitboard bb = pos.pieces(pt, C);
            // Count non pawn material
            if (pt != TYPE_PAWN) {
                cache.nonPawnMaterial[C] += PIECE_VALUE[pt] * bb.count();
            }
            // Count piece square table score
            while (bb) {
                Square sq = bb.pop();
                uint8_t index = (C == White ? sq.index() : sq.flip().index());
                cache.psqt[C] += PIECE_SQUARE_TABLES[pt][index];
            }
        }
    }

    template <EvalColor C>
    void _init() {
        // Initialize attack map
        cache.attackedBy[C][AllPieceTypes] = 0;
        for (PieceType pt : {TYPE_PAWN, TYPE_KNIGHT, TYPE_BISHOP, TYPE_ROOK,
                             TYPE_QUEEN, TYPE_KING}) {
            Bitboard bb = pos.pieces(pt, C);
            cache.attackedBy[C][pt] = 0; // init bitboard
            while (bb) {
                Square sq = bb.pop();
                cache.attackedBy[C][pt] |= pos.getAttackMap(sq);
            }
            cache.attackedBy[C][AllPieceTypes] |= cache.attackedBy[C][pt];
        }
        // Initialize king safety
        cache.kingAttackersCount[C] = 0;
        cache.kingAttackersWeight[C] = 0;
        cache.kingAttackSquares[C] = 0;
        cache.kingRing[C] = KING_RING_BB[pos.kingSq(C).index()];
        // Initialize mobility area. Mobility areas are all squares except:
        // - those attacked by the enemy pawns
        // - king & queen squares
        // - unmoved friendly pawns
        cache.mobilityArea[C] = ~cache.attackedBy[_C][Pawn];
        cache.mobilityArea[C] &= ~pos.pieces(TYPE_KING, C);
        cache.mobilityArea[C] &= ~pos.pieces(TYPE_QUEEN, C);
        cache.mobilityArea[C] &=
            ~(pos.pieces(TYPE_PAWN, C) &
              Bitboard(C == White ? chess::Rank::RANK_2 : chess::Rank::RANK_7));
    }

    /**
     * Evaluates a type of piece for a given color.
     */
    template <EvalColor C, EvalPieceType PT>
    Score _piece() {
        Bitboard bb = pos.pieces(PIECE_TYPE_MAPPING[PT], COLOR_MAPPING[C]);
        Score total;

        while (bb) {
            const Square sq = bb.pop();
            const Bitboard sqbb = 1 << sq.index();
            // Find attacked squares, including x-ray attacks.
            const Bitboard attackMap =
                PT == Bishop ? chess::attacks::bishop(
                                   sq, pos.occ() ^ pos.pieces(TYPE_QUEEN))
                : PT == Queen
                    ? chess::attacks::queen(sq, pos.occ() ^
                                                    pos.pieces(TYPE_QUEEN) ^
                                                    pos.pieces(TYPE_ROOK))
                    : pos.getAttackMap(sq);
            // Update king attackers info
            if (attackMap & cache.kingRing[_C]) {
                cache.kingAttackersCount[C]++;
                cache.kingAttackersWeight[C] += KING_ATTACKER_WEIGHT[PT];
                cache.kingAttackSquares[C] +=
                    (attackMap & cache.kingRing[_C]).count();
            }
            // Update mobility score
            int mobility = (attackMap & cache.mobilityArea[C]).count();
            total += MOBILITY_BONUS[PT][mobility];

            if (PT == Knight || PT == Bishop) {
                // Bonus for being on a outpost square
                const Bitboard outpostMask =
                    OUTPOST_SQUARES[C] & (~cache.attackedBy[_C][Pawn]);
                if (outpostMask & sqbb) {
                    bool supported = cache.attackedBy[C][Pawn] & sqbb;
                    total += OUTPOST_BONUS[PT == Bishop][supported];
                }

                if (PT == Bishop) {
                    // Penalty for having too many pawns on the same color
                    // square as the bishop
                    Bitboard pawns = pos.pieces(TYPE_PAWN, C);
                    int counter = 0;
                    while (pawns) {
                        counter += Square::same_color(Square(pawns.pop()), sq);
                    }
                    total -= BISHOP_PAWN_PENALTY * counter;
                }
            }

            if (PT == Rook) {
                // Bonus for being on a (semi-)open file
                const Bitboard fileMask = Bitboard(sq.file());
                if (pos.pieces(TYPE_PAWN, C) & fileMask == 0) {
                    if (pos.pieces(TYPE_PAWN, _C) & fileMask == 0) { // open
                        total += OPEN_ROOK_BONUS[1];
                    } else { // semi-open
                        total += OPEN_ROOK_BONUS[0];
                    }
                }
                // Penalty for being trapped by the king, and even more
                // if the king cannot castle
                if (attackMap.count() <= 3) {
                    chess::File f = sq.file();
                    chess::File kingFile = pos.kingSq(C).file();
                    if ((f > chess::File::FILE_E &&
                         kingFile >= chess::File::FILE_E) ||
                        (f < chess::File::FILE_D &&
                         kingFile <= chess::File::FILE_D)) {
                        total -= TRAPPED_ROOK_PENALTY;
                        if (!pos.castlingRights().has(C)) {
                            total -= TRAPPED_ROOK_PENALTY;
                        }
                    }
                }
            }
        }

        return total;
    }

    template <EvalColor C>
    Score _king() {
        int kingDanger =
            cache.kingAttackersCount[C] * cache.kingAttackersWeight[C] +
            20 * cache.kingAttackSquares[C];

        Score total = Score(kingDanger * kingDanger / 6144, kingDanger / 24);
        return ~total;
    }

    /**
     * Evaluate pawn structures for a given color.
     */
    template <EvalColor C>
    Score _pawns() {
        const Bitboard ours = pos.pieces(TYPE_PAWN, C);
        const Bitboard theirs = pos.pieces(TYPE_PAWN, _C);
        Bitboard pawns = pos.pieces(TYPE_PAWN, C);
        Score total;

        cache.passedPawns[C].clear();

        while (pawns) {
            Square sq = pawns.pop();

            // Get properties of the pawn
            const Bitboard neighbors = ours & NEIGHBOR_FILES_BB[(int)sq.file()];
            const Bitboard stoppers =
                theirs & PAWN_STOPPER_MASK[C == Black][sq.index()];
            const Bitboard phalanx = neighbors & sq.rank().bb();
            const Bitboard supported = ours & chess::attacks::pawn(_C, sq);
            const Bitboard doubled =
                ours &
                (C == White ? (sq.rank().bb() << 8) : (sq.rank().bb() >> 8));
            // Punish isolated pawns
            if (!neighbors) {
                total -= ISOLATED_PAWN_PENALTY;
            }
            // Punish doubled and unspported pawns
            if (doubled && !supported) {
                total -= DOUBLED_PAWN_PENALTY;
            }

            // Cache passed pawns so we can evaluate them later
            if (!stoppers && !doubled) {
                cache.passedPawns[C].set(sq.index());
            }
        }

        return total;
    }

    /**
     * Score passed pawns
     */
    template <EvalColor C>
    Score _passed() {
        Bitboard b = cache.passedPawns[C];
        Score total;
        while (b) {
            Square sq = b.pop();
            int distance = C == White ? (7 - sq.rank()) : ((int)sq.rank());
            total += PASSED_PAWN_BONUS[distance];
        }
        return total;
    }

    /**
     * Estimate game phase from material. This must be called after
     * `_check_material`.
     */
    int _phase() {
        int m = (cache.nonPawnMaterial[0] + cache.nonPawnMaterial[1]).mg;
        constexpr int ENDGAME_LIMIT = 1600;
        constexpr int MIDGAME_LIMIT = 7700;
        m = std::clamp(m, ENDGAME_LIMIT, MIDGAME_LIMIT);
        return ((m - ENDGAME_LIMIT) * ALL_GAME_PHASES) /
               (MIDGAME_LIMIT - ENDGAME_LIMIT);
    }

  public:
    Value operator()() {
        // First, check material and cache the results
        _countMaterial<White>();
        _countMaterial<Black>();
        Score total =
            cache.nonPawnMaterial[White] - cache.nonPawnMaterial[Black];
        total += cache.psqt[White] - cache.psqt[Black];
        total += PIECE_VALUE[Pawn] * (pos.countPieces(TYPE_PAWN, WHITE) -
                                      pos.countPieces(TYPE_PAWN, BLACK));
        // Initialize
        _init<White>();
        _init<Black>();
        // Evaluate different aspects of the position
        total += _piece<White, Knight>() - _piece<Black, Knight>();
        total += _piece<White, Bishop>() - _piece<Black, Bishop>();
        total += _piece<White, Rook>() - _piece<Black, Rook>();
        total += _piece<White, Queen>() - _piece<Black, Queen>();
        total += _pawns<White>() - _pawns<Black>();
        total += _passed<White>() - _passed<Black>();

        total += _king<White>() - _king<Black>();

        int phase = _phase();
        Value v = total.fuse(phase);

        if (pos.sideToMove() == BLACK) {
            v = ~v; // flip the score so it is always POV
        }
        // Give a tempo bonus to the side to move
        v += TEMPO_BONUS;

        return v;
    }
};

#undef _C

} // namespace

/**
 * Main evaluation function.
 */
Value evaluate(Position &pos) {
    return Evaluator(pos)();
}

/**
 * info depth 2 score cp 29 nodes    962 pv b1c3
 * info depth 4 score cp 46 nodes   6453 pv b1c3
 * info depth 6 score cp 57 nodes  48132 pv d2d4
 * info depth 8 score cp 60 nodes 305803 pv g1f3
 */