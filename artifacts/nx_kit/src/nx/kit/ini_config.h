// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <iostream>
#include <vector>
#include <mutex>

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
 * - Windows: "%NX_INI_DIR%\" (if NX_INI_DIR env var is defined), or "%LOCALAPPDATA%\nx_ini\"
 *     (otherwise). ATTENTION: If LOCALAPPDATA env var contains non-ASCII chars, it will yield a
 *     non-existing path.
 * - Linux, MacOS: "$NX_INI_DIR/" (if NX_INI_DIR env var is defined), or "$HOME/.config/nx_ini/"
 *     (if HOME env var is defined), "/etc/nx_ini/" (otherwise).
 * - Android: "/sdcard/".
 * - iOS: Not supported yet.
 *
 * Each derived class represents a dedicated .ini file. If, on attempt to load a file, it is found
 * empty, the file is filled with default values and descriptions. The names of .ini files and
 * their values are printed to stderr (configurable), overridden values marked with "*".
 *
 * The format of a .ini file is as follows: lines beginning with `#` are ignored (treated as
 * comments); whitespace around the name, the equals sign, and the value is ignored; the value can
 * be surrounded with quotes (`"`), in which case it is parsed using a C-style-escaping, supporting
 * concatenation of consecutive enquoted strings, but not supporting `\uXXXX` and `\UXXXXXXXX`.
 *
 * Usage example, .h file:
 * <pre><code>
 *
 *     struct Ini: nx::kit::IniConfig
 *     {
 *         // NOTE: Omit `reload();` if load-on-start is not needed.
 *         Ini(): IniConfig("my_module.ini") { reload(); }
 *
 *         NX_INI_FLAG(0, myFlag, "Here 0 stands for 'false' as the default value.");
 *         NX_INI_INT(7, myInt, "Here 7 is the default value.");
 *         NX_INI_STRING("Default value", myStr, "Description.");
 *         NX_INI_FLOAT(1.0, myFloat, "Here 1.0 is the default value.");
 *     };
 *
 *     Ini& ini();
 *
 * </code></pre>
 *
 * Usage example, .cpp file:
 * <pre><code>
 *
 *     Ini& ini()
 *     {
 *         static Ini ini;
 *         return ini;
 *     }
 *
 * </code></pre>
 *
 * In the code, use ini().<param-name> to access the values. Call ini().reload() when needed, e.g.
 * when certain activity starts or at regular intervals.
 *
 * NOTE: The function that owns an IniConfig instance can be placed in the header file as 'inline',
 * but it is safe only when the header is local to a single dynamic library or executable.
 * Otherwise (if the header is included into more than one dynamic library or executable), due to
 * compiler/linker limitations, such variables may have duplicate instances, thus, the function
 * definition must be moved to a .cpp.
 */
class NX_KIT_API IniConfig
{
public:
    /**
     * @return Whether reading .ini files is enabled. If disabled, the values are frozen at
     *     defaults. Enabled by default, can be disabled by setEnabled(false) or by defining a macro
     *     at compiling ini_config.cpp: -DNX_INI_CONFIG_DISABLED to minimize performance overhead.
     */
    static bool isEnabled();

    static void setEnabled(bool value);

    /**
     * Change the output stream, affecting all instances, null means silent. Before this call, all
     * output goes to std::cerr, which can be changed defining a macro at compiling ini_config.cpp:
     * -DNX_INI_CONFIG_DEFAULT_OUTPUT=<stream-address>, where <stream-address> can be e.g.
     * '&std::cout', or 'nullptr' for no output.
     */
    static void setOutput(std::ostream* output);

    /** @return Currently used path to .ini files, including the trailing slash or backslash. */
    static const char* iniFilesDir();

    /**
     * Use the specified directory for .ini files. If iniFilesDir is null or empty, and also before
     * this call, a platform-dependent directory is used (described in this class comment), which
     * can be changed defining a macro at compiling ini_config.cpp:
     * -DNX_INI_CONFIG_DEFAULT_INI_FILES_DIR=<enquoted-path-with-trailing-slash-or-backslash>
     * @param iniFilesDir Should include the trailing slash or backslash.
     */
    static void setIniFilesDir(const char* iniFilesDir);

    /** @param iniFile Name of .ini file without a path but including ".ini" suffix. */
    explicit IniConfig(const char* iniFile);

    virtual ~IniConfig();

