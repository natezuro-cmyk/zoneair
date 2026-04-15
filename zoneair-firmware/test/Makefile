CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O0 -g

SRC = ../src/protocol/tuya.cpp test_tuya_protocol.cpp
OUT = test_tuya_protocol

all: run

$(OUT): ../src/protocol/tuya.cpp ../src/protocol/tuya.h ../src/state/ac_state.h test_tuya_protocol.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_tuya_protocol.cpp ../src/protocol/tuya.cpp

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT)
	rm -rf $(OUT).dSYM

.PHONY: all run clean
