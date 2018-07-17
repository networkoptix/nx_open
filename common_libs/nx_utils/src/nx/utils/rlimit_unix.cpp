#include "rlimit.h"

#include <sys/resource.h>

#if defined(__APPLE__)
    #include <sys/syslimits.h>

    constexpr rlim_t kFallbackMaximumValue = OPEN_MAX;
#else
    constexpr rlim_t kFallbackMaximumValue = 1024;
#endif

#include <algorithm>

namespace nx {
namespace utils {
namespace rlimit {

unsigned long getNoFile()
{
    ::rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
        return 0;

    return limit.rlim_cur;
}

unsigned long setNoFile(unsigned long value)
{
    ::rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
        return 0;

    limit.rlim_cur = std::min((rlim_t) value, limit.rlim_max);

    if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
    {
        if (limit.rlim_cur <= kFallbackMaximumValue)
            return 0;

        // In MacOS getrlimit() can return invalid value in rlim_max. In this case OPEN_MAX macro
        // should be used as hard limit.
        limit.rlim_cur = kFallbackMaximumValue;
        if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
            return 0;
    }

    return limit.rlim_cur;
}

} // namespace rlimit
} // namespace utils
} // namespace nx
