/**
 * Commands abstraction support. Aimed to simplify command name-to-action management (registration,
 * execution, getting help).
 */
#include <assert.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "commands.h"

/**
 * Command context stored in cache. 'argNames' and 'isDirect' fields are used only for documentation
 * generation and logging purposes. ARG_NAME in the 'argNames' array may start with the 'opt_'
 * prefix. In this case when generating doc string this prefix will be removed and the remnants of
 * the ARG_NAME will be enclosed in '[' ']'.
 */
struct CommandContext
{
    CommandContext(std::vector<std::string> argNames, Action action, bool isDirect):
            argNames(std::move(argNames)),
            action(std::move(action)),
            isDirect(isDirect)
    {}

    CommandContext(const CommandContext&) = default;

    std::vector<std::string> argNames;
    Action action;
    bool isDirect;
};

/**
 * Provides additional runtime context needed for the command functor execution.
 */
struct ExecutableCommandContext: CommandContext
{
    ExecutableCommandContext(
            const CommandContext& commandContext,
            const std::string& commandString,
            boost::optional<int> transportFd)
            :
            CommandContext(commandContext),
            commandString(commandString),
            transportFd(transportFd)
    {
    }

    std::string commandString;
    boost::optional<int> transportFd;
};

CommandsFactory::CommandsFactory() = default;
CommandsFactory::~CommandsFactory() = default;

/**
 * Register command context in cache. 'isDirect' designates if the command is supposed to be
 * executed directly via calling the root-tool executable and providing command-line arguments or
 * received via socket. Returns reference to self to enable chaining.
 */
CommandsFactory& CommandsFactory::reg(
    const std::vector<std::string>& names,
    std::vector<std::string> argNames,
    Action action,
    bool isDirect)
{
    assert(!names.empty());
    if (names.empty())
        return *this;

    using namespace std;
    auto commandPtr = make_shared<CommandContext>(move(argNames), move(action), isDirect);
    for (const auto& name: names)
        m_commands.insert(std::make_pair(name, commandPtr));

    return *this;
}

/**
 * Parses COMMAND_STRING and extracts command name. In case of success searches for the associated
 * CommandContext object and constructs and returns ExecutableCommandContext object ready for the
 * execution, otherwise returns nullptr.
 */
ExecutableCommandContextPtr CommandsFactory::get(
    const std::string& commandString,
    boost::optional<int> transportFd) const
{
    std::string baseCmd;
    auto begin = commandString.cbegin();
    extractWord(&begin, commandString.cend(), &baseCmd);
    auto commandIt = m_commands.find(baseCmd);

    if (commandIt == m_commands.cend())
        return ExecutableCommandContextPtr();

    return std::make_unique<ExecutableCommandContext>(
        *commandIt->second,
        commandString,
        transportFd);
}

/**
 * Generates help string from the given command context taking into account optionesness of the
 * command arguments.
 */
static std::string help(const CommandContext& commandContext)
{
    std::stringstream out;
    for (const auto& argName: commandContext.argNames)
    {
        if (argName.size() > 4 && argName.substr(0, 4) == "opt_")
            out << "[" << argName.substr(4) << "] ";
        else
            out << "<" << argName << "> ";
    }

    return out.str();
}

/**
 * Generates help string for all registered commands. Groups them by the execution type (direct,
 * received via socket).
 */
std::string CommandsFactory::help() const
{
    std::stringstream out;
    out << "Nx root tool service. Accepts and server Nx Mediaserver tcp requests"
        << " and is called directly in some cases." << std::endl << std::endl
        << "Supported socket commands:" << std::endl;

    int firstColumnWidth = 0;
    for (const auto& p: m_commands)
    {
        if (p.first.size() > firstColumnWidth)
            firstColumnWidth = (int) p.first.size();
    }

    auto appendLine =
        [&](const std::string& name, const CommandContext& commandContext)
        {
            out << " " << std::setw(firstColumnWidth + 1) << std::left << name
                << ::help(commandContext) << std::endl;
        };

    for (const auto& p: m_commands)
    {
        if (!p.second->isDirect)
            appendLine(p.first, *p.second);
    }

    out << std::endl << "Supported direct commands:" << std::endl;
    for (const auto& p: m_commands)
    {
        if (p.second->isDirect)
            appendLine(p.first, *p.second);
    }

    return out.str();
}

/**
 * Executes the given command context functor and reports result via socket if the context contains
 * a valid socket descriptor.
 */
Result execute(const ExecutableCommandContextPtr& command)
{
    auto result = command->action(
        command->commandString,
        command->transportFd ? *command->transportFd : -1);

    if (command->transportFd)
        ::close(*command->transportFd);

    return result;
}

bool isDirect(const ExecutableCommandContextPtr& command)
{
    return command->isDirect;
}

