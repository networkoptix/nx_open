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

Command* CommandsFactory::get(const char*** argv) const
{
    if (**argv == nullptr)
    {
        std::cout << "This program is not supposed to be used without argv." << std::endl;
        return nullptr;
    }

    if (*(++*argv) == nullptr)
    {
        std::cout << "Command required" << std::endl;
        return nullptr;
    }

    auto commandIt = m_commands.find(**argv);
    if (commandIt == m_commands.cend())
    {
        std::cout << "Unknown command: " << **argv << std::endl;
        return nullptr;
    }

    ++*argv;

    return commandIt->second.get();
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
