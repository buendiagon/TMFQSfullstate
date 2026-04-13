# TMFQS

TMFQS is a C++17 state-vector quantum simulation library for experimenting with dense and compressed quantum-state storage. It includes a public library (`tmfqsfs`), example programs, integration tests, backend benchmarks, report helpers, and comparison utilities for checking accuracy across storage strategies.

## Getting Started

Requirements:

- CMake 3.21 or newer.
- A C++17 compiler such as `g++`, Clang, or IntelLLVM.
- Optional Blosc2 development files for the `blosc` backend.
- Optional ZFP development files for the `zfp` backend.

Configure and build the release preset:

```bash
cmake --preset release
cmake --build --preset release
```

Run a quick example:

```bash
./build/release/bin/grover 3 5
./build/release/bin/groverLazy 3 5
./build/release/bin/qftG 18 --strategy dense
```

Run tests from a preset that builds tests:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Build presets:

- `dev`: `RelWithDebInfo`, examples, tests, and benchmarks.
- `debug`: unoptimized debug build, examples, tests, and benchmarks.
- `release`: optimized release build, examples and benchmarks.
- `asan`: debug build with AddressSanitizer and UndefinedBehaviorSanitizer.

Useful CMake options:

- `-DTMFQS_BUILD_EXAMPLES=ON|OFF`
- `-DTMFQS_BUILD_TESTS=ON|OFF`
- `-DTMFQS_BUILD_BENCHMARKS=ON|OFF`
- `-DTMFQS_WITH_BLOSC2=AUTO|ON|OFF`
- `-DTMFQS_WITH_ZFP=AUTO|ON|OFF`
- `-DTMFQS_ENABLE_SANITIZERS=address,undefined`
- `-DTMFQS_WARNINGS_AS_ERRORS=ON|OFF`
- `-DTMFQS_ENABLE_IPO=ON|OFF`

## Example Programs

Built examples are written to `build/<preset>/bin/`.

`grover` runs Grover search with materialized diffusion:

```bash
./build/release/bin/grover <num_qubits> <marked_state[,marked_state...]> [options]
```

`groverLazy` runs the same Grover circuit with lazy affine diffusion:

```bash
./build/release/bin/groverLazy <num_qubits> <marked_state[,marked_state...]> [options]
```

`qftG` runs a QFT experiment:

```bash
./build/release/bin/qftG <num_qubits> [--input-family pattern|random-phase] [options]
```

Common storage options:

- `--strategy dense|blosc|zfp|auto`
- `--chunk-states N`
- `--cache-slots N`
- `--clevel N`
- `--nthreads N`
- `--threshold-mb N`
- `--zfp-mode rate|precision|accuracy`
- `--zfp-rate R`
- `--zfp-precision B`
- `--zfp-accuracy A`
- `--zfp-chunk-states N`
- `--zfp-cache-slots N`

Examples:

```bash
./build/release/bin/grover 24 2097152 --strategy blosc
./build/release/bin/groverLazy 24 2097152 --strategy blosc
./build/release/bin/qftG 22 --input-family random-phase --strategy blosc
./build/release/bin/qftG 22 --strategy zfp --zfp-mode precision --zfp-precision 40
./build/release/bin/zfp_error_analysis --qubits 18,19,20 --csv /tmp/tmfqs_zfp_error.csv
```

## Library Usage

Include the umbrella header:

```cpp
#include "tmfqsfs.h"
```

Build and run a circuit:

```cpp
#include "tmfqsfs.h"

int main() {
    using namespace tmfqs;

    RegisterConfig config;
    config.strategy = StorageStrategyKind::Blosc;
    config.workloadHint = StorageWorkloadHint::Qft;

    sim::ExecutionConfig execution;
    execution.backend = config;
    execution.observability.traceOperations = true;

    circuit::Circuit circuit = circuit::makeQft(12);
    state::QuantumState initial = state::QuantumState::basis(12);

    sim::RunResult result = sim::Simulator(execution).run(circuit, initial);

    Mt19937RandomSource rng(123456u);
    StateIndex measured = result.state.measure(rng);
    (void)measured;

    return 0;
}
```

You can also build circuits manually:

```cpp
circuit::Circuit circuit(3);
circuit.h(0)
       .cx(0, 1)
       .controlledPhase(1, 2, kPi / 4.0)
       .swap(0, 2);
```

For lower-level storage experiments, use `QuantumRegister` directly:

```cpp
RegisterConfig config;
config.strategy = StorageStrategyKind::Zfp;
config.zfp.mode = ZfpCompressionMode::FixedPrecision;
config.zfp.precision = 40;

QuantumRegister reg(10, 0, config);
reg.applyHadamard(0);
reg.applyControlledNot(0, 5);
double probability = reg.totalProbability();
```

## Storage Backends

TMFQS supports these runtime strategies:

- `Dense`: uncompressed in-memory amplitude storage.
- `Blosc`: chunked lossless compression through Blosc2.
- `Zfp`: chunked floating-point compression through ZFP.
- `Auto`: dense below `autoThresholdBytes`, otherwise Blosc if available, then ZFP, then dense.

`RegisterConfig` controls backend selection:

```cpp
RegisterConfig config;
config.strategy = StorageStrategyKind::Auto;
config.autoThresholdBytes = 8u * 1024u * 1024u;
config.workloadHint = StorageWorkloadHint::Grover;
```

Current backend defaults are stable unless you change the config or pass CLI overrides:

