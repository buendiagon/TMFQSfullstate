LIBS = -L ./lib64 

.PHONY: all subsystem tests sanitize benchmarks perf clean

all: subsystem

subsystem:
	$(MAKE) -C src
	$(MAKE) install -C src
	$(MAKE) -C examples

tests:
	$(MAKE) -C tests

benchmarks:
	$(MAKE) -C benchmarks run

perf:
	$(MAKE) clean -C src
	$(MAKE) -C src EXTRA_CXXFLAGS="-O3 -DNDEBUG"
	$(MAKE) install -C src
	$(MAKE) -C tests perf
	$(MAKE) -C benchmarks run

sanitize:
	$(MAKE) clean
	$(MAKE) -C src EXTRA_CXXFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
		EXTRA_LDFLAGS="-fsanitize=address,undefined"
	$(MAKE) install -C src
	$(MAKE) -C tests sanitize

clean:
	rm -rf build
	$(MAKE) clean -C benchmarks
	$(MAKE) clean -C tests
