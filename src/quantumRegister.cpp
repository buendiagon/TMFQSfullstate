#include "quantumRegister.h"
#include "utils.h"


#include <cstring>
#include <cstdint>

// Quantum Register class implementation
//
//Constructors ###################################
// Default contructor
QuantumRegister::QuantumRegister() {
	this->numQubits = 0;
	this->numStates = 0;
}

// Parametrized Constructor 1
QuantumRegister::QuantumRegister(unsigned int numQubits) {
	this->numQubits = numQubits;
	this->numStates = pow(2,this->numQubits);
	this->amplitudes.resize(this->numStates*2, 0.0);
	this->amplitudes[0] = 1.0;
}


// Parametrized Constructor 2
QuantumRegister::QuantumRegister(unsigned int numQubits, unsigned int initState) {
	this->numQubits = numQubits;
	this->numStates = pow(2,this->numQubits);
	this->amplitudes.resize(this->numStates*2, 0.0);
	this->amplitudes[2*initState] = 1.0;
}


// Parametrized Constructor 3
QuantumRegister::QuantumRegister(unsigned int numQubits, unsigned int initState, Amplitude amp) {
	this->numQubits = numQubits;
	this->numStates = pow(2,this->numQubits);
	this->amplitudes.resize(this->numStates*2, 0.0);
	this->amplitudes[2*initState] = amp.real;
	this->amplitudes[2*initState + 1] = amp.imag;
}


// Parametrized Constructor 4
/*
QuantumRegister::QuantumRegister(unsigned int numQubits, bool isRandom) {
	this->numQubits = numQubits;
	this->numStates = pow(2,this->numQubits);
	this->amplitudes.resize(this->numStates*2, 0.0);

	if(isRandom == false) {
		this->amplitudes[0] = 1.0;
	} else {
		// Normalizamos el arreglo de amplitudes
		this->amplitudes = normalizeArray(this->amplitudes);
	}
	// TODO: separar la impresión de las amplitudes por aparte, un método para ello
	cout << endl << "Amplitudes iniciales: " << endl;
	for(unsigned int i=0; i<this->amplitudes.size()/2; i++) {
		cout << "\ta_" << i << " -> \t" ;
		cout << std::left << std::setw(20) << std::setprecision(16) << this->amplitudes[i*2] << " \t" ;
		cout << std::left << std::setw(20) << std::setprecision(16) << this->amplitudes[(i*2)+1] << endl;
	}
}
*/

QuantumRegister::QuantumRegister(unsigned int numQubits, string filePath) {
	this->numQubits = numQubits;
	this->numStates = pow(2,this->numQubits);
	this->amplitudes.resize(this->numStates*2, 0.0);

	unsigned int li=0;

	FILE *in_file;
	
	string dateTime;
	bool isData = true;
	in_file = fopen(filePath.c_str(), "r");

	if(in_file == NULL) {
        printErrors(5);
    } else {
		char buffer[20];
		while(fgets(buffer, sizeof(buffer), in_file)) {
			if(isData == true) {
				if(std::strstr(buffer, "*****") != nullptr) {
					isData = false;
				} else {
					this->amplitudes[li] = stod(buffer);
				}
			} else {
				dateTime = buffer;
			}
			++li;
        }
		fclose(in_file);
	}
/*
	cout << endl << "Amplitudes iniciales: " << endl;
	for(unsigned int i=0; i<this->amplitudes.size()/2; i++) {
			cout << "\ta_" << i << " -> \t" ;
			cout << std::left << std::setw(20) << std::setprecision(16) << this->amplitudes[i*2] << " \t" ;
			cout << std::left << std::setw(20) << std::setprecision(16) << this->amplitudes[(i*2)+1] << endl;
	}
*/
}

//Copy Constructor 
QuantumRegister::QuantumRegister(const QuantumRegister& qreg) {
	this->numQubits = qreg.numQubits;
	this->numStates = qreg.numStates;
	this->amplitudes = qreg.amplitudes;
	this->states = qreg.states;
}

int QuantumRegister::getSize(){
	return this->amplitudes.size();
}