- Blosc: `chunkStates = 32768`, `gateCacheSlots = 8`, `clevel = 1`, `nthreads = 4`, `compcode = 1`, `useShuffle = true`.
- ZFP: `mode = FixedPrecision`, `precision = 40`, `chunkStates = 32768`, `gateCacheSlots = 8`, `nthreads = 4`.

Workload hints describe the algorithm to the storage layer without rewriting those defaults:

- `Generic`: conservative defaults for mixed access patterns.
- `Grover`: repeated full-register sweeps.
- `Qft`: transform-style full-register sweeps.

Explicit override markers preserve hand-tuned values during auto tuning. For example:

```cpp
config.blosc.chunkStates = 16384;
config.blosc.gateCacheSlots = 64;
config.bloscOverrides.chunkStates = true;
config.bloscOverrides.gateCacheSlots = true;
```

Blosc options:

- `chunkStates`: basis states per compressed chunk.
- `gateCacheSlots`: decompressed chunks kept in the LRU cache during gates.
- `clevel`: compression level from 0 to 9.
- `nthreads`: Blosc worker threads.
- `compcode`: Blosc compressor id.
- `useShuffle`: byte shuffle filter.

ZFP options:

- `mode`: `FixedRate`, `FixedPrecision`, or `FixedAccuracy`.
- `rate`: bits per value for fixed-rate mode.
- `precision`: significant bits for fixed-precision mode.
- `accuracy`: absolute error target for fixed-accuracy mode.
- `chunkStates`, `gateCacheSlots`, and `nthreads`.

## Observability

`sim::RunResult` returns both the final state and a `RunReport`:

```cpp
sim::RunResult run = sim::Simulator(execution).run(circuit, initial);
double seconds = run.report.executionSeconds;
size_t operations = run.report.operationCount;
```

Enable per-operation tracing:

```cpp
execution.observability.traceOperations = true;
```

Serialize reports:

```cpp
std::string json = experiment::toJson(run.report);
std::string csv = experiment::toCsv(run.report);
experiment::writeJsonReport(run.report, "run.json");
experiment::writeCsvReport(run.report, "run.csv");
```

Simulator results stay backend-backed after a run. Calling `measure()`, `amplitude()`, or `totalProbability()` on `RunResult::state` does not export a full dense amplitude vector. Calling `amplitudes()` is the explicit dense export path.

Codec/cache metrics are controlled by environment variables:

```bash
TMFQS_CODEC_METRICS=1 ./build/release/bin/qftG 22 --strategy blosc
TMFQS_BACKEND_METRICS=1 ./build/release/bin/grover 22 2097152 --strategy blosc
```

The metrics include cache loads, stores, flushes, evictions, encode/decode calls, bytes, and codec time.

## Comparing Results

Use `tmfqs::experiment::compareStates` to compare a reference state against a candidate state:

```cpp
experiment::StateComparison comparison =
    experiment::compareStates(denseResult.state, compressedResult.state);

std::string json = experiment::toJson(comparison);
std::string csvRow = experiment::toCsvRow("qft", "blosc", 22, comparison);
```

Comparison reads states through their normal amplitude access path instead of allocating two full amplitude vectors. Reported fields include:

- `bitwiseEqual`
- `maxAbsAmplitudeError`
- `maxAbsComponentError`
- `rmseAmplitude`
- `relL2`
- `maxAbsProbabilityError`
- `totalProbabilityDiff`
- `worstState`

`examples/zfp_error_analysis.cpp` shows a complete workflow for comparing dense and ZFP runs across Grover and QFT scenarios and optionally writing CSV output.

## Benchmarks

Build benchmarks with a preset that enables them:

```bash
cmake --preset release
cmake --build --preset release
./build/release/bin/benchmark_backends
./build/release/bin/benchmark_backends --csv /tmp/tmfqs_bench.csv
```

`benchmark_regression_check` compares two benchmark CSV files and is useful for CI or local performance checks.

## Adding Storage Backends

A new backend can be added either as a full `IStateBackend` implementation or by reusing the compressed backend template.

For an experimental compressed backend:

1. Add a codec wrapper with the operations expected by `CompressedStateBackend`: configure storage, compress a chunk, decompress a chunk, clone/copy state, and report availability.
2. Add a codec policy like the existing Blosc and ZFP policies. The policy supplies backend names, metrics names, `chunkStates`, and `gateCacheSlots`.
3. Add config structs and override markers to `RegisterConfig` for backend-specific knobs.
4. Add a new `StorageStrategyKind` value and wire it through `StorageStrategyRegistry::isAvailable`, `resolve`, `toString`, `tuneConfig`, and `createSelection`.
5. Add a public factory helper and include it from `tmfqsfs.h` when it is part of the public surface.
6. Add parity tests against dense storage. Cover direct register operations, simulator circuit execution, cache eviction, copy/assignment, and measurement.
7. Add benchmark coverage if the backend is intended for performance experiments.

When the backend has a different storage model, implement `IStateBackend` directly. The required operations are initialization, load/export, chunk visitation, amplitude read/write, probability/measurement, built-in gate kernels, arbitrary `applyGate`, and optional operation batching.

## Project Layout

- `include/`: public headers.
- `src/`: library implementation.
- `examples/`: runnable programs.
- `tests/integration/`: integration and parity tests.
- `benchmarks/`: backend benchmarks and regression checker.
- `tasks/`: project-local workflow notes.
