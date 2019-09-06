#pragma once

#include <cctype>
#include <iterator>
#include <type_traits>

#include <QString>

enum class MatchMode
{
    caseSensitive = 0,
    caseInsensitive,
};

namespace nx::utils::detail {

template <typename CharType>
struct CharTraits {};

template <>
struct CharTraits<char>
{
    using CharType = char;

    static bool equal(CharType one, CharType two, MatchMode mode)
    {
        return mode == MatchMode::caseSensitive
            ? one == two
            : std::tolower(one) == std::tolower(two);
    }
};

template <>
struct CharTraits<QChar>
{
    using CharType = QChar;

    static bool equal(CharType one, CharType two, MatchMode mode)
    {
        return mode == MatchMode::caseSensitive
            ? one == two
            : one.toLower() == two.toLower();
    }
};

} // namespace nx::utils::detail

/**
 * Check whether a string matches a wildcard expression.
 *
 * @param str String being validated to match the wildcard expression.
 * @param mask Wildcard expression. Allowed to contain special characters:
 * '?' (matches any character except '.')
 * and '*' (matches any number of any characters).
 * @return true if mask matches str.
 */
template<typename Iter>
// requires ForwardIter<Iter>
bool wildcardMatch(
    Iter mask, Iter maskEnd,
    Iter str, Iter strEnd,
    MatchMode mode)
{
    using CharType = std::decay_t<decltype(*mask)>;

    while (str != strEnd && mask != maskEnd)
    {
        if (*mask == CharType('*'))
        {
            if (*(std::next(mask)) == CharType('\0'))
                return true; //< We have '*' at the end.
            for (auto str1 = str; str1 != strEnd; ++str1)
            {
                if (wildcardMatch(std::next(mask), maskEnd, str1, strEnd, mode))
                    return true;
            }
            return false; //< Not matching.
        }
        else if (*mask == CharType('?'))
        {
            if (*str == CharType('.'))
                return false;
        }
        else
        {
            if (!nx::utils::detail::CharTraits<CharType>::equal(*str, *mask, mode))
                return false;
        }

        ++str;
        ++mask;
    }

    while (mask != maskEnd)
    {
        if (*mask != CharType('*'))
            return false;
        ++mask;
    }

    if (str != strEnd)
        return false;

    return true;
}

/**
 * Matches null-terminated strings.
 */
template <typename CharType>
bool wildcardMatch(
    const CharType* mask,
    const CharType* str,
    MatchMode mode)
{
    return wildcardMatch(
        mask, mask + std::strlen(mask),
        str, str + std::strlen(str),
        mode);
}

inline bool wildcardMatch(
    const QString& mask,
    const QString& str,
    MatchMode mode = MatchMode::caseSensitive)
{
    return wildcardMatch(
        mask.begin(), mask.end(),
        str.begin(), str.end(),
        mode);
}
