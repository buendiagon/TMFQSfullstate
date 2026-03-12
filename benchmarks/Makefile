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
BUILDDIR = ../build/benchmarks
OBJDIR = $(BUILDDIR)/obj

.PHONY: all run compare clean

all: ../bin/benchmark_backends ../bin/benchmark_regression_check

run: all
	LD_LIBRARY_PATH=../lib64:$$LD_LIBRARY_PATH ../bin/benchmark_backends

compare: all
	@if [ -z "$(BASELINE)" ] || [ -z "$(CANDIDATE)" ]; then \
		echo "Usage: make compare BASELINE=<baseline.csv> CANDIDATE=<candidate.csv> [THRESHOLD=0.05]"; \
		exit 2; \
	fi
	../bin/benchmark_regression_check "$(BASELINE)" "$(CANDIDATE)" "$(if $(THRESHOLD),$(THRESHOLD),0.05)"

../bin/benchmark_backends: $(OBJDIR)/benchmark_backends.o
	$(CXX) $(CXXFLAGS) $< $(LIBS) -o $@

../bin/benchmark_regression_check: $(OBJDIR)/benchmark_regression_check.o
	$(CXX) $(CXXFLAGS) $< -o $@

$(OBJDIR)/benchmark_backends.o: benchmark_backends.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

$(OBJDIR)/benchmark_regression_check.o: benchmark_regression_check.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)
	rm -f ../bin/benchmark_backends ../bin/benchmark_regression_check
