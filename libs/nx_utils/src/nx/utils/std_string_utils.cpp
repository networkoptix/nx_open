// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "std_string_utils.h"

#include <QtCore/QByteArray>

namespace nx::utils {

int stricmp(std::string_view left, std::string_view right)
{
    if (left.size() == right.size())
        return qstrnicmp(left.data(), right.data(), (int) left.size());

    const auto result = qstrnicmp(left.data(), right.data(), (int) std::min(left.size(), right.size()));
    if (result != 0)
        return result;

    return left.size() < right.size() ? -1 : 1;
}

static bool isOctalDigit(char c)
{
    return c >= '0' && c <= '7';
}

std::string decodeOctalEncoded(std::string_view str)
{
    static constexpr std::size_t kDigits = 3;

    std::string result;
    result.reserve(str.size());
    for (std::size_t i = 0; i < str.size(); ++i)
    {
        const auto digits = str.substr(i + 1, kDigits);
        if (str[i] == '\\' && digits.size() == kDigits
            && std::ranges::all_of(digits, isOctalDigit))
        {
            const int code =
                ((digits[0] - '0') << 6) | ((digits[1] - '0') << 3) | (digits[2] - '0');
            if (code <= 0xFF)
            {
                result += static_cast<char>(code);
                i += kDigits;
                continue;
            }
        }
        result += str[i];
    }
    return result;
}

} // namespace nx::utils
