# Overview

This is a toy chess engine I developed over two weeks as part of my winter break project. It is **UCI-compatible**, and plays roughly at the level of **1600 Elo** (unofficial; tested on my laptop, TC 10+0.1).

This software **does not** come with a GUI, so if you want to play it you will need to download one from Internet. I recommend [en-croissant](https://github.com/franciscoBSalgueiro/en-croissant).

This engine is still **WIP** and will be updated from time to time, though I do not currently have any plans for updating.

## Compiling

I currently have a small build script that compiles the engine with `g++` (`build.py`). Why, you may ask, don't you use `Makefile`? Because I don't know how to use it yet... (What a shame) I will probably move to `Makefile` in the future.

Nevertheless, I have compiled the binary for Windows and you can find it in the release page.

## License

This project is licensed under the MIT License.

## Acknowledgements

During the development, I have referred to and learned from a lot of resources, including:

- [Stockfish](https://github.com/official-stockfish/Stockfish/tree/master) source code (and its previous versions)
- [Stockfish Evaluation Guide](https://hxim.github.io/Stockfish-Evaluation-Guide/) - thank you for this comprehensive interactive wiki!
- Sebastian Lague's [Coding Adventure Bot](https://github.com/SebLague/Chess-Coding-Adventure/tree/Chess-V2-UCI) and his excellent [Youtube videos](https://www.youtube.com/watch?v=_vqlIPDR2TU) on chess programming!
- [Chess Programming Wiki](https://www.chessprogramming.org/Main_Page/) - an amazing place to learn about every aspects of chess programming!
- [Disservin](https://github.com/Disservin)'s [chess library](https://github.com/Disservin/chess-library) and his engine testing tool [fastchess](https://github.com/Disservin/fastchess)!

