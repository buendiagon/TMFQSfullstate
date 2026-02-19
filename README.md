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

- **Compiler**: Intel® oneAPI DPC++/C++ Compiler (`icpx`) with C++17 support
- **OS**: Linux (tested on Fedora)
- **Build**: GNU Make

### Setting up Intel oneAPI

Before compiling, initialize the Intel oneAPI environment:

```bash
source /opt/intel/oneapi/setvars.sh
```

Or add to your shell profile for persistent configuration.

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
```

This will:
1. Compile the shared library `libtmfqsfs.so` in `lib64/`
2. Compile all example programs in `bin/`

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

---

## API Reference

### QuantumRegister Class

```cpp
#include "tmfqsfs.h"

// Create a register with n qubits (initialized to |0⟩)
QuantumRegister qreg(n);

// Create a register initialized to a specific state
QuantumRegister qreg(n, initial_state);

// Apply quantum gates
qreg.Hadamard(qubit);
qreg.ControlledNot(control, target);
qreg.ControlledPhaseShift(control, target, theta);
qreg.Swap(qubit1, qubit2);

// Query state
Amplitude amp = qreg.amplitude(state);
double prob = qreg.probability(state);
double total = qreg.probabilitySumatory();  // Should equal 1.0

// Display
qreg.printStatesVector();
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
IntegerVector qubits = {0, 1};  // Target qubits
qreg.applyGate(gate, qubits);
```

### Quantum Algorithms

```cpp
#include "tmfqsfs.h"

// Quantum Fourier Transform
QuantumRegister qreg(n, initial_state);
quantumFourierTransform(&qreg);

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
icpx -I /path/to/QSTest/include -L /path/to/QSTest/lib64 -ltmfqsfs myprogram.cpp -o myprogram

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
│   ├── quantumRegister.h    # Quantum register class
│   ├── quantumGate.h        # Quantum gate class
│   ├── quantumAlgorithms.h  # QFT, Grover, etc.
│   ├── types.h              # Type definitions (Amplitude, vectors)
│   └── utils.h              # Utility functions
├── src/                      # Source files
│   ├── quantumRegister.cpp
│   ├── quantumGate.cpp
│   ├── quantumAlgorithms.cpp
│   ├── utils.cpp
│   └── Makefile
├── examples/                 # Example programs
│   ├── qft.cpp
│   ├── grover.cpp
│   ├── applyHadamard.cpp
│   └── ...
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

- Intel for the oneAPI toolkit
- The quantum computing research community
