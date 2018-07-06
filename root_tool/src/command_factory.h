#pragma once

#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include "common.h"

class Command;

class CommandsFactory
{
public:
    CommandsFactory();
    ~CommandsFactory();
    void reg(
        const std::vector<std::string>& names,
        const std::vector<std::string>& argsNames,
        Action action);

    Command* get(const char*** argv) const;
    std::string help() const;

private:
    std::unordered_map<std::string, std::unique_ptr<Command>> m_commands;
};
