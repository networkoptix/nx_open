#include "debug.h"

#define NX_DEBUG_ENABLE_OUTPUT enableOutput()
#define NX_PRINT_PREFIX printPrefix()
#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace utils {

bool Debug::debugOutputSettings(
    const nxpl::Setting* settings, int count, const std::string& caption) const
{
    if (count > 0 && settings == nullptr)
    {
        NX_PRINT << "ERROR: " << caption << ": ptr is null and count is " << count;
        return false;
    }

    if (count == 0 && settings != nullptr)
    {
        NX_PRINT << "ERROR: " << caption << ": ptr is not null and count is 0";
        return false;
    }

    if (count < 0)
    {
        NX_PRINT << "ERROR: " << caption << ": count is " << count;
        return false;
    }

    NX_OUTPUT << caption << ":";
    NX_OUTPUT << "{";
    for (int i = 0; i < count; ++i)
    {
        NX_OUTPUT << "    " << nx::kit::debug::toString(settings[i].name)
            << ": " << nx::kit::debug::toString(settings[i].value)
            << ((i < count - 1) ? "," : "");
    }
    NX_OUTPUT << "}";
    return true;
}

} // namespace utils
} // namespace sdk
} // namespace nx
