ifneq ($(filter default undefined,$(origin CXX)),)
ifneq ($(shell command -v icpx 2>/dev/null),)
CXX := icpx
else
CXX := g++
endif
endif

CXXFLAGS = -std=c++17 -Wall -Wextra
PERF_CXXFLAGS = -std=c++17 -O3 -Wall -Wextra
INCS = -I ../include
LIBS = -L ../lib64 -l tmfqsfs
BUILDDIR = ../build/tests
OBJDIR = $(BUILDDIR)/obj

.PHONY: all integration sanitize perf clean

all: integration

TEST_NAMES = algorithms register_validation storage_parity storage_cache
TEST_SOURCES = integration/algorithms/test_algorithms.cpp \
               integration/register/test_register_validation.cpp \
               integration/storage/test_storage_parity.cpp \
               integration/storage/test_storage_cache.cpp
TEST_BINS = ../bin/test_algorithms ../bin/test_register_validation ../bin/test_storage_parity ../bin/test_storage_cache
TEST_OBJS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(TEST_NAMES)))

integration: $(TEST_BINS)
	@for test_bin in $(TEST_BINS); do \
		LD_LIBRARY_PATH=../lib64:$$LD_LIBRARY_PATH $$test_bin; \
	done
	@echo "All integration tests passed."

../bin/test_algorithms: $(OBJDIR)/algorithms.o
	$(CXX) $(CXXFLAGS) $< $(LIBS) -o $@

../bin/test_register_validation: $(OBJDIR)/register_validation.o
	$(CXX) $(CXXFLAGS) $< $(LIBS) -o $@

../bin/test_storage_parity: $(OBJDIR)/storage_parity.o
	$(CXX) $(CXXFLAGS) $< $(LIBS) -o $@

../bin/test_storage_cache: $(OBJDIR)/storage_cache.o
	$(CXX) $(CXXFLAGS) $< $(LIBS) -o $@

$(OBJDIR)/algorithms.o: integration/algorithms/test_algorithms.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

$(OBJDIR)/register_validation.o: integration/register/test_register_validation.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

$(OBJDIR)/storage_parity.o: integration/storage/test_storage_parity.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

$(OBJDIR)/storage_cache.o: integration/storage/test_storage_cache.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

sanitize: $(TEST_BINS)
	@for test_bin in $(TEST_BINS); do \
		LD_LIBRARY_PATH=../lib64:$$LD_LIBRARY_PATH ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 $$test_bin; \
	done

perf: $(TEST_BINS)
	@for test_bin in $(TEST_BINS); do \
		LD_LIBRARY_PATH=../lib64:$$LD_LIBRARY_PATH $$test_bin; \
	done

clean:
	rm -rf $(BUILDDIR)
	rm -f $(TEST_BINS)
