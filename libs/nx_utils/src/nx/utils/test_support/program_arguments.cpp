// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "program_arguments.h"

#include <cstdlib>

namespace nx {
namespace utils {
namespace test {

ProgramArguments::~ProgramArguments()
{
    clearArgs();
}

void ProgramArguments::addArg(const char* arg)
{
    auto b = std::back_inserter(m_args);
    *b = strdup(arg);
}

void ProgramArguments::addArg(const char* arg, const char* value)
{
    addArg(arg);
    addArg(value);
}

void ProgramArguments::clearArgs()
{
    for (auto ptr : m_args)
        free(ptr);
    m_args.clear();
}

std::vector<char*>& ProgramArguments::args()
{
    return m_args;
}

int ProgramArguments::argc() const
{
    return (int) m_args.size();
}

char** ProgramArguments::argv()
{
    return m_args.data();
}

const char** ProgramArguments::constArgv() const
{
    return const_cast<const char **>(m_args.data());
}

} // namespace test
} // namespace utils
} // namespace nx