    IniConfig(const IniConfig&) = delete;
    IniConfig(IniConfig&&) = delete;
    IniConfig& operator=(const IniConfig&) = delete;
    IniConfig& operator=(IniConfig&&) = delete;

    const char* iniFile() const; /**< @return Stored copy of the string supplied to the ctor. */
    const char* iniFilePath() const; /**< @return iniFilesDir() + iniFile(). */

    /**
     * Reload values from .ini file, logging the values first time, or if changed.
     *
     * Can be called from any thread. Creates a fence, so the values in the fields are available to
     * other threads immediately after reloading finishes. The fields themselves, though not
     * declared atomic, are 32-bit values and thus will always be updated atomically.
     */
    void reload();

    class Tweaks;

    enum class ParamType
    {
        boolean, /**< bool */
        string, /**< const char* */
        integer, /**< int */
        float_, /**< float */
    };

    /**
     * Get parameter type and value by its name.
     * @return True on success, false if the parameter with the specified name isn't found.
     */
    bool getParamTypeAndValue(
        const char* paramName, ParamType* outType, const void** outData) const;

protected:
    #define NX_INI_FLAG(DEFAULT, PARAM, DESCRIPTION) \
        const bool PARAM = regBoolParam(&(PARAM), DEFAULT, #PARAM, DESCRIPTION)

    #define NX_INI_INT(DEFAULT, PARAM, DESCRIPTION) \
        const int PARAM = regIntParam(&(PARAM), DEFAULT, #PARAM, DESCRIPTION)

    #define NX_INI_STRING(DEFAULT, PARAM, DESCRIPTION) \
        const char* const PARAM = regStringParam(&(PARAM), DEFAULT, #PARAM, DESCRIPTION)

    #define NX_INI_FLOAT(DEFAULT, PARAM, DESCRIPTION) \
        const float PARAM = regFloatParam(&(PARAM), DEFAULT, #PARAM, DESCRIPTION)

protected: // Used by the above macros.
    bool regBoolParam(const bool* pValue, bool defaultValue,
        const char* paramName, const char* description);

    int regIntParam(const int* pValue, int defaultValue,
        const char* paramName, const char* description);

    const char* regStringParam(const char* const* pValue, const char* defaultValue,
        const char* paramName, const char* description);

    float regFloatParam(const float* pValue, float defaultValue,
        const char* paramName, const char* description);

private:
    class Impl;
    Impl* const d;
};

//-------------------------------------------------------------------------------------------------

/**
 * Allows to tweak IniConfig values for the duration defined by this object life time.
 * Re-reading the values is disabled while at least one Tweaks instance exists. Intended for tests.
 *
 * Usage:
 * ```
 * {
 *     IniConfig::Tweaks tweaks;
 *     tweaks.set(&ini().parameter, value);
 *     // The value will be altered up to the end of this scope.
 * }
 * ```
 */
class NX_KIT_API IniConfig::Tweaks
{
public:
    Tweaks()
    {
        const std::lock_guard<std::mutex> lock(*s_mutexHolder.mutex);

        if (++s_tweaksInstanceCount == 1)
        {
            s_originalValueOfIsEnabled = isEnabled();
            setEnabled(false);
        }
    }

    ~Tweaks()
    {
        for (const auto& guard: *m_guards)
            guard();

        {
            const std::lock_guard<std::mutex> lock(*s_mutexHolder.mutex);

            if (--s_tweaksInstanceCount == 0)
                setEnabled(s_originalValueOfIsEnabled);
        }

        delete m_guards;
    }

    Tweaks(const Tweaks&) = delete;
    Tweaks(Tweaks&&) = delete;
    Tweaks& operator=(const Tweaks&) = delete;
    Tweaks& operator=(Tweaks&&) = delete;

    template<typename T>
    void set(const T* field, T newValue)
    {
        const auto oldValue = *field;
        T* const mutableField = const_cast<T*>(field);
        m_guards->push_back([=]() { *mutableField = oldValue; });
        *mutableField = newValue;
    }

private:
    struct MutexHolder
    {
        std::mutex* const mutex = new std::mutex();
        ~MutexHolder() { delete mutex; }
    };

    static MutexHolder s_mutexHolder;
    static int s_tweaksInstanceCount;
    static bool s_originalValueOfIsEnabled;

    std::vector<std::function<void()>>* const m_guards =
        new std::vector<std::function<void()>>();
};

} // namespace kit
} // namespace nx
