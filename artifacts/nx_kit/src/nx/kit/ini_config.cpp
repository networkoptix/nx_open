// Copyright 2018 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
#include "ini_config.h"

#include <fstream>
#include <vector>
#include <map>
#include <memory>
#include <climits>
#include <sstream>
#include <cstring>
#include <string>
#include <cstdlib>
#include <cerrno>

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

static bool fileExists(const std::string& filename)
{
    return static_cast<bool>(std::ifstream(filename.c_str()));
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

static std::string determineIniFilesDir()
{
    #if defined(ANDROID) || defined(__ANDROID__)
        return "/sdcard/";
    #else
        #if defined(_WIN32)
            static const char kSeparator = '\\';
            static const char* const kEnvVar = "LOCALAPPDATA";
            static const std::string extraDir = "";
        #else
            static const char kSeparator = '/';
            static const char* const kEnvVar = "HOME";
            static const std::string extraDir = std::string(".config") + kSeparator;
        #endif

        const char* const env_NX_INI_DIR = getenv("NX_INI_DIR");
        if (env_NX_INI_DIR != nullptr)
            return std::string(env_NX_INI_DIR) + kSeparator;

        const char* const env = getenv(kEnvVar);
        if (env != nullptr)
            return std::string(env) + kSeparator + extraDir + "nx_ini" + kSeparator;

        // If the path cannot be determined, use empty string.
        return "";
    #endif

    #if 0 // Using OS temp dir has been abandoned.
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
    #endif // 0
}

//-------------------------------------------------------------------------------------------------
// Param registration objects

struct AbstractParam
{
    const char* const name;
    const char* const description;

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
            const char* const descriptionPrefix = (description[0] == '\0') ? "" : " # ";

            *output << prefix << value << valueNameSeparator << name << error
                << descriptionPrefix << description << std::endl;
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
            *pValue = (double) v;
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
        iniFile(validateIniFile(iniFile)),
        iniFilePath(iniFilesDir() + iniFile)
    {
    }

    /**
     * @param goingToSet Used to avoid calling determineIniFilesDir() if the getter was never
     *     called before the setter.
     */
    static std::string& iniFilesDir(bool goingToSet = false)
    {
        static std::string iniFilesDir = goingToSet
            ? std::string()
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
    const std::string iniFilePath;

private:
    static std::string validateIniFile(const char* iniFile);
    bool parseIniFile(std::ostream* output, bool* outputIsNeeded);
    void createDefaultIniFile(std::ostream* output);
    void reloadParams(std::ostream* output, bool* outputIsNeeded) const;

private:
    bool firstTimeReload = true;
    bool iniFileEverExisted = false;
    std::map<std::string, std::string> iniMap;
    std::vector<std::unique_ptr<AbstractParam>> params;
};

template<typename Value>
Value IniConfig::Impl::regParam(
    const Value* pValue, const Value& defaultValue, const char* paramName, const char* description)
{
    const Value& validatedDefaultValue = Param<Value>::validateDefaultValue(defaultValue);
    if (isEnabled())
    {
        params.push_back(std::unique_ptr<Param<Value>>(new Param<Value>(
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
    constexpr const char* ext = ".ini";
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
    iniMap.clear();

    std::ifstream file(iniFilePath.c_str());
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
                    << ", file " << iniFilePath << std::endl;
                if (outputIsNeeded != nullptr)
                    *outputIsNeeded = true;
            }
            continue;
        }

        if (!name.empty())
            iniMap[name] = value;
    }

    return line > 0;
}

void IniConfig::Impl::createDefaultIniFile(std::ostream* output)
{
    std::ofstream file(iniFilePath.c_str());
    if (!file.good())
    {
        if (output)
            *output << iniFile << " ERROR: Cannot rewrite file " << iniFilePath << std::endl;
        return;
    }

    for (const auto& param: params)
    {
        std::string description = param->description;
        if (description.size() > 0)
            description += " ";
        file << "# " << description << "Default: " << param->defaultValueStr() << "\n";
        file << param->name << "=" << param->defaultValueStr() << "\n";
        file << "\n";
    }
}

void IniConfig::Impl::reloadParams(std::ostream* output, bool* outputIsNeeded) const
{
    for (const auto& param: params)
    {
        const std::string* value = nullptr;
        const auto it = iniMap.find(param->name);
        if (it != iniMap.end())
            value = &it->second;
        if (param->reload(value, output))
        {
            if (output && outputIsNeeded != nullptr)
                *outputIsNeeded = true;
        }
    }
}

void IniConfig::Impl::reload()
{
    if (!isEnabled())
        return;

    const bool iniFileExists = fileExists(iniFilePath);
    if (iniFileExists)
        iniFileEverExisted = true;
    if (!firstTimeReload && !iniFileEverExisted)
        return;

    std::ostringstream outputString;
    std::ostream* s = output() ? &outputString : nullptr;

    bool outputIsNeeded = firstTimeReload;

    if (iniFileExists)
    {
        if (s)
            *s << iniFile << " [" << iniFilePath << "]" << std::endl;
        if (!parseIniFile(s, &outputIsNeeded))
        {
            if (s)
            {
                *s << "    ATTENTION: .ini file is empty; filling in defaults." << std::endl;
                outputIsNeeded = true;
            }
            createDefaultIniFile(s);
        }
    }
    else
    {
        if (s)
            *s << iniFile << " (absent) To fill in defaults, touch " << iniFilePath << std::endl;
        iniMap.clear();
    }

    reloadParams(iniFileExists ? s : nullptr, &outputIsNeeded);

    if (s && outputIsNeeded)
        *Impl::output() << outputString.str();

    if (firstTimeReload)
        firstTimeReload = false;
}

//-------------------------------------------------------------------------------------------------
// IniConfig

/*static*/ bool IniConfig::isEnabled()
{
    #if defined(NX_INI_CONFIG_DISABLED)
        return false;
    #else
        return true;
    #endif
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
    return d->iniFilePath.c_str();
}

void IniConfig::reload()
{
    return d->reload();
}

} // namespace kit
} // namespace nx
