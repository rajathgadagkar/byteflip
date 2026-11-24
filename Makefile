CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude

BUILD_DIR := build
BIN_DIR   := $(BUILD_DIR)/bin
OBJ_DIR   := $(BUILD_DIR)/obj

REVERSER_BIN := $(BIN_DIR)/byteflip
VERIFIER_BIN := $(BIN_DIR)/byteflip-verify
DATAGEN_BIN  := $(BIN_DIR)/byteflip-datagen

COMMON_OBJ   := $(OBJ_DIR)/sys_io.o
REVERSER_OBJ := $(OBJ_DIR)/reverser.o $(OBJ_DIR)/reverser_main.o
VERIFIER_OBJ := $(OBJ_DIR)/verifier.o $(OBJ_DIR)/verifier_main.o

.PHONY: all clean test dirs

all: $(REVERSER_BIN) $(VERIFIER_BIN) $(DATAGEN_BIN)

dirs:
	@mkdir -p $(BIN_DIR) $(OBJ_DIR)

$(OBJ_DIR)/sys_io.o: src/common/sys_io.cpp include/common.hpp include/sys_io.hpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/reverser.o: src/reverser/reverser.cpp include/common.hpp include/sys_io.hpp include/reverser.hpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/reverser_main.o: src/reverser/main.cpp include/common.hpp include/sys_io.hpp include/reverser.hpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/verifier.o: src/verifier/verifier.cpp include/common.hpp include/sys_io.hpp include/verifier.hpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/verifier_main.o: src/verifier/main.cpp include/common.hpp include/sys_io.hpp include/verifier.hpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(REVERSER_BIN): $(COMMON_OBJ) $(REVERSER_OBJ) | dirs
	$(CXX) $(CXXFLAGS) $^ -o $@

$(VERIFIER_BIN): $(COMMON_OBJ) $(VERIFIER_OBJ) | dirs
	$(CXX) $(CXXFLAGS) $^ -o $@

$(DATAGEN_BIN): tools/datagen.cpp | dirs
	$(CXX) $(CXXFLAGS) $< -o $@

test: all
	@bash tests/run_tests.sh

clean:
	rm -rf $(BUILD_DIR) output
