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

.PHONY: all integration sanitize clean

all: integration

integration: ../bin/test_bugfixes
	../bin/test_bugfixes

../bin/test_bugfixes: integration/test_bugfixes.cpp
	$(CXX) $(CXXFLAGS) $(INCS) $< $(LIBS) -o $@

sanitize: ../bin/test_bugfixes_sanitize
	LD_LIBRARY_PATH=../lib64:$$LD_LIBRARY_PATH ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
		../bin/test_bugfixes_sanitize

../bin/test_bugfixes_sanitize: integration/test_bugfixes.cpp
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(INCS) \
		$< $(LIBS) -fsanitize=address,undefined -o $@

clean:
	rm -f ../bin/test_bugfixes ../bin/test_bugfixes_sanitize
