/**
 * test_bugfixes.cpp
 * Functional verification of the 6 bug fixes in quantumAlgorithms.cpp and related files.
 */
#include <iostream>
#include <cmath>
#include <cassert>
#include <stdexcept>
#include <vector>
#include "tmfqsfs.h"
#include "utils.h"

using namespace std;

bool approxEqual(double a, double b, double tol = 1e-10) {
    return fabs(a - b) < tol;
}

// ============================================================
// TEST Bug 1: exp() precision in eRaisedToComplex
// ============================================================
void testBug1_expPrecision() {
    cout << "=== Bug 1: exp() precision ===" << endl;

    // e^(0 + i*0) should be exactly 1 + 0i
    Amplitude input1 = {0.0, 0.0};
    Amplitude result1 = eRaisedToComplex(input1);
    cout << "  e^(0+0i) = " << result1.real << " + " << result1.imag << "i" << endl;
    assert(approxEqual(result1.real, 1.0) && approxEqual(result1.imag, 0.0));

    // e^(0 + i*pi) should be -1 + 0i  (Euler's identity)
    Amplitude input2 = {0.0, pi};
    Amplitude result2 = eRaisedToComplex(input2);
    cout << "  e^(i*pi) = " << result2.real << " + " << result2.imag << "i (expect -1 + 0i)" << endl;
    assert(approxEqual(result2.real, -1.0) && approxEqual(result2.imag, 0.0));

    // e^(1 + i*0) should be e + 0i
    Amplitude input3 = {1.0, 0.0};
    Amplitude result3 = eRaisedToComplex(input3);
    cout << "  e^(1+0i) = " << result3.real << " (expect " << exp(1.0) << ")" << endl;
    assert(approxEqual(result3.real, exp(1.0)));

    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST Bug 2: Grover() returns correct result (not always 0)
// ============================================================
void testBug2_groverMeasure() {
    cout << "=== Bug 2: Grover measurement ===" << endl;

    unsigned int omega = 5;
    unsigned int numBits = 3;
    int successes = 0;
    int trials = 20;

    cout << "  Searching for omega=" << omega << " with " << numBits << " qubits (" << trials << " trials)" << endl;

    for (int t = 0; t < trials; t++) {
        unsigned int result = Grover(omega, numBits, false);
        if (result == omega) successes++;
        cout << "    trial " << t << ": got " << result << (result == omega ? " ✓" : " ✗") << endl;
    }

    double successRate = static_cast<double>(successes) / trials;
    cout << "  Success rate: " << successes << "/" << trials << " = " << (successRate * 100) << "%" << endl;
    // Grover should find omega with high probability (> 50% at minimum)
    assert(successes > 0 && "Grover should find omega at least once in 20 trials");
    cout << "  PASSED! (found omega at least once)" << endl << endl;
}

// ============================================================
// TEST Bug 5: QuantumGate::operator* scalar multiplication
// ============================================================
void testBug5_scalarMultiplication() {
    cout << "=== Bug 5: scalar multiplication ===" << endl;

    QuantumGate H = QuantumGate::Hadamard();
    Amplitude scalar = {2.0, 0.0};
    QuantumGate result = H * scalar;

    double expected00 = 2.0 / sqrt(2.0);
    cout << "  H[0][0] = " << H[0][0].real << ", 2*H[0][0] = " << result[0][0].real
         << " (expect " << expected00 << ")" << endl;

    // Before the fix, result was always zero matrix
    assert(!approxEqual(result[0][0].real, 0.0) && "Scalar mult should NOT produce zero");
    assert(approxEqual(result[0][0].real, expected00));
    assert(approxEqual(result[1][1].real, -expected00));

    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST QFT: quantumFourierTransform runs correctly
// ============================================================
void testQFT() {
    cout << "=== QFT functional test ===" << endl;

    QuantumRegister qureg(3, 0);  // |000>
    quantumFourierTransform(qureg);

    // QFT of |0> should give equal superposition: all amplitudes = 1/sqrt(8)
    double expectedAmp = 1.0 / sqrt(8.0);
    cout << "  QFT|000> amplitudes (expect ~" << expectedAmp << " for all):" << endl;

    bool allCorrect = true;
    for (unsigned int s = 0; s < 8; s++) {
        Amplitude amp = qureg.amplitude(s);
        cout << "    |" << s << "> : " << amp.real << " + " << amp.imag << "i" << endl;
        if (!approxEqual(amp.real, expectedAmp, 1e-6)) allCorrect = false;
    }
    assert(allCorrect && "QFT|000> amplitudes should be uniform");

    double probSum = qureg.totalProbability();
    cout << "  Probability sum = " << probSum << " (expect 1.0)" << endl;
    assert(approxEqual(probSum, 1.0, 1e-6) && "Probabilities must sum to 1");

    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST: backend-native Grover primitives on QuantumRegister
// ============================================================
void testGroverPrimitives() {
    cout << "=== Grover primitive ops test ===" << endl;

    QuantumRegister qureg(2, 0);  // |00>
    qureg.Hadamard(0);
    qureg.Hadamard(1);            // uniform superposition over 4 states

    // Phase-flip one basis state and verify sign inversion.
    Amplitude before = qureg.amplitude(3);
    qureg.phaseFlipBasisState(3);
    Amplitude after = qureg.amplitude(3);
    assert(approxEqual(after.real, -before.real, 1e-9));
    assert(approxEqual(after.imag, -before.imag, 1e-9));

    // Diffusion should preserve normalization.
    qureg.inversionAboutMean();
    double probSum = qureg.totalProbability();
    assert(approxEqual(probSum, 1.0, 1e-6));

    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST: Grover invalid input throws
// ============================================================
void testGroverInvalidInputThrows() {
    cout << "=== Grover invalid input test ===" << endl;
    bool threw = false;
    try {
        (void)Grover(8, 3, false);  // invalid: valid range is [0, 7]
    } catch(const std::invalid_argument &) {
        threw = true;
    }
    assert(threw && "Grover should throw std::invalid_argument for out-of-range omega");
    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST: Algorithm operation executor correctness
// ============================================================
void testAlgorithmOperationExecutor() {
    cout << "=== Algorithm operation executor test ===" << endl;
    QuantumRegister qureg(2, 0);
    std::vector<AlgorithmOperation> ops = {
        {AlgorithmOperationKind::Hadamard, 0},
        {AlgorithmOperationKind::Swap, 0, 1},
        {AlgorithmOperationKind::Swap, 0, 1}
    };
    executeAlgorithmOperations(qureg, ops);
    Amplitude a00 = qureg.amplitude(0);
    Amplitude a10 = qureg.amplitude(2);
    assert(approxEqual(a00.real, 1.0 / sqrt(2.0), 1e-6));
    assert(approxEqual(a10.real, 1.0 / sqrt(2.0), 1e-6));
    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST: compiled algorithm plan repeat blocks
// ============================================================
void testCompiledAlgorithmPlanRepeat() {
    cout << "=== Compiled plan repeat-block test ===" << endl;
    QuantumRegister qureg(2, 0);

    CompiledAlgorithmPlan plan;
    plan.addOperation({AlgorithmOperationKind::Hadamard, 0});
    plan.addRepeatBlock({{AlgorithmOperationKind::Swap, 0, 1}}, 2);
    executeCompiledAlgorithmPlan(qureg, plan);

    Amplitude a00 = qureg.amplitude(0);
    Amplitude a10 = qureg.amplitude(2);
    assert(approxEqual(a00.real, 1.0 / sqrt(2.0), 1e-6));
    assert(approxEqual(a10.real, 1.0 / sqrt(2.0), 1e-6));
    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST: deterministic RNG control through setRandomSeed
// ============================================================
void testDeterministicGroverSeed() {
    cout << "=== Grover deterministic seed test ===" << endl;
    setRandomSeed(12345u);
    unsigned int r1 = Grover(5, 3, false);
    setRandomSeed(12345u);
    unsigned int r2 = Grover(5, 3, false);
    assert(r1 == r2 && "Grover results should be reproducible with the same RNG seed");
    cout << "  PASSED! r1=r2=" << r1 << endl << endl;
}

// ============================================================
// TEST: default constructor creates a valid 0-qubit register
// ============================================================
void testDefaultConstructedRegisterIsValid() {
    cout << "=== Default register validity test ===" << endl;
    QuantumRegister qureg;
    assert(qureg.qubitCount() == 0u);
    assert(qureg.stateCount() == 1u);
    assert(qureg.amplitudeElementCount() == 2u);
    Amplitude amp = qureg.amplitude(0);
    assert(approxEqual(amp.real, 1.0, 1e-12));
    assert(approxEqual(amp.imag, 0.0, 1e-12));
    assert(approxEqual(qureg.totalProbability(), 1.0, 1e-12));
    assert(qureg.measure() == 0u);

    bool threw = false;
    try {
        qureg.Hadamard(0);
    } catch(const std::out_of_range &) {
        threw = true;
    }
    assert(threw && "Applying a gate on non-existent qubit must throw");
    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST: out-of-range point queries throw consistently
// ============================================================
void testOutOfRangeQueriesThrow() {
    cout << "=== Out-of-range query behavior test ===" << endl;
    QuantumRegister qureg(2, 0);

    bool ampThrew = false;
    try {
        (void)qureg.amplitude(4);
    } catch(const std::out_of_range &) {
        ampThrew = true;
    }
    assert(ampThrew && "amplitude() must throw on out-of-range state");

    bool probThrew = false;
    try {
        (void)qureg.probability(4);
    } catch(const std::out_of_range &) {
        probThrew = true;
    }
    assert(probThrew && "probability() must throw on out-of-range state");
    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST: register reports the same resolved strategy as factory
// ============================================================
void testResolvedStrategyConsistency() {
    cout << "=== Strategy resolution consistency test ===" << endl;
    RegisterConfig cfg;
    cfg.strategy = StorageStrategyKind::Auto;
    cfg.autoThresholdBytes = 1u;
    QuantumRegister qureg(3, cfg);
    StorageStrategyKind expected = resolveStorageStrategy(3, cfg);
    assert(qureg.storageStrategy() == expected);
    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST: facade-level gate helper validation
// ============================================================
void testPublicGateValidation() {
    cout << "=== Public gate validation test ===" << endl;
    QuantumRegister qureg(2, 0);

    bool hadamardThrew = false;
    try { qureg.Hadamard(2); } catch(const std::out_of_range &) { hadamardThrew = true; }
    assert(hadamardThrew && "Hadamard must throw on out-of-range qubit");

    bool pauliXThrew = false;
    try { qureg.PauliX(2); } catch(const std::out_of_range &) { pauliXThrew = true; }
    assert(pauliXThrew && "PauliX must throw on out-of-range qubit");

    bool cnotRangeThrew = false;
    try { qureg.ControlledNot(0, 2); } catch(const std::out_of_range &) { cnotRangeThrew = true; }
    assert(cnotRangeThrew && "ControlledNot must throw on out-of-range qubit");

    bool cnotDistinctThrew = false;
    try { qureg.ControlledNot(1, 1); } catch(const std::invalid_argument &) { cnotDistinctThrew = true; }
    assert(cnotDistinctThrew && "ControlledNot must throw when control==target");

    bool swapDistinctThrew = false;
    try { qureg.Swap(0, 0); } catch(const std::invalid_argument &) { swapDistinctThrew = true; }
    assert(swapDistinctThrew && "Swap must throw when qubits are equal");
    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST: applyGate duplicate and range checks
// ============================================================
void testApplyGateValidation() {
    cout << "=== applyGate validation test ===" << endl;
    QuantumRegister qureg(2, 0);

    bool duplicateThrew = false;
    try {
        qureg.applyGate(QuantumGate::ControlledNot(), QubitList{1, 1});
    } catch(const std::invalid_argument &) {
        duplicateThrew = true;
    }
    assert(duplicateThrew && "applyGate must reject duplicate qubits");

    bool rangeThrew = false;
    try {
        qureg.applyGate(QuantumGate::Hadamard(), QubitList{2});
    } catch(const std::out_of_range &) {
        rangeThrew = true;
    }
    assert(rangeThrew && "applyGate must reject out-of-range qubits");
    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST: strict initUniformSuperposition validation
// ============================================================
void testUniformSuperpositionValidation() {
    cout << "=== Uniform superposition validation test ===" << endl;
    QuantumRegister qureg(3, 0);

    bool emptyThrew = false;
    try {
        qureg.initUniformSuperposition(BasisStateList{});
    } catch(const std::invalid_argument &) {
        emptyThrew = true;
    }
    assert(emptyThrew && "initUniformSuperposition must reject empty input");

    bool outOfRangeThrew = false;
    try {
        qureg.initUniformSuperposition(BasisStateList{0, 8});
    } catch(const std::out_of_range &) {
        outOfRangeThrew = true;
    }
    assert(outOfRangeThrew && "initUniformSuperposition must reject out-of-range states");
    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST: invalid Blosc config rejection
// ============================================================
void testInvalidBloscConfigValidation() {
    cout << "=== Blosc config validation test ===" << endl;
    if(!isStrategyAvailable(StorageStrategyKind::Blosc)) {
        cout << "  SKIPPED (blosc backend unavailable)" << endl << endl;
        return;
    }

    RegisterConfig cfg;
    cfg.strategy = StorageStrategyKind::Blosc;
    cfg.blosc.chunkStates = 0;
    bool chunkStatesThrew = false;
    try {
        QuantumRegister q(3, cfg);
        (void)q;
    } catch(const std::invalid_argument &) {
        chunkStatesThrew = true;
    }
    assert(chunkStatesThrew && "Blosc config with chunkStates=0 must throw");

    cfg = RegisterConfig{};
    cfg.strategy = StorageStrategyKind::Blosc;
    cfg.blosc.gateCacheSlots = 0;
    bool cacheSlotsThrew = false;
    try {
        QuantumRegister q(3, cfg);
        (void)q;
    } catch(const std::invalid_argument &) {
        cacheSlotsThrew = true;
    }
    assert(cacheSlotsThrew && "Blosc config with gateCacheSlots=0 must throw");

    cfg = RegisterConfig{};
    cfg.strategy = StorageStrategyKind::Auto;
    cfg.autoThresholdBytes = 1u; // force Blosc selection when available
    cfg.blosc.chunkStates = 0;
    bool autoThrew = false;
    try {
        QuantumRegister q(3, cfg);
        (void)q;
    } catch(const std::invalid_argument &) {
        autoThrew = true;
    }
    assert(autoThrew && "Auto strategy selecting Blosc must validate Blosc config");
    cout << "  PASSED!" << endl << endl;
}

// ============================================================
// TEST: Dense vs Blosc operation parity + measure parity
// ============================================================
void testDenseBloscParityAndMeasure() {
    cout << "=== Dense/Blosc parity + measure test ===" << endl;
    if(!isStrategyAvailable(StorageStrategyKind::Blosc)) {
        cout << "  SKIPPED (blosc backend unavailable)" << endl << endl;
        return;
    }

    RegisterConfig denseCfg;
    denseCfg.strategy = StorageStrategyKind::Dense;
    RegisterConfig bloscCfg;
    bloscCfg.strategy = StorageStrategyKind::Blosc;

    QuantumRegister dense(3, denseCfg);
    QuantumRegister blosc(3, bloscCfg);

    std::vector<AlgorithmOperation> ops = {
        {AlgorithmOperationKind::Hadamard, 0},
        {AlgorithmOperationKind::ControlledNot, 0, 2},
        {AlgorithmOperationKind::ControlledPhaseShift, 2, 1, pi / 8.0},
        {AlgorithmOperationKind::PauliX, 1}
    };
    executeAlgorithmOperations(dense, ops);
    executeAlgorithmOperations(blosc, ops);
    dense.applyGate(QuantumGate::Swap(), QubitList{0, 2});
    blosc.applyGate(QuantumGate::Swap(), QubitList{0, 2});

    for(unsigned int s = 0; s < 8; ++s) {
        Amplitude a = dense.amplitude(s);
        Amplitude b = blosc.amplitude(s);
        assert(approxEqual(a.real, b.real, 1e-9));
        assert(approxEqual(a.imag, b.imag, 1e-9));
    }

    AmplitudesVector equalSuperposition(8, 0.5); // two-qubit: [0.5,0, 0.5,0, 0.5,0, 0.5,0]
    equalSuperposition[1] = 0.0;
    equalSuperposition[3] = 0.0;
    equalSuperposition[5] = 0.0;
    equalSuperposition[7] = 0.0;
    QuantumRegister denseMeasure(2, equalSuperposition, denseCfg);
    QuantumRegister bloscMeasure(2, equalSuperposition, bloscCfg);
    setRandomSeed(20260306u);
    unsigned int denseSample = denseMeasure.measure();
    setRandomSeed(20260306u);
    unsigned int bloscSample = bloscMeasure.measure();
    assert(denseSample == bloscSample && "measure() should be backend-consistent for same seeded RNG and state");
    cout << "  PASSED!" << endl << endl;
}

// ============================================================
int main() {
    cout << endl << "===== FUNCTIONAL VERIFICATION OF BUG FIXES =====" << endl << endl;

    testBug1_expPrecision();
    testBug5_scalarMultiplication();
    testQFT();
    testBug2_groverMeasure();
    testGroverPrimitives();
    testGroverInvalidInputThrows();
    testAlgorithmOperationExecutor();
    testCompiledAlgorithmPlanRepeat();
    testDeterministicGroverSeed();
    testDefaultConstructedRegisterIsValid();
    testOutOfRangeQueriesThrow();
    testResolvedStrategyConsistency();
    testPublicGateValidation();
    testApplyGateValidation();
    testUniformSuperpositionValidation();
    testInvalidBloscConfigValidation();
    testDenseBloscParityAndMeasure();

    cout << "===== ALL TESTS PASSED =====" << endl << endl;
    return 0;
}
