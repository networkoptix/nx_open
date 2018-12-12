#include "log_utils.h"

#define NX_DEBUG_ENABLE_OUTPUT (this->enableOutput)
#define NX_PRINT_PREFIX (this->printPrefix)
#include <nx/kit/debug.h>

#include <nx/sdk/i_string_map.h>

namespace nx {
namespace sdk {
namespace common {

bool LogUtils::convertAndOutputStringMap(
    std::map<std::string, std::string>* outMap,
    const nx::sdk::IStringMap* stringMap,
    const std::string& caption,
    int outputIndent) const
{
    if (!stringMap)
    {
        NX_PRINT << "ERROR: stringMap is null";
        return false;
    }

    const auto count = stringMap->count();
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
            NX_OUTPUT << indentStr << "    " << nx::kit::debug::toString(stringMap->key(i))
                << ": " << nx::kit::debug::toString(stringMap->value(i))
                << ((i < count - 1) ? "," : "");
        }
        NX_OUTPUT << indentStr << "}";
    }

    for (int i = 0; i < count; ++i)
        (*outMap)[stringMap->key(i)] = stringMap->value(i);

    return true;
}

} // namespace common
} // namespace sdk
} // namespace nx
