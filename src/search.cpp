#include "search.h"
#include "eval.h"
#include "history.h"
#include "movepick.h"
#include "timecontrol.h"
#include "tt.h"
#include "types.h"

#include <array>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

/**
 * Global flag to signal the search thread to stop. This should be
 * checked frequently during the search function.
 */
std::atomic<bool> g_stopRequested(false);
TimeControl       g_timeControl;

namespace {

/**
 * Statistics for the search.
 */
struct SearchStats {
    uint64_t nodes    = 0;
    int      selDepth = 0;
};

/**
 * Entry in the search stack.
 */
struct SearchStackEntry {
    Value staticEval = VALUE_NONE;
    Move  bestMove;
    Move  excludedMove;
    bool  inCheck     = false;
    bool  canNullMove = true;
};
using SearchStack = std::array<SearchStackEntry, MAX_PLY>;

std::thread searchThread;

// Global variables =====================================================================
SearchStats   searchStats;
SearchStack   searchStack;
SearchHistory searchHistory;

std::vector<Move> extractPv(Position pos, int maxDepth = 64) {
    std::vector<Move> pv;
    pv.reserve(maxDepth);
    for (int d = 0; d < maxDepth; ++d) {
        TTEntry* e = tt.probe(pos);
        if (!e || !e->hasInitialized() || e->zobrist != pos.hash())
            break;
        Move m = e->move();
        if (m.move() == 0 || !pos.isLegal(m))
            break;
        pv.push_back(m);
        pos.makeMove(m);
    }
    return pv;
}

/**
 * Quiescence search. Search for quiet positions to yield a better evaluation.
 */
Value qsearch(Position& pos, int depth, int ply, Value alpha, Value beta) {
    // Exit immediately on stop requests only
    if (g_stopRequested.load()) {
        return alpha;
    }
    // Update statistics
    searchStats.nodes++;
    searchStats.selDepth = std::max(searchStats.selDepth, ply);
    // Draw detection
    if (pos.isDraw()) {
        return DRAW_VALUE;
    }

    Value standPat  = evaluate(pos);
    Value bestScore = standPat;

    // Force stop on maximum depth
    if (depth <= 0) {
        return standPat;
    }
    // Early pruning
    if (bestScore >= beta) {
        return bestScore;
    }
    if (bestScore > alpha) {
        alpha = bestScore;
    }

    MovePicker mp(pos, searchHistory, ply, Move::NO_MOVE, true);

    while (true) {
        const Move m = mp.next();
        if (m.move() == Move::NO_MOVE) {
            break;
        }

        pos.makeMove(m);
        Value score = -qsearch(pos, depth - 1, ply + 1, -beta, -alpha);
        pos.unmakeMove(m);

        if (score >= beta) {
            return score;
        }
        if (score > bestScore) {
            bestScore = score;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    return bestScore;
}

/**
 * Negamax search algorithm
 *
 * @param pos The current position
 * @param depth The current search depth (plys to search)
 * @param ply The current ply (plys searched so far)
 * @param alpha Lower bound for the score (We are at least this good)
 * @param beta Upper bound for the score (We are at most this good)
 */
template <bool isPV>
Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, bool cutnode) {
    // Exit immediately on timeouts or stop requests
    if (g_timeControl.hitHardLimit(depth, searchStats.nodes) || g_stopRequested.load()) {
        return alpha;
    }

    assert(isPV || (alpha == beta - 1));
    assert(!(isPV && cutnode));
    const bool isRoot  = (ply == 0);
    const bool inCheck = pos.inCheck();

    // Quiescence search
    if (depth <= 0 && !inCheck) {
        return qsearch(pos, 8, ply + 1, alpha, beta);
    }

    // Draw detection
    if (ply > 0 && pos.isDraw()) {
        return DRAW_VALUE;
    }

    // Mate distance pruning
    alpha = std::max(alpha, Value::matedIn(ply));
    beta  = std::min(beta, Value::mateIn(ply));
    if (alpha >= beta) {
        return alpha;
    }

    // Set up working environment
    SearchStackEntry* currSS = &searchStack[ply];
    SearchStackEntry* prevSS = ply > 0 ? &searchStack[ply - 1] : nullptr;
    currSS->inCheck          = inCheck;

    searchHistory.killerTable[ply + 1].clear();
    searchStats.nodes++;

    // Transposition table lookup
    // See if this node has been visited before. If so, we can reuse the data
    // if this isn't a PV node; if not, we can still use part of the data.
    const TTEntry* ttEntry         = tt.probe(pos);
    const bool     ttHit           = ttEntry != nullptr;
    const uint16_t ttMoveCode      = ttHit ? ttEntry->move_code : 0;
    const int      ttRequiredDepth = depth + (isPV ? 2 : 0);
    bool           ttPruned        = false;
    if (!isRoot && ttHit                        // if there is an entry
        && ttEntry->depth >= ttRequiredDepth    // with reliably high depth
        && (ttEntry->value <= alpha || cutnode) // okay to perform beta cutoff
    ) {
        const auto ttType    = ttEntry->type;
        const bool isBounded = ttEntry->value.isValid() &&
                               ((ttType == EntryType::EXACT) ||
                                (ttType == EntryType::UPPER_BOUND && ttEntry->value <= alpha) ||
                                (ttType == EntryType::LOWER_BOUND && ttEntry->value >= beta));
        if (isBounded) {
            if (!isPV) {
                return ttEntry->value; // in non-PV nodes we can safely return the value
            } else {
                depth--; // in PV nodes, reduce search depth
                ttPruned = true;
            }
        }
    }

    // Static evaluation
    Value staticEval   = inCheck ? VALUE_NONE : evaluate(pos);
    currSS->staticEval = staticEval;

    // Pre-move-loop pruning
    // If static evaluation is a fail-high or fail-low, we can likely prune
    // without doing any further work.
    if (!isPV && !inCheck) {
        // Null move pruning
        if (depth >= 6                                       // enough depth
            && currSS->canNullMove                           // prev move not null move
            && staticEval >= beta                            // value is too strong
            && (!ttHit || cutnode || ttEntry->value >= beta) //
            && pos.hasNonPawnMaterial()                      // avoid zugzwang in endgame
        ) {
            int r                            = 2 + depth / 3;
            searchStack[ply + 1].canNullMove = false; // disable null move for next ply

            pos.makeNullMove();
            Value score = -negamax<false>(pos, depth - r, ply + 1, -beta, -beta + 1, !cutnode);
            pos.unmakeNullMove();

            searchStack[ply + 1].canNullMove = true; // restore

            if (score >= beta) {
                // do not report unverified mate scores
                return score.isMate() ? beta : score;
            }
        }
    }

    Move      bestMove     = Move::NO_MOVE;
    Value     bestScore    = MATED_VALUE;
    EntryType ttFlag       = EntryType::UPPER_BOUND;
    int       moveSearched = 0;

    MovePicker mp(pos, searchHistory, ply, ttMoveCode, false);

    while (true) {
        Move m = mp.next();
        if (m.move() == Move::NO_MOVE) {
            break;
        }
        moveSearched++;

        // todo reductions and prunings

        pos.makeMove(m);
        // todo implement principal variation search
        Value score = -negamax<true>(pos, depth - 1, ply + 1, -beta, -alpha, false);
        pos.unmakeMove(m);

        // Stop searching if time control is hit
        if (g_timeControl.hitHardLimit(depth, searchStats.nodes) || g_stopRequested.load()) {
            return alpha;
        }

        // Update search status
        if (score > bestScore) {
            bestScore = score;
        }
        if (score > alpha) {
            bestMove         = m;
            alpha            = score;
            ttFlag           = EntryType::EXACT;
            currSS->bestMove = m;
            if (score >= beta) {
                ttFlag = EntryType::LOWER_BOUND;
                break;
            }
        }
    }

    if (moveSearched == 0) {
        return inCheck ? Value::matedIn(ply) : DRAW_VALUE;
    }

    if (!ttPruned) {
        tt.store(pos, ttFlag, depth, bestMove, bestScore);
    }

    return bestScore;
}

void searchWorker(SearchParams params, Position pos) {
    g_stopRequested.store(false);
    searchStats = SearchStats();
    searchStack.fill(SearchStackEntry {});
    tt.incGeneration();

    g_timeControl = TimeControl(pos.sideToMove(), params, TimeControl::now());
    int maxDepth  = params.depth > 0 ? (int) params.depth : 64;

    Move  rootBestMove;
    Value rootBestScore = MATED_VALUE;

    for (int depth = 1; depth <= maxDepth; ++depth) {
        if (g_stopRequested.load())
            break;
        if (g_timeControl.hitSoftLimit(depth, (int) searchStats.nodes, 0))
            break;

        Value score = negamax<true>(pos, depth, 0, MATED_VALUE, MATE_VALUE, false);
        if (g_stopRequested.load())
            break;

        auto pv = extractPv(pos, depth);
        if (!pv.empty())
            rootBestMove = pv.front();
        rootBestScore = score;

        std::cout << "info depth " << depth << " score " << score << " nodes " << searchStats.nodes
                  << " seldepth " << searchStats.selDepth << " pv";
        for (auto& m : pv)
            std::cout << " " << m;
        std::cout << std::endl;

        if (g_timeControl.hitSoftLimit(depth, (int) searchStats.nodes, 0))
            break;
    }

    if (rootBestMove.move() != 0)
        std::cout << "bestmove " << rootBestMove << std::endl;
    else
        std::cout << "bestmove 0000" << std::endl;
}

} // namespace

void think(SearchParams params, const Position pos) {
    if (searchThread.joinable()) {
        g_stopRequested.store(true);
        searchThread.join();
    }
    g_stopRequested.store(false);
    searchStats = SearchStats();
    searchStack.fill(SearchStackEntry {});
    searchThread = std::thread(searchWorker, params, pos);
}

void stopThinking() {
    g_stopRequested.store(true);
    if (searchThread.joinable())
        searchThread.join();
}
