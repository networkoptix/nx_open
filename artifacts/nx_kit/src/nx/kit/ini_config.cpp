// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ini_config.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>

#include "utils.h"

namespace nx {
namespace kit {

namespace {

#if !defined(NX_INI_CONFIG_DEFAULT_OUTPUT)
    #define NX_INI_CONFIG_DEFAULT_OUTPUT &std::cerr
#endif

#if !defined(NX_INI_CONFIG_DEFAULT_INI_FILES_DIR)
    #define NX_INI_CONFIG_DEFAULT_INI_FILES_DIR nullptr
#endif

//-------------------------------------------------------------------------------------------------
// Utils

static bool isWhitespace(char c)
{
    // NOTE: Chars 128..255 should be treated as non-whitespace, thus, isprint() will not do.
    return (((unsigned char) c) <= 32) || (c == 127);
}

static void skipWhitespace(const char** const pp)
{
    while (**pp != '\0' && isWhitespace(**pp))
        ++(*pp);
}

namespace {

struct ParsedNameValue
{
    std::string name; //< Empty if the line must be ignored.
    std::string value;
    std::string error; //< Empty on success.
};

} // namespace

static ParsedNameValue parseNameValue(const std::string& lineStr)
{
    ParsedNameValue result;

    const char* p = lineStr.c_str();

    skipWhitespace(&p);
    if (*p == '\0' || *p == '#') //< Empty or comment line.
        return result;
    while (*p != '\0' && *p != '=' && !isWhitespace(*p))
        result.name += *(p++);
    if (result.name.empty())
    {
        result.error = "The name part (before \"=\") is empty.";
        return result;
    }
    skipWhitespace(&p);

    if (*(p++) != '=')
    {
        result.error = "Missing \"=\" after the name " + result.name + ".";
        return result;
    }
    skipWhitespace(&p);

    while (*p != '\0')
        result.value += *(p++);

    // Trim trailing whitespace in the value.
    int i = (int) result.value.size() - 1;
    while (i >= 0 && isWhitespace(result.value[i]))
        --i;
    result.value = result.value.substr(0, i + 1);

    return result;
}

static std::string getEnv(const char* envVar)
{
    #if defined(_WIN32)
        // MSVC gives a warning that getenv() is unsafe, thus, implementing it via "safe"
        // MSVC-specific getenv_s().

        std::string result;
        result.resize(1024); //< Supporting longer environment variable values is not needed.
        size_t size;
        if (getenv_s(&size, &result[0], result.size(), envVar) != 0)
            return "";
        return std::string(result.c_str()); //< Strip trailing '\0', if any.
    #else
        const char* const value = getenv(envVar);
        if (value == nullptr)
            return "";
        return value;
    #endif
}

static std::string determineIniFilesDir()
{
    #if defined(ANDROID) || defined(__ANDROID__)
        // On Android, tmpnam() returns "/tmp", which does not exist.
        return "/sdcard/";
    #else
        #if 0 // Using system temp dir has been abandoned.
            char iniFileDir[L_tmpnam] = "";
            if (std::tmpnam(iniFileDir))
            {
                // Extract dir name from the file path - truncate after the last path separator.
                for (int i = (int) strlen(iniFileDir) - 1; i >= 0; --i)
                {
                    if (iniFileDir[i] == '/' || iniFileDir[i] == '\\')
                    {
                        iniFileDir[i + 1] = '\0';
                        break;
                    }
                }
            }
            return std::string(iniFileDir);
        #else
            #if defined(_WIN32)
                static const char kPathSeparator = '\\';
                static const char* const kIniDirEnvVar = "LOCALAPPDATA";
                static const std::string extraDir = "";
                static const std::string defaultDir = ""; //< Current directory.
            #else
                static const char kPathSeparator = '/';
                static const char* const kIniDirEnvVar = "HOME";
                static const std::string extraDir = std::string(".config") + kPathSeparator;
                static const std::string defaultDir = "/etc/nx_ini/";
            #endif

            const std::string env_NX_INI_DIR = getEnv("NX_INI_DIR");
            if (!env_NX_INI_DIR.empty())
                return std::string(env_NX_INI_DIR) + kPathSeparator;

            const std::string env = getEnv(kIniDirEnvVar);
            if (!env.empty())
                return std::string(env) + kPathSeparator + extraDir + "nx_ini" + kPathSeparator;

            return defaultDir;
        #endif
    #endif
}

//-------------------------------------------------------------------------------------------------
// Param registration objects

struct AbstractParam
{
    const std::string name;
    const std::string description;

    AbstractParam(const char* name, const char* description):
        name(name), description(description)
    {
    }

