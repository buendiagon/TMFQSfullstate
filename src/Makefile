ifneq ($(filter default undefined,$(origin CXX)),)
ifneq ($(shell command -v icpx 2>/dev/null),)
CXX := icpx
else
CXX := g++
endif
endif

SOURCES = quantumRegister.cpp quantumGate.cpp quantumAlgorithms.cpp utils.cpp \
          storage/StateBackendFactory.cpp storage/DenseStateBackend.cpp storage/BloscStateBackend.cpp
TARGET_LIB = libtmfqsfs.so
INSTALLDIR = ../lib64
INCS = -I ./ -I ../include
LIBS =

ifneq ($(wildcard /usr/include/blosc2.h),)
CXXFLAGS += -DHAVE_BLOSC2
LIBS += -lblosc2
endif

CXXFLAGS += -fPIC -std=c++17 -Wall -Wextra $(INCS)
CXXFLAGS += $(EXTRA_CXXFLAGS)
CLEANEXTS = o so

.PHONY: all
all: $(TARGET_LIB)

$(TARGET_LIB): $(subst .cpp,.o,$(SOURCES))
	$(CXX) -shared -fPIC $(LDFLAGS) $(EXTRA_LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.PHONY: install
install:
	mkdir -p $(INSTALLDIR)
	cp -p $(TARGET_LIB) $(INSTALLDIR)

.PHONY: clean
clean:
	for file in $(CLEANEXTS); do rm -f *.$$file; done
	rm -f storage/*.o storage/*.d

include $(subst .cpp,.d,$(SOURCES))

%.d: %.cpp
	$(CXX) -M $(CPPFLAGS) $(INCS) $< > $@.$$$$; sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; rm -f $@.$$$$
