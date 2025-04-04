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
#include <thread>

// #define DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define DEBUG(...)                                                             \
    do {                                                                       \
        std::cerr << __FILE__ << ":" << __LINE__ << " - " << __VA_ARGS__       \
                  << "\n";                                                     \
    } while (0);
#else
#define DEBUG(...)
#endif

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
    uint16_t pvIndex = 0;
    bool nullMoveAllowed = true;
};

struct SearchStatistics {
    uint32_t nodes = 0;
    uint16_t depth = 1;
    uint16_t seldepth = 1;
};

struct SearchResult {
    Value score = VALUE_NONE;
    int16_t depth = -1;
    int16_t seldepth = -1;
    uint32_t nodes = 0;
    uint16_t time = 0;
    uint16_t bestmove = 0;
    const uint16_t *pvTable = nullptr;

    static SearchResult from(const SearchStatistics &stats,
                             const TimeControl &tc, Value bestValue,
                             Move bestMove, const uint16_t *pvTable) {
        SearchResult item;
        item.depth = stats.depth;
        item.seldepth = stats.seldepth;
        item.nodes = stats.nodes;
        item.score = bestValue;
        item.bestmove = bestMove.move();
        item.time = tc._elapsed();
        item.pvTable = pvTable;
        return item;
    }

    void print() const {
        std::cout << "info depth " << depth << " score " << score << " nodes "
                  << nodes << " seldepth " << seldepth << " time " << time
                  << " pv " << Move(bestmove);
        std::cout << std::endl;
    }
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
    bool searchInterrupted = false;

    uint16_t pvTable[(MAX_PLY * MAX_PLY + MAX_PLY) / 2]; // triangular array

public:
    Searcher(const Position &pos) : pos(pos) {
    }

    void search(SearchResult &result, TimeControl timeControl) {
        // Reset all local variables
        bestMoveCurr = Move::NO_MOVE;
        bestEvalCurr = VALUE_NONE;
        history.clear();
        stats = SearchStatistics();
        for (size_t i = 0; i < MAX_PLY; ++i) {
            stack[i] = Scratchpad();
        }
        searchInterrupted = false;

        this->tc = timeControl;

        MovePicker mp(pos);
        mp.init();

        // Handle no legal move
        if (mp.size() == 0) {
            result = SearchResult::from(stats, tc, DRAW_VALUE, Move::NO_MOVE,
                                        pvTable);
            std::cerr << "info string no legal moves" << std::endl;
            return;
        }
        // We don't have to waste time searching if there is only one reply in
        // competition
        if (tc.competitionMode && mp.size() == 1) {
            Value staticEval = evaluate(pos);
            result =
                SearchResult::from(stats, tc, staticEval, mp.pick(), pvTable);
            return;
        }

        Value alpha = Value::matedIn(0);
        Value beta = Value::mateIn(0);
        Value bestEvalRoot = Value::none();
        Move bestMoveRoot = Move::NO_MOVE;

        while (!hasReachedSoftLimit() && stats.depth < MAX_PLY) {
            // Reset variable for this iteration
            bestMoveCurr = Move::NO_MOVE;
            bestEvalCurr = VALUE_NONE;
            const Value score =
                negamax<PVNode>(alpha, beta, stats.depth, 0, false);
            DEBUG("search finished");

            if (bestMoveCurr.isValid() && !searchInterrupted) {
                DEBUG("0");
                // Update evaluation stability statistics
                history.updateStability(bestEvalRoot, bestEvalCurr);
                bestMoveRoot = bestMoveCurr;
                bestEvalRoot = bestEvalCurr;
                DEBUG("1");
                // Output the information about this iteration
                SearchResult::from(stats, tc, bestEvalCurr, bestMoveRoot.move(),
                                   pvTable)
                    .print();
                DEBUG("2");
            }
            if (hasReachedHardLimit()) {
                break;
            }
            if (tc.competitionMode && score.isMate()) {
                break;
            }

            stats.depth++;
        }

        if (!bestMoveRoot.isValid()) {
            std::cout << "info string no move was found\n";
            bestMoveRoot = mp.pick();
        }

        result =
            SearchResult::from(stats, tc, bestEvalRoot, bestMoveRoot, pvTable);
    }

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

        DEBUG("negamax(alpha=" << (int)alpha << ", beta=" << (int)beta
                               << ", depth=" << depth << ", ply=" << ply
                               << ", cutnode=" << cutnode << ")");
        stats.nodes++;

