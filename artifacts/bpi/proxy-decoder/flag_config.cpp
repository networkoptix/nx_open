#include "flag_config.h"

#include <fstream>
#include <iostream>
#include <cstring>

namespace nx {
namespace utils {

namespace {

static bool fileExists(const char* filename)
{
    return static_cast<bool>(std::ifstream(filename));
}

} // namespace

//-------------------------------------------------------------------------------------------------

FlagConfig::FlagConfig(const char* moduleName)
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

    printHeader();
}

const char* FlagConfig::tempPath() const
{
    return m_tempPath;
}

const char* FlagConfig::moduleName() const
{
    return m_moduleName;
}

void FlagConfig::printHeader() const
{
    std::cerr << m_moduleName << " config ("
        << flagFilename("<0|1>", "<FLAG>") << ", "
        << txtFilename("<INT_PARAM>") <<"):\n";
}

bool FlagConfig::regFlagParam(
    bool* pValue, bool defaultValue, const char* paramName, const char* descr)
{
    m_params.push_back(std::unique_ptr<FlagParam>(
        new FlagParam(this, pValue, defaultValue, paramName, descr)));
    m_params.back()->reload(/*printLog*/ true);
    return defaultValue;
}

int FlagConfig::regIntParam(
    int* pValue, int defaultValue, const char* paramName, const char* descr)
{
    m_params.push_back(std::unique_ptr<IntParam>(
        new IntParam(this, pValue, defaultValue, paramName, descr)));
    m_params.back()->reload(/*printLog*/ true);
    return defaultValue;
}

std::string FlagConfig::flagFilename(const char* value, const char* paramName) const
{
    return std::string(m_tempPath) + m_moduleName + "_" + value + "_" + paramName + ".flag";
}

std::string FlagConfig::txtFilename(const char* paramName) const
{
    return std::string(m_tempPath) + m_moduleName + "_" + paramName + ".txt";
}

void FlagConfig::FlagParam::reload(bool printLog) const
{
    const bool exists1 = fileExists(owner->flagFilename("1", name).c_str());
    const bool exists0 = fileExists(owner->flagFilename("0", name).c_str());
    if (exists1 == exists0)
        *pValue = defaultValue;
    else
        *pValue = exists1;

    if (printLog)
    {
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

void FlagConfig::Param::printLine(const std::string& value, const char* valueNameSeparator,
    const char* note, bool error, bool equalsDefault) const
{
    const char* const prefix = error ? "!!! " : (equalsDefault ? "    " : "  + ");
    const char* const descr_prefix = (strlen(descr) == 0) ? "" : " // ";
    std::cerr << prefix << value << valueNameSeparator << name
        << note << descr_prefix << descr << "\n";
}

void FlagConfig::IntParam::reload(bool printLog) const
{
    *pValue = defaultValue;

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

    if (printLog)
    {
        const char* note = error ? " [unable to read from .txt]" : (txtExists ? " [.txt]" : "");
        printLine(std::to_string(*pValue), " = ", note, error, *pValue == defaultValue);
    }
}

void FlagConfig::reloadSingleFlag(bool* pValue) const
{
    for (const auto& param: m_params)
    {
        if (auto flag = dynamic_cast<const FlagParam*>(param.get()))
        {
            if (pValue == flag->pValue)
            {
                flag->reload(/*printLog*/ false);
                return;
            }
        }
    }
    std::cerr << m_moduleName << " configuration WARNING: Param to reload not found.\n";
}

void FlagConfig::reload() const
{
    printHeader();
    for (const auto& param: m_params)
        param->reload(/*printLog*/ true);
}

} // namespace utils
} // namespace nx
