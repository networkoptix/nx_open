// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cmath>

#if defined(ANDROID) || defined(__ANDROID__)

namespace std {

using ::round;

} // namespace std

#endif
