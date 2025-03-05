#include "search.h"
#include "eval.h"
#include "evalconstants.h"
#include "moveorder.h"
#include "timemgr.h"
#include "tt.h"

#include <chrono>
#include <iomanip>
#include <iostream>

constexpr int QSEARCH_DEPTH = 8; // can be tuned

template <NodeType NT>
Value Searcher::negamax(Scratchpad *sp, Value alpha, Value beta, int depth,
                        bool cutnode) {
    // Get basic information about the current node
    constexpr bool isPVNode = NT == PVNode;
    const bool isRootNode = PVNode && ((sp - 1)->ply == 0);

    assert(MATED_VALUE <= alpha && alpha <= MATE_VALUE);
    assert(MATED_VALUE <= beta && beta <= MATE_VALUE);
    assert(isPVNode || (alpha == beta - 1));
    assert(!(isPVNode || cutnode));

    uint16_t pv[MAX_PLY + 1];
    Value eval = DRAW_VALUE;
    Move bestMove = Move::NO_MOVE;

    /**
     * (1) Initialize the search, and return early on special cases
     */
    const bool isInCheck = pos.inCheck();
    Value bestValue = MATED_VALUE;
    sp->ply = (sp - 1)->ply + 1;

    if (searchCallCount > 1000) {
        searchCallCount = 0; // reset counter
        if (checkTime()) {   // abort immediately if time is up
            searchAborted = true;
            return 0;
        }
    }

    diagnosis.nodes++; // increase node counter
    if (isPVNode && (sp->ply > diagnosis.seldepth)) {
        diagnosis.seldepth = sp->ply; // update seldepth
    }

    if (!isRootNode) {
        if (pos.isHalfMoveDraw()) {
            auto [_, drawType] = pos.getHalfMoveDrawType();
            if (drawType == chess::GameResult::DRAW) {
                return DRAW_VALUE;
            } else {
                return Value::matedIn(sp->ply);
            }
        }
        if (pos.isRepetition()) {
            return DRAW_VALUE;
        }

        /**
         * (2) Mate Distance Pruning.
         *
         * If a forced mate has been found, cut trees and adjust bounds of lines
         * where no shorter mate is possible. This normally doesn't help improve
         * playing strength, but it still helps to find faster mate proofs.
         */
        alpha = std::max(alpha, Value::matedIn(sp->ply));
        beta = std::min(beta, Value::mateIn(sp->ply));
        if (alpha >= beta) {
            return alpha;
        }
    }

    /**
     * (3) Transposition Table lookup.
     */
    TTEntry *ttEntry = tt.probe(pos);
    const ttHit = ttEntry != nullptr;
    Value ttValue = DRAW_VALUE;
    if (ttHit) {
        ttValue =
            ttEntry->value >= MATE_VALUE_THRESHOLD    ? Value::mateIn(sp->ply)
            : ttEntry->value <= -MATE_VALUE_THRESHOLD ? Value::matedIn(sp->ply)
                                                      : ttEntry->value;
    }
    Move ttMove = ttHit ? ttEntry->move() : Move::NO_MOVE;

    if (!isPVNode                   // at non-PV nodes
        && ttHit                    // entry found
        && ttEntry->depth > sp->ply // deeper so more accurate
        && (ttValue >= beta ? (ttEntry->type == EntryType::UPPER_BOUND)
                            : (ttEntry->type == EntryType::LOWER_BOUND))) {
        return ttValue; // TT cutoff
    }
    if (isPVNode                                      // at PV nodes
        && ttHit && ttEntry->type == EntryType::EXACT // exact bound only
        && ttEntry->depth >= sp->ply                  // deeper analysis
    ) {
        return ttValue; // Exact position has been evaluated before
    }

    /**
     * (4) Static evaluation when not in check.
     */
    if (isInCheck) {
        goto loop;
    } else {
        sp->staticValue = eval = evaluate(pos);
    }

    if (sp->skipPruning)
        goto loop;

    /**
     * (5) Futility pruning.
     */
    if (!isRootNode                   // at child node
        && depth < 7                  // near frontier nodes
        && eval >= beta + depth * 100 // large margin
        && !isInCheck && pos.hasNonPawnMaterial()) {
        return eval;
    }

    /**
     * (6) Null move pruning
     */
    if (!isPVNode                   // at non-PV nodes
        && eval >= beta             //
        && depth >= 5               // ensure enough depth
        && pos.hasNonPawnMaterial() // prevent zugswang
    ) {
        pos.makeNullMove();
        (sp + 1)->skipPruning = true;
        Value nullVal =
            -negamax<NonPVNode>(sp + 1, -beta, -beta + 1, depth - 3, !cutnode);
        (sp + 1)->skipPruning = false;
        pos.unmakeNullMove();

        if (nullVal >= beta) {
            if (nullVal.isMate()) { // Could be unproven mate
                return beta;
            } else {
                return nullVal; // TODO verify value at high depth
            }
        }
    }

loop: // This label marks the beginning of the search loop
    /**
     * (7) Loop through all the moves
     */
    MoveOrderer<OrderMode::DEFAULT> orderer;
    orderer.init(pos, sp->killers, ttMove);

    int moveCount = 0;
    Move m;

    while ((m = orderer.get()) != Move::NO_MOVE) {
        moveCount++;
    }

    // Check for checkmate or stalemate
    if (moveCount == 0) {
        bestValue = isInCheck ? Value::matedIn(sp->ply) : DRAW_VALUE;
    }

    tt.store(pos,
             (bestValue >= beta)              ? EntryType::LOWER_BOUND
             : (PVNode && bestMove.isValid()) ? EntryType::EXACT
                                              : EntryType::UPPER_BOUND,
             depth, bestMove, bestValue);
    return bestValue;
}