    virtual ~AbstractParam() = default;

    /** @return Whether the value has changed. */
    virtual bool reload(const std::string* value, std::ostream* output) = 0;

    virtual std::string defaultValueStr() const = 0;

    template<typename Value>
    void printValueLine(
        std::ostream* output,
        const Value& value,
        const char* error,
        bool equalsDefault) const
    {
        if (output)
        {
            std::stringstream s;
            s << ((error[0] != '\0') ? "  ! " : (equalsDefault ? "    " : "  * "));
            s << name << "=" << nx::kit::utils::toString(value) << error << "\n";
            *output << s.str(); //< Output in one piece to minimize multi-threaded races.
        }
    }
};

/**
 * For each param value type, a class derived from AbstractParam is created via this template.
 */
template<typename Value>
struct Param: AbstractParam
{
    Value* const pValue;
    const Value defaultValue;

    Param(const char* name, const char* description,
        Value* pValue, Value defaultValue)
        :
        AbstractParam(name, description),
        pValue(pValue),
        defaultValue(defaultValue)
    {
    }

    virtual ~Param() override {};
    virtual bool reload(const std::string* value, std::ostream* output) override;

    virtual std::string defaultValueStr() const override
    {
        return nx::kit::utils::toString(defaultValue);
    }

    /**
     * Specializations may report and fix invalid default values.
     */
    static Value validateDefaultValue(
        Value defaultValue,
        std::ostream* /*output*/,
        const char* /*paramName*/,
        const char* /*iniFile*/)
    {
        return defaultValue;
    }
};

template<typename Value>
bool Param<Value>::reload(const std::string* value, std::ostream* output)
{
    const auto oldValue = *pValue;
    *pValue = defaultValue;
    const char* error = "";
    if (value && !value->empty()) //< Existing but empty values are treated as default.
    {
        if (!nx::kit::utils::fromString(*value, pValue))
            error = " [invalid value in file]";
    }
    printValueLine(output, *pValue, error, *pValue == defaultValue);
    return oldValue != *pValue;
}

template<>
const char* Param<const char*>::validateDefaultValue(
    const char* defaultValue, std::ostream* output, const char* paramName, const char* iniFile)
{
    if (defaultValue == nullptr)
        return "";

    for (const char* p = defaultValue; *p != '\0'; ++p)
    {
        if (!nx::kit::utils::isAsciiPrintable(*p))
        {
            if (output)
            {
                *output << "INTERNAL ERROR: Invalid char with code "
                    << (int) (unsigned char) *p << " in the default value of " << paramName
                    << " in " << iniFile << "." << std::endl;
            }
        }
    }

    return defaultValue;
}

template<>
bool Param<const char*>::reload(const std::string* value, std::ostream* output)
{
    const std::string oldValue = *pValue ? *pValue : "";

    if (*pValue != defaultValue)
    {
        free(const_cast<char*>(*pValue));
        *pValue = defaultValue;
    }

    std::string error;
    if (value) //< Exists in .ini file, and can be empty: copy all chars to *pValue.
    {
        std::string str;
        if ((*value)[0] == '"')
            str = nx::kit::utils::decodeEscapedString(*value, &error);
        else
            str = *value;

        if (str.find('\0') != std::string::npos) //< `str` contains '\0' in the middle.
            error = "'\\0' found in the value.";

        if (error.empty())
        {
            *pValue = (char*) malloc(str.size() + 1);
            memcpy(const_cast<char*>(*pValue), str.c_str(), str.size() + 1);
        }
        else
        {
            if (output)
                *output << "  ERROR in the value of " << name << ": " << error << "\n";
        }
    }

    const std::string newValue{*pValue ? *pValue : ""};
    printValueLine(output, newValue, error.empty() ? "" : " [invalid value in file]",
        newValue == (defaultValue ? defaultValue : ""));
    return oldValue != newValue;
}

template<>
Param<const char*>::~Param()
{
    if (*pValue != defaultValue)
        free(const_cast<char*>(*pValue));
}

} // namespace

//-------------------------------------------------------------------------------------------------
// IniConfig::Impl

class IniConfig::Impl
{
public:
    Impl(const char* iniFile):
        iniFile(validateIniFile(iniFile))
    {
    }

    static bool& isEnabled()
    {
        #if defined(NX_INI_CONFIG_DISABLED)
            static bool isEnabled = false;
        #else
            static bool isEnabled = true;
        #endif
        return isEnabled;
    }

