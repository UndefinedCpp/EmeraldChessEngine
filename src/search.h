#pragma once

#include "eval.h"
#include "position.h"
#include "types.h"
#include <chrono>

struct SearchParams {
    bool infinite; // Infinite search
    bool ponder;
    uint32_t wtime;
    uint32_t btime;
    uint32_t winc;
    uint32_t binc;
    uint32_t movestogo;
    uint32_t depth;
    uint32_t nodes;
    uint32_t mate;
    uint32_t movetime;

    SearchParams()
        : infinite(false), ponder(false), wtime(0), btime(0), winc(0), binc(0),
          movestogo(0), depth(0), nodes(0), mate(0), movetime(0) {
    }
};

struct SearchDiagnosis {
    int32_t evals;
    int32_t nodes;
    int32_t qnodes;
    int32_t seldepth;

    SearchDiagnosis() : evals(0), qnodes(0), nodes(0), seldepth(0) {
    }
};

/**
 * This keeps track of information gained while searching. It is indexed by
 * ply.
 */
struct Scratchpad {
    int ply;                  // current ply
    KillerHeuristics killers; // killer moves
    uint16_t *pv;             // principal variation
    Value staticValue;        // static evaluation
    bool skipPruning;         // skip pruning

    Scratchpad() : ply(0), pv(nullptr), staticValue(0), skipPruning(false) {
    }
};

/**
 * Used as a template parameter to distinguish between PV and non-PV nodes.
 */
enum NodeType { PVNode, NonPVNode };

class Searcher {
  private:
    KillerHeuristics killerMoves[MAX_PLY];
    Move pvMoveFromIteration; // move from previous iteration
    Value pvScoreFromIteration;

    uint32_t maxThinkingTime;
    TimePoint startTime;
    bool searchAborted;

    uint32_t searchCallCount;

  public:
    SearchDiagnosis diagnosis;
    Position pos;
    SearchParams params;

    Searcher(Position &pos, SearchParams &params)
        : pos(pos), params(params), diagnosis() {};
    ~Searcher() = default;

    std::pair<Move, Value> search();

  private:
    template <NodeType NT>
    Value negamax(Scratchpad *sp, Value alpha, Value beta, int depth,
                  bool cutnode);

    template <NodeType NT>
    Value qsearch(Scratchpad *sp, Value alpha, Value beta, int depth);

    bool checkTime();
};

void think(Position &pos, SearchParams &params);