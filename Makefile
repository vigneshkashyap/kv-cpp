    CXX ?= clang++
    CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
    INC := -Iinclude
    SRC := src/engine.cpp src/repl.cpp
    BIN := kv

    all: $(BIN)

    $(BIN): $(SRC) include/engine.h
	$(CXX) $(CXXFLAGS) $(INC) $(SRC) -o $@

    clean:
	rm -f $(BIN)
