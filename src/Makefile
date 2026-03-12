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
          tmfqs/storage/factory/state_backend_factory.cpp \
          tmfqs/storage/factory/storage_strategy_registry.cpp \
          tmfqs/storage/dense/dense_state_backend.cpp \
          tmfqs/storage/blosc/blosc_codec.cpp \
          tmfqs/storage/blosc/blosc_state_backend.cpp \
          tmfqs/storage/zfp/zfp_codec.cpp \
          tmfqs/storage/zfp/zfp_state_backend.cpp
TARGET_LIB = libtmfqsfs.so
INSTALLDIR = ../lib64
INCS = -I ./ -I ../include
LIBS =
BUILDDIR = ../build/src
OBJDIR = $(BUILDDIR)/obj
DEPDIR = $(BUILDDIR)/dep
OBJECTS = $(addprefix $(OBJDIR)/,$(SOURCES:.cpp=.o))
DEPS = $(addprefix $(DEPDIR)/,$(SOURCES:.cpp=.d))

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

.PHONY: all
all: $(TARGET_LIB)

$(TARGET_LIB): $(OBJECTS)
	$(CXX) -shared -fPIC $(LDFLAGS) $(EXTRA_LDFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@) $(dir $(DEPDIR)/$*.d)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF $(DEPDIR)/$*.d -c $< -o $@

.PHONY: install
install:
	mkdir -p $(INSTALLDIR)
	cp -p $(TARGET_LIB) $(INSTALLDIR)

.PHONY: clean
clean:
	rm -rf $(BUILDDIR)
	rm -f $(TARGET_LIB)

-include $(DEPS)
