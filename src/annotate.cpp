#include "annotate.h"
#include "position.h"
#include "search.h"
#include <chrono>
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
        save(pos, Move(bestMove), bestValue);

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

    void save(const Position& pos, Move move, Value value) {
        // We save bitboards of a position, and flipped if black to move
        const Bitboard wp = pos.pieces(TYPE_PAWN, WHITE), bp = pos.pieces(TYPE_PAWN, BLACK),
                       wn = pos.pieces(TYPE_KNIGHT, WHITE), bn = pos.pieces(TYPE_KNIGHT, BLACK),
                       wb = pos.pieces(TYPE_BISHOP, WHITE), bb = pos.pieces(TYPE_BISHOP, BLACK),
                       wr = pos.pieces(TYPE_ROOK, WHITE), br = pos.pieces(TYPE_ROOK, BLACK),
                       wq = pos.pieces(TYPE_QUEEN, WHITE), bq = pos.pieces(TYPE_QUEEN, BLACK),
                       wk = pos.pieces(TYPE_KING, WHITE), bk = pos.pieces(TYPE_KING, BLACK);
        Bitboard bbs[12];
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