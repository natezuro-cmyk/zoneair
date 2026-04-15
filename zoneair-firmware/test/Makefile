CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O0 -g

SRC = ../src/protocol/tcl.cpp test_tcl_protocol.cpp
OUT = test_tcl_protocol

# tcl.cpp is compiled only when it exists; until then, the link step produces
# the expected "undefined reference to TclProtocol::buildQuery" linker error.
TCL_CPP = ../src/protocol/tcl.cpp
TCL_OBJ = $(if $(wildcard $(TCL_CPP)),$(TCL_CPP),)

all: run

$(OUT): $(wildcard $(TCL_CPP)) test_tcl_protocol.cpp ../src/protocol/tcl.h ../src/state/ac_state.h fixtures/tcl_frames.h
	$(CXX) $(CXXFLAGS) -o $@ test_tcl_protocol.cpp $(TCL_OBJ)

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT)

.PHONY: all run clean
