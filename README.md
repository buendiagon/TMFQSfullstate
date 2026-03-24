# TMFQS

TMFQS is a C++17 state-vector quantum simulator for learning, experimentation, and backend strategy research. It provides a shared library (`tmfqsfs`), public C++ headers (`tmfqsfs.h`), example binaries, integration tests, and backend benchmarking tools.

## What You Get

- A C++ library built from `src/` and emitted under the selected CMake build directory
- Public API headers under `include/` with umbrella include `tmfqsfs.h`
- Example programs in `examples/` compiled into `build/<preset>/bin/`
- Integration test suites in `tests/integration/`
- Runtime-selectable storage backends: Dense, Blosc, ZFP, and Auto
- Benchmark tools in `benchmarks/` for backend performance and regression checks

## Requirements

This repository is currently documented for Linux only (verified workflow).

- Compiler: any C++17 compiler, for example Intel oneAPI `icpx` or `g++`
- Build tools: CMake >= 3.21 and a native build tool supported by CMake
- Optional backend dependencies:
  - Blosc2 (`blosc2.h`, `libblosc2`) to enable the Blosc backend
  - ZFP (`zfp.h`, `libzfp`) to enable the ZFP backend

If optional dependencies are not found during configure, the corresponding backend is not compiled in unless you force it with `TMFQS_WITH_BLOSC2=ON` or `TMFQS_WITH_ZFP=ON`.

## Quickstart

```bash
git clone https://github.com/diaztoro/TMFQS.git
cd TMFQS

cmake --preset dev
cmake --build --preset dev
ctest --preset dev

./build/dev/bin/grover 3 5
```

Expected result: the program runs Grover search for state `|5>` in a 3-qubit space and prints the resolved backend and measured state.

## Usage

Preset builds place runnable binaries in `build/<preset>/bin/`. The build tree is configured so local binaries run without setting `LD_LIBRARY_PATH`.

### Binary Quick Reference

```bash
./build/dev/bin/qft <num_qubits> <initial_state>
./build/dev/bin/grover <num_qubits> <marked_state[,marked_state...]> [options]
./build/dev/bin/applyHadamard <num_qubits> <qubit> <init_state>
./build/dev/bin/applyControlledNot <num_qubits> <control_qubit> <target_qubit>
./build/dev/bin/applyControlledPhaseShift <num_qubits> <control_qubit> <target_qubit> <init_state>
./build/dev/bin/getSumOfProbabilities <num_qubits>
./build/dev/bin/qftG <num_qubits> [options]
```

### Example Commands

```bash
./build/dev/bin/qft 3 0
./build/dev/bin/grover 4 11
./build/dev/bin/grover 22 17,131071 --strategy blosc --chunk-states 16384 --cache-slots 8
./build/dev/bin/applyHadamard 3 1 0
./build/dev/bin/applyControlledNot 3 0 2
./build/dev/bin/applyControlledPhaseShift 3 0 2 0
./build/dev/bin/getSumOfProbabilities 5
```

## `grover` CLI Reference

Usage:

```bash
./build/dev/bin/grover <num_qubits> <marked_state[,marked_state...]> [--verbose] \
  [--strategy dense|blosc|zfp|auto] [--chunk-states N] [--cache-slots N] \
  [--clevel N] [--nthreads N] [--threshold-mb N] \
  [--zfp-mode rate|precision|accuracy] [--zfp-rate R] [--zfp-precision B] \
  [--zfp-accuracy A] [--zfp-chunk-states N] [--zfp-cache-slots N]
```

Notes:

- Pass one marked state for classic Grover or a comma-separated list for a multi-marked oracle.
- The backend options match `qftG`, and the program prints the resolved strategy before the measurement result.

## `qftG` CLI Reference

Usage:

```bash
./build/dev/bin/qftG <num_qubits> [--strategy dense|blosc|zfp|auto] \
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
./build/dev/bin/qftG 22 --strategy dense
./build/dev/bin/qftG 22 --strategy blosc --chunk-states 16384 --clevel 1 --nthreads 2
./build/dev/bin/qftG 22 --strategy zfp --zfp-mode rate --zfp-rate 24 --zfp-chunk-states 32768
./build/dev/bin/qftG 22 --strategy auto --threshold-mb 8
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

    RegisterConfig groverCfg;
    groverCfg.strategy = StorageStrategyKind::Blosc;
    groverCfg.blosc.chunkStates = 16384;
    groverCfg.blosc.gateCacheSlots = 8;
    algorithms::GroverConfig cfg{BasisStateList{5u, 11u}, 22u, false, groverCfg};
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
cmake --install build/dev --prefix ./install

icpx -std=c++17 my_program.cpp \
  -I ./install/include \
  -L ./install/lib64 -ltmfqsfs \
  -Wl,-rpath,"$PWD/install/lib64" \
  -o my_program
```

### Use TMFQS From Another CMake Project

```cmake
find_package(TMFQS CONFIG REQUIRED)

add_executable(my_program my_program.cpp)
target_link_libraries(my_program PRIVATE TMFQS::tmfqsfs)
```

After installing TMFQS, configure your downstream project with `-DCMAKE_PREFIX_PATH=<install-prefix>`.

## Build, Test, and Benchmark Workflow

All commands below run from repository root.

### Configure and Build

```bash
cmake --preset dev
cmake --build --preset dev
```

Build output highlights:

- Shared library in `build/dev/lib/`
- Example binaries in `build/dev/bin/`

### Run Integration Tests

```bash
ctest --preset dev
```

This runs integration suites:

- `test_algorithms`
- `test_register_validation`
- `test_storage_parity`
- `test_storage_cache`

### Release Build

```bash
cmake --preset release
cmake --build --preset release
```

Use `build/release/bin/` for optimized binaries.

### Run Benchmarks

```bash
./build/dev/bin/benchmark_backends
```

This runs the benchmark executable produced by the `dev` or `release` preset.

### Sanitizer Build + Tests

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

This configures AddressSanitizer and UndefinedBehaviorSanitizer through CMake target options.

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
├── examples/           # Example programs
├── tests/              # Integration tests
├── benchmarks/         # Benchmark sources
├── build/              # CMake build trees
├── cmake/              # Find modules and package config template
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
