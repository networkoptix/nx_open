#include "rlimit.h"

#include <sys/resource.h>

#if defined(__APPLE__)
    #include <sys/syslimits.h>
#endif

#include <algorithm>

namespace nx {
namespace utils {
namespace rlimit {

unsigned long getMaxFileDescriptiors()
{
    ::rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
        return 0;

    return limit.rlim_cur;
}

unsigned long setMaxFileDescriptors(unsigned long value)
{
    ::rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
        return 0;

    limit.rlim_cur = std::min((rlim_t) value, limit.rlim_max);

    if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
    {
        #if defined(__APPLE__)
            // In MacOS getrlimit() can return an invalid value in rlim_max. In this case OPEN_MAX
            // macro should be used as hard limit.
            if (limit.rlim_cur <= OPEN_MAX)
                return 0;

            limit.rlim_cur = OPEN_MAX;
            if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
                return 0;
        #endif
    }

    return limit.rlim_cur;
}

} // namespace rlimit
} // namespace utils
} // namespace nx
