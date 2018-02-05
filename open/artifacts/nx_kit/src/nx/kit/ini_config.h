// Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
#pragma once

#include <iostream>

#if !defined(NX_KIT_API)
    #define NX_KIT_API
#endif

namespace nx {
namespace kit {

/**
 * Mechanism for configuration variables that can alter the behavior of the code for the purpose of
 * debugging and experimenting, with very little performance overhead. The default (initial) values
 * are defined in the code and lead to the nominal behavior, which can be overridden by creating
 * .ini files (with name=value lines) in the directory determined by the platform:
 * - Windows: "%NX_INI_DIR%\" (if env var defined), or "%LOCALAPPDATA%\nx_ini\" (otherwise).
 * - Unix-like: "$NX_INI_DIR/" (if env var defined), or "$HOME/.config/nx_ini/" (otherwise).
 * - Android: "/sdcard/".
 * - iOS: Not supported yet.
 *
 * This unit can be compiled in the context of any C++ project.
 *
 * Each derived class represents a dedicated .ini file. If, on attempt to load a file, it is found
 * empty, the file is filled with default values and descriptions. The names of .ini files and
 * their values are printed to stderr (configurable), overridden values marked with "*".
 *
 * Usage example:
 * <pre><code>
 *
 *     struct Ini: nx::kit::IniConfig
 *     {
 *         // NOTE: Omit "reload();" if load-on-start is not needed.
 *         Ini(): IniConfig("my_module.ini") { reload(); }
 *
 *         NX_INI_FLAG(0, myFlag, "Here 0 stands for 'false' as the default value.");
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
     * If needed, reading .ini files can be disabled defining a macro at compiling ini_config.cpp:
     * -DNX_INI_CONFIG_DISABLED - minimizes performance overhead and freezes values at defaults.
     * @return Whether the macro NX_INI_CONFIG_DISABLED was defined.
     */
    static bool isEnabled();

    /**
     * Change the output stream, affecting all instances, null means silent. Before this call, all
     * output goes to std::cerr, which can be changed defining a macro at compiling ini_config.cpp:
     * -DNX_INI_CONFIG_DEFAULT_OUTPUT=<stream-address>, where <stream-address> can be e.g.
     * '&std::cout', or 'nullptr' for no output.
     */
    static void setOutput(std::ostream* output);

    /**
     * Use the specified directory for .ini files. If iniFileDir is null or empty, and also before
     * this call, a platform-dependent system temp directory is used, which can be changed defining
     * a macro at compiling ini_config.cpp:
     * -DNX_INI_CONFIG_DEFAULT_INI_FILES_DIR=<enquoted-path-with-trailing-slash-or-backslash>
     */
    static void setIniFilesDir(const char* iniFilesDir);

    /** @param iniFile Name of .ini file without a path but including ".ini" suffix. */
    explicit IniConfig(const char* iniFile);

    virtual ~IniConfig();

    IniConfig(const IniConfig& /*other*/) = delete; //< Disable the copy constructor.
    IniConfig& operator=(const IniConfig& /*other*/) = delete; //< Disable the assignment operator.

    /** Reload values from .ini file, logging the values first time, or if changed. */
    void reload();

    const char* iniFile() const; /**< @return Stored copy of the string supplied to the ctor. */
    const char* iniFileDir() const; /**< @return Path including the trailing slash or backslash. */
    const char* iniFilePath() const; /**< @return iniFileDir() + iniFile(). */

protected:
    #define NX_INI_FLAG(DEFAULT, PARAM, DESCRIPTION) \
        const bool PARAM = regBoolParam(&PARAM, DEFAULT, #PARAM, DESCRIPTION)

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
