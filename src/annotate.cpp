#include "annotate.h"
#include "position.h"
#include "search.h"
#include <chrono>
#include <cstdint>
#include <fstream>

constexpr SearchParams DEFAULT_SEARCH_PARAMS = ([]() {
    SearchParams params;
    params.nodes = 2000;
    return params;
})();

class Annotator {
public:
    Annotator(const char* inputFileName, const char* outputFileName) :
        ifile(inputFileName), ofile(outputFileName, std::ios::binary), progress(0) {
        total = std::count(
            std::istreambuf_iterator<char>(ifile), std::istreambuf_iterator<char>(), '\n');
        start = std::chrono::high_resolution_clock::now();
        ifile.seekg(0);
    }

    /**
     * Annotate the next position in the input file and write the result to the output file.
     */
    void process() {
        // Read FEN notation
        std::string fen;
        std::getline(ifile, fen);

        // Parse position and do search
        Position pos(fen);
        auto [bestMove, bestValue] = internalSearch(DEFAULT_SEARCH_PARAMS, pos);
        if (bestMove == 0) { // no legal move found
            std::cout << "\nWarning: skipping for no legal move found: " << fen << std::endl;
            return;
        }

        // Save annotated position to output file
        adjustAndSave(pos, Move(bestMove), bestValue);

        // Update progress
        progress++;
    }

    void flush() { ofile.flush(); }

    void report() {
        auto  end     = std::chrono::high_resolution_clock::now();
        auto  elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        int   speed   = (int) ((float) progress / elapsed * 1000); // unit: p/s
        int   eta     = (int) ((total - progress) / speed);
        int   minutesToGo = eta / 60;
        int   secondsToGo = eta % 60;
        float prog        = (float) progress / total;
        std::cout << "Annotating in progress: " << progress << "/" << total << " "
                  << int(prog * 100) << "%"
                  << " - speed: " << speed << " p/s; ETA: " << minutesToGo << "m " << secondsToGo
                  << "s"
                  << "        \r";
    }

    int getTotal() const { return total; }
    int getProgress() const { return progress; }

private:
    std::ifstream                                  ifile;
    std::ofstream                                  ofile;
    int                                            total;
    int                                            progress;
    std::chrono::high_resolution_clock::time_point start;

    /**
     * Save data to output stream.
     */
    void adjustAndSave(Position& pos, Move move, Value value) {
        // Get all bitboards
        const uint64_t pieceBB[] = {
            pos.pieces(TYPE_PAWN).getBits(),
            pos.pieces(TYPE_KNIGHT).getBits(),
            pos.pieces(TYPE_BISHOP).getBits(),
            pos.pieces(TYPE_ROOK).getBits(),
            pos.pieces(TYPE_QUEEN).getBits(),
            pos.pieces(TYPE_KING).getBits(),
        };
        const uint64_t whiteBB = pos.us(WHITE).getBits();
        const uint64_t blackBB = pos.us(BLACK).getBits();

        // Save bitboards according to side to move
        if (pos.sideToMove() == WHITE) {
            ofile.write(reinterpret_cast<const char*>(pieceBB), sizeof(pieceBB))
                .write(reinterpret_cast<const char*>(&whiteBB), sizeof(whiteBB))
                .write(reinterpret_cast<const char*>(&blackBB), sizeof(blackBB));
        } else {
            uint64_t swappedPcBB[6];
            for (int i = 0; i < 6; ++i) {
                swappedPcBB[i] = _byteswap_uint64(pieceBB[i]);
            }
            const uint64_t swappedWhiteBB = _byteswap_uint64(whiteBB);
            const uint64_t swappedBlackBB = _byteswap_uint64(blackBB);
            ofile.write(reinterpret_cast<const char*>(swappedPcBB), sizeof(swappedPcBB))
                .write(reinterpret_cast<const char*>(&swappedBlackBB), sizeof(swappedBlackBB))
                .write(reinterpret_cast<const char*>(&swappedWhiteBB), sizeof(swappedWhiteBB));
        }
        // Adjust score
        int16_t score = std::clamp(value.value(), -3200, 3200); // note that this is already PoV
        int     materialDiff = getMaterialDifference(pos);
        if (score > 100) {
            // Bonus for being better than what material difference says
            if (score > materialDiff) {
                score = score + score / 4; // 25% bonus
            }
            // Bonus for being ahead in development
            int whiteBackrankPieces =
                (pos.us(WHITE) & Bitboard(chess::Rank(chess::Rank::RANK_1))).count();
            int blackBackrankPieces =
                (pos.us(BLACK) & Bitboard(chess::Rank(chess::Rank::RANK_8))).count();
            // positive diff indicates ahead in development
            int diff = pos.sideToMove() == WHITE ? blackBackrankPieces - whiteBackrankPieces
                                                 : whiteBackrankPieces - blackBackrankPieces;
            if (diff > 2) {
                score += 50; // 50 cp bonus
            }
        }
        // Save score
        ofile.write(reinterpret_cast<const char*>(&score), sizeof(score));
    }

    int getMaterialDifference(const Position& pos) {
        int d =
            100 * (pos.pieces(TYPE_PAWN, WHITE).count() - pos.pieces(TYPE_PAWN, BLACK).count()) +
            300 *
                (pos.pieces(TYPE_KNIGHT, WHITE).count() - pos.pieces(TYPE_KNIGHT, BLACK).count()) +
            330 *
                (pos.pieces(TYPE_BISHOP, WHITE).count() - pos.pieces(TYPE_BISHOP, BLACK).count()) +
            500 * (pos.pieces(TYPE_ROOK, WHITE).count() - pos.pieces(TYPE_ROOK, BLACK).count()) +
            900 * (pos.pieces(TYPE_QUEEN, WHITE).count() - pos.pieces(TYPE_QUEEN, BLACK).count());
        return pos.sideToMove() == WHITE ? d : -d;
    }
};

/**
 * Evaluate positions in a PGN file for training NNUE.
 */
void annotate_main(const char* inputFileName) {
    std::string outputName = std::string(inputFileName) + ".analysis";
    std::cout << "Starting annotation of " << inputFileName << std::endl;

    Annotator annotator(inputFileName, outputName.c_str());
    const int N = annotator.getTotal();
    for (int i = 0; i < N; ++i) {
        annotator.process();
        if (i % 13 == 0) {
            annotator.flush();
            annotator.report();
        }
    }
    annotator.flush();
    std::cout << std::endl;
}