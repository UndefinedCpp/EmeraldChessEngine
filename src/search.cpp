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

/**
 * Used as a template parameter to distinguish between PV and non-PV nodes.
 */
enum NodeType { PVNode, NonPVNode };

/**
 * Quiescence search.
 *
 * The primary goal of quiescence search is to avoid the "horizon effect," a
 * phenomenon where the search ends right before an immediate tactics, resulting
 * in poor performance. By extending the search in non-quiet positions,
 * quiescence search ensures a more accurate evaluation in these cases.
 *
 * See https://www.chessprogramming.org/Quiescence_Search.
 */
Value Searcher::qsearch(Value alpha, Value beta, int plyRemaining,
                        int plyFromRoot) {
    // Check game status first.
    const auto [gameOver, gameResultValue] = checkGameStatus(pos);
    if (gameOver) {
        return gameResultValue.addPly(plyFromRoot);
    }

    // A player isn't typically forced to make a capture, so even if we searched
    // a bad value, it doesn't nessessarily mean we were doomed to lose. So we
    // keep this static evaluation as lower bound. A more detailed explanation
    // and discussion can be found at the link above.
    Value eval = evaluate(pos);
    diagnosis.qnodes++;
    diagnosis.nodes++;
    if (plyFromRoot >= diagnosis.seldepth) {
        diagnosis.seldepth = plyFromRoot;
    }
    if (plyRemaining <= 0) {
        // An extreme case is that there are so many captures that we have gone
        // too deep into the tree. In this case, we just return the static
        // value.
        return eval;
    }
    if (eval >= beta) { // beta cutoff
        return beta;
    }

    if (eval > alpha) {
        alpha = eval;
    }
    // Generate moves to search. Here, we only search captures and promotions.
    MoveOrderer<OrderMode::QUIET> orderer;
    Move m;

    orderer.init(pos);
    while ((m = orderer.get()).isValid()) {
        pos.makeMove(m);
        eval = -qsearch(-beta, -alpha, plyRemaining - 1, plyFromRoot + 1);
        pos.unmakeMove(m);

        if (eval >= beta) {
            return beta;
        }
        if (eval > alpha) {
            alpha = eval;
        }
    }
    return alpha;
}

/**
 * Implements search based on negamax framework.
 */
template <NodeType NT>
Value Searcher::negamax(Value alpha, Value beta, int plyRemaining,
                        int plyFromRoot, bool canNullMove) {
    // Check game status
    const auto [gameOver, gameResultValue] = checkGameStatus(pos);
    if (gameOver) {
        return gameResultValue.addPly(plyFromRoot);
    }
    // Check time
    if (checkTime()) {
        searchAborted = true;
        return 0;
    }

    diagnosis.nodes++;

    if (plyFromRoot >= diagnosis.seldepth) {
        diagnosis.seldepth = plyFromRoot;
    }

    constexpr bool isPVNode = NT == PVNode;

    // Transposition table lookup
    TTEntry *entry = tt.probe(pos);
    if (entry && entry->depth >= plyFromRoot) {
        if (!isPVNode && entry->type == EntryType::EXACT) {
            return entry->value;
        }
        if (entry->type == EntryType::LOWER_BOUND) {
            alpha = std::max(alpha, entry->value);
        }
        if (entry->type == EntryType::UPPER_BOUND) {
            beta = std::min(beta, entry->value);
        }
        if (alpha >= beta) {
            return entry->value;
        }
    }

    // Quiescence search if we hit search depth limit
    if (plyRemaining <= 0) {
        return qsearch(alpha, beta, QSEARCH_DEPTH, plyFromRoot);
    }

    // Set up move ordering
    MoveOrderer<OrderMode::DEFAULT> orderer;

    if (entry) { // Hash move from transposition table if available
        Move ttHashMove = entry->move();
        orderer.init(pos, &killerMoves[plyFromRoot], &ttHashMove);
    } else {
        orderer.init(pos, &killerMoves[plyFromRoot]);
    }

    Move bestMove(Move::NO_MOVE);
    Move m;
    EntryType ttEntryType = EntryType::UPPER_BOUND;
    int movesSearched = 0;

    while ((m = orderer.get()).isValid()) {
        /**
         * Null Move Pruning.
         *
         * In almost all chess positions, making a null move is worse than the
         * best legal move. If a reduced search on a null move fails high, the
         * best move will possibly fail high too.
         *
         * See https://www.chessprogramming.org/Null_Move_Pruning.
         */
        if (!isPVNode && canNullMove && plyRemaining >= 3 && !pos.inCheck()) {
            pos.makeNullMove();
            Value eval = -negamax<NonPVNode>(-beta, -beta + 1, plyRemaining - 3,
                                             plyFromRoot + 1, false);
            pos.unmakeNullMove();
            if (eval >= beta) {
                return eval;
            }
        }

        // Principal Variation Search
        pos.makeMove(m);
        Value eval;
        if (movesSearched == 0) {
            eval = -negamax<NT>(-beta, -alpha, plyRemaining - 1,
                                plyFromRoot + 1, true);
        } else {
            eval = -negamax<NonPVNode>(-alpha - 1, -alpha, plyRemaining - 1,
                                       plyFromRoot + 1, true);
            if (eval > alpha && isPVNode) { // research
                eval = -negamax<PVNode>(-beta, -alpha, plyRemaining - 1,
                                        plyFromRoot + 1, true);
            }
        }
        pos.unmakeMove(m);

        // The position is too good for us that the opponent should have played
        // another move to prevent this. (Fail high)
        if (eval >= beta) {
            // Store evaluation in transposition table
            tt.store(pos, EntryType::LOWER_BOUND, plyFromRoot, m, beta);
            // Update killer and history heuristics for non-capture moves
            if (!pos.isCapture(m)) {
                this->killerMoves[plyFromRoot].add(m);
            }
            return beta;
        }
        // A new best move has been found
        if (eval > alpha) {
            alpha = eval;
            bestMove = m;
            ttEntryType = EntryType::EXACT;
        }

        movesSearched++;
    }

    tt.store(pos, ttEntryType, plyFromRoot, bestMove, alpha);
    return alpha;
}

std::pair<Move, Value> Searcher::negamax_root(int depth, Value alpha,
                                              Value beta) {
    Move bestMove = Move::NO_MOVE;
    Value bestScore = MATED_VALUE;
    Move m;
    MoveOrderer<OrderMode::DEFAULT> orderer;
    orderer.init(pos, nullptr, &pvMoveFromIteration);

    searchAborted = false;
    int movesSearched = 0;
    while ((m = orderer.get()).isValid()) {
        pos.makeMove(m);
        Value eval;
        if (movesSearched == 0) {
            eval = -negamax<PVNode>(beta, alpha, depth - 1, 1, true);
        } else {
            eval = -negamax<NonPVNode>(alpha - 1, alpha, depth - 1, 1, true);
            if (eval > alpha) { // research
                eval = -negamax<PVNode>(beta, alpha, depth - 1, 1, true);
            }
        }
        pos.unmakeMove(m);

        if (eval >= bestScore) {
            bestScore = eval;
            bestMove = m;
        }
    }

    return {bestMove, bestScore};
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
