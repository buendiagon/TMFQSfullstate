#ifndef TMFQS_CORE_MATH_H
#define TMFQS_CORE_MATH_H

#include "tmfqs/core/types.h"

namespace tmfqs {

Amplitude amplitudeAdd(Amplitude a, Amplitude b);
Amplitude amplitudeMultiply(Amplitude a, Amplitude b);
Amplitude complexExp(Amplitude exponent);

} // namespace tmfqs

#endif // TMFQS_CORE_MATH_H
