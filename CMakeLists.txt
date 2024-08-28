cmake_minimum_required(VERSION 3.16)
project(Anduril_Engine)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")

# add warning flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# USAGE:
# First set architecture: x86, apple-silicon, arm neon, arm dotprod
# Second set extra hardware extensions: BMI2, AVXVNNI, AVX-152, VNNI512
if(x86)

    # set the compile to be statically linked
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")

    # minimum requirements: AVX2, 64 bit, popcount, sse, sse3, sse3 sse4.1, ssse3
    add_compile_options(-march=native -mpopcnt -msse -msse2 -msse3 -msse4.1 -mssse3 -mavx2 -mbmi)
    add_compile_definitions(IS_64BIT USE_SSE USE_SSE2 USE_SSE3 USE_SSE41 USE_SSSE3 USE_AVX2)

    # set hardware extension level
    if(BMI2)
        add_compile_options(-mbmi2)
        add_compile_definitions(USE_PEXT)
    endif()

    if(AVXVNNI)
        add_compile_options(-mbmi2 -mavxvnni)
        add_compile_definitions(USE_PEXT USE_VNNI USE_AVXVNNI)
    endif()

    if(AVX-512)
        add_compile_options(-mbmi2 -mavx512f -mavx512bw)
        add_compile_definitions(USE_PEXT USE_AVX512)
    endif()

    if(VNNI512)
        add_compile_options(-mbmi2 -mavx512f -mavx512bw -mavx512dq -mavx512vl -mavx512vnni -mprefer-vector-width=512)
        add_compile_definitions(USE_PEXT USE_AVX512 USE_VNNI)
    endif()


elseif (apple-silicon)

    add_compile_options(-march=native -mfpu=neon)
    add_compile_definitions(IS_64BIT USE_NEON=8 USE_NEON_DOTPROD)

elseif (arm-neon)

    add_compile_options(-march=native -mfpu=neon)
    add_compile_definitions(IS_64BIT USE_NEON=7)

elseif(arm-dotprod)

    add_compile_options(-march=arm8.2-a+dotprod -mfpu=neon)
    add_compile_definitions(IS_64BIT USE_NEON=8 USE_NEON_DOTPROD)

endif()

# add debug flags if needed
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g)
endif()

# release with debug flags
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_options(-g -O2)
    if (!apple-silicon)
        add_link_options(-fuse-ld=lld)
    endif ()
endif()

# add release flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-Ofast -flto -fuse-linker-plugin)
    add_compile_definitions(NDEBUG)
    if (!apple-silicon)
        add_link_options(-fuse-ld=lld)
    endif ()
endif()

# add profile generation
if (DEFINED MAKE_PROFILE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate")
endif()

add_executable(Anduril_Engine main.cpp Anduril.cpp Anduril.h Node.h PolyglotBook.cpp PolyglotBook.h TranspositionTable.cpp TranspositionTable.h evaluation.cpp UCI.cpp UCI.h limit.h ZobristHasher.cpp ZobristHasher.h MovePicker.cpp MovePicker.h History.h misc.cpp misc.h Thread.cpp Thread.h
        nnue-probe/nnue.h
        nnue-probe/nnue.cpp
        nnue-probe/misc.cpp
        perft.cpp)
