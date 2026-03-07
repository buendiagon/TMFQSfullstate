# TMFQSFS - Quantum Simulator

<p align="center">
  <strong>A lightweight quantum computing simulator written in C++17</strong>
</p>

---

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Performance Profiling (psrecord)](#performance-profiling-psrecord)
- [API Reference](#api-reference)
- [Examples](#examples)
- [Project Structure](#project-structure)
- [License](#license)

---

## Overview

**TMFQSFS** (Too Many Fancy Quantum Simulator For Science) is a quantum computing simulator that provides a framework for simulating quantum registers, quantum gates, and quantum algorithms. It uses state vector simulation to represent quantum states and supports common quantum operations.

The simulator is designed for:
- Educational purposes and learning quantum computing concepts
- Prototyping and testing quantum algorithms
- Research in quantum computing fundamentals

---

## Features

### Quantum Register
- Create registers with arbitrary number of qubits
- Initialize registers to specific basis states
- Query amplitudes and probabilities
- Print state vectors

### Quantum Gates
| Single-Qubit Gates | Multi-Qubit Gates |
|-------------------|-------------------|
| Hadamard (H) | Controlled-NOT (CNOT) |
| Pauli-X, Y, Z | Controlled Phase Shift |
| Phase Shift | Toffoli (CCNOT) |
| T Gate (π/8) | SWAP |
| Identity | Ising |

### Quantum Algorithms
- **Quantum Fourier Transform (QFT)** - Transforms computational basis to Fourier basis
- **Grover's Search Algorithm** - Searches for marked states with quadratic speedup

---

## Requirements

- **Compiler**: Intel oneAPI `icpx` (preferred, auto-detected) or `g++` with C++17 support
- **OS**: Linux (tested on Fedora)
- **Build**: GNU Make
- **Optional dependency**: Blosc2 (`libblosc2`) for compressed storage strategy

---

## Installation

### Clone the Repository

```bash
git clone https://github.com/diaztoro/TMFQS.git
cd TMFQS
```

### Build

```bash
# Clean previous builds (optional)
make clean

# Build the library and examples
make

# Build integration tests (optional)
make tests

```

This will:
1. Compile the shared library `libtmfqsfs.so` in `lib64/`
2. Compile all example programs in `bin/`
3. Enable Blosc runtime strategy automatically if `blosc2.h` is available on the system

### Verify Installation

```bash
export LD_LIBRARY_PATH="$PWD/lib64:$LD_LIBRARY_PATH"
./bin/grover 3 5
```

Expected output: Grover's algorithm finding state |5⟩ with high probability.

---

## Usage

### Environment Setup

Before running any program, set the library path:

```bash
export LD_LIBRARY_PATH="/path/to/QSTest/lib64:$LD_LIBRARY_PATH"
```

### Running Programs

#### Quantum Fourier Transform
```bash
./bin/qft <num_qubits> <initial_state>

# Example: QFT on 3 qubits starting from |0⟩
./bin/qft 3 0
```

#### Grover's Search Algorithm
```bash
./bin/grover <num_qubits> <marked_state>

# Example: Search for |5⟩ in a 3-qubit system
./bin/grover 3 5

# Example: Search for |11⟩ in a 4-qubit system
./bin/grover 4 11
```

#### Applying Individual Gates
```bash
./bin/applyHadamard <num_qubits> <target_qubit>
./bin/applyControlledNot <num_qubits> <control_qubit> <target_qubit>
./bin/applyControlledPhaseShift <num_qubits> <control> <target> <theta>
```

#### QFTG With Runtime Storage Strategy
```bash
./bin/qftG <num_qubits> [--strategy dense|blosc|auto] [--chunk-states N] [--cache-slots N] [--clevel N] [--nthreads N] [--threshold-mb N]

# Example: force Dense
./bin/qftG 22 --strategy dense

# Example: force Blosc
./bin/qftG 22 --strategy blosc --chunk-states 16384 --clevel 1

# Example: auto-select by threshold
./bin/qftG 22 --strategy auto --threshold-mb 8
```

---

## Performance Profiling (psrecord)

`qftG` now keeps algorithm logic only. For execution time and RAM graphs, use external profiling with `psrecord`.

### Install on Rocky Linux

```bash
sudo dnf install -y python3 python3-pip
python3 -m venv .venv-prof
source .venv-prof/bin/activate
python -m pip install --upgrade pip
python -m pip install "psrecord[plot]"
```

### Generate PNG timeline (CPU + RAM vs time)

```bash
mkdir -p profiling
psrecord "./bin/qftG 20" \
  --interval 0.02 \
  --plot profiling/qftG_20_timeline.png \
  --log profiling/qftG_20_timeline.log
```

Output files:
- `profiling/qftG_20_timeline.png`: timeline graph
- `profiling/qftG_20_timeline.log`: sampled raw data

---

## API Reference

### QuantumRegister Class

```cpp
#include "tmfqsfs.h"

// Create a register with n qubits (initialized to |0⟩)
QuantumRegister qreg(n);

// Create a register initialized to a specific state
QuantumRegister qreg(n, initial_state);

// Create with explicit storage strategy
RegisterConfig cfg;
cfg.strategy = StorageStrategyKind::Blosc;
cfg.blosc.chunkStates = 16384;
cfg.blosc.gateCacheSlots = 8;
QuantumRegister qreg_compressed(n, cfg);

// Apply quantum gates
qreg.Hadamard(qubit);
qreg.ControlledNot(control, target);
qreg.ControlledPhaseShift(control, target, theta);
qreg.Swap(qubit1, qubit2);

// Query state
Amplitude amp = qreg.amplitude(state);
double prob = qreg.probability(state);
double total = qreg.totalProbability();  // Should equal 1.0
unsigned int sampled = qreg.measure();
size_t elemCount = qreg.amplitudeElementCount();

// Display
qreg.printStatesVector();        // default epsilon = 1e-12
qreg.printStatesVector(1e-9);   // custom epsilon
```

### QuantumGate Class

```cpp
// Create standard gates
QuantumGate H = QuantumGate::Hadamard();
QuantumGate X = QuantumGate::PauliX();
QuantumGate CNOT = QuantumGate::ControlledNot();
QuantumGate I = QuantumGate::Identity(dimension);

// Gate operations
QuantumGate result = gate1 * gate2;  // Matrix multiplication
QuantumGate scaled = gate * scalar;  // Scalar multiplication

// Apply arbitrary gate to register
QubitList qubits = {0, 1};  // Target qubits
qreg.applyGate(gate, qubits);
```

### Quantum Algorithms

```cpp
#include "tmfqsfs.h"

// Quantum Fourier Transform
QuantumRegister qreg(n, initial_state);
quantumFourierTransform(qreg);

// Grover's Search (returns measured state)
unsigned int result = Grover(marked_state, num_qubits, verbose);
```

---

## Examples

### Example 1: Creating a Bell State

```cpp
#include "tmfqsfs.h"
#include <iostream>

int main() {
    // Create |00⟩
    QuantumRegister qreg(2, 0);
    
    // Apply H to qubit 0: (|0⟩ + |1⟩)/√2 ⊗ |0⟩
    qreg.Hadamard(0);
    
    // Apply CNOT: (|00⟩ + |11⟩)/√2
    qreg.ControlledNot(0, 1);
    
    std::cout << "Bell State |Φ+⟩:" << std::endl;
    qreg.printStatesVector();
    
    return 0;
}
```

### Example 2: Grover's Algorithm

```cpp
#include "tmfqsfs.h"
#include <iostream>

int main() {
    unsigned int target = 5;   // State to find
    unsigned int qubits = 3;   // 2^3 = 8 possible states
    
    unsigned int result = Grover(target, qubits, true);
    
    std::cout << "Found: " << result << std::endl;
    std::cout << "Expected: " << target << std::endl;
    
    return 0;
}
```

### Compiling Your Own Programs

```bash
# Compile
icpx -std=c++17 -I /path/to/QSTest/include -L /path/to/QSTest/lib64 -ltmfqsfs myprogram.cpp -o myprogram

# Run
export LD_LIBRARY_PATH="/path/to/QSTest/lib64:$LD_LIBRARY_PATH"
./myprogram
```

---

## Project Structure

```
QSTest/
├── include/                  # Header files
│   ├── tmfqsfs.h            # Main include (includes all headers)
│   ├── stateSpace.h         # Checked qubit/state-size helpers
│   ├── quantumRegister.h    # Quantum register class
│   ├── quantumGate.h        # Quantum gate class
│   ├── quantumAlgorithms.h  # QFT, Grover, etc.
│   ├── types.h              # Type definitions (Amplitude, vectors)
│   ├── utils.h              # Utility functions
│   └── storage/
│       ├── IStateBackend.h
│       └── StateBackendFactory.h
├── src/                      # Source files
│   ├── quantumRegister.cpp
│   ├── quantumGate.cpp
│   ├── quantumAlgorithms.cpp
│   ├── utils.cpp
│   ├── storage/              # Runtime backend strategies
│   │   ├── DenseStateBackend.cpp
│   │   ├── BloscStateBackend.cpp
│   │   └── StateBackendFactory.cpp
│   └── Makefile
├── examples/                 # Example programs
│   ├── qft.cpp
│   ├── grover.cpp
│   ├── applyHadamard.cpp
│   └── ...
├── tests/
│   └── integration/
│       └── test_bugfixes.cpp
├── benchmarks/               # Reserved for future benchmark targets
├── bin/                      # Compiled binaries
├── lib64/                    # Shared library
│   └── libtmfqsfs.so
├── Makefile                  # Main build file
├── LICENSE
└── README.md
```

---

## Technical Notes

### State Vector Representation

Quantum states are stored as a vector of complex amplitudes:
- For `n` qubits, the state vector has `2^n` complex entries
- Each amplitude is stored as `{real, imag}` pairs
- Memory usage: `2^(n+1) * sizeof(double)` bytes

### Limitations

- **Scalability**: Memory grows exponentially with qubit count
- **Maximum qubits**: Practical limit ~25-30 qubits depending on available RAM
- **No noise simulation**: Ideal quantum operations only

---

## License

This project is licensed under the terms specified in the [LICENSE](LICENSE) file.

---

## Authors

- **Gilberto Javier Díaz Toro** - *Initial development*

---

## Acknowledgments

- The quantum computing research community
