# PicoCollider host gate — plain g++, no MCU headers.
CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -I../include -Wall -Wextra
BUILD := build

all: $(BUILD)/test_kit

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_kit: test_kit.cc wav_io.h | $(BUILD)
	$(CXX) $(CXXFLAGS) test_kit.cc -o $@

test: all
	./$(BUILD)/test_kit

refs:
	cd ref && bash run_ugen_ref.sh

clean:
	rm -rf $(BUILD)

.PHONY: all test refs clean
