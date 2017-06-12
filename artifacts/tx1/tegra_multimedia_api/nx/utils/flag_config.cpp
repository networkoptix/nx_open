#include "flag_config.h"

#if !(defined(ANDROID) || defined(__ANDROID__))

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>
#include <cstring>
#include <map>

namespace nx {
namespace utils {

namespace {

static const bool kUseIniFile = true;

static bool fileExists(const char* filename)
{
    return static_cast<bool>(std::ifstream(filename));
}

static bool isWhitespace(char c)
{
    // NOTE: Chars 128..255 should be treated as non-whitespace, thus, isprint() will not do:
    //return isspace(c) || !isprint(c);
    return (((unsigned char) c) <= 32) || (c == 127);
}

static void skipWhitespace(const char** const pp)
{
    while (**pp != '\0' && isWhitespace(**pp))
        ++(*pp);
}

/**
 * @param outName Set to empty string if the line is empty or comment.
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

    // Trim trailing spaces in value.
    int i = (int) outValue->size() - 1;
    while (i >= 0 && isWhitespace((*outValue)[i]))
        --i;
    *outValue = outValue->substr(0, i + 1);

    // If value is quoted, unquote.
    if (outValue->size() >= 2 && (*outValue)[0] == '"' && (*outValue)[outValue->size() - 1] == '"')
        *outValue = outValue->substr(1, outValue->size() - 2);

    return true;
}

} // namespace

class FlagConfig::Impl
{
public:
    Impl(const char* moduleName);
    void reload();
    void skipNextReload();
    const char* tempPath() const;
    const char* moduleName() const;

public:
    struct AbstractParam
    {
        FlagConfig::Impl* owner;
        const char* const name;
        const char* const descr;

        AbstractParam(FlagConfig::Impl* owner, const char* name, const char* descr)
        :
            owner(owner),
            name(name),
            descr(descr)
        {
        }

        virtual ~AbstractParam() = default;

        void printLine(const std::string& value, const char* valueNameSeparator,
            const char* note, bool error, bool equalsDefault) const;

        virtual void reload() = 0;
        virtual std::string defaultValueStr() const = 0;
    };

    struct FlagParam;
    struct IntParam;
    struct StringParam;

public:
    std::vector<std::unique_ptr<AbstractParam>> m_params;

private:
    char m_tempPath[L_tmpnam];
    const char* const m_moduleName;
    std::map<std::string, std::string> m_paramsMap;
    bool m_skipNextReload = false;

private:
    bool parseIniFile();
    void printFlagFileHeader() const;
    std::string iniFilename() const;
    std::string flagFilename(const char* value, const char* paramName) const;
    std::string txtFilename(const char* paramName) const;
};

//-------------------------------------------------------------------------------------------------
// AbstractParam

void FlagConfig::Impl::AbstractParam::printLine(
    const std::string& value, const char* valueNameSeparator,
    const char* note, bool error, bool equalsDefault) const
{
    const char* const prefix = error ? "!!! " : (equalsDefault ? "    " : "  + ");
    const char* const descr_prefix = (strlen(descr) == 0) ? "" : " // ";
    std::cerr << prefix << value << valueNameSeparator << name
        << note << descr_prefix << descr << "\n";
}

//-------------------------------------------------------------------------------------------------
// FlagParam

struct FlagConfig::Impl::FlagParam: FlagConfig::Impl::AbstractParam
{
    bool* const pValue;
    const bool defaultValue;

    FlagParam(FlagConfig::Impl* owner, bool* pValue, bool defaultValue, const char* name,
        const char* descr)
    :
        AbstractParam(owner, name, descr),
        pValue(pValue),
        defaultValue(defaultValue)
    {
    }

    virtual void reload() override;
    virtual std::string defaultValueStr() const override;

    static bool strToBool(const std::string& s, bool* pValue);
};

void FlagConfig::Impl::FlagParam::reload()
{
    if (kUseIniFile)
    {
        *pValue = defaultValue;
        const std::string value = owner->m_paramsMap[name];
        bool error = false;
        const char* note = "";
        if (!value.empty()) //< Value is present in .ini file and is not empty.
        {
            if (!strToBool(value, pValue))
            {
                error = true;
                note = " [.ini: invalid value]";
            }
        }
        printLine(std::to_string(*pValue), " ", note, error, *pValue == defaultValue);
    }
    else
    {
        const bool exists1 = fileExists(owner->flagFilename("1", name).c_str());
        const bool exists0 = fileExists(owner->flagFilename("0", name).c_str());
        if (exists1 == exists0)
            *pValue = defaultValue;
        else
            *pValue = exists1;

        bool error = false;
        const char* note = "";
        if (exists1 && exists0)
        {
            note = " [conflicting .flag files]";
            error = true;
        }
        else if (*pValue != defaultValue)
        {
            note = " [.flag]";
        }
        printLine(std::to_string(*pValue), "_", note, error, *pValue == defaultValue);
    }
}

std::string FlagConfig::Impl::FlagParam::defaultValueStr() const
{
    return defaultValue ? "true" : "false";
}

bool FlagConfig::Impl::FlagParam::strToBool(const std::string& s, bool* pValue)
{
    if (s == "true" || s == "True" || s == "TRUE" || s == "1")
        *pValue = true;
    else if (s == "false" || s == "False" || s == "FALSE" || s == "0")
        *pValue = false;
    else
        return false;
    return true;
}

//-------------------------------------------------------------------------------------------------
// IntParam

struct FlagConfig::Impl::IntParam: FlagConfig::Impl::AbstractParam
{
    int* const pValue;
    const int defaultValue;

    IntParam(FlagConfig::Impl* owner, int* pValue, int defaultValue, const char* name,
        const char* descr)
    :
        AbstractParam(owner, name, descr),
        pValue(pValue),
        defaultValue(defaultValue)
    {
    }

    virtual void reload() override;
    virtual std::string defaultValueStr() const override;
};

void FlagConfig::Impl::IntParam::reload()
{
    *pValue = defaultValue;

    if (kUseIniFile)
    {
        const std::string value = owner->m_paramsMap[name];
        bool error = false;
        const char* note = "";
        if (!value.empty()) //< Value is present in .ini file and is not empty.
        {
            try
            {
                *pValue = std::stoi(value);
            }
            catch (...)
            {
                error = true;
                note = " [.ini: invalid value]";
            }
        }
        printLine(std::to_string(*pValue), " = ", note, error, *pValue == defaultValue);
    }
    else
    {
        std::string txt = owner->txtFilename(name);
        std::ifstream f(txt);
        bool txtExists = static_cast<bool>(f);
        bool error = false;
        if (txtExists)
        {
            if (!(f >> *pValue))
            {
                *pValue = defaultValue;
                error = true;
            }
        }
        const char* note = error ? " [unable to read from .txt]" : (txtExists ? " [.txt]" : "");
        printLine(std::to_string(*pValue), " = ", note, error, *pValue == defaultValue);
    }
}

std::string FlagConfig::Impl::IntParam::defaultValueStr() const
{
    return std::to_string(defaultValue);
}

//-------------------------------------------------------------------------------------------------
// StringParam

struct FlagConfig::Impl::StringParam: FlagConfig::Impl::AbstractParam
{
    std::string strValue; //< Owned non-default value.
    const char** const pValue;
    const char* const defaultValue;

    StringParam(FlagConfig::Impl* owner,
        const char** pValue, const char* defaultValue, const char* name,const char* descr)
    :
        AbstractParam(owner, name, descr),
        pValue(pValue),
        defaultValue(defaultValue)
    {
    }

    virtual void reload() override;
    virtual std::string defaultValueStr() const override;
};

void FlagConfig::Impl::StringParam::reload()
{
    *pValue = defaultValue;

    if (kUseIniFile)
    {
        bool error = false;
        const char* note = "";
        if (owner->m_paramsMap.count(name) == 0) //< Missing from .ini file.
        {
            strValue = "";
        }
        else
        {
            strValue = owner->m_paramsMap[name];
            *pValue = strValue.c_str();
        }
        std::string valueString(*pValue);
        printLine("\"" + valueString + "\"", " = ", note, error, valueString == defaultValue);
    }
    else
    {
        std::string txt = owner->txtFilename(name);
        std::ifstream f(txt);
        bool txtExists = static_cast<bool>(f);
        bool error = false;
        if (txtExists)
        {
            if (!(f >> strValue))
            {
                *pValue = defaultValue;
                error = true;
            }
            else
            {
                *pValue = strValue.c_str();
            }
        }
        const char* note = error ? " [unable to read from .txt]" : (txtExists ? " [.txt]" : "");
        printLine(*pValue, " = ", note, error, std::string(*pValue) == defaultValue);
    }
}

std::string FlagConfig::Impl::StringParam::defaultValueStr() const
{
    return std::string("\"") + defaultValue + "\"";
}

//-------------------------------------------------------------------------------------------------
// Impl

FlagConfig::Impl::Impl(const char* moduleName)
    :
    m_moduleName(moduleName)
{
    // Get temp directory path.
    if (!std::tmpnam(m_tempPath))
    {
        m_tempPath[0] = '\0';
        std::cerr << m_moduleName << " configuration WARNING: Unable to get temp path.\n";
    }
    else
    {
        for (int i = (int) strlen(m_tempPath) - 1; i >= 0; --i)
        {
            if (m_tempPath[i] == '/' || m_tempPath[i] == '\\')
            {
                m_tempPath[i + 1] = '\0';
                break;
            }
        }
    }
}

const char* FlagConfig::Impl::tempPath() const
{
    return m_tempPath;
}

const char* FlagConfig::Impl::moduleName() const
{
    return m_moduleName;
}

bool FlagConfig::Impl::parseIniFile()
{
    std::string filename = iniFilename();

    m_paramsMap.clear();

    std::ifstream file(filename);
    if(!file.good())
        return false;

    int line = 0;
    std::string lineStr;
    while (std::getline(file, lineStr))
    {
        ++line;

        std::string value;
        std::string name;
        if (!parseNameValue(lineStr.c_str(), &name, &value))
        {
            std::cerr << m_moduleName << " configuration WARNING: "
                << "Unable to parse .ini: line " << line << ", file: " << filename << "\n";
            continue;
        }
        if (!name.empty())
            m_paramsMap[name] = value;
    }
    if (line == 0) //< .ini file is empty: create the file with default values.
    {
        std::cerr << "    ATTENTION: .ini file is empty; filling with defaults.\n";

        std::ofstream file(filename);
        if (!file.good())
        {
            std::cerr << "    ERRPR: Unable to rewrite .ini file.\n";
        }
        else
        {
            for (const auto& param: m_params)
            {
                std::string descr = param->descr;
                if (descr.size() > 0)
                    descr += " ";
                file << "# " << descr << "Default: " << param->defaultValueStr() << "\n";
                file << param->name << "=" << param->defaultValueStr() << "\n";
                file << "\n";
            }
        }
    }
    return true;
}

void FlagConfig::Impl::printFlagFileHeader() const
{
    std::cerr << m_moduleName << " config ("
        << flagFilename("<0|1>", "<FLAG>") << ", "
        << txtFilename("<INT_PARAM>") << "):\n";
}

std::string FlagConfig::Impl::iniFilename() const
{
    return std::string(m_tempPath) + m_moduleName + ".ini";
}

std::string FlagConfig::Impl::flagFilename(const char* value, const char* paramName) const
{
    return std::string(m_tempPath) + m_moduleName + "_" + value + "_" + paramName + ".flag";
}

std::string FlagConfig::Impl::txtFilename(const char* paramName) const
{
    return std::string(m_tempPath) + m_moduleName + "_" + paramName + ".txt";
}

void FlagConfig::Impl::reload()
{
    if (m_skipNextReload)
    {
        m_skipNextReload = false;
        return;
    }

    if (kUseIniFile)
    {
        bool iniExists = fileExists(iniFilename().c_str());
        std::cerr << m_moduleName << " config (" << iniFilename()
            << (iniExists ? "" : " not found; touch to fill with defaults") << "):\n";
        if (iniExists)
            parseIniFile();
        else
            m_paramsMap.clear();
    }
    else
    {
        printFlagFileHeader();
    }

    for (const auto& param: m_params)
        param->reload();
}

void FlagConfig::Impl::skipNextReload()
{
    m_skipNextReload = true;
}

//-------------------------------------------------------------------------------------------------
// FlagConfig

FlagConfig::FlagConfig(const char* moduleName)
:
    d(new Impl(moduleName))
{
}

FlagConfig::~FlagConfig()
{
    delete d;
}

const char* FlagConfig::tempPath() const
{
    return d->tempPath();
}

const char* FlagConfig::moduleName() const
{
    return d->moduleName();
}

bool FlagConfig::regFlagParam(
    bool* pValue, bool defaultValue, const char* paramName, const char* descr)
{
    d->m_params.push_back(std::unique_ptr<Impl::FlagParam>(
        new Impl::FlagParam(d, pValue, defaultValue, paramName, descr)));
    return defaultValue;
}

int FlagConfig::regIntParam(
    int* pValue, int defaultValue, const char* paramName, const char* descr)
{
    d->m_params.push_back(std::unique_ptr<Impl::IntParam>(
        new Impl::IntParam(d, pValue, defaultValue, paramName, descr)));
    return defaultValue;
}

const char* FlagConfig::regStringParam(
    const char** pValue, const char* defaultValue, const char* paramName, const char* descr)
{
    d->m_params.push_back(std::unique_ptr<Impl::StringParam>(
        new Impl::StringParam(d, pValue, defaultValue, paramName, descr)));
    return defaultValue;
}

void FlagConfig::reload()
{
    d->reload();
}

void FlagConfig::skipNextReload()
{
    d->skipNextReload();
}

} // namespace utils
} // namespace nx

//-------------------------------------------------------------------------------------------------
#else // ANDROID || __ANDROID__

// Stub implementation which never changes hard-coded defaults.
// NOTE: Android NDK does not support std::to_string

namespace nx {
namespace utils {

struct FlagConfig::Impl
{
    const char* moduleName;
};

FlagConfig::FlagConfig(const char* moduleName)
:
    d(new Impl())
{
    d->moduleName = moduleName;
}

FlagConfig::~FlagConfig()
{
    delete d;
}

const char* FlagConfig::tempPath() const
{
    return "/tmp";
}

const char* FlagConfig::moduleName() const
{
    return d->moduleName;
}

bool FlagConfig::regFlagParam(
    bool* pValue, bool defaultValue, const char* paramName, const char* descr)
{
    return defaultValue;
}

int FlagConfig::regIntParam(
    int* pValue, int defaultValue, const char* paramName, const char* descr)
{
    return defaultValue;
}

const char* FlagConfig::regStringParam(
    const char** pValue, const char* defaultValue, const char* paramName, const char* descr)
{
    return defaultValue;
}

void FlagConfig::reload()
{
}

void FlagConfig::skipNextReload()
{
}

} // namespace utils
} // namespace nx

#endif // ANDROID || __ANDROID__
