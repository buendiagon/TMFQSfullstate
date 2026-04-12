#include "tmfqs/experiment/report.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "tmfqs/storage/factory/storage_strategy_registry.h"

namespace tmfqs {
namespace experiment {
namespace {

std::string jsonEscape(const std::string &value) {
	std::ostringstream out;
	for(char ch : value) {
		switch(ch) {
			case '\\': out << "\\\\"; break;
			case '"': out << "\\\""; break;
			case '\n': out << "\\n"; break;
			case '\r': out << "\\r"; break;
			case '\t': out << "\\t"; break;
			default: out << ch; break;
		}
	}
	return out.str();
}

void writeText(const std::string &path, const std::string &content) {
	std::ofstream out(path);
	if(!out) {
		throw std::runtime_error("Failed to open report output: " + path);
	}
	out << content;
}

} // namespace

std::string toJson(const RunReport &report) {
	std::ostringstream out;
	out << std::setprecision(17);
	out << "{\n";
	out << "  \"qubits\": " << report.qubits << ",\n";
	out << "  \"operation_count\": " << report.operationCount << ",\n";
	out << "  \"strategy\": \"" << StorageStrategyRegistry::toString(report.strategy) << "\",\n";
	out << "  \"execution_seconds\": " << report.executionSeconds << ",\n";
	out << "  \"operations\": [";
	for(size_t i = 0; i < report.operations.size(); ++i) {
		const OperationTrace &op = report.operations[i];
		out << (i == 0u ? "\n" : ",\n");
		out << "    {\"index\": " << op.index
		    << ", \"name\": \"" << jsonEscape(op.name)
		    << "\", \"seconds\": " << op.seconds << "}";
	}
	if(!report.operations.empty()) {
		out << "\n  ";
	}
	out << "]\n";
	out << "}\n";
	return out.str();
}

std::string toCsv(const RunReport &report) {
	std::ostringstream out;
	out << std::setprecision(17);
	out << "qubits,strategy,total_operations,total_seconds,operation_index,operation_name,operation_seconds\n";
	if(report.operations.empty()) {
		out << report.qubits << ','
		    << StorageStrategyRegistry::toString(report.strategy) << ','
		    << report.operationCount << ','
		    << report.executionSeconds << ",,,\n";
		return out.str();
	}
	for(const OperationTrace &op : report.operations) {
		out << report.qubits << ','
		    << StorageStrategyRegistry::toString(report.strategy) << ','
		    << report.operationCount << ','
		    << report.executionSeconds << ','
		    << op.index << ','
		    << op.name << ','
		    << op.seconds << '\n';
	}
	return out.str();
}

void writeJsonReport(const RunReport &report, const std::string &path) {
	writeText(path, toJson(report));
}

void writeCsvReport(const RunReport &report, const std::string &path) {
	writeText(path, toCsv(report));
}

} // namespace experiment
} // namespace tmfqs
