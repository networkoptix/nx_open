// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx::utils::memory {

/**
 * @return false on error. Use SystemError::getLastOsErrorCode() to get error description.
 */
NX_UTILS_API bool mallocInfo(
    std::string* data,
    std::string* contentType);

} // namespace nx::utils::memory
