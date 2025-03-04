#include "uci.h"
#include "chess.hpp"
#include "search.h"

namespace uci {

/**
 * This is the position specified by the "position" command.
 */
Position board;

/**
 * Executes a UCI command line.
 *
 */
void execute(const std::string &command) {
    // Setup input stream
    std::string token;
    std::istringstream iss(command);
    iss >> token;

    // <Command> uci
    if (token == "uci") {
        std::cout << "id name Emerald " << ENGINE_VERSION << std::endl;
        std::cout << "id author UndefinedCpp" << std::endl;
        // TODO Send options
        std::cout << "uciok" << std::endl;
        return;
    }

    // <Command> isready
    if (token == "isready") {
        std::cout << "readyok" << std::endl;
        return;
    }

    // <Command> debug
    if (token == "debug") {
        // TODO
        return;
    }

    // <Command> setoption
    if (token == "setoption") {
        std::cout << "info string setoption not implemented" << std::endl;
    }

    // <Command> ucinewgame
    if (token == "ucinewgame") {
        return;
    }

    // <Command> position
    // <Grammar> position fen <fenstring>  [moves <move1> ... <movei>]
    //           position [startpos] [moves <move1> ... <movei>]
    if (token == "position") {
        iss >> token;
        if (token == "fen") { // start from a custom position
            // A FEN string consists of 6 fields separated by a space
            std::string fen;
            fen.reserve(128);
            for (int i = 0; i < 6; ++i) {
                iss >> token;
                fen += token;
                if (i < 5)
                    fen += " ";
            }
            // Set the position
            uci::board.setFen(fen);
        } else if (token == "startpos") { // start from the start position
            uci::board.setFen(chess::constants::STARTPOS);
        } else {
            // Invalid command, ignore. This is undefined behavior.
            return;
        }

        // Handle moves
        while (iss >> token) {
            if (token == "moves") {
                while (iss >> token) {
                    Move move = chess::uci::uciToMove(uci::board, token);
                    uci::board.makeMove(move);
                }
            } else {
                return; // Invalid command
            }
        }
        return;
    }

    // <Command> go
    if (token == "go") {
        // Parse "go" options
        SearchParams params;
        while (iss >> token) {
            if (token == "infinite") {
                params.infinite = true;
            } else if (token == "wtime") {
                iss >> params.wtime;
            } else if (token == "btime") {
                iss >> params.btime;
            } else if (token == "winc") {
                iss >> params.winc;
            } else if (token == "binc") {
                iss >> params.binc;
            } else if (token == "movestogo") {
                iss >> params.movestogo;
            } else if (token == "depth") {
                iss >> params.depth;
            } else if (token == "nodes") {
                iss >> params.nodes;
            } else if (token == "mate") {
                iss >> params.mate;
            } else if (token == "movetime") {
                iss >> params.movetime;
            } else if (token == "ponder") {
                params.ponder = true;
            } else {
                break; // stop on unknown token
            }
        }
        // Initiate the search process
        think(uci::board, params);
        return;
    }

    // <Command> stop
    if (token == "stop") {
        return;
    }

    // <Command> ponderhit
    if (token == "ponderhit") {
        return;
    }

    // <Util> d
    // Display the board
    if (token == "d") {
        std::cout << uci::board << "\n"
                  << "Evaluation: " << evaluate(uci::board) << std::endl;
        return;
    }

    // Uncognized commands
    std::cout << "Unrecognized command" << std::endl;
}

} // namespace uci
