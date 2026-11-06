# Host unit tests with a 100% line + branch coverage gate. One test binary per target:
#   make                 everything
#   make lib             AF1 library + CheckpointA        (test_lib.cpp)
#   make map             MapBoard sketch                  (test_map.cpp)
#   make standings       StandingsBoard sketch            (test_standings.cpp)
#   make drama           DramaBoard sketch                (test_drama.cpp)
# A board's test binary links the real AF1 library, the board's sketch (its .ino plus any .cpp
# files in the sketch folder), the shared fakes and its test file. Coverage is checked on the
# board's own files; the library's coverage is checked by `make lib`.
ARDUINOJSON ?= $(HOME)/Arduino/libraries/ArduinoJson/src
PNGDEC ?= $(HOME)/Arduino/libraries/PNGdec/src
TEST := $(CURDIR)
ROOT := $(abspath $(TEST)/..)
CXXFLAGS := -std=gnu++17 -O0 -g --coverage -Wall -Wno-unused-function \
  -I$(TEST)/fakes -I$(ROOT)/AF1/src -I$(abspath $(ARDUINOJSON)) -I$(abspath $(PNGDEC)) \
  -DARDUINOJSON_ENABLE_ARDUINO_STREAM=1 -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 \
  -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0
LDFLAGS := --coverage -Wl,--wrap=time,--wrap=gettimeofday
LIB := $(ROOT)/AF1/src/AF1.cpp $(ROOT)/AF1/src/F1Data.cpp $(TEST)/fakes/fakes.cpp
HEADERS := $(wildcard $(TEST)/fakes/*.h) $(ROOT)/AF1/src/AF1.h

all: lib map standings drama

# $(1)=target $(2)=sketch folder (empty for lib) $(3)=extra sources
define board
$(1): build/$(1)/tests
	rm -f build/$(1)/*.gcda
	build/$(1)/tests
	cd build/$(1) && gcov -b -c *.gcda >/dev/null
	python3 coverage.py $$(GCOV_$(1))

build/$(1)/tests: $(LIB) $(HEADERS) $(TEST)/test_$(1).cpp $(3) $$(wildcard $(ROOT)/$(2)/*)
	@mkdir -p build/$(1)
	rm -f build/$(1)/*.gcda build/$(1)/*.gcno
	cd build/$(1) && $(CXX) $(CXXFLAGS) $(LIB) $(3) $(TEST)/test_$(1).cpp $(LDFLAGS) -o tests
endef

# Sketch wrappers: compile the board's .ino as C++ (plus its other .cpp files)
SKETCH = $(TEST)/sketch_$(1).cpp $(filter-out %/maps.cpp,$(wildcard $(ROOT)/$(2)/*.cpp))

GCOV_lib := build/lib/AF1.cpp.gcov build/lib/F1Data.cpp.gcov build/lib/CheckpointA.ino.gcov
GCOV_map := $(patsubst %,build/map/%.gcov,MapBoard.ino $(notdir $(wildcard $(ROOT)/MapBoard/*.cpp)))
GCOV_standings := $(patsubst %,build/standings/%.gcov,StandingsBoard.ino $(notdir $(wildcard $(ROOT)/StandingsBoard/*.cpp)))
GCOV_drama := $(patsubst %,build/drama/%.gcov,DramaBoard.ino $(notdir $(wildcard $(ROOT)/DramaBoard/*.cpp)))

$(eval $(call board,lib,CheckpointA,$(TEST)/sketch_lib.cpp))
$(eval $(call board,map,MapBoard,$(call SKETCH,map,MapBoard) $(wildcard $(ROOT)/MapBoard/src/*.cpp) $(wildcard $(abspath $(PNGDEC))/*.c*)))
$(eval $(call board,standings,StandingsBoard,$(call SKETCH,standings,StandingsBoard)))
$(eval $(call board,drama,DramaBoard,$(call SKETCH,drama,DramaBoard)))

clean:
	rm -rf build

.PHONY: all lib map standings drama clean
