#pragma once

namespace nx {
namespace utils {

/**
 * Mechanism for configuration variables with initial values defined in the code, which can be
 * overridden by (depending on the bool const kUseIniFile):
 * (a) creating empty .flag files (for booleans) or .txt files (for other values) with the value in
 * system temp directory. Files should be named as follows:
 * <pre><code>
 *     mymodule_<0|1>_myFlag.flag
 *     mymodule_myInt.txt
 * </code></pre>
 * (b) creating mymodule.ini in system temp directory with name=value lines.
 *
 * To use, define a derived class and its instance as follows:
 * <pre><code>
 *
 *     struct MyModuleFlagConfig: public nx::utils::FlagConfig
 *     {
 *         using nx::utils::FlagConfig::FlagConfig;
 *         NX_FLAG(0, myFlag "Here 0 stands for 'false' as the default value.");
 *         NX_INT_PARAM(7, myInt, "Here 7 is the default value.");
 *         NX_STRING_PARAM("Default value", myStr, "Description.");
 *     };
 *     MyModuleFlagConfig conf("mymodule");
 *
 * </code></pre>
 * In the code, call conf.reload() when needed to reload from file(s), and use conf.<param-name> to
 * access the values. In the build system define NX_UTILS_API, e.g. like this:
 * <pre><code>
 *     -DNX_UTILS_API='__attribute__ ((visibility ("hidden")))'
 * </code></pre>
 */
class NX_UTILS_API FlagConfig // abstract
{
public:
    /** @param moduleName Is a prefix for .flag files. */
    FlagConfig(const char* moduleName);

    virtual ~FlagConfig();

    /** Called by NX_FLAG() macro; @return defaultValue. */
    bool regFlagParam(bool* pValue, bool defaultValue, const char* paramName, const char* descr);

    /** Called by NX_INT_PARAM() macro; @return defaultValue. */
    int regIntParam(int* pValue, int defaultValue, const char* paramName, const char* descr);

    /** Called by NX_STRING_PARAM() macro; @return defaultValue. */
    const char* regStringParam(const char** pValue, const char* defaultValue, const char* paramName, const char* descr);

    /** Reload values from file(s), logging the values to std::cerr. */
    void reload();

    /** Allows to avoid reloading on next call to reload(), which will only restore this flag. */
    void skipNextReload();

    /** Detected temp path including trailing slash or backslash. */
    const char* tempPath() const;

    const char* moduleName() const;

private:
    class Impl;
    Impl* const d;
};

#define NX_FLAG(DEFAULT, NAME, DESCR) bool NAME = regFlagParam(&NAME, (DEFAULT), #NAME, (DESCR))
#define NX_INT_PARAM(DEFAULT, NAME, DESCR) int NAME = regIntParam(&NAME, (DEFAULT), #NAME, (DESCR))
#define NX_STRING_PARAM(DEFAULT, NAME, DESCR) const char* NAME = regStringParam(&NAME, (DEFAULT), #NAME, (DESCR))

} // namespace utils
} // namespace nx
