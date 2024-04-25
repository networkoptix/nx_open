// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_utils.h"

#include <nx/sdk/i_string_map.h>

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->printPrefix)
#undef NX_DEBUG_ENABLE_OUTPUT
#define NX_DEBUG_ENABLE_OUTPUT (this->enableOutput)
#include <nx/kit/debug.h>

namespace nx::sdk {

void LogUtils::setPrintPrefix(std::string newPrefix)
{
    printPrefix = std::move(newPrefix);
}

bool LogUtils::convertAndOutputStringMap(
    std::map<std::string, std::string>* outMap,
    const IStringMap* stringMap,
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
            NX_OUTPUT << indentStr << "    " << nx::kit::utils::toString(stringMap->key(i))
                << ": " << nx::kit::utils::toString(stringMap->value(i))
                << ((i < count - 1) ? "," : "");
        }
        NX_OUTPUT << indentStr << "}";
    }

    for (int i = 0; i < count; ++i)
        (*outMap)[stringMap->key(i)] = stringMap->value(i);

    return true;
}

} // namespace nx::sdk
