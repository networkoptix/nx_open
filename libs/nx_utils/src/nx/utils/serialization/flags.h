// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cctype>
#include <charconv>
#include <type_traits>
#include <typeinfo>

#include <QtCore/QFlags>

#include <nx/reflect/enum_string_conversion.h>

namespace nx::utils::flags_detail {

// These functions are needed to avoid inclusion of heavy nx/utils headers.

NX_UTILS_API std::string_view trim(const std::string_view& str);

NX_UTILS_API void assertInvalidFlagValue(
    const std::type_info& type, int value, int unrecognizedFlags);
NX_UTILS_API void logInvalidFlagValue(
    const std::type_info& type, int value, int unrecognizedFlags);
NX_UTILS_API void logInvalidFlagRepresentation(
    const std::type_info& type, const std::string_view& flag);

template<typename T, typename Visitor>
void processBits(const QFlags<T>& value, const Visitor& visitor)
{
    using Int = typename QFlags<T>::Int;

    constexpr int kBitCount = sizeof(Int) * 8 - (std::is_signed_v<Int> ? 1 : 0);
    for (int i = 0; i < kBitCount; ++i)
    {
         const auto flag = (Int) 1 << i;
         if (value & flag)
             visitor(flag);
    }
}

} // namespace nx::utils::flags_detail

template<typename T, typename = std::enable_if_t<nx::reflect::IsInstrumentedEnumV<T>>>
std::string toString(const QFlags<T>& value)
{
    using Int = typename QFlags<T>::Int;

    std::string result;

    if (!value)
    {
        result = nx::reflect::enumeration::toString(T());
        return (result == "0") ? "" : result;
    }

    Int unrecognizedFlags = 0;

    const auto processFlag =
        [&unrecognizedFlags, &result](Int flag)
        {
            const auto flagStr = nx::reflect::enumeration::toString((T) flag);
            if (flagStr.empty() || std::isdigit((unsigned char) flagStr[0]))
            {
                unrecognizedFlags |= flag;
                return;
            }

            if (!result.empty())
                result += '|';
            result += flagStr;
        };

    nx::utils::flags_detail::processBits(value, processFlag);

    if (unrecognizedFlags != 0 || value < 0)
        nx::utils::flags_detail::assertInvalidFlagValue(typeid(T), value, unrecognizedFlags);

    return result;
}

template<typename T, typename = std::enable_if_t<nx::reflect::IsInstrumentedEnumV<T>>>
bool fromString(const std::string_view& str, QFlags<T>* result)
{
    *result = {};

    std::string_view::size_type pos = 0;

    bool valid = true;

    while (pos < str.length())
    {
        std::string_view flag;

        const auto splitterPos = str.find('|', pos);
        if (splitterPos == std::string_view::npos)
        {
            flag = nx::utils::flags_detail::trim(str.substr(pos));
            pos = str.length();
        }
        else
        {
            flag = nx::utils::flags_detail::trim(str.substr(pos, splitterPos - pos));
            pos = splitterPos + 1;
        }

        if (flag.empty())
            continue;

        T flagValue;
        if (nx::reflect::enumeration::fromString(flag, &flagValue))
        {
            *result |= flagValue;
        }
        else
        {
            using Int = typename QFlags<T>::Int;
            Int number = 0;
            if (nx::reflect::enumeration::detail::parseNumber(flag, &number))
            {
                Int unrecognizedFlags = 0;

                const auto processFlag =
                    [&unrecognizedFlags, &number](Int flag)
                    {
                        if (!nx::reflect::enumeration::isValidEnumValue((T) flag))
                        {
                            unrecognizedFlags |= flag;
                            number |= ~flag;
                        }
                    };

                nx::utils::flags_detail::processBits(QFlags<T>(number), processFlag);

                if (unrecognizedFlags == 0 && number >= 0)
                {
                    *result |= (T) number;
                }
                else
                {
                    nx::utils::flags_detail::logInvalidFlagValue(
                        typeid(T), number, unrecognizedFlags);
                    valid = false;
                }
            }
            else
            {
                nx::utils::flags_detail::logInvalidFlagRepresentation(typeid(T), flag);
                valid = false;
            }
        }
    }

    return valid;
}

NX_REFLECTION_TAG_TEMPLATE_TYPE(QFlags, useStringConversionForSerialization)
