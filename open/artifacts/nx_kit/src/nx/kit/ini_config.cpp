#include "ini_config.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>

#if !defined(NX_INI_CONFIG_DEFAULT_OUTPUT)
    #define NX_INI_CONFIG_DEFAULT_OUTPUT &std::cerr
#endif

#if !defined(NX_INI_CONFIG_DEFAULT_INI_FILES_DIR)
    #define NX_INI_CONFIG_DEFAULT_INI_FILES_DIR nullptr
#endif

namespace nx {
namespace kit {

namespace {

//-------------------------------------------------------------------------------------------------
// Utils

static bool fileExists(const char* filename)
{
    return static_cast<bool>(std::ifstream(filename));
}

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

template<typename T>
std::string toString(const T& value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

static void stringReplaceAllChars(std::string* s, char sample, char replacement)
{
    std::transform(s->cbegin(), s->cend(), s->begin(),
        [=](char c) { return c == sample ? replacement : c; });
}

static void stringInsertAfterAll(std::string* s, char sample, const char* const insertion)
{
    for (int i = (int) s->size() - 1; i >= 0; --i)
    {
        if ((*s)[i] == sample)
            s->insert((size_t) (i + 1), insertion);
    }
}

/**
 * @param outName Set to an empty string if the line is empty or comment.
 * @return Whether no errors occurred.
 */
static bool parseNameValue(const char* const s, std::string* outName, std::string* outValue)
{
    const char* p = s;

    skipWhitespace(&p);
    *outName = "";
    while (*p != '\0' && *p != '=' && *p != '#' && !isWhitespace(*p))
        *outName += *(p++);
    if (*p == '\0' || *p == '#') //< Empty or comment line.
        return true;
    skipWhitespace(&p);

    if (*(p++) != '=')
        return false;
    skipWhitespace(&p);

    *outValue = "";
    while (*p != '\0')
        *outValue += *(p++);

    // Trim trailing spaces in the value.
    int i = (int) outValue->size() - 1;
    while (i >= 0 && isWhitespace((*outValue)[i]))
        --i;
    *outValue = outValue->substr(0, (unsigned long) (i + 1));

    // If the value is quoted, unquote.
    if (outValue->size() >= 2 && (*outValue)[0] == '"' && (*outValue)[outValue->size() - 1] == '"')
        *outValue = outValue->substr(1, outValue->size() - 2);

    return true;
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
                return std::string(env) + kPathSeparator + extraDir + "nx_ini" +kPathSeparator;

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
        const char* valueNameSeparator,
        const char* error,
        bool eqDefault) const
    {
        if (output)
        {
            const char* const prefix = (error[0] != '\0') ? "!!! " : (eqDefault ? "    " : "  * ");
            std::string descriptionStr;
            if (!description.empty())
            {
                descriptionStr = " # " + description;
                stringReplaceAllChars(&descriptionStr, '\n', ' ');
            }
            *output << prefix << value << valueNameSeparator << name << error << descriptionStr
                << std::endl;
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
    virtual std::string defaultValueStr() const override;

    /**
     * Specializations may report and fix invalid default values.
     */
    static Value validateDefaultValue(Value defaultValue)
    {
        return defaultValue;
    }
};

template<>
bool Param<bool>::reload(const std::string* value, std::ostream* output)
{
    const bool oldValue = *pValue;
    *pValue = defaultValue;
    const char* error = "";
    if (value && !value->empty()) //< Existing but empty bool values are treated as default.
    {
        if (*value == "true" || *value == "True" || *value == "TRUE" || *value == "1")
            *pValue = true;
        else if (*value == "false" || *value == "False" || *value == "FALSE" || *value == "0")
            *pValue = false;
        else
            error = " [invalid value]";
    }
    printValueLine(output, *pValue ? "1" : "0", " ", error, *pValue == defaultValue);
    return oldValue != *pValue;
}

template<>
std::string Param<bool>::defaultValueStr() const
{
    return defaultValue ? "1" : "0";
}

template<>
bool Param<int>::reload(const std::string* value, std::ostream* output)
{
    const int oldValue = *pValue;
    *pValue = defaultValue;
    const char* error = "";
    if (value && !value->empty()) //< Existing but empty int values are treated as default.
    {
        // NOTE: std::stoi() is missing on Android.
        char* pEnd = nullptr;
        errno = 0; //< Required before strtol().
        const long v = std::strtol(value->c_str(), &pEnd, /*base*/ 0);
        if (v > INT_MAX || v < INT_MIN || errno != 0 || *pEnd != '\0')
            error = " [invalid value]";
        else
            *pValue = (int) v;
    }
    printValueLine(output, *pValue, " = ", error, *pValue == defaultValue);
    return oldValue != *pValue;
}

template<>
bool Param<float>::reload(const std::string* value, std::ostream* output)
{
    const float oldValue = *pValue;
    *pValue = defaultValue;
    const char* error = "";
    if (value && !value->empty()) //< Existing but empty float values are treated as default.
    {
        // NOTE: std::stof() is missing on Android.
        char* pEnd = nullptr;
        errno = 0; //< Required before strtof().
        // Android NDK does not support std::strtof.
        const float v = (float) std::strtod(value->c_str(), &pEnd);
        if (errno == ERANGE || *pEnd != '\0')
            error = " [invalid value]";
        else
            *pValue = (float) v;
    }
    printValueLine(output, *pValue, " = ", error, *pValue == defaultValue);
    return oldValue != *pValue;
}

template<>
bool Param<double>::reload(const std::string* value, std::ostream* output)
{
    const double oldValue = *pValue;
    *pValue = defaultValue;
    const char* error = "";
    if (value && !value->empty()) //< Existing but empty double values are treated as default.
    {
        // NOTE: std::stof() is missing on Android.
        char* pEnd = nullptr;
        errno = 0; //< Required before strtod().
        const double v = std::strtod(value->c_str(), &pEnd);
        if (errno == ERANGE || *pEnd != '\0')
            error = " [invalid value]";
        else
            *pValue = v;
    }
    printValueLine(output, *pValue, " = ", error, *pValue == defaultValue);
    return oldValue != *pValue;
}

template<>
std::string Param<int>::defaultValueStr() const
{
    return toString(defaultValue);
}

template<>
std::string Param<float>::defaultValueStr() const
{
    return toString(defaultValue);
}

template<>
std::string Param<double>::defaultValueStr() const
{
    return toString(defaultValue);
}

template<>
const char* Param<const char*>::validateDefaultValue(
    const char* defaultValue)
{
    if (defaultValue == nullptr)
        return "";
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
    if (value) //< Exists in .ini file, and can be empty: copy all chars to *pValue.
    {
        *pValue = (char*) malloc(value->size() + 1);
        memcpy(const_cast<char*>(*pValue), value->c_str(), value->size() + 1);
    }
    const std::string newValue{*pValue ? *pValue : ""};
    printValueLine(output, "\"" + newValue + "\"", " = ", /*error*/ "",
        newValue == (defaultValue ? defaultValue : ""));
    return oldValue != newValue;
}

template<>
std::string Param<const char*>::defaultValueStr() const
{
    return std::string("\"") + defaultValue + "\"";
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
    const Value& validatedDefaultValue = Param<Value>::validateDefaultValue(defaultValue);
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

    int line = 0;
    std::string lineStr;
    while (std::getline(file, lineStr))
    {
        ++line;

        std::string name;
        std::string value;
        if (!parseNameValue(lineStr.c_str(), &name, &value))
        {
            if (output)
            {
                *output << iniFile << " ERROR: Cannot parse line " << line
                    << ", file " << iniFilePath() << std::endl;
                if (outputIsNeeded != nullptr)
                    *outputIsNeeded = true;
            }
            continue;
        }

        if (!name.empty())
            m_iniMap[name] = value;
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
        std::string description = param->description;
        if (!description.empty())
        {
            description += " ";
            stringInsertAfterAll(&description, '\n', "# ");
        }

        file << "# " << description << "Default: " << param->defaultValueStr() << "\n";
        file << param->name << "=" << param->defaultValueStr() << "\n";
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
            *output << "!!! " << entry.first << " [unexpected param in file]" << std::endl;
    }
}

void IniConfig::Impl::reload()
{
    if (!isEnabled())
        return;

    const bool iniFileExists = fileExists(iniFilePath());
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

static std::atomic<int> s_tweaksInstances(false);
static bool s_tweaksIsEnabledBackup(false);

IniConfig::Tweaks::Tweaks()
{
    m_guards = new std::vector<std::function<void()>>();
    if (++s_tweaksInstances != 0)
    {
        s_tweaksIsEnabledBackup = isEnabled();
        setEnabled(false);
    }
}

IniConfig::Tweaks::~Tweaks()
{
    for (const auto& guard: *m_guards)
        guard();
    delete m_guards;

    if (--s_tweaksInstances == 0)
        setEnabled(s_tweaksIsEnabledBackup);
}

} // namespace kit
} // namespace nx
