// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <string>
#include <string_view>
#include <type_traits>

#include "instrument.h"

namespace nx::reflect::enumeration {

namespace detail {

template<typename Iterator, typename T, typename BinaryPredicate>
constexpr Iterator lowerBound(
    Iterator first, Iterator last, const T& value, const BinaryPredicate& comp)
{
    auto count = last - first;

    while (count > 0)
    {
        auto it = first;
        auto step = count / 2;

        it += step;
        if (comp(*it, value))
        {
            first = ++it;
            count -= step + 1;
        }
        else
        {
            count = step;
        }
    }

    return first;
}

template<typename Iterator, typename T, typename BinaryPredicate>
constexpr Iterator dumbLowerBound(
    Iterator first, Iterator last, const T& value, const BinaryPredicate& comp)
{
    auto count = last - first;

    auto i = count - 1;
    while (i >= 0 && comp(value, *(first + i)))
        --i;
    ++i;

    return first + i;
}

template<typename Enum, typename BinaryPredicate, typename... Items>
constexpr auto getSortedItems(BinaryPredicate&& comp, bool dumbSorting, Items&&... items)
{
    std::array<Item<Enum>, sizeof...(items)> result{};
    auto insert =
        [end = result.begin(), &result, &comp, dumbSorting](const Item<Enum>& item) mutable
        {
            // Get insertion position to `i`.
            auto it = dumbSorting
                ? dumbLowerBound(result.begin(), end, item, comp)
                : lowerBound(result.begin(), end, item, comp);

            // Shift items right from the insertion position.
            for (auto jt = end; jt > it; --jt)
                *jt = *(jt - 1);

            *it = item;

            ++end;
        };

    (insert(items), ...);

    return result;
}

constexpr char toAsciiLower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

constexpr std::string_view toStdStringView(const nx::reflect::detail::string_view& str)
{
    return std::string_view(str.data(), (std::string_view::size_type) str.length());
}

constexpr nx::reflect::detail::string_view fromStdStringView(const std::string_view& str)
{
    return nx::reflect::detail::string_view(str.data(), (int) str.length());
}

template<typename T>
bool parseNumber(std::string_view str, T* value)
{
    if (str.empty())
        return false;

    const bool isHex = str.starts_with("0x") || str.starts_with("0X");
    if (isHex)
        str.remove_prefix(2);

    const auto [ptr, ec] = std::from_chars(
        str.data(), str.data() + str.size(),
        *value,
        /* base*/ isHex ? 16 : 10);

    return ec == std::errc() && ptr == str.data() + str.size();
}

} // namespace detail

template<typename Enum>
std::string toString(Enum value)
{
    auto cmp = [](const Item<Enum>& a, const Item<Enum>& b) { return a.value < b.value; };

    constexpr auto items = visitAllItems<Enum>(
        [&cmp](auto&&... items)
        {
            return detail::getSortedItems<Enum>(cmp, /*dumbSorting*/ true, items...);
        });

    const auto it = std::lower_bound(items.begin(), items.end(), Item<Enum>{value, {}}, cmp);
    if (it == items.end() || it->value != value)
        return std::to_string(static_cast<std::underlying_type_t<Enum>>(value));

    return std::string(detail::toStdStringView(it->name));
}

template<typename Enum>
bool isValidEnumValue(Enum value)
{
    auto cmp = [](const Item<Enum>& a, const Item<Enum>& b) { return a.value < b.value; };

    constexpr auto items = visitAllItems<Enum>(
        [&cmp](auto&&... items)
        {
            return detail::getSortedItems<Enum>(cmp, /*dumbSorting*/ true, items...);
        });

    return std::binary_search(items.begin(), items.end(), Item<Enum>{value, {}}, cmp);
}

template<typename Enum>
bool fromString(const std::string_view& str, Enum* value)
{
    static constexpr auto caseInsensitiveLess =
        [](const nx::reflect::detail::string_view& a, const nx::reflect::detail::string_view& b)
        {
            return std::lexicographical_compare(
                a.data(), a.data() + a.length(),
                b.data(), b.data() + b.length(),
                [](char a, char b) { return detail::toAsciiLower(a) < detail::toAsciiLower(b); });
        };

    auto cmp =
        [](const Item<Enum>& a, const Item<Enum>& b)
        {
            return caseInsensitiveLess(a.name, b.name);
        };

    constexpr auto items = visitAllItems<Enum>(
        [&cmp](auto&&... items)
        {
            return detail::getSortedItems<Enum>(cmp, /*dumbSorting*/ false, items...);
        });

    const auto itemStr = detail::fromStdStringView(str);

    const auto it = std::lower_bound(items.begin(), items.end(), Item<Enum>{{}, itemStr}, cmp);
    if (it == items.end() || caseInsensitiveLess(itemStr, it->name))
    {
        // Fallback to parsing an integer.
        std::underlying_type_t<Enum> number;
        if (!detail::parseNumber(str, &number))
            return false;

        *value = static_cast<Enum>(number);
        return true;
    }

    *value = it->value;
    return true;
}

} // namespace nx::reflect::enumeration
