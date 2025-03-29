#include "search.h"
#include "eval.h"
#include "evalconstants.h"
#include "history.h"
#include "movepick.h"
#include "timecontrol.h"
#include "tt.h"

#include <chrono>
#include <iomanip>
#include <iostream>

namespace {

constexpr int MAX_QSEARCH_DEPTH = 8;

enum NodeType {
    PVNode,
    NonPVNode,
};

struct Scratchpad {
    Value staticEval = Value::none();
    uint16_t currentMove = 0;
    uint16_t bestMove = 0;
    bool nullMoveAllowed = true;
};

struct SearchStatistics {
    uint32_t nodes;
    uint16_t depth;
    uint16_t seldepth;
};

/**
 * This class holds the context and algorithm needed to perform a search.
 * Specifically, it implements the negamax algorithm within the iterative
 * deepening (ID) and principle variation search (PVS) frameworks.
 */
class Searcher {
private:
    Move bestMoveCurr;
    Value bestEvalCurr;

    SearchHistory history;
    Scratchpad stack[MAX_PLY];
    SearchStatistics stats;

    Position pos;
    TimeControl tc;

    bool searchAborted = false;

public:
    /**
     * Negamax search.
     *
     * @param alpha   The score guaranteed so far. (lower bound)
     * @param beta    The score we can get at most. (upper bound)
     * @param depth   Plys left to search.
     * @param ply     Plys searched.
     * @param cutnode Whether this node is an expected cutnode. (fail-high node)
     */
    template <NodeType NT>
    Value negamax(Value alpha, Value beta, int depth, int ply, bool cutnode) {
        constexpr bool isPVNode = NT == PVNode;
        const bool isRootNode = ply == 0;
        const bool inCheck = pos.inCheck();

        // If running out of time, return immediately
        if (hasReachedHardLimit()) {
            return alpha;
        }
        // Go into quiescence search if no more plys are left to search
        if (depth <= 0 && !inCheck) {
            return qsearch<NT>(alpha, beta, depth, ply);
        }
        // Draw detection
        if (ply > 0 && isDraw()) {
            return DRAW_VALUE;
        }

        /**
         * Mate Distance Pruning.
         */
        alpha = std::max(alpha, Value::matedIn(ply));
        beta = std::min(beta, Value::mateIn(ply - 1));
        if (alpha >= beta) {
            return alpha;
        }

        history.killerTable[ply + 1].clear();

        /**
         * Transposition Table Probing.
         *
         * If a position is reached before and its evaluation is stored in the
         * table, we can potentially reuse the result from previous searches.
         */
        const TTEntry *ttEntry = tt.probe(pos);
        const bool ttHit = ttEntry != nullptr;
        const Move ttMove = ttHit ? Move::NO_MOVE : Move(ttEntry->move_code);

        const bool eligibleTTPrune = 
            !isRootNode && ttHit &&
            // Ensure sufficient depth, and even higher for PV nodes
            (ttEntry->depth >= (depth + (isPVNode ? 2 : 0))) &&
            (ttEntry->value <= alpha || cutnode) &&
            isInEntryBound(ttEntry, alpha, beta));
        if (eligibleTTPrune) {
            if (!isPVNode) { // at non-PV nodes, safely prune the node
                return ttEntry->value;
            } else { // but at PV nodes, just reduce the depth
                depth--;
            }
        }

        // Static evaluation. This guides pruning and reduction.
        Value staticEval = VALUE_NONE;
        if (!inCheck) {
            staticEval = evaluate(pos);
        }
        stack[ply].staticEval = staticEval;

        const bool improving = isImproving(ply, staticEval);

        // Pruning before looping over the moves.
        if (!isPVNode && !inCheck) {
            /**
             * Reverse Futility Pruning.
             *
             * If the static evaluation is far above beta, it is likely
             * to fail high.
             */
            const Value futilityMargin = std::max(depth, 0) * 75;
            if (depth <= 7 && !alpha.isMate() &&
                staticEval >= beta + futilityMargin) {
                return beta + (staticEval - beta) / 3;
            }

            /**
             * Null Move Pruning.
             *
             * In almost all chess positions, making a null move (passing a
             * turn) is worse than the best legal move.
             */
            if (stack[ply].nullMoveAllowed && // last ply wasn't a null move
                depth >= 3 && staticEval >= beta &&
                (!ttHit || cutnode || ttEntry->value >= beta) &&
                pos.hasNonPawnMaterial() // zugzwang can break things
            ) {
                stack[ply + 1].nullMoveAllowed =
                    false; // disable NMP on next ply
                const int reduction = 2 + depth / 3;

                pos.makeNullMove();
                Value nullMoveValue = -negamax<NonPVNode>(
                    -beta, -beta + 1, depth - reduction, ply + 1, !cutnode);
                pos.unmakeNullMove();

                stack[ply + 1].nullMoveAllowed = true; // reset
                if (score >= beta) {
                    // TODO perform verification search at high depth
                    // Do not return unproven mates
                    return nullMoveValue.isMate() ? beta : nullMoveValue;
                }
            }
        }

        // Now we have decided to search this node.
        Move bestMove = Move::NO_MOVE;
        Value bestValue = VALUE_NONE;
        EntryType ttEntryType = EntryType::UPPER_BOUND;
        int movesSearched = 0;

