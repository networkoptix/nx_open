#pragma once

#include <iostream>

#if !defined(NX_KIT_API)
    #define NX_KIT_API
#endif

namespace nx {
namespace kit {

/**
 * Mechanism for configuration variables with initial values defined in the code, which can be
 * overridden creating .ini files in the system temp directory (or /sdcard on Android), with
 * name=value lines. When the file is empty, it is filled with defaults.
 *
 * This unit can be compiled in the context of any C++ project.
 *
 * To use, define a derived class and its instance as follows:
 * <pre><code>
 *
 *     struct Ini: nx::kit::IniConfig
 *     {
 *         // NOTE: Omit the call to reload() if no load-on-start if needed..
 *         Ini(): IniConfig("my_module.ini") { reload(); }
 *
 *         NX_INI_FLAG(f, myFlag, "Default value can be f, false, 0, or t, true, 1.");
 *         NX_INI_INT(7, myInt, "Here 7 is the default value.");
 *         NX_INI_STRING("Default value", myStr, "Description.");
 *     };
 *
 *     inline Ini& ini()
 *     {
 *         static Ini ini;
 *         return ini;
 *     }
 *
 * </code></pre>
 *
 * In the code, use ini().<param-name> to access the values. Call ini().reload() when needed, e.g.
 * when certain activity starts or at regular intervals.
 */
class NX_KIT_API IniConfig
{
public:
    /**
     * If needed, reading .ini files can be disabled defining a project-level macro:
     * -DNX_INI_CONFIG_DISABLED - minimizes performance overhead and freezes values at defaults.
     * @ereturn Whether the macro NX_INI_CONFIG_DISABLED is defined.
     */
    static bool isEnabled();

    /**
     * Change the output stream, affecting all instances, null means silent. Before this call, all
     * output goes to std::cerr, which can be changed defining a project-level macro:
     * -DNX_INI_CONFIG_DEFAULT_OUTPUT=<stream-address>, where <stream-address> can be e.g.
     * '&std::cout', or 'nullptr' for no output.
     */
    static void setOutput(std::ostream* output);

    /**
     * @param iniFile Name of .ini file without a path but including ".ini" suffix.
     */
    IniConfig(const char* iniFile);

    virtual ~IniConfig();

    /** Reload values from .ini file, logging the values first time, or if changed. */
    void reload();

    const char* iniFile() const;
    const char* iniFileDir() const; /**< Includes trailing slash or backslash. */
    const char* iniFilePath() const; /**< @return iniFileDir + iniFile. */

protected:
    #define NX_INI_FLAG(DEFAULT, PARAM, DESCRIPTION) \
        const bool PARAM = regBoolParam(&PARAM, \
            [&](){ const bool f = false, t = true; (void) f; (void) t; return DEFAULT; }(), \
            #PARAM, DESCRIPTION)

    #define NX_INI_INT(DEFAULT, PARAM, DESCRIPTION) \
        const int PARAM = regIntParam(&PARAM, DEFAULT, #PARAM, DESCRIPTION)

    #define NX_INI_STRING(DEFAULT, PARAM, DESCRIPTION) \
        const char* const PARAM = regStringParam(&PARAM, DEFAULT, #PARAM, DESCRIPTION)

protected: // Used by macros.
    bool regBoolParam(const bool* pValue, bool defaultValue,
        const char* paramName, const char* description);

    int regIntParam(const int* pValue, int defaultValue,
        const char* paramName, const char* description);

    const char* regStringParam(const char* const* pValue, const char* defaultValue,
        const char* paramName, const char* description);

private:
    struct Impl;
    Impl* const d;
};

} // namespace kit
} // namespace nx
