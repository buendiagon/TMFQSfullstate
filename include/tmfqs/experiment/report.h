#ifndef TMFQS_EXPERIMENT_REPORT_H
#define TMFQS_EXPERIMENT_REPORT_H

#include <cstddef>
#include <string>
#include <vector>

#include "tmfqs/config/register_config.h"

namespace tmfqs {
namespace experiment {

struct OperationTrace {
	size_t index = 0u;
	std::string name;
	double seconds = 0.0;
};

struct RunReport {
	unsigned int qubits = 0;
	size_t operationCount = 0u;
	StorageStrategyKind strategy = StorageStrategyKind::Dense;
	double executionSeconds = 0.0;
	std::vector<OperationTrace> operations;
};

std::string toJson(const RunReport &report);
std::string toCsv(const RunReport &report);
void writeJsonReport(const RunReport &report, const std::string &path);
void writeCsvReport(const RunReport &report, const std::string &path);

} // namespace experiment
} // namespace tmfqs

#endif // TMFQS_EXPERIMENT_REPORT_H