        MovePicker mp;
        mp.init(&history.killerTable, &history.historyTable);

        while (true) {
            Move m = mp.pick();
            if (!m.isValid()) {
                break; // no more moves
            }

            // TODO pruning and reduction goes here

            pos.makeMove(m);
            Value score = VALUE_NONE;

            // Principal Variation Search
            if (movesSearched == 0) {
                score = -negamax<NT>(-beta, -alpha, depth - 1, ply + 1,
                                     !isPVNode && !cutnode);
            } else {
                // Perform null window search
                score = -negamax<NonPVNode>(-alpha - 1, -alpha, depth - 1,
                                            ply + 1, !cutnode);
                if (score > alpha && score < beta) {
                    // re-search with full window
                    score = -negamax<PVNode>(-beta, -alpha, depth - 1, ply + 1,
                                             false);
                }
            }

            pos.unmakeMove(m);

            if (hasReachedHardLimit()) {
                return alpha;
            }

            if (score > bestValue) {
                bestValue = score;
            }

            if (score > alpha) {
                bestMove = m;
                alpha = score;
                ttEntryType = EntryType::EXACT;

                if (ply == 0) {
                    bestEvalCurr = score;
                    bestMoveCurr = bestMove;
                }

                if (score >= beta) {
                    ttEntryType = EntryType::LOWER_BOUND;
                    break; // cutoff
                }
            }

            movesSearched++;
        }

        if (movesSearched == 0 && inCheck) {
            return Value::matedIn(ply);
        }

        tt.store(pos, ttEntryType, depth, bestMove, bestValue);
        return bestValue;
    }

    /**
     * Quiescence search.
     *
     * The purpose of this search is to only evaluate "quiet" positions, or
     * positions where there are no winning tactical moves to be made. This
     * search is needed to avoid the horizon effect.
     *
     * See https://www.chessprogramming.org/Quiescence_Search.
     */
    template <NodeType NT>
    Value qsearch(Value alpha, Value beta, int depth, int ply) {
        constexpr bool isPVNode = NT == PVNode;
        // If running out of time, return immediately
        if (hasReachedHardLimit()) {
            return alpha;
        }
        // Draw detection
        if (isDraw()) {
            return DRAW_VALUE;
        }
        // Return if we are going too deep
        if (depth <= 0 || ply >= MAX_PLY) {
            return pos.inCheck() ? DRAW_VALUE : evaluate(pos);
        }
        // Update selective depth
        if (isPVNode && ply > stats.seldepth) {
            stats.seldepth = ply;
        }
        // At non-PV nodes we perform early TT cutoff
        if (!isPVNode) {
            TTEntry *entry = tt.probe(pos);
            if (entry && isInEntryBound(entry, alpha, beta)) {
                return entry->value;
            }
        }

        const bool inCheck = pos.inCheck();

        MovePicker mp(pos);
        if (inCheck) {
            mp.init(nullptr, &history.historyTable); // Always evade checks
        } else {
            mp.initQuiet(&history.historyTable);
        }

        int movesSearched = 0;
        Value bestValue = MATED_VALUE;
        while (true) {
            Move m = mp.pick();
            if (!m.isValid()) {
                break;
            }

            // TODO add pruning!

            pos.makeMove(m);
            const Value v = -qsearch<NT>(-beta, -alpha, depth - 1, ply + 1);
            pos.unmakeMove(m);

            if (v > bestValue) {
                bestValue = v;
            }
            if (v > alpha) {
                alpha = v;
                if (v >= beta) {
                    break;
                }
            }

            movesSearched++;
        }

        if (movesSearched == 0 && inCheck) {
            return Value::matedIn(ply);
        }

        return bestValue;
    }

    void abortSearch() {
        searchAborted = true;
    }

private:
    bool hasReachedHardLimit() {
        if (searchAborted) {
            return true;
        }
        return tc.hitHardLimit(stats.depth, stats.nodes);
    }

    bool hasReachedSoftLimit() {
        if (searchAborted) {
            return true;
        }
        return tc.hitSoftLimit(stats.depth, stats.nodes, history.evalStability);
    }

    /**
     * Test if the current position is a draw by repetition, insufficient
     * material or fifty moves rule. This does not check for stalemate or
     * checkmate, even though in edge cases one's 50th move could be a
     * checkmate.
     */
    bool isDraw() {
        return pos.isHalfMoveDraw() || pos.isRepetition() ||
               pos.isInsufficientMaterial();
    }

    bool isInEntryBound(TTEntry *entry, Value alpha, Value beta) {
        assert(entry != nullptr);
        return entry->type == EntryType::EXACT ||
               (entry->type == EntryType::UPPER_BOUND &&
                entry->value <= alpha) ||
               (entry->type == EntryType::LOWER_BOUND && entry->value >= beta);
    }

    /**
     * Determine the improving heuristic.
     */
    bool isImproving(int ply, Value eval) {
        if (!eval.isValid()) { // currently in check
            return false;
        }
        if (ply < 2) {
            return false; // no history to compare to
        }
        const Value eval2 = stack[ply - 2].staticEval;
        return eval2 < eval;
    }
};

}; // namespace

/**
 * The entry point of the search.
 */
void think(Position &pos, SearchParams &params) {
    return;
}
