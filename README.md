# genesis
A handmade game project.

The project uses no libraries aside from the operating system. The few libc functions we use are reimplemented as needed.

The objective is to make use of SIMD (like in audio.cpp, mix_samples), multithreading and other low-level programming techniques in the context of a larger project in order to learn proper software engineering.

The project uses SVN internally; this repository is a mirror for the source code. The code can be compiled as is (I am using MSVC 2017), but running the game requires asset files that are not provided here.

For a short rundown of some notable features, we have:
- A multithreaded job queue, albeit with no jobs being used for the game yet.
- A SIMD sound mixer and a lock-free asynchronous sound streaming system.
- An interactive profiler that supports multithreaded logging and frame lookback.
- Custom asset formats fitted to our needs: texture atlases and uninterleaved chunk-based audio.

Some areas to work on:
- Graphics are very barebones: very basic OpenGL 3 quad renderer, no graphical effects to speak of.
- Asset management, outside of the audio mixer, is not asynchronous.
- No compression/decompression whatsoever on assets.
