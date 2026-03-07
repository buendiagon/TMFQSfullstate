#ifndef TMFQS_ALGORITHMS_QFT_H
#define TMFQS_ALGORITHMS_QFT_H

#include "tmfqs/register/quantum_register.h"

namespace tmfqs {
namespace algorithms {

void qftInPlace(QuantumRegister &quantumRegister);

} // namespace algorithms
} // namespace tmfqs

#endif // TMFQS_ALGORITHMS_QFT_H
