#include "utils.h"

#define NX_DEBUG_ENABLE_OUTPUT (this->enableOutput)
#define NX_PRINT_PREFIX (this->printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace sdk {

bool Utils::fillAndOutputSettingsMap(
    std::map<std::string, std::string>* map, const nxpl::Setting* settings, int count,
    const std::string& caption, int outputIndent) const
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

    if (NX_DEBUG_ENABLE_OUTPUT)
    {
        const std::string indentStr(outputIndent, ' ');

        if (!caption.empty())
            NX_OUTPUT << indentStr << caption << ":";

        NX_OUTPUT << indentStr << "{";
        for (int i = 0; i < count; ++i)
        {
            NX_OUTPUT << indentStr << "    " << nx::kit::debug::toString(settings[i].name)
                << ": " << nx::kit::debug::toString(settings[i].value)
                << ((i < count - 1) ? "," : "");
        }
        NX_OUTPUT << indentStr << "}";
    }

    for (int i = 0; i < count; ++i)
        (*map)[settings[i].name] = settings[i].value;

    return true;
}

} // namespace sdk
} // namespace nx
