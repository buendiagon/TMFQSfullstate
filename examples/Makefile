ifneq ($(filter default undefined,$(origin CXX)),)
ifneq ($(shell command -v icpx 2>/dev/null),)
CXX := icpx
else
CXX := g++
endif
endif
CXXFLAGS = -c -std=c++17 -Wall -Wextra
INCS = -I ../include -I ./
LIBS = -L ../lib64 -l tmfqsfs
DEBUG_FLAGS =

PROGRAMS = applyHadamard applyControlledPhaseShift applyControlledNot qft qftG grover getSumOfProbabilities
OBJECTS = $(addsuffix .o,$(PROGRAMS))

all: $(PROGRAMS)

$(PROGRAMS): %: %.o
	$(CXX) $(DEBUG_FLAGS) $< $(LIBS) -o ../bin/$@

%.o: %.cpp
	$(CXX) $(INCS) $(CXXFLAGS) $< -o $@

clean:
	rm -f *.o
