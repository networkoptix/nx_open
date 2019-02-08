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
