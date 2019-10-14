#pragma once

#include <string>

namespace nx::utils {

/**
 * @return the stack trace including this function.
 */
NX_UTILS_API std::string stackTrace();

} // namespace nx::utils