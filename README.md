# Overview

This is a toy chess engine I developed over two weeks as part of my winter break project. It is **UCI-compatible**, and plays at the level of **1306 Elo** on [CCRL](http://computerchess.org.uk/ccrl/404/cgi/engine_details.cgi?print=Details&each_game=1&eng=Emerald%200.1.1%2064-bit#Emerald_0_1_1_64-bit).

This software **does not** come with a GUI, so if you want to play it you will need to download one from Internet. I recommend [en-croissant](https://github.com/franciscoBSalgueiro/en-croissant).

This engine is still **WIP** and will be updated from time to time. Currently I am refactoring and optimizing and hope to hit 1600 Elo soon.

## Compiling

Use the Makefile to compile the engine.

## License

This project is licensed under the MIT License.

## Acknowledgements

During the development, I have referred to and learned from a lot of resources, including:

- [Stockfish](https://github.com/official-stockfish/Stockfish/tree/master) source code (and its previous versions)
- [Stockfish Evaluation Guide](https://hxim.github.io/Stockfish-Evaluation-Guide/) - thank you for this comprehensive interactive wiki!
- Sebastian Lague's [Coding Adventure Bot](https://github.com/SebLague/Chess-Coding-Adventure/tree/Chess-V2-UCI) and his excellent [Youtube videos](https://www.youtube.com/watch?v=_vqlIPDR2TU) on chess programming!
- [Chess Programming Wiki](https://www.chessprogramming.org/Main_Page/) - an amazing place to learn about every aspects of chess programming!
- [Disservin](https://github.com/Disservin)'s [chess library](https://github.com/Disservin/chess-library) and his engine testing tool [fastchess](https://github.com/Disservin/fastchess)!

