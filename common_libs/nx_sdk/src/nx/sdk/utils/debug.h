#pragma once

#include <string>

#include <plugins/plugin_api.h>

namespace nx {
namespace sdk {
namespace utils {

/**
 * Debugging tools that use NX_PRINT/NX_OUTPUT for debug output. To use these tools, it is
 * recommended to protectedly inherit from this class - this allows to use NX_PRINT/NX_OUTPUT in a
 * derived class after adding the following lines to its .cpp:
 * <pre><code>
 *     #define NX_PRINT_PREFIX printPrefix()
 *     #define NX_DEBUG_ENABLE_OUTPUT enableOutput()
 *     #include <nx/kit/debug.h>
 * </code></pre>
 */
class Debug
{
private:
    const bool m_enableOutput;
    const std::string m_printPrefix;

public:
    Debug(bool enableOutput, const std::string& printPrefix):
        m_enableOutput(enableOutput), m_printPrefix(printPrefix)
    {
    }

    bool enableOutput() const { return m_enableOutput; }
    std::string printPrefix() const { return m_printPrefix; }

    /**
     * Dump settings using NX_OUTPUT. Errors (if any) are printed with NX_PRINT.
     * @param caption Name, including the word "settings", to be used in the output.
     * @return Whether the settings are valid.
     */
    bool debugOutputSettings(
        const nxpl::Setting* settings, int count, const std::string& caption) const;
};

} // namespace utils
} // namespace sdk
} // namespace nx
