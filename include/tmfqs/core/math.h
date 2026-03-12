#ifndef TMFQS_CORE_MATH_H
#define TMFQS_CORE_MATH_H

#include "tmfqs/core/types.h"

namespace tmfqs {

/**
 * @brief Adds two complex amplitudes.
 * @param a Left operand.
 * @param b Right operand.
 * @return Complex sum.
 */
Amplitude amplitudeAdd(Amplitude a, Amplitude b);
/**
 * @brief Multiplies two complex amplitudes.
 * @param a Left operand.
 * @param b Right operand.
 * @return Complex product.
 */
Amplitude amplitudeMultiply(Amplitude a, Amplitude b);
/**
 * @brief Computes complex exponential.
 * @param exponent Complex exponent `x + i*y`.
 * @return `e^(x + i*y)`.
 */
Amplitude complexExp(Amplitude exponent);

} // namespace tmfqs

#endif // TMFQS_CORE_MATH_H
