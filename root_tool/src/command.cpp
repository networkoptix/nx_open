#include <assert.h>
#include <sstream>
#include <iostream>
#include "command.h"

Command::Command(const std::string& name, const std::vector<std::string>& argNames, Action action):
    m_name(name),
    m_argNames(argNames),
    m_action(action)
{
    assert(m_action);
}

Result Command::exec(const std::string& command, int transportFd)
{
    auto result = m_action(command, transportFd);
    ::close(transportFd);
    std::cout << "Command " << command << " --> " << toString(result) << std::endl;
    return result;
}

std::string Command::help() const
{
    std::stringstream out;
    for (const auto& argName: m_argNames)
    {
        if (argName.size() > 4 && argName.substr(0, 4) == "opt_")
            out << "[" << argName.substr(4) << "] ";
        else
            out << "<" << argName << "> ";
    }

    return out.str();
}
