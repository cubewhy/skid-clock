CXX = g++
CXXFLAGS = -O3 -std=c++23 -pthread

GEN_SRC = fk_level_generator.cpp
GEN_BIN = ./fk_level_generator
PIO_INC_DIR = include
LEVELS_H = $(PIO_INC_DIR)/FKLevels.h

.PHONY: all generate build upload monitor clean

all: upload

generate: $(LEVELS_H)

$(LEVELS_H): $(GEN_SRC)
	@echo ">>> Compiling Free The Key Level Generator..."
	$(CXX) $(CXXFLAGS) $(GEN_SRC) -o $(GEN_BIN)
	@echo ">>> Running Level Generator..."
	$(GEN_BIN)

build: generate
	@echo ">>> Compiling firmware via PlatformIO..."
	pio run

upload:
	@echo ">>> Flashing firmware to ESP32..."
	pio run --target upload

monitor:
	@echo ">>> Opening PlatformIO Serial Monitor..."
	pio device monitor

clean:
	@echo ">>> Cleaning build artifacts..."
	@rm -f $(GEN_BIN)
	pio run --target clean
