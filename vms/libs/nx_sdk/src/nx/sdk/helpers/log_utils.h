#pragma once

#include <map>
#include <string>

#include <plugins/plugin_api.h>

namespace nx { namespace sdk { class IStringMap; } }

namespace nx {
namespace sdk {

/**
 * Plugin utils providing convenience for using NX_PRINT/NX_OUTPUT with their settings
 * (printPrefix string and enableOutput flag) taken from the particular plugin: these settings are
 * stored in this class.
 *
 * To use this tool in a class, make a field "const nx::sdk::LogUtils logUtils", and add
 * the following lines:
 * <pre><code>
 *     #define NX_PRINT_PREFIX (this->logUtils.printPrefix)
 *     #define NX_DEBUG_ENABLE_OUTPUT (this->logUtils.enableOutput)
 *     #include <nx/kit/debug.h>
 * </code></pre>
 */
struct LogUtils
{
    const bool enableOutput;
    const std::string printPrefix;

    LogUtils(bool enableOutput, const std::string& printPrefix):
        enableOutput(enableOutput), printPrefix(printPrefix)
    {
    }

    /**
     * Convert IStringMap to an std::map and log via NX_OUTPUT. Errors are printed with NX_PRINT.
     * @param caption To be printed before the map and errors; will be followed by ":".
     * @param outputIndent Number of spaces to be printed before each line printed with NX_OUTPUT.
     * @return Whether the map is valid.
     */
    bool convertAndOutputStringMap(
        std::map<std::string, std::string>* outMap,
        const nx::sdk::IStringMap* stringMap,
        const std::string& caption,
        int outputIndent = 0) const;
};

} // namespace sdk
} // namespace nx
