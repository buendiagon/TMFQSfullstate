ifneq ($(filter default undefined,$(origin CXX)),)
ifneq ($(shell command -v icpx 2>/dev/null),)
CXX := icpx
else
CXX := g++
endif
endif

CXXFLAGS = -std=c++17 -Wall -Wextra
INCS = -I ../include
LIBS = -L ../lib64 -l tmfqsfs

.PHONY: all integration clean

all: integration

integration: ../bin/test_bugfixes
	../bin/test_bugfixes

../bin/test_bugfixes: integration/test_bugfixes.cpp
	$(CXX) $(CXXFLAGS) $(INCS) $< $(LIBS) -o $@

clean:
	rm -f ../bin/test_bugfixes
