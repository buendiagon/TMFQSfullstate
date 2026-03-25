#ifndef TMFQS_CONFIG_REGISTER_CONFIG_H
#define TMFQS_CONFIG_REGISTER_CONFIG_H

#include <cstddef>

namespace tmfqs {

/** @brief Selects which backend implementation stores the state vector. */
enum class StorageStrategyKind {
	/** @brief Plain dense in-memory amplitude vector. */
	Dense,
	/** @brief Blosc2 chunked compression backend. */
	Blosc,
	/** @brief ZFP chunked compression backend. */
	Zfp,
	/** @brief Automatically choose backend from register size and availability. */
	Auto
};

/** @brief Describes the expected state-access pattern for backend tuning. */
enum class StorageWorkloadHint {
	/** @brief No specific workload knowledge is available. */
	Generic,
	/** @brief Repeated full-register sweeps such as Grover iterations. */
	Grover,
	/** @brief Transform-style full-register sweeps such as QFT. */
	Qft
};

/** @brief Compression tuning mode used by the ZFP backend. */
enum class ZfpCompressionMode {
	/** @brief Keep a target bits-per-value rate. */
	FixedRate,
	/** @brief Keep a target precision in significant bits. */
	FixedPrecision,
	/** @brief Keep a target absolute error. */
	FixedAccuracy
};

/** @brief Runtime configuration for the Blosc backend. */
struct BloscConfig {
	/** @brief Number of basis states grouped into one compressed chunk. */
	size_t chunkStates = 16384;
	/** @brief Compression level in `[0, 9]` (higher is usually smaller but slower). */
	int clevel = 1;
	/** @brief Number of threads used by Blosc operations. */
	int nthreads = 1;
	/** @brief Blosc compressor id (`BLOSC_*` compcode values). */
	int compcode = 1;
	/** @brief Enables byte shuffle pre-filter to improve compression ratio. */
	bool useShuffle = true;
	/** @brief Number of decompressed chunks cached during gate application. */
	size_t gateCacheSlots = 8;
};

/** @brief Per-field override markers used by backend auto-tuning. */
struct BloscTuningOverrides {
	bool chunkStates = false;
	bool clevel = false;
	bool nthreads = false;
	bool gateCacheSlots = false;
};

/** @brief Runtime configuration for the ZFP backend. */
struct ZfpConfig {
	/** @brief Active compression control mode. */
	ZfpCompressionMode mode = ZfpCompressionMode::FixedPrecision;
	/** @brief Target compressed rate used in fixed-rate mode. */
	double rate = 32.0;
	/** @brief Target precision in bits used in fixed-precision mode. */
	unsigned int precision = 40;
	/** @brief Target absolute error used in fixed-accuracy mode. */
	double accuracy = 1e-8;
	/** @brief Number of basis states grouped into one compressed chunk. */
	size_t chunkStates = 16384;
	/** @brief Number of threads used by ZFP execution. */
	int nthreads = 1;
	/** @brief Number of decompressed chunks cached during gate application. */
	size_t gateCacheSlots = 8;
};

/** @brief Per-field override markers used by backend auto-tuning. */
struct ZfpTuningOverrides {
	bool mode = false;
	bool rate = false;
	bool precision = false;
	bool accuracy = false;
	bool chunkStates = false;
	bool nthreads = false;
	bool gateCacheSlots = false;
};

/** @brief Top-level configuration for register construction and backend selection. */
struct RegisterConfig {
	/** @brief Preferred storage backend strategy. */
	StorageStrategyKind strategy = StorageStrategyKind::Dense;
	/** @brief Dense/auto cutoff in bytes when `strategy == Auto`. */
	size_t autoThresholdBytes = 8u * 1024u * 1024u;
	/** @brief Optional workload hint that guides backend tuning heuristics. */
	StorageWorkloadHint workloadHint = StorageWorkloadHint::Generic;
	/** @brief Blosc backend settings. */
	BloscConfig blosc;
	/** @brief Explicit-user override markers for Blosc knobs. */
	BloscTuningOverrides bloscOverrides;
	/** @brief ZFP backend settings. */
	ZfpConfig zfp;
	/** @brief Explicit-user override markers for ZFP knobs. */
	ZfpTuningOverrides zfpOverrides;
};

} // namespace tmfqs

#endif // TMFQS_CONFIG_REGISTER_CONFIG_H