//Get methods ####################################
//
//Get the element i-th of linearized amplitudes vector
Amplitude QuantumRegister::amplitude(unsigned int state){
#ifdef USE_BLOSC
	if(this->isCompressed) this->decompressAmplitudes();
#endif
	Amplitude amp;
	if (state < this->numStates){
		amp.real = this->amplitudes[state*2];
		amp.imag = this->amplitudes[state*2 + 1];
	}
	else{
		amp.real = 0.0;
		amp.imag = 0.0;
	}
	return amp;
}




//Get the probability (|amplitude|^2) of the state
double QuantumRegister::probability(unsigned int state){
	Amplitude amp = amplitude(state);
	return pow(amp.real, 2) + pow(amp.imag, 2);
}

//Get the sum of magnitudes of the amplitudes
double QuantumRegister::probabilitySumatory(){
#ifdef USE_BLOSC
	if(this->isCompressed) this->decompressAmplitudes();
#endif
	unsigned int i;
	double sum = 0.0;
	if(this->amplitudes.size() > 0) {
		for(i=0; i<this->numStates; i++){
			sum += pow(this->amplitudes[i*2], 2) + pow(this->amplitudes[i*2 + 1], 2);
		}
	}
	return sum;
}


//Set methods ####################################
void QuantumRegister::setSize(unsigned int numQubits){
	this->numQubits = numQubits;
	this->numStates = pow(2, numQubits);
	this->states.resize(this->numStates, 0);
	this->amplitudes.resize(this->numStates*2, 0.0);
}


// Fill the states vector ramdonly
void QuantumRegister::fillStatesVector(){
	int i;
	if ( this->states.size() < this->numQubits ){
		this->setSize(this->numQubits);
	}
	for (i=0; i < pow(2, this->numQubits); i++){
		this->states[i] = i;
		this->amplitudes[i*2] = i;
		this->amplitudes[i*2 + 1] = i;
	}
}

//Miscelaneous methods ###########################
//Print states vector
std::ostream &operator << (std::ostream &os, QuantumRegister &reg) {
#ifdef USE_BLOSC
	if(reg.isCompressed) reg.decompressAmplitudes();
#endif
	for(unsigned int i=0; i<reg.numStates; i++){
		if(reg.amplitudes.size() > 0 && (reg.amplitudes[i*2] != 0.0 || reg.amplitudes[(i*2)+1] !=0.0)) {
			cout << i << ": " << reg.amplitudes[i*2] << " + " << reg.amplitudes[i*2 + 1] << "i" << endl;
		/*
			cout << std::right << std::setw(2) << i << ": ";
			cout << std::right << std::setw(7) << std::setprecision(3) << reg.amplitudes[i*2]; 
			cout << std::right << std::setw(7) << std::setprecision(3) << reg.amplitudes[(i*2)+1] << endl;
		*/
		}
	}
	return os;
}

void QuantumRegister::printStatesVector(){
	cout << *this;
}


//Destructor #####################################
QuantumRegister::~QuantumRegister(){
#ifdef USE_BLOSC
	if(this->compressedSchunk) {
		blosc2_schunk_free(this->compressedSchunk);
		this->compressedSchunk = nullptr;
	}
	this->isCompressed = false;
#endif
}



//Quantum Gates operations

//Get all states according the number of qubits
StatesVector getAllStates(unsigned int qubits){
	StatesVector v;
	int i;

	for(i=0; i < (1 << qubits); i++){
		v.push_back(i);
	}
	return v;
}


string QuantumRegister::getNthBit(unsigned int state, unsigned int qubit){
	unsigned int pos, bit;
	pos = this->numQubits - qubit - 1;   
	bit = (state >> pos) & 1;
	return to_string(bit);
}

int QuantumRegister::findState(unsigned int state){
	int index;
	auto it = std::find(this->states.begin(), this->states.end(), state); 
	if (it != this->states.end()) {
		index = it - this->states.begin();
	}
	else {
		index = -1;
	}
	return index;
}



double QuantumRegister::getProbability(unsigned int state){
#ifdef USE_BLOSC
	if(this->isCompressed) this->decompressAmplitudes();
#endif
	Amplitude amp;
	amp.real = this->amplitudes[state*2];
	amp.imag = this->amplitudes[state*2+1];
	return pow(amp.real, 2) + pow(amp.imag, 2);
}



