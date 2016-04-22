#pragma once

#include <string>
#include <vector>

/**
 * Mechanism for bool configuration variables with initial values defined in the code, which can
 * be overridden by creating empty .flag files in system temp directory named as follows:
 * mymodule_<0|1>_MY_FLAG.flag
 *
 * To use, define a derived class and its instance as follows:
 * <pre><code>
 *
 *     class MyModuleConf: public Conf
 *     {
 *     public:
 *         using Conf::Conf;
 *         FLAG(0, MY_FLAG); //< Here 0 stands for 'false' as default value.
 *     };
 *     MyModuleConf conf("mymodule");
 *
 * </code></pre>
 * In the code, use conf.MY_CONF_VAR to test the value.
 */
class Conf // abstract
{
public:
    /** @param moduleName Is a prefix for .flag files. */
    Conf(const char* moduleName);

    /** Called by FLAG() macro. */
    void regFlag(bool* pFlag, bool defaultValue, const char* flagName);

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
    std::string flagFilename(const char* value, const char* flagName);
    void reloadFlag(const Flag& flag, bool printLog);
};

#define FLAG(DEFAULT, NAME) bool NAME = (regFlag(&NAME, DEFAULT, #NAME) /*operator,*/, DEFAULT)
