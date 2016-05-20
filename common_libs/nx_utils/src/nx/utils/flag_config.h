#pragma once

#include <string>
#include <vector>
#include <memory>

namespace nx {
namespace utils {

/**
 * Mechanism for int or bool configuration variables with initial values defined in the code, which
 * can be overridden by creating empty .flag files or .txt files with int value in system temp
 * directory. Files should be named as follows:
 * <pre><code>
 *     mymodule_<0|1>_myFlag.flag
 *     mymodule_myInt.txt
 * </code></pre>
 *
 * To use, define a derived class and its instance as follows:
 * <pre><code>
 *
 *     class MyModuleFlagConfig: public nx::utils::FlagConfig
 *     {
 *     public:
 *         using nx::utils::FlagConfig::FlagConfig;
 *         NX_FLAG(0, myFlag "Here 0 stands for 'false' as the default value.");
 *         NX_INT_PARAM(7, myInt, "Here 7 is the default value.");
 *     };
 *     MyModuleFlagConfig conf("mymodule");
 *
 * </code></pre>
 * In the code, use conf.myFlag to test the value.
 */
class NX_UTILS_API FlagConfig // abstract
{
public:
    /** @param moduleName Is a prefix for .flag files. */
    FlagConfig(const char* moduleName);

    /** Called by NX_FLAG() macro; @return defaultValue. */
    bool regFlagParam(bool* pValue, bool defaultValue, const char* paramName, const char* descr);

    /** Called by NX_INT_PARAM() macro; @return defaultValue. */
    int regIntParam(int* pValue, int defaultValue, const char* paramName, const char* descr);

    /** Scan .flag files and set each variable accordingly, logging the values to std::cerr. */
    void reload() const;

    /** Scan .flag files and set only the specified flag value, without logging. */
    void reloadSingleFlag(bool* pValue) const;

    /** Detected temp path including trailing slash or backslash. */
    const char* tempPath() const;

    const char* moduleName() const;

private:
    char m_tempPath[L_tmpnam];
    const char* const m_moduleName;

    struct Param // abstract
    {
        FlagConfig* owner;
        const char* const name;
        const char* const descr;

        Param(FlagConfig* owner, const char* name, const char* descr)
        :
            owner(owner),
            name(name),
            descr(descr)
        {
        }

        void printLine(const std::string& value, const char* valueNameSeparator,
            const char* note, bool error, bool equalsDefault) const;
        virtual ~Param() = default;
        virtual void reload(bool printLog) const = 0;
    };

    struct FlagParam: Param
    {
        bool* const pValue;
        const bool defaultValue;

        FlagParam(FlagConfig* owner, bool* pValue, bool defaultValue, const char* name,
            const char* descr)
        :
            Param(owner, name, descr),
            pValue(pValue),
            defaultValue(defaultValue)
        {
        }

        virtual void reload(bool printLog) const override;
    };

    struct IntParam: Param
    {
        int* const pValue;
        const int defaultValue;

        IntParam(FlagConfig* owner, int* pValue, int defaultValue, const char* name,
            const char* descr)
        :
            Param(owner, name, descr),
            pValue(pValue),
            defaultValue(defaultValue)
        {
        }

        virtual void reload(bool printLog) const override;
    };

    std::vector<std::unique_ptr<Param>> m_params;

private:
    void printHeader() const;
    std::string flagFilename(const char* value, const char* paramName) const;
    std::string txtFilename(const char* paramName) const;
};

#define NX_FLAG(DEFAULT, NAME, DESCR) bool NAME = regFlagParam(&NAME, (DEFAULT), #NAME, (DESCR))
#define NX_INT_PARAM(DEFAULT, NAME, DESCR) int NAME = regIntParam(&NAME, (DEFAULT), #NAME, (DESCR))

} // namespace utils
} // namespace nx
