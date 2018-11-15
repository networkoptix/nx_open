#include "utils.h"

#define NX_DEBUG_ENABLE_OUTPUT (this->enableOutput)
#define NX_PRINT_PREFIX (this->printPrefix)
#include <nx/kit/debug.h>
#include <nx/sdk/settings.h>

namespace nx {
namespace sdk {

bool Utils::fillAndOutputSettingsMap(
    std::map<std::string, std::string>* map,
    const nx::sdk::Settings* settings,
    const std::string& caption,
    int outputIndent) const
{
    if (!settings)
    {
        NX_PRINT << "ERROR: settings is null";
        return false;
    }

    const auto count = settings->count();
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
            NX_OUTPUT << indentStr << "    " << nx::kit::debug::toString(settings->key(i))
                << ": " << nx::kit::debug::toString(settings->value(i))
                << ((i < count - 1) ? "," : "");
        }
        NX_OUTPUT << indentStr << "}";
    }

    for (int i = 0; i < count; ++i)
        (*map)[settings->key(i)] = settings->value(i);

    return true;
}

} // namespace sdk
} // namespace nx
