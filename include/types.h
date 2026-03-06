#ifndef TYPES_INCLUDE
#define TYPES_INCLUDE

#include <cstddef>
#include <vector>

struct Amplitude {
	double real, imag;
};

using AmplitudesVector = std::vector<double>;
using StatesVector = std::vector<unsigned int>;
using IntegerVector = std::vector<unsigned int>;

enum class StorageStrategyKind {
	Dense,
	Blosc,
	Auto
};

struct BloscConfig {
	size_t chunkStates = 16384;
	int clevel = 1;
	int nthreads = 1;
	int compcode = 1; // BLOSC_LZ4
	bool useShuffle = true;
};

struct RegisterConfig {
	StorageStrategyKind strategy = StorageStrategyKind::Dense;
	size_t autoThresholdBytes = 8u * 1024u * 1024u;
	BloscConfig blosc;
};

#endif
