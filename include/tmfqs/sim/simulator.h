#ifndef TMFQS_SIM_SIMULATOR_H
#define TMFQS_SIM_SIMULATOR_H

#include <cstddef>
#include <string>
#include <vector>

#include "tmfqs/circuit/circuit.h"
#include "tmfqs/config/register_config.h"
#include "tmfqs/experiment/report.h"
#include "tmfqs/state/quantum_state.h"

namespace tmfqs {
namespace sim {

struct ObservabilityConfig {
	bool collectMetrics = false;
	bool traceOperations = false;
};

struct ExecutionConfig {
	RegisterConfig backend;
	ObservabilityConfig observability;
};

struct RunResult {
	state::QuantumState state;
	experiment::RunReport report;
};

class Simulator {
	public:
		Simulator() = default;
		explicit Simulator(ExecutionConfig config);

		const ExecutionConfig &config() const noexcept;
		RunResult run(const circuit::Circuit &circuit, const state::QuantumState &initialState) const;

	private:
		ExecutionConfig config_;
};

} // namespace sim
} // namespace tmfqs

#endif // TMFQS_SIM_SIMULATOR_H
