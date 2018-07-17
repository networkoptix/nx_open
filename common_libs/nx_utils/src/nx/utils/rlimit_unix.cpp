#include "rlimit.h"

#include <sys/resource.h>

#include <algorithm>

namespace nx {
namespace utils {
namespace rlimit {

unsigned long getNoFile()
{
    struct rlimit limit;
    if (!getrlimit(RLIMIT_NOFILE, &limit))
        return 0;

    return limit.rlim_cur;
}

unsigned long setNoFile(unsigned long value)
{
    struct rlimit limit;
    getrlimit(RLIMIT_NOFILE, &limit);

    limit.rlim_cur = std::min((rlim_t) value, limit.rlim_max);
    if (!setrlimit(RLIMIT_NOFILE, &limit))
        return 0;

    return limit.rlim_cur;
}

} // namespace rlimit
} // namespace utils
} // namespace nx
