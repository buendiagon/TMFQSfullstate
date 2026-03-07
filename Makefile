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
	rm -f examples/*.d
	rm -f examples/*.o
	rm -f src/*.d
	rm -f src/*.o
	rm -f src/tmfqs/core/*.d src/tmfqs/core/*.o
	rm -f src/tmfqs/gates/*.d src/tmfqs/gates/*.o
	rm -f src/tmfqs/register/*.d src/tmfqs/register/*.o
	rm -f src/tmfqs/algorithms/*.d src/tmfqs/algorithms/*.o
	rm -f src/tmfqs/storage/*.d src/tmfqs/storage/*.o
	$(MAKE) clean -C benchmarks
	$(MAKE) clean -C tests
