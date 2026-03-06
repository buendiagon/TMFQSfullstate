#ifndef TYPES_INCLUDE
#define TYPES_INCLUDE

#include <cstddef>
#include <vector>

// Complex amplitude represented as separate real/imaginary parts.
struct Amplitude {
	double real, imag;
};

// Interleaved amplitudes: [real0, imag0, real1, imag1, ...].
using AmplitudesVector = std::vector<double>;
// Explicit list of basis-state indexes.
using StatesVector = std::vector<unsigned int>;
// Generic list of qubit indexes used by gate application APIs.
using IntegerVector = std::vector<unsigned int>;

// Runtime-selectable storage backend.
enum class StorageStrategyKind {
	Dense,
	Blosc,
	Auto
};

// Tunables for the Blosc backend.
struct BloscConfig {
	// Number of basis states per compressed chunk.
	size_t chunkStates = 16384;
	// Compression level: 0 (fast) .. 9 (higher ratio).
	int clevel = 1;
	// Number of threads used by Blosc internal work.
	int nthreads = 1;
	// Compression codec id (1 maps to BLOSC_LZ4).
	int compcode = 1; // BLOSC_LZ4
	// Enables byte/bit shuffle preconditioner when supported.
	bool useShuffle = true;
};

// User-facing configuration for register storage strategy selection.
struct RegisterConfig {
	StorageStrategyKind strategy = StorageStrategyKind::Dense;
	// Used when strategy=Auto to switch from Dense to Blosc.
	size_t autoThresholdBytes = 8u * 1024u * 1024u;
	BloscConfig blosc;
};

#endif
