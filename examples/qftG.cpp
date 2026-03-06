#include "tmfqsfs.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

using StateList = vector<unsigned int>;

// Parse unsigned integer from argv token.
static bool parseUnsigned(const char *text, unsigned int &valueOut) {
	char *end = nullptr;
	unsigned long value = strtoul(text, &end, 10);
	if (text == end || *end != '\0') {
		return false;
	}
	valueOut = static_cast<unsigned int>(value);
	return true;
}

// Keep only valid basis states and remove duplicates.
static StateList sanitizeStates(const StateList &input, unsigned int totalStates) {
	StateList states;
	for (unsigned int state : input) {
		if (state < totalStates) {
			states.push_back(state);
		}
	}
	sort(states.begin(), states.end());
	states.erase(unique(states.begin(), states.end()), states.end());
	return states;
}

// Sample one basis state according to probabilities.
static unsigned int measureState(QuantumRegister &qureg, unsigned int totalStates) {
	double randomValue = getRandomNumber();
	double cumulativeProbability = 0.0;
	unsigned int measuredState = totalStates - 1;

	for (unsigned int basisState = 0; basisState < totalStates; ++basisState) {
		cumulativeProbability += qureg.probability(basisState);
		if (randomValue <= cumulativeProbability) {
			measuredState = basisState;
			break;
		}
	}
	return measuredState;
}

#ifdef USE_BLOSC
// Sample directly from compressed chunks to avoid full-state decompression at the end.
static unsigned int measureStateCompressed(const QuantumRegister &qureg, unsigned int totalStates) {
	if (!qureg.isCompressed || qureg.compressedSchunk == nullptr) {
		return totalStates - 1;
	}

	const double randomValue = getRandomNumber();
	double cumulativeProbability = 0.0;
	const size_t elemsPerChunk = QuantumRegister::CHUNK_STATES * 2;
	const size_t totalElems = static_cast<size_t>(totalStates) * 2;
	vector<double> chunkBuffer(elemsPerChunk, 0.0);

	for (int64_t chunkIndex = 0; chunkIndex < qureg.compressedSchunk->nchunks; ++chunkIndex) {
		size_t offset = static_cast<size_t>(chunkIndex) * elemsPerChunk;
		size_t count = min(elemsPerChunk, totalElems - offset);
		int dsz = blosc2_schunk_decompress_chunk(
			qureg.compressedSchunk,
			chunkIndex,
			chunkBuffer.data(),
			static_cast<int32_t>(count * sizeof(double)));
		if (dsz < 0) {
			return totalStates - 1;
		}

		const unsigned int statesInChunk = static_cast<unsigned int>(count / 2);
		const unsigned int chunkStateBase = static_cast<unsigned int>(chunkIndex) * QuantumRegister::CHUNK_STATES;
		for (unsigned int localState = 0; localState < statesInChunk; ++localState) {
			const double real = chunkBuffer[2 * localState];
			const double imag = chunkBuffer[2 * localState + 1];
			cumulativeProbability += real * real + imag * imag;
			if (randomValue <= cumulativeProbability) {
				return chunkStateBase + localState;
			}
		}
	}

	return totalStates - 1;
}
#endif

// Prepare |psi> as a uniform superposition over selected basis states.
static void loadUniformSuperposition(QuantumRegister &qureg, const StateList &basisStates) {
	fill(qureg.amplitudes.begin(), qureg.amplitudes.end(), 0.0);
	const double realAmplitude = 1.0 / sqrt(static_cast<double>(basisStates.size()));
	for (unsigned int state : basisStates) {
		// amplitudes layout: [real0, imag0, real1, imag1, ...]
		qureg.amplitudes[2 * state] = realAmplitude;
		qureg.amplitudes[2 * state + 1] = 0.0;
	}
}

