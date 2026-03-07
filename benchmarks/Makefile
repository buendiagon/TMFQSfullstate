ifneq ($(filter default undefined,$(origin CXX)),)
ifneq ($(shell command -v icpx 2>/dev/null),)
CXX := icpx
else
CXX := g++
endif
endif

CXXFLAGS = -std=c++17 -O3 -DNDEBUG -Wall -Wextra
INCS = -I ../include
LIBS = -L ../lib64 -l tmfqsfs

.PHONY: all run clean

all: ../bin/benchmark_backends

run: all
	LD_LIBRARY_PATH=../lib64:$$LD_LIBRARY_PATH ../bin/benchmark_backends

../bin/benchmark_backends: benchmark_backends.cpp
	$(CXX) $(CXXFLAGS) $(INCS) $< $(LIBS) -o $@

clean:
	rm -f ../bin/benchmark_backends