#ifdef USE_BLOSC
void QuantumRegister::compressAmplitudes() {
    if(this->isCompressed) return;

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.compcode = BLOSC_LZ4;
    cparams.clevel = 1;
    cparams.nthreads = 1;
    cparams.typesize = sizeof(double);
    cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;

    blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
    storage.cparams = &cparams;

    this->compressedSchunk = blosc2_schunk_new(&storage);
    if(!this->compressedSchunk) {
        printf("Blosc2: failed to create schunk!\n");
        return;
    }

    const size_t elemsPerChunk = CHUNK_STATES * 2;
    const size_t totalElems    = this->amplitudes.size();

    // Append one Blosc2 chunk per CHUNK_STATES states. Each append processes
    // only elemsPerChunk * sizeof(double) bytes, keeping peak allocation minimal.
    for(size_t offset = 0; offset < totalElems; offset += elemsPerChunk) {
        size_t count = std::min(elemsPerChunk, totalElems - offset);
        int64_t nchunks = blosc2_schunk_append_buffer(this->compressedSchunk,
            this->amplitudes.data() + offset, (int32_t)(count * sizeof(double)));
        if(nchunks < 0) {
            printf("Blosc2: schunk append failed at offset %zu!\n", offset);
            blosc2_schunk_free(this->compressedSchunk);
            this->compressedSchunk = nullptr;
            return;
        }
    }

    // Release the full amplitudes vector now — schunk holds only compressed bytes
    std::vector<double>().swap(this->amplitudes);
    this->isCompressed = true;
}

void QuantumRegister::decompressAmplitudes() {
    if(!this->isCompressed || !this->compressedSchunk) return;

    this->amplitudes.resize(this->numStates * 2);
    const size_t elemsPerChunk = CHUNK_STATES * 2;
    int64_t nchunks = this->compressedSchunk->nchunks;

    for(int64_t ci = 0; ci < nchunks; ci++) {
        size_t offset = (size_t)ci * elemsPerChunk;
        size_t count  = std::min(elemsPerChunk, (size_t)this->numStates * 2 - offset);
        blosc2_schunk_decompress_chunk(this->compressedSchunk, ci,
            this->amplitudes.data() + offset, (int32_t)(count * sizeof(double)));
    }

    blosc2_schunk_free(this->compressedSchunk);
    this->compressedSchunk = nullptr;
    this->isCompressed = false;
}
#endif

