#pragma once

#include <vector>
#include <string>
#include <functional>
#include "common.h"

class Command
{
public:
    Command(const std::string& name, const std::vector<std::string>& argNames, Action action);
    Result exec(const char** argv);
    std::string help() const;

private:
    std::string m_name;
    std::vector<std::string> m_argNames;
    Action m_action;
};
