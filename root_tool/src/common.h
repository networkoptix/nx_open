#pragma once

#include <functional>

enum class Result
{
    ok,
    invalidArg,
    execFailed
};

using Action = std::function<Result(const char**)>;
