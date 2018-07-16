#pragma once

namespace nx {
namespace utils {
namespace rlimit {

NX_UTILS_API unsigned long getNoFile();
NX_UTILS_API unsigned long setNoFile(unsigned long value);

inline void setDefaultNoFile()
{
    constexpr unsigned long kDefaultNoFile = 32000;
    setNoFile(kDefaultNoFile);
}

} // namespace rlimit
} // namespace utils
} // namespace nx