#ifdef USE_BLOSC
// Build the initial superposition directly into compressed chunks.
static bool loadUniformSuperpositionCompressed(
	QuantumRegister &qureg,
	unsigned int qubitCount,
	const StateList &basisStates) {
	if (basisStates.empty()) {
		return false;
	}

	qureg.numQubits = qubitCount;
	qureg.numStates = 1u << qubitCount;
	qureg.amplitudes.clear();
	qureg.states.clear();
	if (qureg.compressedSchunk) {
		blosc2_schunk_free(qureg.compressedSchunk);
		qureg.compressedSchunk = nullptr;
	}
	qureg.isCompressed = false;

	blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
	cparams.compcode = BLOSC_LZ4;
	cparams.clevel = 1;
	cparams.nthreads = 1;
	cparams.typesize = sizeof(double);
	cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;

	blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
	storage.cparams = &cparams;

	qureg.compressedSchunk = blosc2_schunk_new(&storage);
	if (!qureg.compressedSchunk) {
		return false;
	}

	const unsigned int totalStates = 1u << qubitCount;
	const double realAmplitude = 1.0 / sqrt(static_cast<double>(basisStates.size()));
	vector<double> chunkBuffer(QuantumRegister::CHUNK_STATES * 2, 0.0);
	size_t basisPos = 0;

	for (unsigned int chunkBase = 0; chunkBase < totalStates; chunkBase += QuantumRegister::CHUNK_STATES) {
		const unsigned int statesInChunk = min(
			QuantumRegister::CHUNK_STATES,
			static_cast<size_t>(totalStates - chunkBase));
		fill(chunkBuffer.begin(), chunkBuffer.begin() + static_cast<size_t>(statesInChunk) * 2, 0.0);

		while (basisPos < basisStates.size() && basisStates[basisPos] < chunkBase + statesInChunk) {
			const unsigned int localState = basisStates[basisPos] - chunkBase;
			chunkBuffer[2 * localState] = realAmplitude;
			chunkBuffer[2 * localState + 1] = 0.0;
			++basisPos;
		}

		int64_t nchunks = blosc2_schunk_append_buffer(
			qureg.compressedSchunk,
			chunkBuffer.data(),
			static_cast<int32_t>(statesInChunk * 2 * sizeof(double)));
		if (nchunks < 0) {
			blosc2_schunk_free(qureg.compressedSchunk);
			qureg.compressedSchunk = nullptr;
			return false;
		}
	}

	qureg.isCompressed = true;
	return true;
}
#endif

// Edit this function to define the input state set for qftG.
static StateList chooseInputStates(unsigned int totalStates) {
	StateList states;

	// Default example: all even states.
	for (unsigned int x = 0;; ++x) {
		// unsigned int y = 8u * x + (x % 2u);
		unsigned int y = 2*x;
		if (y >= totalStates) break;
		states.push_back(y);
	}
	return states;
}

static void printUsage() {
	cout << "Usage: ./qftG <num_qubits>" << endl;
}

static bool parseQubitCount(int argc, char *argv[], unsigned int &qubitCountOut) {
	if (argc != 2) {
		printUsage();
		return false;
	}

	unsigned int qubitCount = 0;
	if (!parseUnsigned(argv[1], qubitCount) || qubitCount == 0) {
		cout << "Number of qubits must be an integer >= 1" << endl;
		return false;
	}
	if (qubitCount >= (sizeof(unsigned int) * 8)) {
		cout << "Number of qubits is too large for this example." << endl;
		return false;
	}

	qubitCountOut = qubitCount;
	return true;
}

static bool buildInputStates(unsigned int totalStates, StateList &selectedStatesOut) {
	selectedStatesOut = sanitizeStates(chooseInputStates(totalStates), totalStates);
	if (selectedStatesOut.empty()) {
		cout << "chooseInputStates(totalStates) returned no valid states in [0, "
		     << (totalStates - 1) << "]." << endl;
		return false;
	}
	return true;
}

static void printResult(unsigned int measuredState, unsigned int totalStates) {
	cout << "Measured state (k): " << measuredState << endl;
	cout << "Calculated r (N/k): ";
	if (measuredState == 0) {
		cout << "undefined (division by zero)" << endl;
	} else {
		cout << (static_cast<double>(totalStates) / static_cast<double>(measuredState)) << endl;
	}
}

int main(int argc, char *argv[]) {
	unsigned int qubitCount = 0;
	if (!parseQubitCount(argc, argv, qubitCount)) {
		return 1;
	}

	const unsigned int totalStates = 1u << qubitCount;

	StateList selectedStates;
	if (!buildInputStates(totalStates, selectedStates)) {
		return 1;
	}

#ifdef USE_BLOSC
	QuantumRegister qureg;
	const size_t stateBytes = static_cast<size_t>(totalStates) * 2 * sizeof(double);
	const size_t compressionThresholdBytes = 8u * 1024u * 1024u;  // below this, compression overhead dominates
	if (stateBytes >= compressionThresholdBytes) {
		if (!loadUniformSuperpositionCompressed(qureg, qubitCount, selectedStates)) {
			cout << "Failed to build compressed initial state." << endl;
			return 1;
		}
	} else {
		qureg.setSize(qubitCount);
		loadUniformSuperposition(qureg, selectedStates);
	}
#else
	QuantumRegister qureg(qubitCount);
	loadUniformSuperposition(qureg, selectedStates);
#endif

	quantumFourierTransform(&qureg);

	unsigned int measuredState = 0;
#ifdef USE_BLOSC
	if (qureg.isCompressed) {
		measuredState = measureStateCompressed(qureg, totalStates);
	} else {
		measuredState = measureState(qureg, totalStates);
	}
#else
	measuredState = measureState(qureg, totalStates);
#endif
	printResult(measuredState, totalStates);
	return 0;
}
