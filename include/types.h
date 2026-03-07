#ifndef TYPES_INCLUDE
#define TYPES_INCLUDE

#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

// Canonical index types used across the public API.
using QubitIndex = unsigned int;
using StateIndex = unsigned int;

// Complex amplitude represented as separate real/imaginary parts.
struct Amplitude {
	double real, imag;
};

// Interleaved amplitudes: [real0, imag0, real1, imag1, ...].
using AmplitudesVector = std::vector<double>;

// Distinct wrappers avoid mixing qubit lists and basis-state lists by accident.
class QubitList {

	private:
		std::vector<QubitIndex> values_;

	public:
		QubitList() = default;
		QubitList(std::initializer_list<QubitIndex> init) : values_(init) {}
		explicit QubitList(std::vector<QubitIndex> values) : values_(std::move(values)) {}

		size_t size() const noexcept { return values_.size(); }
		bool empty() const noexcept { return values_.empty(); }
		void reserve(size_t n) { values_.reserve(n); }
		void push_back(QubitIndex value) { values_.push_back(value); }

		QubitIndex operator[](size_t idx) const { return values_[idx]; }
		QubitIndex &operator[](size_t idx) { return values_[idx]; }

		std::vector<QubitIndex>::const_iterator begin() const noexcept { return values_.begin(); }
		std::vector<QubitIndex>::const_iterator end() const noexcept { return values_.end(); }
		std::vector<QubitIndex>::iterator begin() noexcept { return values_.begin(); }
		std::vector<QubitIndex>::iterator end() noexcept { return values_.end(); }

		const std::vector<QubitIndex>& values() const noexcept { return values_; }
};

class BasisStateList {

	private:
		std::vector<StateIndex> values_;

	public:
		BasisStateList() = default;
		BasisStateList(std::initializer_list<StateIndex> init) : values_(init) {}
		explicit BasisStateList(std::vector<StateIndex> values) : values_(std::move(values)) {}

		size_t size() const noexcept { return values_.size(); }
		bool empty() const noexcept { return values_.empty(); }
		void reserve(size_t n) { values_.reserve(n); }
		void push_back(StateIndex value) { values_.push_back(value); }

		StateIndex operator[](size_t idx) const { return values_[idx]; }
		StateIndex &operator[](size_t idx) { return values_[idx]; }

		std::vector<StateIndex>::const_iterator begin() const noexcept { return values_.begin(); }
		std::vector<StateIndex>::const_iterator end() const noexcept { return values_.end(); }
		std::vector<StateIndex>::iterator begin() noexcept { return values_.begin(); }
		std::vector<StateIndex>::iterator end() noexcept { return values_.end(); }

		const std::vector<StateIndex>& values() const noexcept { return values_; }
};

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
	// Chunk-cache slots used during gate application.
	size_t gateCacheSlots = 8;
};

// User-facing configuration for register storage strategy selection.
struct RegisterConfig {
	StorageStrategyKind strategy = StorageStrategyKind::Dense;
	// Used when strategy=Auto to switch from Dense to Blosc.
	size_t autoThresholdBytes = 8u * 1024u * 1024u;
	BloscConfig blosc;
};

#endif
