#pragma once

#include <limits>

namespace nx {
namespace utils {
namespace rlimit {

/**
 * @return The current value of RLIMIT_NOFILE.
 */
NX_UTILS_API unsigned long getMaxFileDescriptiors();

/**
 * Set maximum number of file descriptors (RLIMIT_NOFILE). The given value is bounded by the
 * current hard limit.
 * @return The new value of the limit if the call is successful, 0 otherwise.
 */
NX_UTILS_API unsigned long setMaxFileDescriptors(
    unsigned long value = std::numeric_limits<unsigned long>::max());

} // namespace rlimit
} // namespace utils
} // namespace nx
