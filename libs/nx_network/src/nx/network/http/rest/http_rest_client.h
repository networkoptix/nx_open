// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <initializer_list>
#include <string>

#include <nx/utils/log/assert.h>

namespace nx::network::http::rest {

/**
 * Replaces the first occurance of "{<any_token>}" in the path with the given value
 * @return true if replaced a token. false if nothing was found.
 */
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

template<typename ArgumentType>
bool substituteParameters(
    const std::string& pathTemplate,
    std::string* resultPath,
    std::initializer_list<ArgumentType> arguments)
{
    *resultPath = pathTemplate;
    for (const auto& argument: arguments)
    {
        if (!substituteNextParameter(resultPath, argument))
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

} // namespace nx::network::http::rest
