# Makefile required for OpenBench
# For now, this will be simple, I may expand as needed.
# Only supports Clang at the moment

# Executable
EXE=Anduril_Engine

# Compiler
CXX=clang++

# Flags
# Currently requires at least BMI2 and AVX2
CXXFLAGS=-std=c++20 -Wall -Wextra -Ofast -march=native -DNDEBUG -msse -msse3 -mpopcnt -mavx2 -mbmi -msse4.1 -mssse3 -msse2 -mbmi2 -DUSE_PEXT -static

# Source files
SRC=../Anduril.cpp ../evaluation.cpp ../main.cpp ../misc.cpp ../MovePicker.cpp ../PolyglotBook.cpp ../Thread.cpp ../TranspositionTable.cpp ../UCI.cpp ../ZobristHasher.cpp
OBJS=$(SRC:.cpp=.o)

# Linker flags
LDFLAGS=-flto -fuse-linker-plugin -fuse-ld=lld

all:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SRC) -o $(EXE)