        // If running out of time, return immediately
        if (hasReachedHardLimit()) {
            return alpha;
        }
        // Go into quiescence search if no more plys are left to search
        if (depth <= 0) {
            return qsearch<NT>(alpha, beta, 8, ply);
        }
        // Draw detection
        if (ply > 0 && isDraw()) {
            return DRAW_VALUE;
        }
        /**
         * Mate Distance Pruning.
         */
        alpha = std::max(alpha, Value::matedIn(ply));
        beta = std::min(beta, Value::mateIn(ply));
        if (alpha >= beta) {
            return alpha;
        }

        history.killerTable[ply + 1].clear();

        const uint16_t pvIndex = stack[ply].pvIndex;
        pvTable[pvIndex] = Move::NO_MOVE;
        stack[ply + 1].pvIndex = pvIndex + MAX_PLY - ply;

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
            isInEntryBound(ttEntry, alpha, beta);
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

        DEBUG("static eval finished, eval=" << staticEval);

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
                DEBUG("reverse futility pruned");
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
                if (nullMoveValue >= beta) {
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

        MovePicker mp(pos);
        mp.init(&history.killerTable[ply], &history.historyTable);

        while (true) {
            Move m = mp.pick();
            if (!m.isValid()) {
                break; // no more moves
            }

            movesSearched++;
            DEBUG("  pick move " << m);

            // TODO pruning and reduction goes here

            pos.makeMove(m);
            Value score = VALUE_NONE;

            // Principal Variation Search
            if (movesSearched == 1) {
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
                searchInterrupted = true;
                return alpha;
            }

            if (score > bestValue) {
                DEBUG("score " << score << " replaced " << bestValue
                               << " for move " << m);
                bestValue = score;
            }

            if (score > alpha) {
                bestMove = m;
                alpha = score;
                ttEntryType = EntryType::EXACT;

                if (isPVNode && ply > 0) {
                    // New PV node, update PV table
                    pvTable[pvIndex] = m.move();
                    uint16_t n = MAX_PLY - ply - 1;
                    uint16_t *target = pvTable + pvIndex + 1;
                    uint16_t *source = pvTable + stack[ply + 1].pvIndex;
                    while (n-- && (*target++ = *source++))
                        ;
                }

                if (ply == 0) { // Root node
                    bestEvalCurr = score;
                    bestMoveCurr = m;
                }

                if (score >= beta) {
                    ttEntryType = EntryType::LOWER_BOUND;
                    break; // cutoff
                }
            }
        }

        DEBUG("<< move loop finished");

        if (movesSearched == 0 && inCheck) {
            return Value::matedIn(ply);
        }

        if (!eligibleTTPrune) {
            tt.store(pos, ttEntryType, depth, bestMove, bestValue);
        }
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
        DEBUG("qsearch(alpha=" << (int)alpha << ", beta=" << (int)beta
                               << ", depth=" << depth << ", ply=" << ply
                               << ")");
        // If running out of time, return immediately
        if (hasReachedHardLimit()) {
            searchInterrupted = true;
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

        Value staticEval = VALUE_NONE;
        if (!inCheck) {
            staticEval = evaluate(pos);
            if (staticEval >= beta) {
                return staticEval;
            }
            if (staticEval > alpha) {
                alpha = staticEval;
            }
        }

        Value bestValue = alpha;

        while (true) {
            Move m = mp.pick();
            if (!m.isValid()) {
                break;
            }
            movesSearched++;

            // TODO add pruning!

            pos.makeMove(m);
            const Value v = -qsearch<NT>(-beta, -alpha, depth - 1, ply + 1);
            pos.unmakeMove(m);

            if (v > bestValue) {
                DEBUG("in qsearch, move " << m << ": " << v << " replaced "
                                          << bestValue);
                bestValue = v;
            }
            if (v > alpha) {
                alpha = v;
                if (v >= beta) {
                    break;
                }
            }
        }

        if (movesSearched == 0 && inCheck) {
            return Value::matedIn(ply);
        }
        DEBUG("qsearch quit with " << bestValue);
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

    bool isInEntryBound(const TTEntry *entry, Value alpha, Value beta) {
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

Searcher *searcher = nullptr;

void think(SearchParams params, const Position pos) {
    SearchResult result;
    while (searcher != nullptr) { // wait until searcher is released
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    searcher = new Searcher(pos);

    const Color stm = pos.sideToMove();
    const TimePoint tp = TimeControl::now();
    const TimeControl tc = TimeControl(stm, params, tp);
    std::cout << "info string tc " << tc.hardTimeWall << " " << tc.softTimeWall
              << "\n";
    searcher->search(result, tc);

    std::cout << "bestmove " << chess::uci::moveToUci(Move(result.bestmove))
              << std::endl;
    std::cout << std::flush;

    delete searcher;
    searcher = nullptr; // release
}

void stopThinking() {
    if (searcher != nullptr) {
        searcher->abortSearch();
    }
}