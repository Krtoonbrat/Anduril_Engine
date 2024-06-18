cmake_minimum_required(VERSION 3.16)
project(Anduril_Engine)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")

# set the compile to be statically linked
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")

# add warning flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# add debug flags if needed
#if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -Og -ginline-points")
#endif()

if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2 -march=native -flto -fuse-ld=lld -msse -msse3 -mpopcnt -mavx2 -mbmi -msse4.1 -mssse3 -msse2")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
endif()

# add release flags
# I am going to assume the processor is at least avx2 compatible and make BMI2(for PEXT) up to the user
# add these switches for avx512: -mavx512f -mavx512bw -mavx512cd -mavx512dq -mavx512vl
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-Ofast -march=native -flto -fuse-linker-plugin -msse -msse3 -mpopcnt -mavx2 -mbmi -msse4.1 -mssse3 -msse2)
    add_compile_definitions(NDEBUG)
    add_link_options(-fuse-ld=lld)
endif()

# add profile generation
if (DEFINED MAKE_PROFILE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate")
endif()

# add pext if defined
if (USE_PEXT)
    add_compile_options(-mbmi2)
    add_compile_definitions(USE_PEXT)
endif()

add_executable(Anduril_Engine main.cpp Anduril.cpp Anduril.h Node.h PolyglotBook.cpp PolyglotBook.h TranspositionTable.cpp TranspositionTable.h evaluation.cpp UCI.cpp UCI.h limit.h ZobristHasher.cpp ZobristHasher.h MovePicker.cpp MovePicker.h History.h misc.cpp misc.h Thread.cpp Thread.h)
