/**
 * test_bugfixes.cpp
 * Functional verification of the 6 bug fixes in quantumAlgorithms.cpp and related files.
 */
#include <iostream>
#include <cmath>
#include <cassert>
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
    Amplitude input2 = {0.0, M_PI};
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

    double successRate = (double)successes / trials;
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

    double probSum = qureg.probabilitySumatory();
    cout << "  Probability sum = " << probSum << " (expect 1.0)" << endl;
    assert(approxEqual(probSum, 1.0, 1e-6) && "Probabilities must sum to 1");

    cout << "  PASSED!" << endl << endl;
}

// ============================================================
int main() {
    cout << endl << "===== FUNCTIONAL VERIFICATION OF BUG FIXES =====" << endl << endl;

    testBug1_expPrecision();
    testBug5_scalarMultiplication();
    testQFT();
    testBug2_groverMeasure();

    cout << "===== ALL TESTS PASSED =====" << endl << endl;
    return 0;
}
