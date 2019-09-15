# genesis
A handmade game project.

The project uses no libraries aside from the operating system. The few libc functions we use are reimplemented as needed.

The objective is to make use of SIMD (like in audio.cpp, mix_samples), multithreading and other low-level programming techniques in the context of a larger project in order to learn proper software engineering.

The project uses SVN internally; this repository is a mirror for the source code. The code can be compiled as is (I am using MSVC 2017), but running the game requires asset files that are not provided here.

For a short rundown of some notable features, we have:
- A multithreaded job queue, albeit with minimal thread usage.
- An interactive profiler that supports multithreaded logging and frame lookback.
- Custom asset formats fitted to our needs.
- A mesh-based (as opposed to sprite-based) D3D12 renderer which is similar to a traditional 3D renderer, only more specialised for our 2D use case.
- A real-time subtractive synthesizer for audio.

The latest systems, ie. the renderer and synth, are still barebones. The game used to have a sprite renderer and a SIMD-accelerated audio mixer with asynchronous resource streaming, which were both replaced for artistic reasons.
