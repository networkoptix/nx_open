#pragma once

namespace nx {
namespace utils {
namespace rlimit {

NX_UTILS_API unsigned long getMaxFileDescriptiors();
NX_UTILS_API unsigned long setMaxFileDescriptors(unsigned long value);

inline void setDefaultMaxFileDescriptiors()
{
    constexpr unsigned long kDefaultMaxFileDescriptors = 32000;
    setMaxFileDescriptors(kDefaultMaxFileDescriptors);
}

} // namespace rlimit
} // namespace utils
} // namespace nx
