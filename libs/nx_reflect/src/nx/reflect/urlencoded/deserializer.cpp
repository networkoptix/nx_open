// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "deserializer.h"

namespace nx::reflect::urlencoded::detail {

std::tuple<std::string, bool> decode(const std::string_view& str)
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
                        return {"", false};
                    result += static_cast<char>(val);
                    i += 2;
                }
                else
                    return {"", false};
                break;
            default:
                result += str[i];
        }
    }
    return {result, true};
}

std::tuple<std::string_view, bool> trancateCurlBraces(const std::string_view& str)
{
    if (str.length() < 2)
        return {str, false};
    return {str.substr(1, str.length() - 2), true};
}

NX_REFLECT_API std::tuple<std::vector<std::string_view>, bool> tokenizeRequest(
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
            return {requestTokenized, false};
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
    return {requestTokenized, true};
}

} // namespace nx::reflect::urlencoded::detail
