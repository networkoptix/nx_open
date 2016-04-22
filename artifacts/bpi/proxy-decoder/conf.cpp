#include "conf.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <cstring>

static bool fileExists(const char* filename)
{
    return static_cast<bool>(std::ifstream(filename));
}

Conf::Conf(const char* moduleName)
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

void Conf::regFlag(bool* pFlag, bool defaultValue, const char* flagName)
{
    m_flags.push_back(Flag{pFlag, defaultValue, flagName});
}

std::string Conf::flagFilename(const char* value, const char* flagName)
{
    return std::string(m_tempPath) + m_moduleName + "_" + value + "_" + flagName + ".flag";
}

void Conf::reloadFlag(const Flag& flag, bool printLog)
{
    const bool exists1 = fileExists(flagFilename("1", flag.name).c_str());
    const bool exists0 = fileExists(flagFilename("0", flag.name).c_str());
    if (exists1 == exists0)
        *flag.pFlag = flag.defaultValue;
    else
        *flag.pFlag = exists1;

    if (printLog)
    {
        std::cerr << "    " << *flag.pFlag << "_" << flag.name
            << ((exists1 && exists0) ?  " (conflicting .flag files)" : "")
            << ((*flag.pFlag != flag.defaultValue) ? " (.flag)" : "") << "\n";
    }
}

void Conf::reloadSingleFlag(bool* pFlag)
{
    for (const auto& flag: m_flags)
    {
        if (flag.pFlag == pFlag)
        {
            reloadFlag(flag, /*printLog*/ false);
            return;
        }
    }
    std::cerr << m_moduleName << " configuration WARNING: Flag to reload not found.\n";
}

void Conf::reload()
{
    std::cerr << m_moduleName << " configuration (touch "
        << flagFilename("<0|1>", "<CONF>") << " to override):\n";
    std::cerr << "{\n";
    for (const auto& item: m_flags)
        reloadFlag(item, /*printLog*/ true);
    std::cerr << "}\n";
}
