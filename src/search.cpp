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
#define DEBUG(...)                                                                                 \
    do {                                                                                           \
        std::cerr << __FILE__ << ":" << __LINE__ << " - " << __VA_ARGS__ << "\n";                  \
    } while (0);
#else
#define DEBUG(...)
#endif

namespace {

constexpr int MAX_QSEARCH_DEPTH = 12;

enum NodeType {
    PVNode,
    NonPVNode,
};

struct Scratchpad {
    Value    staticEval      = Value::none();
    uint16_t currentMove     = 0;
    uint16_t bestMove        = 0;
    uint16_t pvIndex         = 0;
    bool     nullMoveAllowed = true;
};

struct SearchStatistics {
    uint32_t nodes    = 0;
    uint16_t depth    = 1;
    uint16_t seldepth = 1;
};

struct SearchResult {
    Value           score    = VALUE_NONE;
    int16_t         depth    = -1;
    int16_t         seldepth = -1;
    uint32_t        nodes    = 0;
    uint16_t        time     = 0;
    uint16_t        bestmove = 0;
    const uint16_t* pvTable  = nullptr;

    static SearchResult from(
        const SearchStatistics& stats,
        const TimeControl&      tc,
        Value                   bestValue,
        Move                    bestMove,
        const uint16_t*         pvTable) {
        SearchResult item;
        item.depth    = stats.depth;
        item.seldepth = stats.seldepth;
        item.nodes    = stats.nodes;
        item.score    = bestValue;
        item.bestmove = bestMove.move();
        item.time     = tc._elapsed();
        item.pvTable  = pvTable;
        return item;
    }

    void print() const {
        std::cout << "info depth " << depth << " score " << score << " nodes " << nodes
                  << " seldepth " << seldepth << " time " << time << " pv " << Move(bestmove);
        std::cout << std::endl;
    }
};

int  LMR_TABLE[256][256];
void initLMRTable() {
    for (int i = 0; i < 256; ++i) {
        for (int j = 0; j < 256; ++j) {
            float r         = 0.5f + std::log(i) * std::log(j) / 2.5f;
            LMR_TABLE[i][j] = static_cast<int>(r);
        }
    }
}

/**
 * This class holds the context and algorithm needed to perform a search.
 * Specifically, it implements the negamax algorithm within the iterative
 * deepening (ID) and principle variation search (PVS) frameworks.
 */
class Searcher {
private:
    Move  bestMoveCurr;
    Value bestEvalCurr;

    SearchHistory    history;
    Scratchpad       stack[MAX_PLY];
    SearchStatistics stats;

    Position    pos;
    TimeControl tc;

    bool searchAborted     = false;
    bool searchInterrupted = false;

    uint16_t pvTable[(MAX_PLY * MAX_PLY + MAX_PLY) / 2]; // triangular array

public:
    Searcher(const Position& pos) : pos(pos) {}

