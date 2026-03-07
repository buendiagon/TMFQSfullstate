ifneq ($(filter default undefined,$(origin CXX)),)
ifneq ($(shell command -v icpx 2>/dev/null),)
CXX := icpx
else
CXX := g++
endif
endif

CXXFLAGS = -std=c++17 -Wall -Wextra -O3
INCS = -I ../include
LIBS = -L ../lib64 -l tmfqsfs
TARGET = ../bin/benchmark_backends

.PHONY: all run clean

all: $(TARGET)

run: $(TARGET)
	LD_LIBRARY_PATH=../lib64:$$LD_LIBRARY_PATH $(TARGET)

$(TARGET): benchmark_backends.cpp
	$(CXX) $(CXXFLAGS) $(INCS) $< $(LIBS) -o $@

clean:
	rm -f $(TARGET)
