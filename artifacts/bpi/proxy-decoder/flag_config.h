#pragma once

#include <string>
#include <vector>

namespace nx {
namespace utils {

/**
 * Mechanism for bool configuration variables with initial values defined in the code, which can
 * be overridden by creating empty .flag files in system temp directory named as follows:
 * <pre><code>
 *     mymodule_<0|1>_myFlag.flag
 * </code></pre>
 *
 * To use, define a derived class and its instance as follows:
 * <pre><code>
 *
 *     class MyModuleFlagConfig: public nx::utils::FlagConfig
 *     {
 *     public:
 *         using FlagConfig::FlagConfig;
 *         NX_FLAG(0, myFlag); //< Here 0 stands for 'false' as default value.
 *     };
 *     MyModuleFlagConfig conf("mymodule");
 *
 * </code></pre>
 * In the code, use conf.myFlag to test the value.
 */
class FlagConfig // abstract
{
public:
    /** @param moduleName Is a prefix for .flag files. */
    FlagConfig(const char* moduleName);

    /** Called by FLAG() macro; @return defaultValue. */
    bool regFlag(bool* pFlag, bool defaultValue, const char* flagName);

    /** Scan .flag files and set each variable accordingly, logging the values to std::cerr. */
    void reload();

    /** Scan .flag files and set only the specified flag value, without logging. */
    void reloadSingleFlag(bool* pFlag);

private:
    char m_tempPath[L_tmpnam];
    const char* const m_moduleName;

    struct Flag
    {
        bool* const pFlag;
        const bool defaultValue;
        const char* const name;
    };

    std::vector<Flag> m_flags;

private:
    void printHeader() const;
    std::string flagFilename(const char* value, const char* flagName) const;
    void reloadFlag(const Flag& flag, bool printLog);
};

#define NX_FLAG(DEFAULT, NAME) bool NAME = regFlag(&NAME, DEFAULT, #NAME)

} // namespace utils
} // namespace nx
