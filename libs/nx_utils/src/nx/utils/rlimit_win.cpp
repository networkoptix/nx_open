#include "rlimit.h"

namespace nx {
namespace utils {
namespace rlimit {

unsigned long getMaxFileDescriptors()
{
    return 0;
}

unsigned long setMaxFileDescriptors(unsigned long value)
{
    return value;
}

} // namespace rlimit
} // namespace utils
} // namespace nx