// Method to apply a quantum gate to quantum register
void QuantumRegister::applyGate(QuantumGate gate, IntegerVector qubits){

	if (gate.dimension != (unsigned int)(1 << qubits.size())) {
		printf("Unitary matrix dimension is not correct to be applied to the inputs qubits\n");
		return; 
	}

#ifdef USE_BLOSC
	// ---- CHUNK-AWARE PATH: never decompresses the full vector ----
	// Uses a small LRU cache of decompressed chunks and reuses compression context.
	// Peak memory stays bounded while avoiding per-eviction malloc/context overhead.
	if(this->isCompressed) {
		const size_t elemsPerChunk = CHUNK_STATES * 2;  // doubles per chunk
		const int cacheSlots = 8;

		int k = qubits.size();
		unsigned int numBlocks = 1 << (this->numQubits - k);
		unsigned int blockSize = 1 << k;

		std::vector<unsigned int> targetPos(k);
		for(int i = 0; i < k; ++i)
			targetPos[i] = this->numQubits - qubits[i] - 1;

		std::vector<Amplitude> localAmps(blockSize);
		std::vector<unsigned int> stateIndices(blockSize);

		// Cache of decompressed chunks
		std::vector<std::vector<double>> cacheBuf(cacheSlots);
		for(int i = 0; i < cacheSlots; ++i) {
			cacheBuf[i].resize(elemsPerChunk, 0.0);
		}
		std::vector<int64_t> cacheChunkIdx(cacheSlots, -1LL);
		std::vector<bool> cacheDirty(cacheSlots, false);
		std::vector<uint64_t> cacheAge(cacheSlots, 0);
		uint64_t ageCounter = 1;

		blosc2_cparams cparams2 = BLOSC2_CPARAMS_DEFAULTS;
		cparams2.compcode = BLOSC_LZ4;
		cparams2.clevel   = 1;
		cparams2.nthreads = 1;
		cparams2.typesize = sizeof(double);
		cparams2.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
		blosc2_context* cctx = blosc2_create_cctx(cparams2);
		if(!cctx) {
			printf("Blosc2: failed to create compression context, falling back to full decompression.\n");
			this->decompressAmplitudes();
			applyGate(gate, qubits);
			return;
		}
		std::vector<uint8_t> compressedTmp(elemsPerChunk * sizeof(double) + BLOSC2_MAX_OVERHEAD);

		// Write a dirty slot back to the schunk using compress+update_chunk
		auto flushSlot = [&](int slot) {
			if(slot < 0 || slot >= cacheSlots) return;
			if(!cacheDirty[slot] || cacheChunkIdx[slot] < 0) return;
			int64_t ci = cacheChunkIdx[slot];
			size_t  nElems = std::min(elemsPerChunk,
				(size_t)this->numStates * 2 - (size_t)(ci * elemsPerChunk));
			size_t srcBytes   = nElems * sizeof(double);
			int csz = blosc2_compress_ctx(cctx, cacheBuf[slot].data(), (int32_t)srcBytes,
				compressedTmp.data(), (int32_t)compressedTmp.size());
			if(csz > 0) {
				int64_t rc = blosc2_schunk_update_chunk(this->compressedSchunk, ci, compressedTmp.data(), true);
				if(rc < 0) {
					printf("Blosc2: schunk update failed at chunk %lld\n", (long long)ci);
				}
			}
			cacheDirty[slot] = false;
		};

		// Get the slot holding chunkIdx, loading it (evicting LRU) if needed
		auto getSlot = [&](int64_t chunkIdx) -> int {
			for(int slot = 0; slot < cacheSlots; ++slot) {
				if(cacheChunkIdx[slot] == chunkIdx) {
					cacheAge[slot] = ageCounter++;
					return slot;
				}
			}
			int evict = 0;
			bool foundFree = false;
			for(int slot = 0; slot < cacheSlots; ++slot) {
				if(cacheChunkIdx[slot] < 0) {
					evict = slot;
					foundFree = true;
					break;
				}
			}
			if(!foundFree) {
				for(int slot = 1; slot < cacheSlots; ++slot) {
					if(cacheAge[slot] < cacheAge[evict]) evict = slot;
				}
			}
			flushSlot(evict);
			int64_t ci = chunkIdx;
			size_t nElems = std::min(elemsPerChunk,
				(size_t)this->numStates * 2 - (size_t)(ci * elemsPerChunk));
			int dsz = blosc2_schunk_decompress_chunk(this->compressedSchunk, ci,
				cacheBuf[evict].data(), (int32_t)(nElems * sizeof(double)));
			if(dsz < 0) {
				printf("Blosc2: schunk decompress failed at chunk %lld\n", (long long)ci);
			}
			cacheChunkIdx[evict] = chunkIdx;
			cacheDirty[evict]    = false;
			cacheAge[evict]      = ageCounter++;
			return evict;
		};

		for(unsigned int block = 0; block < numBlocks; ++block) {
			unsigned int baseState = 0, tempBlock = block;
			for(int bit = 0; bit < (int)this->numQubits; ++bit) {
				bool isTarget = false;
				for(int i = 0; i < k; ++i)
					if((int)targetPos[i] == bit) { isTarget = true; break; }
				if(!isTarget) {
					baseState |= ((tempBlock & 1) << bit);
					tempBlock >>= 1;
				}
			}

			bool hasNonZero = false;
			for(unsigned int idx = 0; idx < blockSize; ++idx) {
				unsigned int state = baseState;
				for(int i = 0; i < k; ++i){
					unsigned int bitVal = (idx >> (k - 1 - i)) & 1;
					state |= (bitVal << targetPos[i]);
				}
				stateIndices[idx] = state;
				int64_t ci  = (int64_t)(state / CHUNK_STATES);
				size_t  off = (state % CHUNK_STATES) * 2;
				int slot = getSlot(ci);
				localAmps[idx].real = cacheBuf[slot][off];
				localAmps[idx].imag = cacheBuf[slot][off + 1];
				if(localAmps[idx].real != 0.0 || localAmps[idx].imag != 0.0) hasNonZero = true;
			}

			if(!hasNonZero) continue;

			for(unsigned int row = 0; row < blockSize; ++row) {
				Amplitude newAmp = {0.0, 0.0};
				for(unsigned int col = 0; col < blockSize; ++col) {
					Amplitude g = gate[row][col];
					Amplitude a = localAmps[col];
					newAmp.real += g.real * a.real - g.imag * a.imag;
					newAmp.imag += g.real * a.imag + g.imag * a.real;
				}
				unsigned int state = stateIndices[row];
				int64_t ci  = (int64_t)(state / CHUNK_STATES);
				size_t  off = (state % CHUNK_STATES) * 2;
				int slot = getSlot(ci);
				cacheBuf[slot][off]     = newAmp.real;
				cacheBuf[slot][off + 1] = newAmp.imag;
				cacheDirty[slot] = true;
			}
		}

		// Flush any remaining dirty chunks
		for(int slot = 0; slot < cacheSlots; ++slot) {
			flushSlot(slot);
		}
		blosc2_free_ctx(cctx);
		return;
	}
#endif

	// ---- STANDARD (uncompressed) PATH ----
	int k = qubits.size();
	unsigned int numBlocks = 1 << (this->numQubits - k);
	unsigned int blockSize = 1 << k;

	std::vector<unsigned int> targetPos(k);
	for(int i = 0; i < k; ++i) {
		targetPos[i] = this->numQubits - qubits[i] - 1;
	}

	std::vector<Amplitude> localAmps(blockSize);
	std::vector<unsigned int> stateIndices(blockSize);

	for(unsigned int block = 0; block < numBlocks; ++block) {
		unsigned int baseState = 0;
		unsigned int tempBlock = block;
		for(int bit = 0; bit < (int)this->numQubits; ++bit) {
			bool isTarget = false;
			for(int i = 0; i < k; ++i) {
				if((int)targetPos[i] == bit) {
					isTarget = true;
					break;
				}
			}
			if(!isTarget) {
				unsigned int b = tempBlock & 1;
				baseState |= (b << bit);
				tempBlock >>= 1;
			}
		}

		bool hasNonZero = false;
		for(unsigned int idx = 0; idx < blockSize; ++idx) {
			unsigned int state = baseState;
			for(int i = 0; i < k; ++i) {
				unsigned int bitVal = (idx >> (k - 1 - i)) & 1;
				state |= (bitVal << targetPos[i]);
			}
			stateIndices[idx] = state;
			localAmps[idx].real = this->amplitudes[state*2];
			localAmps[idx].imag = this->amplitudes[state*2 + 1];
			if(localAmps[idx].real != 0.0 || localAmps[idx].imag != 0.0) {
				hasNonZero = true;
			}
		}

		if(!hasNonZero) continue;

		for(unsigned int row = 0; row < blockSize; ++row) {
			Amplitude newAmp = {0.0, 0.0};
			for(unsigned int col = 0; col < blockSize; ++col) {
				Amplitude g = gate[row][col];
				Amplitude a = localAmps[col];
				newAmp.real += g.real * a.real - g.imag * a.imag;
				newAmp.imag += g.real * a.imag + g.imag * a.real;
			}
			this->amplitudes[stateIndices[row]*2]     = newAmp.real;
			this->amplitudes[stateIndices[row]*2 + 1] = newAmp.imag;
		}
	}
}



// Method to apply a Hadamard gate to specific qubit of a quantum register
void QuantumRegister::Hadamard(unsigned int qubit){
	IntegerVector v;
	v.push_back(qubit);
	applyGate(QuantumGate::Hadamard(), v);
}


// Method to apply a ControlledPhaseShift gate to specific qubit of a quantum register
void QuantumRegister::ControlledPhaseShift(unsigned int controlQubit, unsigned int targetQubit, double theta){
	IntegerVector v;
	v.push_back(controlQubit);
	v.push_back(targetQubit);
	applyGate(QuantumGate::ControlledPhaseShift(theta), v);
}


void QuantumRegister::ControlledNot(unsigned int controlQubit, unsigned int targetQubit) {
	IntegerVector v; v.push_back(controlQubit); v.push_back(targetQubit);
	applyGate(QuantumGate::ControlledNot(), v);
}


void QuantumRegister::Swap(unsigned int qubit1, unsigned int qubit2) {
	ControlledNot(qubit1, qubit2);
	ControlledNot(qubit2, qubit1);
	ControlledNot(qubit1, qubit2);
}
