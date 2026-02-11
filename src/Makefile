CXX = icpx

# Specify the source files, the target files, 
# and the install directory 
SOURCES  = quantumRegister.cpp quantumGate.cpp quantumAlgorithms.cpp utils.cpp 
TARGET_LIB = libtmfqsfs.so
INSTALLDIR  = ../lib64
#INCS  = -I /usr/local/zfp/include
#LIBS = -L /usr/local/zfp/lib64 -lzfp -lstdc++
INCS  = -I ./ -I ../include
LIBS =  
CXXFLAGS += -fPIC -std=c++17 -Wall -Wextra $(INCS) $(LIBS)

# Specify extensions of files to delete when cleaning
CLEANEXTS   = o so


.PHONY: all
all: $(TARGET_LIB)

# Build lib*.so from all the *.o;
# subst is the search-and-replace 
# function demonstrated in Recipe 1.16
$(TARGET_LIB): $(subst .cpp,.o,$(SOURCES)) 
	$(CXX) -shared -fPIC $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $<

.PHONY: install
install:
	mkdir -p $(INSTALLDIR)
	cp -p $(TARGET_LIB) $(INSTALLDIR)

.PHONY: clean 
clean:
	for file in $(CLEANEXTS); do rm -f *.$$file; done

# Generate dependencies of .ccp files on .hpp files
include $(subst .cpp,.d,$(SOURCES))

%.d: %.cpp
	$(CXX) -M $(CPPFLAGS) $(INCS) $< > $@.$$$$; sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; rm -f $@.$$$$