std::pair<Move, Value> Searcher::search() {
    // Iterative deepening
    std::pair<Move, Value> result;
    TimeManager tm;
    maxThinkingTime = 0;

    if (params.wtime > 0 || params.btime > 0) {
        if (pos.sideToMove() == WHITE) {
            tm.update(params.wtime, params.winc);
        } else {
            tm.update(params.btime, params.binc);
        }
        maxThinkingTime = tm.spareTime(pos.fullMoveNumber());
    } else {
        maxThinkingTime = 5000;
    }

    const auto timeBegin = std::chrono::steady_clock::now();
    startTime = timeBegin;

    for (int depth = 1; depth <= 32; ++depth) {
        const auto timeEnd = std::chrono::steady_clock::now();
        const uint32_t timeElapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd -
                                                                  timeBegin)
                .count();
        if (timeElapsed > maxThinkingTime) {
            break;
        }

        result = negamax_root(depth, MATE_VALUE, MATED_VALUE);
        if (searchAborted) {
            break;
        }

        // Print uci output
        if (timeElapsed >= 1) {
            uint64_t nps = diagnosis.nodes * 1000 / timeElapsed;
            std::cout << "info depth " << depth << " score " << result.second
                      << " seldepth " << (int)diagnosis.seldepth << " hashfull "
                      << tt.hashfull() << " time " << timeElapsed << " nodes "
                      << (int)diagnosis.nodes << " nps " << nps << " pv "
                      << chess::uci::moveToUci(pvMoveFromIteration)
                      << std::endl;
        } else {
            std::cout << "info depth " << depth << " score " << result.second
                      << " seldepth " << (int)diagnosis.seldepth << " hashfull "
                      << tt.hashfull() << " time " << timeElapsed << " nodes "
                      << (int)diagnosis.nodes << std::endl;
        }

        pvMoveFromIteration = result.first;
        pvScoreFromIteration = result.second;
    }
    std::cout << std::flush;
    return {pvMoveFromIteration, pvScoreFromIteration};
}

bool Searcher::checkTime() {
    const auto timeNow = std::chrono::steady_clock::now();
    const uint32_t timeElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(timeNow -
                                                              startTime)
            .count();

    return timeElapsed > maxThinkingTime;
}

/**
 * The entry point for the search.
 */
void think(Position &pos, SearchParams &params) {
    // TODO test code here

    Searcher searcher(pos, params);
    const auto [bestMove, bestScore] = searcher.search();
    tt.incGeneration();
    const SearchDiagnosis &info = searcher.diagnosis;

    auto uci = chess::uci::moveToUci(bestMove);
    std::cout << "bestmove " << uci << std::endl;
}
