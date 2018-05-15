#pragma once

#include <map>
#include <string>

#include <plugins/plugin_api.h>

namespace nx {
namespace sdk {

/**
 * Plugin utils providing convenience for using NX_PRINT/NX_OUTPUT with their settings
 * (printPrefix string and enableOutput flag) taken from the particular plugin: these settings are
 * stored in this class.
 *
 * To use this tool in a class, make a field "const nx::sdk::Utils utils", and add the lines:
 * <pre><code>
 *     #define NX_PRINT_PREFIX (this->utils.printPrefix)
 *     #define NX_DEBUG_ENABLE_OUTPUT (this->utils.enableOutput)
 *     #include <nx/kit/debug.h>
 * </code></pre>
 */
struct Utils
{
    const bool enableOutput;
    const std::string printPrefix;

    Utils(bool enableOutput, const std::string& printPrefix):
        enableOutput(enableOutput), printPrefix(printPrefix)
    {
    }

    /**
     * Convert the settings to a map and log via NX_OUTPUT. Errors are printed with NX_PRINT.
     * @param caption To be printed before the settings and errors; will be followed by ":".
     * @param outputIndent Number of spaces to be printed before each line printed with NX_OUTPUT.
     * @return Whether the settings are valid.
     */
    bool fillAndOutputSettingsMap(
        std::map<std::string, std::string>* map, const nxpl::Setting* settings, int count,
        const std::string& caption, int outputIndent = 0) const;
};

} // namespace sdk
} // namespace nx
