ifneq ($(filter default undefined,$(origin CXX)),)
ifneq ($(shell command -v icpx 2>/dev/null),)
CXX := icpx
else
CXX := g++
endif
endif

SOURCES = tmfqs/register/quantum_register.cpp \
          tmfqs/gates/quantum_gate.cpp \
          tmfqs/core/math.cpp \
          tmfqs/core/random.cpp \
          tmfqs/algorithms/operations.cpp \
          tmfqs/algorithms/qft.cpp \
          tmfqs/algorithms/grover.cpp \
          tmfqs/storage/StateBackendFactory.cpp \
          tmfqs/storage/DenseStateBackend.cpp \
          tmfqs/storage/BloscStateBackend.cpp \
          tmfqs/storage/ZfpStateBackend.cpp
TARGET_LIB = libtmfqsfs.so
INSTALLDIR = ../lib64
INCS = -I ./ -I ../include
LIBS =

ifneq ($(wildcard /usr/include/blosc2.h),)
CXXFLAGS += -DHAVE_BLOSC2
LIBS += -lblosc2
endif

ifneq ($(wildcard /usr/include/zfp.h),)
CXXFLAGS += -DHAVE_ZFP
LIBS += -lzfp
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
	rm -f tmfqs/*/*.o tmfqs/*/*.d

include $(subst .cpp,.d,$(SOURCES))

%.d: %.cpp
	$(CXX) -M $(CPPFLAGS) $(INCS) $< > $@.$$$$; sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; rm -f $@.$$$$
