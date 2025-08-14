#include "tt.h"

TranspositionTable tt; // real definition here

void TranspositionTable::store(
    const Position& pos, EntryType type, int8_t depth, Move move, Value value) {
    uint64_t key   = pos.hash();
    uint32_t index = key % size;
    bool     ok    = false;

    // if (value == MATE_VALUE || value == MATED_VALUE) {
    //     std::cerr << "Error at <" << pos.getFen() << ">: " << (int)type << ", "
    //               << (int)depth << ", " << move << ", " << value << " ("
    //               << (int)value << ")" << std::endl;
    //     assert(false);
    // }

    // Determine if a position can be stored into the table
    if (!db[index].hasInitialized()) { // uninitialized entry
        ok = true;
        occupied++;                        // increase counter
    } else if (db[index].zobrist == key) { // same position, overwrite
        ok = true;
        if (move.move() == Move::NO_MOVE) {
            move = db[index].move();
        }
    } else if (db[index].age != (generation & 0xff)) { // old entry
        ok = true;
    } else if (db[index].depth < depth) { // deeper entry
        ok = true;
    }

    if (ok) {
        db[index] = TTEntry(pos, type, depth, move, value);
    }
}

TTEntry* TranspositionTable::probe(const Position& pos) {
    uint64_t key   = pos.hash();
    uint32_t index = key % size;
    TTEntry* ptr   = &db[index];

    if (ptr->type == EntryType::NONE) {
        return nullptr; // not found because entry is empty
    }
    if (ptr->zobrist != key) {
        return nullptr; // not found because entry is for different position
    }

    return ptr;
}

std::pair<bool, Value> TranspositionTable::lookupEval(
    const Position& pos, int8_t depth, int8_t plyFromRoot, Value alpha, Value beta) {
    // Lookup entry in the transposition table
    TTEntry* entry = probe(pos);
    if (!entry) {
        return std::make_pair(false, Value(0));
    }
    // Only work if the entry is searched at least as deep as the query
    if (entry->depth < depth) {
        return std::make_pair(false, Value(0));
    }
    // Correct the score in case it is a mate score
    Value v = entry->value;
    if (v.isMate()) {
        v = (static_cast<int>(v) > 0) ? (MATE_VALUE - plyFromRoot) : (MATED_VALUE + plyFromRoot);
    }
    // Return the score
    if (entry->type == EntryType::EXACT) {
        return {true, v};
    } else if (entry->type == EntryType::LOWER_BOUND) {
        return {true, std::max(v, beta)};
    } else if (entry->type == EntryType::UPPER_BOUND) {
        return {true, std::min(v, alpha)};
    }

    return {false, Value(0)};
}