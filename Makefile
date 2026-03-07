LIBS = -L ./lib64 

.PHONY: all subsystem tests benchmarks sanitize clean

all: subsystem

subsystem:
	$(MAKE) -C src
	$(MAKE) install -C src
	$(MAKE) -C examples

tests:
	$(MAKE) -C tests

benchmarks:
	$(MAKE) -C benchmarks

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
	rm -f src/storage/*.d
	rm -f src/storage/*.o
	$(MAKE) clean -C tests
	$(MAKE) clean -C benchmarks
