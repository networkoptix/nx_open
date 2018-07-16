#pragma once

#ifdef Q_OS_LINUX

namespace nx {
namespace utils {
namespace rlimit {

unsigned long getNoFile();
unsigned long setNoFile(unsigned long value);

void setDefaultNoFile()
{
    constexpr unsigned long kDefaultNoFile = 8192;
    setNoFile(kDefaultNoFile);
}

} // namespace rlimit
} // namespace utils
} // namespace nx

#endif
