# genesis
A handmade game project.

The project uses no libraries aside from the operating system. The few libc functions we use are reimplemented as needed.

The objective is to make use of SIMD (like in audio.cpp, mix_samples), multithreading and other low-level programming techniques in the context of a larger project.

The project uses SVN internally; this repository is a mirror for the source code. The code can be compiled as is, but running the game requires asset files that are not provided here.

For a short rundown of some notable features, we have:
- A multithreaded job queue, albeit with no jobs being used for the game yet.
- A SIMD sound mixer and sound streaming system. The streaming makes use of a custom, chunk-based file format.
- An interactive profiler that supports multithreaded logging and frame lookback.

For some less notable features, we also have:
- A very basic OpenGL 3 layer.
- No overlapped (or asynchronous) disk I/O.
