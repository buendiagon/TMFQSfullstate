ifneq ($(filter default undefined,$(origin CXX)),)
ifneq ($(shell command -v icpx 2>/dev/null),)
CXX := icpx
else
CXX := g++
endif
endif

CXXFLAGS = -std=c++17 -Wall -Wextra

.PHONY: all clean

all:
	@echo "No benchmark targets are defined yet."

clean:
	@true
