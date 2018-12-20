#pragma once

#include <initializer_list>
#include <string>

namespace nx {
namespace network {
namespace http {
namespace rest {

namespace detail {

template<typename ArgumentType>
bool substituteNextParameter(
    std::string* path,
    const ArgumentType& argument)
{
    const auto openingBracketPos = path->find('{');
    const auto closingBracketPos = path->find('}');

    if (openingBracketPos == std::string::npos ||
        closingBracketPos == std::string::npos ||
        closingBracketPos < openingBracketPos)
    {
        return false;
    }

    path->replace(
        openingBracketPos,
        closingBracketPos - openingBracketPos + 1,
        argument);
    return true;
}

} // namespace detail

template<typename ArgumentType>
bool substituteParameters(
    const std::string& pathTemplate,
    std::string* resultPath,
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
std::string substituteParameters(
    const std::string& pathTemplate,
    std::initializer_list<ArgumentType> arguments)
{
    std::string resultPath;
    if (!substituteParameters(pathTemplate, &resultPath, arguments))
    {
        NX_ASSERT(false);
    }
    return resultPath;
}

} // namespace rest
} // namespace nx
} // namespace network
} // namespace http
