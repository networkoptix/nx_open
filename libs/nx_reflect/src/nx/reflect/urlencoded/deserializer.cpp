// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "deserializer.h"

#include <utility>

namespace nx::reflect::urlencoded::detail {

std::tuple<std::string, DeserializationResult> decode(const std::string_view& str)
{
    std::string result;

    for (size_t i = 0; i < str.size(); ++i)
    {
        switch (str[i])
        {
            case '%':
                if (i + 2 < str.size())
                {
                    int val;
                    std::string substr = std::string(str.substr(i + 1, 2));
                    auto sscanfRes = sscanf(substr.data(), "%x", &val);
                    if (sscanfRes != 1)
                        return {{}, {false, "Failed to decode", std::move(substr)}};
                    result += static_cast<char>(val);
                    i += 2;
                }
                else
                {
                    return {{},
                        {false,
                            "Unexpected end of string with encoded symbol at the end",
                            std::string{str}}};
                }
                break;

            default:
                result += str[i];
        }
    }

    return {std::move(result), true};
}

std::tuple<std::string_view, DeserializationResult> trimBrackets(const std::string_view& str)
{
    static constexpr std::pair<char, char> kBrackets[] = {
        {'{', '}'}, {'(', ')'}, {'[', ']'}
    };

    if (str.length() < 2)
        return {str, true};

    for (const auto& p: kBrackets)
    {
        if (str.starts_with(p.first))
        {
            if (!str.ends_with(p.second))
                return {{}, {false, std::string{"Must end with "} + p.second, std::string{str}}};
            return {str.substr(1, str.length() - 2), true};
        }
    }

    return {str, true};
}

std::tuple<std::vector<std::string_view>, DeserializationResult> tokenizeRequest(
    const std::string_view& request, char delimiter)
{
    std::vector<std::string_view> requestTokenized;

    int bracesCounter = 0;
    unsigned begPos = 0;
    unsigned curPos = 0;
    for (auto c: request)
    {
        if (c == '[' || c == '{')
            ++bracesCounter;
        if ((c == ']' || c == '}'))
            --bracesCounter;
        if (bracesCounter > 0)
        {
            ++curPos;
            continue;
        }
        if (bracesCounter < 0)
        {
            return {std::move(requestTokenized),
                {false,
                    "Unexpected bracket",
                    std::string{request.substr(begPos, curPos - begPos)}}};
        }
        if (c == delimiter)
        {
            requestTokenized.push_back(request.substr(begPos, curPos - begPos));
            begPos = curPos + 1;
        }
        ++curPos;
    }

    // NOTE: Trailing separator is not considered an error.

    if (curPos > begPos)
        requestTokenized.push_back(request.substr(begPos, curPos - begPos));
    return {std::move(requestTokenized), true};
}

} // namespace nx::reflect::urlencoded::detail
