# Makefile required for OpenBench
# For now, this will be simple, I may expand as needed.
# Only supports Clang at the moment

# Executable
EXE=Anduril_Engine

# Compiler
CXX=clang++

# Flags
# Currently requires at least AVX2
CXXFLAGS=-std=c++20 -Wall -Wextra -Ofast -march=native -msse -msse3 -mpopcnt -mavx2 -mbmi -msse4.1 -mssse3 -msse2 -static
CXXDEFINES+=-DUSE_AVX2 -DUSE_SSE41 -DUSE_SSE3 -DUSE_SSSE3 -DUSE_SSE2 -DUSE_SSE -DNDEBUG -DIS_64BIT

CPUFLAGS = $(shell echo | $(CXX) -march=native -E -dM -)

ifneq ($(findstring __BMI2__, $(CPUFLAGS)),)
	CXXFLAGS+= -mbmi2
	CXXDEFINES+= -DUSE_PEXT
endif

ifneq ($(findstring __AVXVNNI__, $(CPUFLAGS)),)
	CXXFLAGS+= -mavxvnni
	CXXDEFINES+= -DUSE_VNNI -DUSE_AVXVNNI
endif

ifneq ($(findstring __AVX512VNNI__, $(CPUFLAGS)),)
	CXXFLAGS+= -mavx512f -mavx512bw -mavx512dq -mavx512vl -mavx512vnni -mprefer-vector-width=512
	CXXDEFINES+= -DUSE_AVX512 -DUSE_VNNI

else
ifneq ($(findstring __AVX512BW__, $(CPUFLAGS)),)
	ifneq ($(findstring __AVX512F__, $(CPUFLAGS)),)
    		CXXFLAGS+= -mavx512f -mavx512bw
    		CXXDEFINES+= -DUSE_AVX512
    endif
endif
endif


# Source files
SRC=../Anduril.cpp ../evaluation.cpp ../main.cpp ../misc.cpp ../MovePicker.cpp ../perft.cpp ../PolyglotBook.cpp ../Thread.cpp ../TranspositionTable.cpp ../UCI.cpp ../ZobristHasher.cpp ../nnue-probe/misc.cpp ../nnue-probe/nnue.cpp
OBJS=$(SRC:.cpp=.o)

# Linker flags
LDFLAGS=-flto -fuse-linker-plugin -fuse-ld=lld

all:
	$(CXX) $(CXXFLAGS) $(CXXDEFINES) $(LDFLAGS) $(SRC) -o $(EXE)