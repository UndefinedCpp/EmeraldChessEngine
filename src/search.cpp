#include "search.h"
#include "eval.h"
#include "timecontrol.h"
#include "tt.h"
#include "types.h"

#include <array>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

std::atomic<bool> g_stopRequested(false);

namespace {

struct SearchStats {
    uint64_t nodes    = 0;
    int      selDepth = 0;
};

struct SearchStackEntry {
    Value staticEval = VALUE_NONE;
    Move  bestMove;
    Move  excludedMove;
    bool  inCheck     = false;
    bool  canNullMove = false;
};

std::thread searchThread;
SearchStats searchStats;

std::array<SearchStackEntry, MAX_PLY> searchStack;

bool isLegalMove(const Position& pos, const Move& m) {
    Movelist legal = pos.legalMoves();
    for (auto lm : legal)
        if (lm.move() == m.move())
            return true;
    return false;
}

std::vector<Move> extractPv(Position pos, int maxDepth = 64) {
    std::vector<Move> pv;
    pv.reserve(maxDepth);
    for (int d = 0; d < maxDepth; ++d) {
        TTEntry* e = tt.probe(pos);
        if (!e || !e->hasInitialized() || e->zobrist != pos.hash())
            break;
        Move m = e->move();
        if (m.move() == 0 || !isLegalMove(pos, m))
            break;
        pv.push_back(m);
        pos.makeMove(m);
    }
    return pv;
}

Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, TimeControl& tc) {
    // todo implement
}

void searchWorker(SearchParams params, Position pos) {
    g_stopRequested.store(false);
    searchStats = SearchStats();
    searchStack.fill(SearchStackEntry {});
    tt.incGeneration();

    TimeControl tc(pos.sideToMove(), params, TimeControl::now());
    int         maxDepth = params.depth > 0 ? (int) params.depth : 64;

    Move  rootBestMove;
    Value rootBestScore = Value(0);

    for (int depth = 1; depth <= maxDepth; ++depth) {
        if (g_stopRequested.load())
            break;
        if (tc.hitSoftLimit(depth, (int) searchStats.nodes, 0))
            break;

        Value score = negamax(pos, depth, MATED_VALUE, MATE_VALUE, 0, tc);
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

        if (tc.hitSoftLimit(depth, (int) searchStats.nodes, 0))
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
