ifneq ($(filter default undefined,$(origin CXX)),)
ifneq ($(shell command -v icpx 2>/dev/null),)
CXX := icpx
else
CXX := g++
endif
endif
CXXFLAGS = -std=c++17 -Wall -Wextra
INCS = -I ../include -I ./
LIBS = -L ../lib64 -l tmfqsfs
DEBUG_FLAGS =
BUILDDIR = ../build/examples
OBJDIR = $(BUILDDIR)/obj

PROGRAMS = applyHadamard applyControlledPhaseShift applyControlledNot qft qftG grover getSumOfProbabilities
OBJECTS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(PROGRAMS)))

all: $(PROGRAMS)

$(PROGRAMS): %: $(OBJDIR)/%.o
	$(CXX) $(DEBUG_FLAGS) $< $(LIBS) -o ../bin/$@

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(INCS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)
