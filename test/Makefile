# Host unit tests for AF1 with a 100% line + branch coverage gate.
#   make          build, run the tests, check coverage
#   make clean
ARDUINOJSON ?= $(HOME)/Arduino/libraries/ArduinoJson/src
BUILD := build
SRCS := ../AF1/src/AF1.cpp ../AF1/src/F1Data.cpp checkpoint.cpp test_af1.cpp
# paths are relative to $(BUILD), where the compiler runs so gcov's files land there
CXXFLAGS := -std=gnu++17 -O0 -g --coverage -Wall -Wno-unused-function \
  -I../fakes -I../../AF1/src -I$(abspath $(ARDUINOJSON)) \
  -DARDUINOJSON_ENABLE_ARDUINO_STREAM=1 -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 \
  -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0
LDFLAGS := --coverage -Wl,--wrap=time,--wrap=gettimeofday

all: coverage

$(BUILD)/tests: $(SRCS) $(wildcard fakes/*.h) ../AF1/src/AF1.h ../CheckpointA/CheckpointA.ino
	@mkdir -p $(BUILD)
	rm -f $(BUILD)/*.gcda
	cd $(BUILD) && $(CXX) $(CXXFLAGS) $(addprefix ../,$(SRCS)) $(LDFLAGS) -o tests

test: $(BUILD)/tests
	rm -f $(BUILD)/*.gcda
	$(BUILD)/tests

coverage: test
	cd $(BUILD) && gcov -b -c tests-AF1.gcda tests-F1Data.gcda tests-checkpoint.gcda >/dev/null
	python3 coverage.py $(BUILD)/AF1.cpp.gcov $(BUILD)/F1Data.cpp.gcov $(BUILD)/CheckpointA.ino.gcov

clean:
	rm -rf $(BUILD)

.PHONY: all test coverage clean
