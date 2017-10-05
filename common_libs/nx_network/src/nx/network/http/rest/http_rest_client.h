#pragma once

#include <initializer_list>

#include "../http_types.h"

namespace nx_http {
namespace rest {

namespace detail {

template<typename Arg>
void replace(QByteArray* where, int pos, int count, const Arg& withWhat)
{
    where->replace(pos, count, withWhat);
}

template<>
inline void replace(QByteArray* where, int pos, int count, const std::string& withWhat)
{
    where->replace(pos, count, withWhat.c_str());
}

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

    replace(
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
} // namespace nx_http
