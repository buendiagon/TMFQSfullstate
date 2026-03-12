#ifndef TMFQS_CORE_RANDOM_H
#define TMFQS_CORE_RANDOM_H

#include <cstdint>
#include <random>

namespace tmfqs {

/**
 * @brief Abstract random source used by algorithms and measurement routines.
 *
 * Implementations must produce values in the half-open interval [0, 1), which
 * allows callers to use cumulative probability sampling safely.
 */
class IRandomSource {
	public:
		/** @brief Virtual destructor for polymorphic use. */
		virtual ~IRandomSource() = default;
		/**
		 * @brief Produces the next random value in [0, 1).
		 * @return Pseudo-random value suitable for probability sampling.
		 */
		virtual double nextUnitDouble() = 0;
};

/**
 * @brief PRNG-backed implementation of @ref IRandomSource using `std::mt19937`.
 *
 * This source can be deterministic (explicit seed) or non-deterministic
 * (seeded from `std::random_device`).
 */
class Mt19937RandomSource : public IRandomSource {
	public:
		/**
		 * @brief Constructs a random source seeded from `std::random_device`.
		 */
		Mt19937RandomSource();
		/**
		 * @brief Constructs a random source with a fixed seed.
		 * @param seed Seed value used to initialize the generator state.
		 */
		explicit Mt19937RandomSource(uint32_t seed);
		/**
		 * @brief Produces a uniformly distributed sample in [0, 1).
		 * @return Random sample value.
		 */
		double nextUnitDouble() override;
		/**
		 * @brief Re-initializes the generator with a new deterministic seed.
		 * @param seed New seed value.
		 */
		void reseed(uint32_t seed);

	private:
		/** @brief Pseudo-random engine state. */
		std::mt19937 engine_;
		/** @brief Uniform distribution used to map engine output to [0, 1). */
		std::uniform_real_distribution<double> distribution_{0.0, 1.0};
};

} // namespace tmfqs

#endif // TMFQS_CORE_RANDOM_H
