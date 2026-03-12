#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

struct Metric {
	double seconds = 0.0;
	unsigned int iterations = 0;
};

std::unordered_map<std::string, Metric> readCsv(const std::string &path) {
	std::ifstream in(path);
	if(!in.is_open()) {
		throw std::runtime_error("Failed to open CSV: " + path);
	}

	std::unordered_map<std::string, Metric> metrics;
	std::string line;
	bool firstLine = true;
	while(std::getline(in, line)) {
		if(line.empty()) continue;
		if(firstLine) {
			firstLine = false;
			continue;
		}
		std::stringstream ss(line);
		std::string name;
		std::string secondsText;
		std::string iterationsText;
		if(!std::getline(ss, name, ',')) continue;
		if(!std::getline(ss, secondsText, ',')) continue;
		if(!std::getline(ss, iterationsText, ',')) continue;
		Metric metric;
		metric.seconds = std::stod(secondsText);
		metric.iterations = static_cast<unsigned int>(std::stoul(iterationsText));
		metrics[name] = metric;
	}
	return metrics;
}

} // namespace

int main(int argc, char *argv[]) {
	if(argc < 3 || argc > 4) {
		std::cerr << "Usage: benchmark_regression_check <baseline.csv> <candidate.csv> [max_regression_ratio]\n";
		return 2;
	}

	const std::string baselinePath = argv[1];
	const std::string candidatePath = argv[2];
	const double threshold = (argc == 4) ? std::atof(argv[3]) : 0.05;
	if(threshold < 0.0) {
		std::cerr << "max_regression_ratio must be >= 0\n";
		return 2;
	}

	try {
		const auto baseline = readCsv(baselinePath);
		const auto candidate = readCsv(candidatePath);
		bool failed = false;

		for(const auto &entry : baseline) {
			const std::string &name = entry.first;
			const Metric &baseMetric = entry.second;
			auto candIt = candidate.find(name);
			if(candIt == candidate.end()) {
				std::cerr << "Missing metric in candidate: " << name << "\n";
				failed = true;
				continue;
			}
			if(baseMetric.seconds <= 0.0) {
				continue;
			}
			const double regression = (candIt->second.seconds - baseMetric.seconds) / baseMetric.seconds;
			const double pct = regression * 100.0;
			std::cout << std::fixed << std::setprecision(2)
			          << name << ": " << pct << "%\n";
			if(regression > threshold) {
				std::cerr << "Regression threshold exceeded for " << name << "\n";
				failed = true;
			}
		}

		if(failed) {
			return 1;
		}

		std::cout << "Benchmark regression check passed (threshold=" << (threshold * 100.0) << "%).\n";
		return 0;
	} catch(const std::exception &ex) {
		std::cerr << "benchmark_regression_check error: " << ex.what() << "\n";
		return 2;
	}
}
