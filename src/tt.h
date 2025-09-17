#pragma once

#include "eval.h"
#include "position.h"

enum class EntryType : uint8_t {
    NONE, // no entry
    EXACT,
    UPPER_BOUND, // fail-low, this position was not good enough for us because
                 // we have a stronger move.
    LOWER_BOUND  // fail-high: this position was too good for us that our
                 // opponent should refute it.
};

/**
 * Transposition table entry.
 */
struct TTEntry {
    uint64_t  zobrist;   // Zobrist hash
    int8_t    depth;     // Search depth
    uint8_t   age;       // How old the entry is
    EntryType type;      // Entry type
    uint16_t  move_code; // Best move cached
    Value     value;     // Evaluation value

    TTEntry() : zobrist(0ull), depth(0), age(0), type(EntryType::NONE), move_code(0), value(0) {}
    TTEntry(const Position& pos, EntryType type, int8_t depth, Move move, Value value) :
        zobrist(pos.hash()),
        depth(depth),
        age(0),
        type(type),
        move_code(move.move()),
        value(value) {};

    /**
     * Get the move associated with this entry.
     */
    inline Move move() { return Move(move_code); }

    inline bool hasInitialized() { return type != EntryType::NONE; }
};

class TranspositionTable {
private:
    TTEntry* db;
    uint32_t size;
    uint32_t occupied;
    uint16_t generation;

public:
    TranspositionTable() : size(0), occupied(0), generation(0) { db = nullptr; }
    ~TranspositionTable() { delete[] db; }
    inline void init(uint32_t size) {
        this->size = size;
        db         = new TTEntry[size];
        clear();
    }
    inline void clear() {
        for (uint32_t i = 0; i < size; i++) {
            db[i] = TTEntry();
        }
    }
    inline void incGeneration() { generation++; }
    inline int  hashfull() { return occupied * 1000 / size; }

    void     store(const Position& pos, EntryType type, int8_t depth, Move move, Value value);
    TTEntry* probe(const Position& pos);
    std::pair<bool, Value>
    lookupEval(const Position& pos, int8_t depth, int8_t plyFromRoot, Value alpha, Value beta);
};

/**
 * Global instance of the transposition table.
 */
extern TranspositionTable tt;