#include "rlimit.h"

namespace nx {
namespace utils {
namespace rlimit {

unsigned long getMaxFileDescriptors()
{
    return 0;
}

unsigned long setMaxFileDescriptors(unsigned long /*value*/)
{
    return 0;
}

} // namespace rlimit
} // namespace utils
} // namespace nx
