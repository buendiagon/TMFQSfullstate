#ifndef TMFQS_CORE_RANDOM_H
#define TMFQS_CORE_RANDOM_H

#include <cstdint>
#include <random>

namespace tmfqs {

class IRandomSource {
	public:
		virtual ~IRandomSource() = default;
		virtual double nextUnitDouble() = 0;
};

class Mt19937RandomSource : public IRandomSource {
	public:
		Mt19937RandomSource();
		explicit Mt19937RandomSource(uint32_t seed);
		double nextUnitDouble() override;
		void reseed(uint32_t seed);

	private:
		std::mt19937 engine_;
		std::uniform_real_distribution<double> distribution_{0.0, 1.0};
};

} // namespace tmfqs

#endif // TMFQS_CORE_RANDOM_H
