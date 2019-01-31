#pragma once

#include <vector>
#include <string>
#include <functional>
#include <unordered_set>
#include <memory>
#include "common.h"

struct CommandContext;
struct ExecutableCommandContext;

using ExecutableCommandContextPtr = std::shared_ptr<ExecutableCommandContext>;

class CommandsFactory
{
public:
    CommandsFactory();
    ~CommandsFactory();
    CommandsFactory& reg(
        const std::vector<std::string>& names,
        std::vector<std::string> argNames,
        Action action,
        bool isDirect = false);

    ExecutableCommandContextPtr get(
        const std::string &commandString,
        boost::optional<int> transportFd) const;
    std::string help() const;

private:
    std::unordered_map<std::string, std::shared_ptr<CommandContext>> m_commands;
};

Result execute(const ExecutableCommandContextPtr& command);
bool isDirect(const ExecutableCommandContextPtr& command);