    /**
     * @param goingToSet Used to avoid calling determineIniFilesDir() if the getter was never
     *     called before the setter.
     */
    static std::string& iniFilesDir(bool goingToSet = false)
    {
        static std::string iniFilesDir = goingToSet
            ? ""
            : ((NX_INI_CONFIG_DEFAULT_INI_FILES_DIR) != nullptr)
                ? (NX_INI_CONFIG_DEFAULT_INI_FILES_DIR)
                : determineIniFilesDir();
        return iniFilesDir;
    }

    static std::ostream*& output()
    {
        static std::ostream* output = (NX_INI_CONFIG_DEFAULT_OUTPUT);
        return output;
    }

    template<typename Value>
    Value regParam(const Value* pValue, const Value& defaultValue,
        const char* paramName, const char* description);

    void reload();

public:
    const std::string iniFile;

    /** Needed to initialize m_cachedIniFilePath using the up-to-date value of iniFilesDir(). */
    const char* iniFilePath()
    {
        if (m_cachedIniFilePath.empty())
            m_cachedIniFilePath = iniFilesDir() + iniFile;
        return m_cachedIniFilePath.c_str();
    }

private:
    static std::string validateIniFile(const char* iniFile);
    bool parseIniFile(std::ostream* output, bool* outputIsNeeded);
    void createDefaultIniFile(std::ostream* output);
    void reloadParams(std::ostream* output, bool* outputIsNeeded) const;

private:
    bool m_firstTimeReload = true;
    bool m_iniFileEverExisted = false;
    std::map<std::string, std::string> m_iniMap;
    std::vector<std::unique_ptr<AbstractParam>> m_params;
    std::unordered_set<std::string> m_paramNames;
    std::string m_cachedIniFilePath; //< Initialized on first call to iniFilePath().
};

template<typename Value>
Value IniConfig::Impl::regParam(
    const Value* pValue, const Value& defaultValue, const char* paramName, const char* description)
{
    const Value& validatedDefaultValue =
        Param<Value>::validateDefaultValue(defaultValue, output(), paramName, iniFile.c_str());
    if (isEnabled())
    {
        m_paramNames.insert(paramName);
        m_params.push_back(std::unique_ptr<Param<Value>>(new Param<Value>(
            paramName, description, const_cast<Value*>(pValue), validatedDefaultValue)));
    }
    return validatedDefaultValue;
}

std::string IniConfig::Impl::validateIniFile(const char* iniFile)
{
    if (iniFile == nullptr)
    {
        if (output())
            *output() << "WARNING: .ini file name is null." << std::endl;
        return "[null]";
    }

    if (iniFile[0] == '\0')
    {
        if (output())
            *output() << "WARNING: .ini file name is empty." << std::endl;
        return "[empty]";
    }

    const size_t len = strlen(iniFile);
    constexpr char ext[] = ".ini";
    if (len < strlen(ext) || strcmp(iniFile + len - strlen(ext), ext) != 0)
    {
        if (output())
        {
            *output() << "WARNING: .ini file name \"" << iniFile
                << "\" does not end with \"" << ext << "\"." << std::endl;
        }
    }

    return std::string(iniFile);
}

/**
 * @return Whether the file was not empty.
 */
bool IniConfig::Impl::parseIniFile(std::ostream* output, bool* outputIsNeeded)
{
    m_iniMap.clear();

    std::ifstream file(iniFilePath());
    if (!file.good())
        return true;

    std::string lineStr;
    int line = 0;
    while (std::getline(file, lineStr))
    {
        ++line;
        const ParsedNameValue parsed = parseNameValue(lineStr);
        if (!parsed.error.empty())
        {
            if (output)
            {
                *output << iniFile << " ERROR: " << parsed.error << " Line " << line
                    << ", file " << iniFilePath() << std::endl;
                if (outputIsNeeded != nullptr)
                    *outputIsNeeded = true;
            }
            continue;
        }

        if (!parsed.name.empty())
            m_iniMap[parsed.name] = parsed.value;
    }

    return line > 0;
}

void IniConfig::Impl::createDefaultIniFile(std::ostream* output)
{
    std::ofstream file(iniFilePath());
    if (!file.good())
    {
        if (output)
            *output << iniFile << " ERROR: Cannot rewrite file " << iniFilePath() << std::endl;
        return;
    }

    for (const auto& param: m_params)
    {
        bool isDescriptionMultiline = false;
        std::string description = param->description;
        if (!description.empty())
        {
            description += " ";
            nx::kit::utils::stringInsertAfterEach(&description, '\n', "# ");
            isDescriptionMultiline = description.find('\n') != std::string::npos;
        }

        file << "# " << description << "Default: " << param->defaultValueStr() << "\n";
        if (isDescriptionMultiline)
            file << "#\n";
        file << "#" << param->name << "=" << param->defaultValueStr() << "\n";
        file << "\n";
    }
}

void IniConfig::Impl::reloadParams(std::ostream* output, bool* outputIsNeeded) const
{
    for (const auto& param: m_params)
    {
        const std::string* value = nullptr;
        const auto it = m_iniMap.find(param->name);
        if (it != m_iniMap.end())
            value = &it->second;
        if (param->reload(value, output))
        {
            if (output && outputIsNeeded != nullptr)
                *outputIsNeeded = true;
        }
    }

    for (const auto& entry: m_iniMap)
    {
        if (m_paramNames.count(entry.first) == 0 && output)
            *output << "  ! " << entry.first << " [unexpected param in file]" << std::endl;
    }
}

void IniConfig::Impl::reload()
{
    if (!isEnabled())
        return;

    const bool iniFileExists = nx::kit::utils::fileExists(iniFilePath());
    if (iniFileExists)
        m_iniFileEverExisted = true;
    if (!m_firstTimeReload && !m_iniFileEverExisted)
        return;

    std::ostringstream outputString;
    std::ostream* out = output() ? &outputString : nullptr;

    bool outputIsNeeded = m_firstTimeReload;

    if (iniFileExists)
    {
        if (out)
            *out << iniFile << " [" << iniFilePath() << "]" << std::endl;
        if (!parseIniFile(out, &outputIsNeeded))
        {
            if (out)
            {
                *out << "    ATTENTION: .ini file is empty; filling in defaults." << std::endl;
                outputIsNeeded = true;
            }
            createDefaultIniFile(out);
        }
    }
    else
    {
        if (out)
        {
            *out << iniFile << " (absent) To fill in defaults, touch " << iniFilePath()
                << std::endl;
        }
        m_iniMap.clear();
    }

    reloadParams(iniFileExists ? out : nullptr, &outputIsNeeded);

    if (out && outputIsNeeded)
        *Impl::output() << outputString.str();

    if (m_firstTimeReload)
        m_firstTimeReload = false;
}

//-------------------------------------------------------------------------------------------------
// IniConfig

/*static*/ bool IniConfig::isEnabled()
{
    return Impl::isEnabled();
}


/*static*/ void IniConfig::setEnabled(bool value)
{
    Impl::isEnabled() = value;
}

/*static*/ const char* IniConfig::iniFilesDir()
{
    return Impl::iniFilesDir().c_str();
}

/*static*/ void IniConfig::setIniFilesDir(const char* iniFilesDir)
{
    Impl::iniFilesDir(/*goingToSet*/ true) = (iniFilesDir != nullptr) ? iniFilesDir : "";
}

/*static*/ void IniConfig::setOutput(std::ostream* output)
{
    Impl::output() = output;
}

IniConfig::IniConfig(const char* iniFile):
    d(new Impl(iniFile))
{
}

IniConfig::~IniConfig()
{
    delete d;
}

bool IniConfig::regBoolParam(
    const bool* pValue, bool defaultValue,
    const char* paramName, const char* description)
{
    return d->regParam<bool>(pValue, defaultValue, paramName, description);
}

int IniConfig::regIntParam(
    const int* pValue, int defaultValue,
    const char* paramName, const char* description)
{
    return d->regParam<int>(pValue, defaultValue, paramName, description);
}

const char* IniConfig::regStringParam(
    const char* const* pValue, const char* defaultValue,
    const char* paramName, const char* description)
{
    return d->regParam<const char*>(pValue, defaultValue, paramName, description);
}

float IniConfig::regFloatParam(
    const float* pValue, float defaultValue,
    const char* paramName, const char* description)
{
    return d->regParam<float>(pValue, defaultValue, paramName, description);
}

double IniConfig::regDoubleParam(
    const double* pValue, double defaultValue,
    const char* paramName, const char* description)
{
    return d->regParam<double>(pValue, defaultValue, paramName, description);
}

const char* IniConfig::iniFile() const
{
    return d->iniFile.c_str();
}

const char* IniConfig::iniFilePath() const
{
    return d->iniFilePath();
}

void IniConfig::reload()
{
    return d->reload();
}

//-------------------------------------------------------------------------------------------------
// Tweaks

/*static*/ IniConfig::Tweaks::MutexHolder IniConfig::Tweaks::s_mutexHolder;
/*static*/ int IniConfig::Tweaks::s_tweaksInstanceCount = 0;
/*static*/ bool IniConfig::Tweaks::s_originalValueOfIsEnabled = false;

} // namespace kit
} // namespace nx
