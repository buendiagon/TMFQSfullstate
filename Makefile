LIBS = -L ./lib64 
subsystem:
	$(MAKE) -C src
	$(MAKE) install -C src
	$(MAKE) -C examples

clean:
	rm -f examples/*.d
	rm -f examples/*.o
	rm -f src/*.d
	rm -f src/*.o
