#include <assert.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "command.h"
#include "command_factory.h"

CommandsFactory::CommandsFactory() = default;
CommandsFactory::~CommandsFactory() = default;

void CommandsFactory::reg(
    const std::vector<std::string>& names,
    const std::vector<std::string>& argsNames,
    Action action)
{
    assert(!names.empty());
    if (names.empty())
        return;

    for (const auto& name: names)
    {
        m_commands.insert(
            std::make_pair(name, std::unique_ptr<Command>(new Command(name, argsNames, action))));
    }
}

Command* CommandsFactory::get(const std::string& command) const
{
    std::string baseCmd;
    auto begin = command.cbegin();
    extractWord(&begin, command.cend(), &baseCmd);
    auto commandIt = m_commands.find(baseCmd);

    return commandIt == m_commands.cend() ? nullptr : commandIt->second.get();
}

std::string CommandsFactory::help() const
{
    std::stringstream out;
    out << "Usage: root_tool <command> <args...>" << std::endl
        << "Supported commands:" << std::endl;

    for (const auto& p: m_commands)
        out << " " << std::setw(10) << std::left << p.first << p.second->help() << std::endl;

    return out.str();
}