    void search(SearchResult& result, TimeControl timeControl) {
        // Reset all local variables
        bestMoveCurr = Move::NO_MOVE;
        bestEvalCurr = VALUE_NONE;
        history.clear();
        stats = SearchStatistics();
        for (size_t i = 0; i < MAX_PLY; ++i) {
            stack[i] = Scratchpad();
        }
        searchInterrupted = false;
        updateEvaluatorState(pos);

        this->tc = timeControl;

        MovePicker mp(pos);
        mp.init();

        // Handle no legal move
        if (mp.size() == 0) {
            result = SearchResult::from(stats, tc, DRAW_VALUE, Move::NO_MOVE, pvTable);
            std::cerr << "info string no legal moves" << std::endl;
            return;
        }
        // We don't have to waste time searching if there is only one reply in
        // competition
        if (tc.competitionMode && mp.size() == 1) {
            updateEvaluatorState(pos); // refresh evaluator
            Value staticEval = evaluate(pos);
            result           = SearchResult::from(stats, tc, staticEval, mp.pick(), pvTable);
            return;
        }

        Value alpha        = Value::matedIn(0);
        Value beta         = Value::mateIn(0);
        Value window       = 18;
        Value bestEvalRoot = Value::none();
        Move  bestMoveRoot = Move::NO_MOVE;

        while (!hasReachedSoftLimit() && stats.depth < MAX_PLY) {
            // Reset variable for this iteration
            bestMoveCurr      = Move::NO_MOVE;
            bestEvalCurr      = VALUE_NONE;
            const Value score = negamax<PVNode>(alpha, beta, stats.depth, 0, false);

            if (bestMoveCurr.isValid() && !searchInterrupted) {
                // Update evaluation stability statistics
                history.updateStability(bestEvalRoot, bestEvalCurr);
                bestMoveRoot = bestMoveCurr;
                bestEvalRoot = bestEvalCurr;
                // Output the information about this iteration
                SearchResult::from(stats, tc, bestEvalCurr, bestMoveRoot.move(), pvTable).print();
            }
            if (hasReachedHardLimit()) {
                break;
            }
            if (tc.competitionMode && score.isMate()) {
                break;
            }

            // Aspiration window
            if (stats.depth >= 3) {
                if (score <= alpha) {
                    beta   = (alpha + beta) / 2;
                    alpha  = alpha - window;
                    window = window * 2;
                    continue;
                }
                if (score >= beta) {
                    beta += window;
                    window = window * 2;
                    continue;
                }
                window = 15;
                alpha  = score - window;
                beta   = score + window;
            }

            stats.depth++;
        }

        if (!bestMoveRoot.isValid()) {
            std::cout << "info string no move was found\n";
            bestMoveRoot = mp.pick();
        }

        result = SearchResult::from(stats, tc, bestEvalRoot, bestMoveRoot, pvTable);
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
        constexpr bool isPVNode   = NT == PVNode;
        const bool     isRootNode = ply == 0;
        const bool     inCheck    = pos.inCheck();

        stats.nodes++;

        // If running out of time, return immediately
        if (hasReachedHardLimit()) {
            return evaluate(pos);
        }
        // Go into quiescence search if no more plys are left to search
        if (depth <= 0) {
            return qsearch<NT>(alpha, beta, 10, ply);
        }
        // Draw detection
        if (ply > 0 && isDraw()) {
            return DRAW_VALUE;
        }
        /**
         * Mate Distance Pruning.
         */
        alpha = std::max(alpha, Value::matedIn(ply));
        beta  = std::min(beta, Value::mateIn(ply));
        if (alpha >= beta) {
            return alpha;
        }

        // history.killerTable[ply + 1].clear();

        const uint16_t pvIndex = stack[ply].pvIndex;
        pvTable[pvIndex]       = Move::NO_MOVE;
        stack[ply + 1].pvIndex = pvIndex + MAX_PLY - ply;

        /**
         * Transposition Table Probing.
         *
         * If a position is reached before and its evaluation is stored in the
         * table, we can potentially reuse the result from previous searches.
         */
        const TTEntry* ttEntry       = tt.probe(pos);
        const bool     ttHit         = ttEntry != nullptr;
        const bool     enoughTTDepth = ttHit && (ttEntry->depth >= depth);
        const Move     ttMove        = ttHit ? Move(ttEntry->move_code) : Move::NO_MOVE;

        const bool eligibleTTPrune = !isRootNode && ttHit && enoughTTDepth && !isPVNode &&
                                     isInEntryBound(ttEntry, alpha, beta);
        if (eligibleTTPrune) {
            return ttEntry->value;
        }

        // Static evaluation. This guides pruning and reduction.
        Value staticEval      = inCheck ? VALUE_NONE : evaluate(pos);
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
            if (depth <= 7 && !alpha.isMate() && staticEval >= beta + futilityMargin) {
                return staticEval;
            }

            /**
             * Null Move Pruning.
             *
             * In almost all chess positions, making a null move (passing a
             * turn) is worse than the best legal move.
             */
            if (stack[ply].nullMoveAllowed && // last ply wasn't a null move
                depth >= 3 && staticEval >= beta && (!ttHit || cutnode || ttEntry->value >= beta) &&
                pos.hasNonPawnMaterial() // zugzwang can break things
            ) {
                stack[ply + 1].nullMoveAllowed = false; // disable NMP on next ply
                const int reduction            = 3 + depth / 4;

                pos.makeNullMove();
                Value nullMoveValue =
                    -negamax<NonPVNode>(-beta, -beta + 1, depth - reduction, ply + 1, !cutnode);
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
        Move      bestMove      = Move::NO_MOVE;
        Value     bestValue     = VALUE_NONE;
        EntryType ttEntryType   = EntryType::UPPER_BOUND;
        int       movesSearched = 0;

        MovePicker mp(pos);
        const Move hashMove = (enoughTTDepth || cutnode) ? ttMove : Move::NO_MOVE;
        mp.init(&history.killerTable[ply], &history.historyTable, hashMove);

        while (true) {
            if (hasReachedHardLimit()) {
                searchInterrupted = true;
                return alpha;
            }

            Move m = mp.pick();
            if (!m.isValid()) {
                break; // no more moves
            }
            const bool moveGivesCheck = pos.isCheckMove(m);
            const bool moveCaptures   = pos.isCapture(m);

            movesSearched++;

            // TODO pruning and reduction goes here
            bool fullSearchRequired = false;

            // Late Move Reduction
            const int lmrMinDepth = isPVNode ? 4 : 3;
            if (movesSearched >= 3 && depth >= lmrMinDepth && !inCheck && cutnode) {
                int reduction = LMR_TABLE[depth][movesSearched];
                if (moveCaptures || moveGivesCheck) { // tactic moves are likely to raise alpha
                    reduction = reduction / 2;
                }
                if (!isPVNode) {
                    reduction += 1;
                }
                reduction = std::clamp(reduction, 1, depth - 1);
                depth -= reduction;
            }

            // SEE Pruning
            const int seePruneThreshold =
                -(20 + ((moveCaptures || moveGivesCheck) ? 24 * depth * depth : 40 * depth));
            if (!isPVNode && !isRootNode && depth <= 8 && movesSearched > 1 &&
                !pos.see(m, seePruneThreshold)) {
                continue;
            }

            updateEvaluatorState(pos, m);
            pos.makeMove(m);
            stack[ply].currentMove = m.move();
            Value score            = VALUE_NONE;

            // Principal Variation Search
            if (movesSearched == 1) {
                score = -negamax<NT>(-beta, -alpha, depth - 1, ply + 1, !isPVNode && !cutnode);
            } else {
                // Perform null window search
                score = -negamax<NonPVNode>(-alpha - 1, -alpha, depth - 1, ply + 1, !cutnode);
                if (score > alpha && score < beta) {
                    // re-search with full window
                    score = -negamax<PVNode>(-beta, -alpha, depth - 1, ply + 1, false);
                }
            }

            pos.unmakeMove(m);
            updateEvaluatorState();
            stack[ply].currentMove = 0;

            if (score > bestValue) {
                bestValue = score;
            }

            if (score > alpha) {
                bestMove    = m;
                alpha       = score;
                ttEntryType = EntryType::EXACT;

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
    template <NodeType NT> Value qsearch(Value alpha, Value beta, int depth, int ply) {
        constexpr bool isPVNode = NT == PVNode;
        // If running out of time, return immediately
        if (hasReachedHardLimit()) {
            searchInterrupted = true;
            return alpha;
        }
        // Return if we are going too deep
        if (depth <= 0 || ply >= MAX_PLY) {
            return pos.inCheck() ? DRAW_VALUE : evaluate(pos);
        }
        // Draw detection
        if (isDraw()) {
            return DRAW_VALUE;
        }
        // Update selective depth
        if (isPVNode && ply > stats.seldepth) {
            stats.seldepth = ply;
        }
        // At non-PV nodes we perform early TT cutoff
        if (!isPVNode) {
            TTEntry* entry = tt.probe(pos);
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

        DEBUG("qsearch: " << pos.getFen());
        while (true) {
            Move m = mp.pick();
            if (!m.isValid()) {
                break;
            }
            movesSearched++;

            // TODO add pruning!
            const Move prevMove    = ply > 0 ? Move::NO_MOVE : stack[ply - 1].currentMove;
            const bool isRecapture = prevMove.isValid() && prevMove.to() == m.to();

            // SEE Pruning
            // DEBUG("Try SEE Pruning..." << m);
            if (!inCheck && !isRecapture && !pos.see(m, -6)) {
                continue;
            }

            updateEvaluatorState(pos, m);
            pos.makeMove(m);
            stack[ply].currentMove = m.move();

            const Value v = -qsearch<NT>(-beta, -alpha, depth - 1, ply + 1);

            pos.unmakeMove(m);
            updateEvaluatorState();
            stack[ply].currentMove = 0;

            if (v > bestValue) {
                bestValue = v;
                if (v > alpha) {
                    alpha = v;
                }
                if (v >= beta) {
                    break;
                }
            }
        }

        if (movesSearched == 0 && inCheck) {
            return Value::matedIn(ply);
        }
        return bestValue;
    }

    void abortSearch() { searchAborted = true; }

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
        return pos.isHalfMoveDraw() || pos.isRepetition() || pos.isInsufficientMaterial();
    }

    bool isInEntryBound(const TTEntry* entry, Value alpha, Value beta) {
        assert(entry != nullptr);
        return entry->type == EntryType::EXACT ||
               (entry->type == EntryType::UPPER_BOUND && entry->value <= alpha) ||
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

Searcher* searcher = nullptr;

void think(SearchParams params, const Position pos) {
    SearchResult result;
    while (searcher != nullptr) { // wait until searcher is released
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    searcher = new Searcher(pos);
    initLMRTable();

    const Color       stm = pos.sideToMove();
    const TimePoint   tp  = TimeControl::now();
    const TimeControl tc  = TimeControl(stm, params, tp);
    std::cout << "info string tc " << tc.hardTimeWall << " " << tc.softTimeWall << "\n";
    searcher->search(result, tc);

    std::cout << "bestmove " << chess::uci::moveToUci(Move(result.bestmove)) << std::endl;
    std::cout << std::flush;

    delete searcher;
    searcher = nullptr; // release
}

void stopThinking() {
    if (searcher != nullptr) {
        searcher->abortSearch();
    }
}

// info depth 10 score cp 66 nodes 4328501 seldepth 17 time 11070 pv d2d4
// info depth 10 score cp 66 nodes 705977 seldepth 19 time 1267 pv d2d4
// info depth 10 score cp 62 nodes 752441 seldepth 19 time 1243 pv d2d4
// (2025/4/19) > Branching factor: 4.609287
//                     3.844846 [2025/4/19]
// >   Renegade 0.7.0  4.480959  (1655)
// >   Stockfish 17    2.270066