// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <string>

namespace nx::sdk {

class IStringMap;

/**
 * Plugin utils providing convenience for using NX_PRINT/NX_OUTPUT with their settings
 * (printPrefix string and enableOutput flag) taken from the particular plugin: these settings are
 * stored in this class.
 *
 * To use this tool in a class, make a field "const nx::sdk::LogUtils logUtils", and add
 * the following lines:
 * <pre><code>
 *     #undef NX_PRINT_PREFIX
 *     #define NX_PRINT_PREFIX (this->logUtils.printPrefix)
 *     #undef NX_DEBUG_ENABLE_OUTPUT
 *     #define NX_DEBUG_ENABLE_OUTPUT (this->logUtils.enableOutput)
 *     #include <nx/kit/debug.h>
 * </code></pre>
 */
struct LogUtils
{
    const bool enableOutput;
    std::string printPrefix;

    LogUtils(bool enableOutput, std::string printPrefix):
        enableOutput(enableOutput), printPrefix(std::move(printPrefix))
    {
    }

    void setPrintPrefix(std::string newPrefix);

    /**
     * Convert IStringMap to an std::map and log via NX_OUTPUT. Errors are printed with NX_PRINT.
     * @param caption To be printed before the map and errors; will be followed by ":".
     * @param outputIndent Number of spaces to be printed before each line printed with NX_OUTPUT.
     * @return Whether the map is valid.
     */
    bool convertAndOutputStringMap(
        std::map<std::string, std::string>* outMap,
        const IStringMap* stringMap,
        const std::string& caption,
        int outputIndent = 0) const;
};

} // namespace nx::sdk
