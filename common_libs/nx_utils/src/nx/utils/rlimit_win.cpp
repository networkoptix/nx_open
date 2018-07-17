#include "rlimit.h"

namespace nx {
namespace utils {
namespace rlimit {

unsigned long getNoFile()
{
    return 0;
}

unsigned long setNoFile(unsigned long /*value*/)
{
    return 0;
}

} // namespace rlimit
} // namespace utils
} // namespace nx
