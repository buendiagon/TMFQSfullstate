# TMFQS

TMFQS is a C++17 state-vector quantum simulator for learning, experimentation, and backend strategy research. It provides a shared library (`libtmfqsfs.so`), public C++ headers (`tmfqsfs.h`), example binaries, integration tests, and backend benchmarking tools.

## What You Get

- A C++ shared library built from `src/` and installed to `lib64/libtmfqsfs.so`
- Public API headers under `include/` with umbrella include `tmfqsfs.h`
- Example programs in `examples/` compiled into `bin/`
- Integration test suites in `tests/integration/`
- Runtime-selectable storage backends: Dense, Blosc, ZFP, and Auto
- Benchmark tools in `benchmarks/` for backend performance and regression checks

## Requirements

This repository is currently documented for Linux only (verified workflow).

- Compiler: Intel oneAPI `icpx` (auto-detected if available) or `g++` with C++17 support
- Build tool: GNU Make
- Runtime: shared-library loading via `LD_LIBRARY_PATH`
- Optional backend dependencies:
  - Blosc2 (`blosc2.h`, `libblosc2`) to enable the Blosc backend
  - ZFP (`zfp.h`, `libzfp`) to enable the ZFP backend

If optional headers are not found during build, the corresponding backend is not compiled in.

## Quickstart

```bash
git clone https://github.com/diaztoro/TMFQS.git
cd TMFQS

make
make tests

export LD_LIBRARY_PATH="$PWD/lib64:${LD_LIBRARY_PATH}"
./bin/grover 3 5
```

Expected result: the program runs Grover search for state `|5>` in a 3-qubit space and prints the measured state.

## Usage

Before running binaries from `bin/`, export the library path once per shell session:

```bash
export LD_LIBRARY_PATH="$PWD/lib64:${LD_LIBRARY_PATH}"
```

### Binary Quick Reference

```bash
./bin/qft <num_qubits> <initial_state>
./bin/grover <num_qubits> <marked_state>
./bin/applyHadamard <num_qubits> <qubit> <init_state>
./bin/applyControlledNot <num_qubits> <control_qubit> <target_qubit>
./bin/applyControlledPhaseShift <num_qubits> <control_qubit> <target_qubit> <init_state>
./bin/getSumOfProbabilities <num_qubits>
./bin/qftG <num_qubits> [options]
```

### Example Commands

```bash
./bin/qft 3 0
./bin/grover 4 11
./bin/applyHadamard 3 1 0
./bin/applyControlledNot 3 0 2
./bin/applyControlledPhaseShift 3 0 2 0
./bin/getSumOfProbabilities 5
```

## `qftG` CLI Reference

Usage:

```bash
./bin/qftG <num_qubits> [--strategy dense|blosc|zfp|auto] \
  [--chunk-states N] [--cache-slots N] [--clevel N] [--nthreads N] \
  [--threshold-mb N] [--zfp-mode rate|precision|accuracy] [--zfp-rate R] \
  [--zfp-precision B] [--zfp-accuracy A] [--zfp-chunk-states N] \
  [--zfp-cache-slots N]
```


| Option                 | Meaning                                                             |
| ---------------------- | ------------------------------------------------------------------- |
| `--strategy dense`     | blosc                                                               |
| `--chunk-states N`     | Blosc: basis states per compressed chunk.                           |
| `--cache-slots N`      | Blosc: decompressed chunk cache slots used during gate application. |
| `--clevel N`           | Blosc compression level (typically `0..9`).                         |
| `--nthreads N`         | Blosc thread count.                                                 |
| `--threshold-mb N`     | Auto strategy dense/compressed cutoff in MB (`autoThresholdBytes`). |
| `--zfp-mode rate`      | precision                                                           |
| `--zfp-rate R`         | ZFP fixed-rate target (bits/value).                                 |
| `--zfp-precision B`    | ZFP fixed-precision target (bits).                                  |
| `--zfp-accuracy A`     | ZFP fixed-accuracy absolute error target.                           |
| `--zfp-chunk-states N` | ZFP: basis states per compressed chunk.                             |
| `--zfp-cache-slots N`  | ZFP: decompressed chunk cache slots used during gate application.   |


Examples:

```bash
./bin/qftG 22 --strategy dense
./bin/qftG 22 --strategy blosc --chunk-states 16384 --clevel 1 --nthreads 2
./bin/qftG 22 --strategy zfp --zfp-mode rate --zfp-rate 24 --zfp-chunk-states 32768
./bin/qftG 22 --strategy auto --threshold-mb 8
```

## Backend Strategies

TMFQS can store amplitudes with different runtime backends:

- Dense:
  - In-memory uncompressed amplitude storage
  - Lowest overhead for smaller registers
- Blosc:
  - Chunked compressed storage using Blosc2
  - Requires build-time Blosc availability
- ZFP:
  - Chunked compressed storage with fixed-rate/precision/accuracy modes
  - Requires build-time ZFP availability
