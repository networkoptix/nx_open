// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <stdint.h>

uintptr_t NX_UTILS_API currentThreadSystemId();

namespace nx::utils {

void NX_UTILS_API setCurrentThreadName(std::string name);

} // namespace nx::utils
