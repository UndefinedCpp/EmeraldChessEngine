# Overview

This is a toy chess engine I developed over two weeks as part of my winter break project. It is **UCI-compatible**, and plays at the level of **1727 Elo** on [CCRL](https://computerchess.org.uk/ccrl/404/cgi/engine_details.cgi?match_length=30&each_game=1&print=Details&each_game=1&eng=Emerald%200.3.0%2064-bit#Emerald_0_3_0_64-bit).

This software **does not** come with a GUI, so if you want to play it you will need to download one from Internet. I recommend [en-croissant](https://github.com/franciscoBSalgueiro/en-croissant).

This engine is still **WIP** and will be updated from time to time.

## Compiling

Use the CMake to compile the engine.

## Technical Details

This engine is based on traditional *alpha-beta search* algorithm with simple optimizations and pruning methods. Features implemented:
- Transposition table
- Iterative deepening
- Null move pruning
- Quiescence search
- Razoring
- Late move reduction
- History heuristics
- A simple NNUE for evaluation

Since v0.4.0 the engine uses **NNUE** for evaluation.
- Uses naive `768->128->1` architecture. No accumulators, king buckets, subnetworks - so in theory this is not actually *efficiently updated*. However, with proper auto-vectorization this runs even *faster* than my previous handcrafted evaluation function.
- Positions used to train this NNUE are collected from Lichess database. About 30% of the data is from chess960 variant. All positions are first evaluated with my own engine (v0.3.0) at depth 8, and then adjusted based on the actual outcome and the "imbalanceness" of the position. Thus, this network is purely original. So far, only 1.5M filtered positions are used to train this reasonably good network.

## License

This project is licensed under the MIT License.

## Acknowledgements

During the development, I have referred to and learned from a lot of resources, including:

- [Stockfish](https://github.com/official-stockfish/Stockfish/tree/master) source code (and its previous versions)
- [Stockfish Evaluation Guide](https://hxim.github.io/Stockfish-Evaluation-Guide/) - thank you for this comprehensive interactive wiki!
- Sebastian Lague's [Coding Adventure Bot](https://github.com/SebLague/Chess-Coding-Adventure/tree/Chess-V2-UCI) and his excellent [Youtube videos](https://www.youtube.com/watch?v=_vqlIPDR2TU) on chess programming!
- [Chess Programming Wiki](https://www.chessprogramming.org/Main_Page/) - an amazing place to learn about every aspects of chess programming!
- [Disservin](https://github.com/Disservin)'s [chess library](https://github.com/Disservin/chess-library) and his engine testing tool [fastchess](https://github.com/Disservin/fastchess)!
- [Patricia](https://github.com/Adam-Kulju/Patricia) for its position filters