- Auto:
  - Chooses Dense for small states
  - For larger states, prefers Blosc if available, otherwise ZFP if available, otherwise Dense
  - Uses `RegisterConfig::autoThresholdBytes` (exposed in CLI as `--threshold-mb`)

If an unavailable backend is explicitly requested, construction fails with a runtime error.

## C++ API Usage

### 1. Basic `QuantumRegister` Flow

```cpp
#include "tmfqsfs.h"

int main() {
    using namespace tmfqs;

    QuantumRegister reg(3, 0);      // |000>
    reg.applyHadamard(0);
    reg.applyControlledNot(0, 1);

    Amplitude a0 = reg.amplitude(0);
    double p0 = reg.probability(0);
    double total = reg.totalProbability();

    (void)a0;
    (void)p0;
    (void)total;
    return 0;
}
```

### 2. Algorithms: QFT and Grover

```cpp
#include "tmfqsfs.h"

int main() {
    using namespace tmfqs;

    QuantumRegister qftReg(3, 0);
    algorithms::qftInPlace(qftReg);

    algorithms::GroverConfig cfg{5u, 3u, false};
    Mt19937RandomSource rng(12345u);
    StateIndex measured = algorithms::groverSearch(cfg, rng);

    (void)measured;
    return 0;
}
```

### 3. Backend Selection with `RegisterConfig`

```cpp
#include "tmfqsfs.h"

int main() {
    using namespace tmfqs;

    RegisterConfig denseCfg;
    denseCfg.strategy = StorageStrategyKind::Dense;
    QuantumRegister denseReg(22, denseCfg);

    RegisterConfig bloscCfg;
    bloscCfg.strategy = StorageStrategyKind::Blosc;
    bloscCfg.blosc.chunkStates = 16384;
    bloscCfg.blosc.gateCacheSlots = 8;
    bloscCfg.blosc.clevel = 1;
    QuantumRegister bloscReg(22, bloscCfg);

    RegisterConfig zfpCfg;
    zfpCfg.strategy = StorageStrategyKind::Zfp;
    zfpCfg.zfp.mode = ZfpCompressionMode::FixedRate;
    zfpCfg.zfp.rate = 24.0;
    zfpCfg.zfp.chunkStates = 32768;
    zfpCfg.zfp.gateCacheSlots = 16;
    QuantumRegister zfpReg(22, zfpCfg);

    (void)denseReg;
    (void)bloscReg;
    (void)zfpReg;
    return 0;
}
```

### Compile and Link Your Own Program

```bash
icpx -std=c++17 my_program.cpp \
  -I ./include \
  -L ./lib64 -ltmfqsfs \
  -o my_program

export LD_LIBRARY_PATH="$PWD/lib64:${LD_LIBRARY_PATH}"
./my_program
```

## Build, Test, and Benchmark Workflow

All commands below run from repository root.

### Build Library + Examples

```bash
make
```

Build output highlights:

- Shared library at `lib64/libtmfqsfs.so`
- Example binaries in `bin/`

### Run Integration Tests

```bash
make tests
```

This compiles and runs integration suites:

- `test_algorithms`
- `test_register_validation`
- `test_storage_parity`
- `test_storage_cache`

### Run Benchmarks

```bash
make benchmarks
```

This builds and runs `benchmark_backends`.

### Performance-Oriented Build + Checks

```bash
make perf
```

This rebuilds with optimized flags and runs tests and benchmarks.

### Sanitizer Build + Tests

```bash
make sanitize
```

This rebuilds with AddressSanitizer and UndefinedBehaviorSanitizer and runs tests.

## Performance Notes

- State-vector simulation grows exponentially with qubit count (`2^n` amplitudes).
- Dense backend is typically best for smaller registers due to lower compression overhead.
- Compressed backends (Blosc/ZFP) trade CPU for memory footprint and may help at larger register sizes.
- Benchmark sources are in `benchmarks/`, with utilities:
  - `benchmark_backends`
  - `benchmark_regression_check`
- Existing profiling artifacts are under `profiling/`.

## Project Layout

```text
TMFQS/
├── include/            # Public headers (tmfqsfs.h + tmfqs/*)
├── src/                # Library implementation
├── examples/           # Example programs -> bin/
├── tests/              # Integration tests
├── benchmarks/         # Benchmark sources and make targets
├── bin/                # Built binaries
├── build/              # Build artifacts
├── lib64/              # Shared library output
├── Makefile            # Top-level build/test/benchmark targets
└── README.md
```

## Limitations

- State-vector memory scales exponentially with qubits.
- Practical qubit limits depend on available RAM and selected backend strategy.
- Simulation models ideal operations; no noise/error model is included.
- Linux workflow is the only one currently documented and verified.

## License

Licensed under the terms in [LICENSE](LICENSE).

## Author

Gilberto Javier Díaz Toro

## Contributors

Daniel Adrián González Buendía

