#include "app_utils.h"

#include <cstring>

#include <nx/utils/log/log.h>

namespace nx::analytics::app_utils {

OptionCont OptionCont::operator||(const OptionCont& right) noexcept
{
    OptionCont result(*this);

    result.equalOptions.insert(result.equalOptions.end(), right.equalOptions.begin(),
        right.equalOptions.end());
    result.shortOptions.insert(result.shortOptions.end(), right.shortOptions.begin(),
        right.shortOptions.end());

    return result;
}

//-------------------------------------------------------------------------------------------------
// Args::ParseState implementation.

Args::ParseState::ParseState(int argcVal, const char* const *argvVal,
    const char* argPosVal) noexcept:
    m_argc(argcVal), m_argv(argvVal), m_argPos(argPosVal)
{
}

bool Args::ParseState::nextWord() noexcept
{
    ++m_argv;
    --m_argc;
    m_argPos = (m_argc > 0) ? *m_argv : 0;
    return m_argc > 0;
}

bool Args::ParseState::end() noexcept
{
    return m_argc == 0;
}

const char* Args::ParseState::currentPos() noexcept
{
    return m_argPos;
}

void Args::ParseState::currentPos(const char* pos) noexcept
{
    m_argPos = pos;
}

//-------------------------------------------------------------------------------------------------
// Args implementation.

bool Args::parseEqOp(ParseState& parseState)
{
    static const char* const kFunctionName = "AppUtils::Args::parseEqOp()";

    const char* curOpt = parseState.currentPos();

    if (curOpt[0] == '-' && curOpt[1] == '-')
    {
        curOpt += 2;
        if (const char* equalPos = ::strchr(curOpt, '='))
        {
            std::string optName(curOpt, equalPos - curOpt);
            OptionSetterMap::iterator it = m_equalOptions.find(optName);

            if (it != m_equalOptions.end())
            {
                if (!it->second->requireValue())
                {
                    throw std::runtime_error(nx::format(
                        "%1: Defined value for option without values").arg(
                        kFunctionName).toStdString());
                }

                it->second->set(optName.c_str(), equalPos + 1);
            }
            else
            {
                 throw std::runtime_error(nx::format("%1: Unknown long option '%2'").arg(
                     kFunctionName).arg(optName).toStdString());
            }
        }
        else
        {
            const std::string optName(curOpt);
            OptionSetterMap::iterator it = m_equalOptions.find(optName);
            if (it != m_equalOptions.end())
            {
                if (it->second->requireValue())
                {
                    throw std::runtime_error(nx::format("%1: Undefined value for option '%2'").arg(
                        kFunctionName).arg(optName).toStdString());
                }

                it->second->set(optName.c_str(), 0);
            }
            else
            {
                throw std::runtime_error(nx::format("%1: Unknown long option '%2'").arg(
                    kFunctionName).arg(curOpt).toStdString());
            }
        }

        parseState.nextWord();

        return true;
    }

    return false;
}

bool Args::parseShortOptSeq(ParseState& parseState)
{
    static const char* const kFunctionName = "AppUtils::Args::parseShortOptSeq()";

    const char* curOpt = parseState.currentPos();
    if (*curOpt == '\0')
    {
        std::ostringstream ostr;
        ostr << kFunctionName << ": Empty option.";
        throw std::runtime_error(ostr.str());
    }

    if (*curOpt != '-')
        return false;

    if (*++curOpt == '\0')
    {
        throw std::runtime_error(nx::format("%1: Empty option name after '-'.").arg(
            kFunctionName).toStdString());
    }

    parseState.currentPos(curOpt);

    while (!parseState.end() && parseShortOpt(parseState))
    {
    }

    return true;
}

bool Args::parseShortOpt(ParseState& parseState)
{
    const char* curOpt = parseState.currentPos();
    const char* next = curOpt + ::strlen(curOpt);

    while (next > curOpt)
    {
        std::string optName(curOpt, next - curOpt);
        OptionSetterMap::iterator shortIt = m_shortOptions.find(optName);

        if (shortIt != m_shortOptions.end())
        {
            parseState.currentPos(next);
            parseShortOpValue(shortIt, optName.c_str(), parseState);
            return false;
        }

        --next;
    }

    return false;
}

void Args::parseShortOpValue(OptionSetterMap::iterator it, const char* optName,
    ParseState& parseState)
{
    static const char* const kFunctionName = "AppUtils::Args::parseShortOpValue()";

    if (it->second->requireValue())
    {
        if (parseState.end())
        {
            throw std::runtime_error(nx::format("%1: Undefined value after option '%2'").arg(
                kFunctionName).arg(optName).toStdString());
        }

        if (*parseState.currentPos())
        {
            it->second->set(optName, parseState.currentPos());
            parseState.nextWord();
        }
        else
        {
            if (parseState.nextWord())
            {
                it->second->set(optName, parseState.currentPos());
                parseState.nextWord();
            }
            else
            {
                throw std::runtime_error(nx::format("%1: Found end of command after option.").arg(
                    kFunctionName).toStdString());
            }
        }
    }
    else
    {
        it->second->set(optName, 0);

        if (*parseState.currentPos() == 0)
            parseState.nextWord();
    }
}

void Args::parse(int argc, const char* const argv[])
{
    static const char* const kFunctionName = "AppUtils::Args::parse()";

    int commandCounter = 0;

    ParseState parseState(argc, argv, argv[0]);

    while (!parseState.end())
    {
        if (!parseEqOp(parseState) &&
            !parseShortOptSeq(parseState))
        {
            if (m_commandCount == -1 || commandCounter < m_commandCount)
            {
                ++commandCounter;
                m_commands.emplace_back(parseState.currentPos());
                parseState.nextWord();
            }
            else
            {
                throw std::runtime_error(nx::format("%1: Unknown option: '%2'").arg(
                    kFunctionName).arg(parseState.currentPos()).toStdString());
            }
        }
    }
}

void Args::usage(std::ostream& output) const
{
    for (Usage::const_iterator it = m_usage.begin(); it != m_usage.end();
         ++it)
    {
        output << it->second << "\n";
    }
}

} // namespace nx::analytics::app_utils
