#pragma once

#include <initializer_list>

#include <nx/network/buffer.h>

#include "../http_types.h"

namespace nx {
namespace network {
namespace http {
namespace rest {

namespace detail {

template<typename ArgumentType>
bool substituteNextParameter(
    nx::String* path,
    const ArgumentType& argument)
{
    const int openingBracketPos = path->indexOf('{');
    const int closingBracketPos = path->indexOf('}');

    if (openingBracketPos == -1 ||
        closingBracketPos == -1 ||
        closingBracketPos < openingBracketPos)
    {
        return false;
    }

    nx::replace(
        path,
        openingBracketPos,
        closingBracketPos - openingBracketPos + 1,
        argument);
    return true;
}

} // namespace detail

template<typename ArgumentType>
bool substituteParameters(
    const nx::String& pathTemplate,
    nx::String* resultPath,
    std::initializer_list<ArgumentType> arguments)
{
    *resultPath = pathTemplate;
    for (const auto& argument: arguments)
    {
        if (!detail::substituteNextParameter(resultPath, argument))
            return false;
    }
    return true;
}

template<typename ArgumentType>
nx::String substituteParameters(
    const nx::String& pathTemplate,
    std::initializer_list<ArgumentType> arguments)
{
    nx::String resultPath;
    if (!substituteParameters(pathTemplate, &resultPath, arguments))
    {
        NX_ASSERT(false);
    }
    return resultPath;
}

template<typename ArgumentType>
std::string substituteParameters(
    const std::string& pathTemplate,
    std::initializer_list<ArgumentType> arguments)
{
    return substituteParameters(
        nx::String::fromStdString(pathTemplate),
        std::move(arguments)).toStdString();
}

template<typename ArgumentType>
std::string substituteParameters(
    const char* pathTemplate,
    std::initializer_list<ArgumentType> arguments)
{
    return substituteParameters(
        nx::String(pathTemplate),
        std::move(arguments)).toStdString();
}

} // namespace rest
} // namespace nx
} // namespace network
} // namespace http
