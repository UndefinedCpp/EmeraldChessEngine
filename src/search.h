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
 * This keeps track of information gained while searching.
 */
struct Scratchpad {
    int ply;                  // current ply
    KillerHeuristics killers; // killer moves
    uint16_t *pv;             // principal variation
};

class Searcher {
  private:
    KillerHeuristics killerMoves[128]; // todo maximum 128 plys
    Move pvMoveFromIteration;          // move from previous iteration
    Value pvScoreFromIteration;

    uint32_t maxThinkingTime;
    TimePoint startTime;
    bool searchAborted;

  public:
    SearchDiagnosis diagnosis;
    Position pos;
    SearchParams params;

    Searcher(Position &pos, SearchParams &params)
        : pos(pos), params(params), diagnosis() {};
    ~Searcher() = default;

    std::pair<Move, Value> search();

  private:
    Value qsearch(Value alpha, Value beta, int plyRemaining, int plyFromRoot);

    template <NodeType NT>
    Value negamax(Value alpha, Value beta, int plyRemaining, int plyFromRoot,
                  bool canNullMove);
    std::pair<Move, Value> negamax_root(int depth, Value alpha, Value beta);

    bool checkTime();
};

void think(Position &pos, SearchParams &params);