# Anduril

Anduril is a UCI compatable chess engine written in C++.
Anduril is built off of [libchess](https://github.com/Mk-Chan/libchess) with a few changes (mostly to improve performance).
The engine uses the negamax algorithm with many enhancements such as alpha-beta pruning, null move pruning, probcut, futility pruning, and more techinques.

Other libraries used in Anduril include:    
- [nnue-probe](https://github.com/dshawul/nnue-probe) by dschawl in order to utilize the original Stockfish NNUE architecture for evaluation.     
- [incbin](https://github.com/graphitemaster/incbin) by graphitemaster in order to embed the NNUE weights in the binary.  
- [Pyrrhic](https://github.com/AndyGrant/Pyrrhic) by AndyGrant for Syzygy tablebase probing.

Testing and tuning is done using the [OpenBench](https://github.com/AndyGrant/OpenBench) project by AndyGrant.

# Building
Anduril can be built for Windows, Linux, and MacOS.  The engine is built using CMake, has been tested with Clang (but should work with any compiler that can parse Clang flags) and requires a C++20 compliant compiler.
The minimum requirements tested for Anduril are 64 bits and at least AVX supported.  The CMake file only gives options for AVX2, however.

A few CMake options need to be defined for the engine to build correctly:
- First, set your ISA.  The engine has been tested on x86-64 and Apple Silicon.  The flags for ISA are:
    - `-Dx86=true` for x86-64
    - `-Dapple-silicon=true` for Apple Silicon
- If you are compiling for x86-64, you also need to specify the highest SIMD capability of your CPU.  The flags for SIMD are:
    - `-DAVX2=true`
    - `-DBMI2=true`
    - `-DAVXVNNI=true`
    - `-DAVX512=true`
    - `-DVNNI512=true`

An example of building the engine for x86-64 with AVXVNNI-512 support would be:
```
cmake -Dx86=true -DVNNI512=true -S <path to Anduril> -B <path to build directory>
cmake --build <path to build directory> --target Anduril_Engine
```

## Alternative Build Option
The OpenBench framework requires Anduril to have the ability to compile using a Makefile.  This Makefile is present in the OpenBench folder.
The Makefile will also run profile guided optimization on the engine, which can improve performance.  To use the Makefile, navigate to the OpenBench folder and simply run `make`.
The Makefile will automatically detect your CPU's SIMD capabilities and compile the engine with the highest SIMD available.  This option only works on x86-64 CPUs.

# History
This is a passion project and is my third attempt at writing a chess engine.  My first engine was built from the ground up and was extremely buggy and slow.
My second attempt was built using python-chess and was better, but I started to reach the limitations of my own programming abilities.
I decided to try to make a third attempt in C++ once I got comfortable enough in the language, and this is the fruits of that labor.

