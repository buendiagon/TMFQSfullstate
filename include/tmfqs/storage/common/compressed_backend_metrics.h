#ifndef TMFQS_STORAGE_COMMON_COMPRESSED_BACKEND_METRICS_H
#define TMFQS_STORAGE_COMMON_COMPRESSED_BACKEND_METRICS_H

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace tmfqs {
namespace storage {

struct CompressedBackendMetrics {
	size_t cacheLoads = 0u;
	size_t cacheStores = 0u;
	size_t cacheFlushes = 0u;
	size_t cacheEvictions = 0u;
	size_t encodeCalls = 0u;
	size_t decodeCalls = 0u;
	size_t encodeInputBytes = 0u;
	size_t encodeOutputBytes = 0u;
	size_t decodeInputBytes = 0u;
	size_t decodeOutputBytes = 0u;
	double encodeSeconds = 0.0;
	double decodeSeconds = 0.0;
};

inline bool compressedBackendMetricsEnabled() {
	static const bool enabled = std::getenv("TMFQS_CODEC_METRICS") != nullptr ||
	                            std::getenv("TMFQS_BACKEND_METRICS") != nullptr;
	return enabled;
}

inline double elapsedSecondsSince(std::chrono::steady_clock::time_point start) {
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

inline void printCompressedBackendMetrics(const char *backendName, const CompressedBackendMetrics &metrics) {
	if(!compressedBackendMetricsEnabled()) {
		return;
	}
	std::ostringstream line;
	line
		<< "[tmfqs][backend-metrics][" << backendName << "] "
		<< "cache_loads=" << metrics.cacheLoads
		<< " cache_stores=" << metrics.cacheStores
		<< " cache_flushes=" << metrics.cacheFlushes
		<< " cache_evictions=" << metrics.cacheEvictions
		<< " encode_calls=" << metrics.encodeCalls
		<< " encode_input_bytes=" << metrics.encodeInputBytes
		<< " encode_output_bytes=" << metrics.encodeOutputBytes
		<< " encode_seconds=" << metrics.encodeSeconds
		<< " decode_calls=" << metrics.decodeCalls
		<< " decode_input_bytes=" << metrics.decodeInputBytes
		<< " decode_output_bytes=" << metrics.decodeOutputBytes
		<< " decode_seconds=" << metrics.decodeSeconds
		<< '\n';
	std::cout.flush();
	std::cerr << line.str();
}

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_COMMON_COMPRESSED_BACKEND_METRICS_H
