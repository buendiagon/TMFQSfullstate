#ifndef TMFQS_CORE_TYPES_H
#define TMFQS_CORE_TYPES_H

#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

namespace tmfqs {

using QubitIndex = unsigned int;
using StateIndex = unsigned int;

struct Amplitude {
	double real;
	double imag;
};

using AmplitudesVector = std::vector<double>;

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

} // namespace tmfqs

#endif // TMFQS_CORE_TYPES_H
