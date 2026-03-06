LIBS = -L ./lib64 

.PHONY: all subsystem tests benchmarks clean

all: subsystem

subsystem:
	$(MAKE) -C src
	$(MAKE) install -C src
	$(MAKE) -C examples

tests:
	$(MAKE) -C tests

benchmarks:
	$(MAKE) -C benchmarks

clean:
	rm -f examples/*.d
	rm -f examples/*.o
	rm -f src/*.d
	rm -f src/*.o
	$(MAKE) clean -C tests
	$(MAKE) clean -C benchmarks
