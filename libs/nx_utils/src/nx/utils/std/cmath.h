#pragma once

#include <cmath>

#if defined(ANDROID) || defined(__ANDROID__)

namespace std {

using ::round;

} // namespace std

#endif
