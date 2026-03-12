#ifndef TMFQS_CONFIG_REGISTER_CONFIG_H
#define TMFQS_CONFIG_REGISTER_CONFIG_H

#include <cstddef>

namespace tmfqs {

enum class StorageStrategyKind {
	Dense,
	Blosc,
	Zfp,
	Auto
};

enum class ZfpCompressionMode {
	FixedRate,
	FixedPrecision,
	FixedAccuracy
};

struct BloscConfig {
	size_t chunkStates = 16384;
	int clevel = 1;
	int nthreads = 1;
	int compcode = 1;
	bool useShuffle = true;
	size_t gateCacheSlots = 8;
};

struct ZfpConfig {
	ZfpCompressionMode mode = ZfpCompressionMode::FixedRate;
	double rate = 32.0;
	unsigned int precision = 32;
	double accuracy = 1e-8;
	size_t chunkStates = 16384;
	size_t gateCacheSlots = 8;
};

struct RegisterConfig {
	StorageStrategyKind strategy = StorageStrategyKind::Dense;
	size_t autoThresholdBytes = 8u * 1024u * 1024u;
	BloscConfig blosc;
	ZfpConfig zfp;
};

} // namespace tmfqs

#endif // TMFQS_CONFIG_REGISTER_CONFIG_H
