// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <string>
#include <string_view>

#include <nx/utils/string.h>
#include <nx/reflect/string_conversion.h>

namespace nx::utils::stree {

namespace detail {

template<typename BoundaryType>
class Range
{
public:
    BoundaryType left;
    BoundaryType right;

    Range():
        left(),
        right()
    {
    }

    Range(
        BoundaryType _left,
        BoundaryType _right)
        :
        left(_left),
        right(_right)
    {
    }

    bool operator<(const Range& rhs) const
    {
        return left < rhs.left;
    }

    bool operator>(const Range& rhs) const
    {
        return rhs < *this;
    }

    std::string toString() const
    {
        return buildString(left, "-", right);
    }
};

} // namespace detail

//-------------------------------------------------------------------------------------------------

/**
 * Finds range that contains given value.
 * If multiple ranges contain given value, then any matching range is returned.
 * Both boundaries of a range are inclusive.
 * @param KeyType Type of range boundary.
 */
template<class KeyType, class MappedType>
class RangeMatchContainer
{
    using Range = typename detail::Range<KeyType>;
    using InternalContainerType = std::multimap<Range, MappedType, std::greater<Range>>;

public:
    using iterator = typename InternalContainerType::iterator;
    using const_iterator = typename InternalContainerType::const_iterator;
    using key_type = typename InternalContainerType::key_type;
    using mapped_type = typename InternalContainerType::mapped_type;
    using value_type = typename InternalContainerType::value_type;

    iterator begin()
    {
        return m_container.begin();
    }

    const_iterator begin() const
    {
        return m_container.begin();
    }

    iterator end()
    {
        return m_container.end();
    }

    const_iterator end() const
    {
        return m_container.end();
    }

    const_iterator find(const KeyType& valueToFind) const
    {
        const auto it = m_container.lower_bound(Range(valueToFind, valueToFind));
        if (it == m_container.end())
            return it;
        if (valueToFind > it->first.right)
            return m_container.end();
        return it;
    }

    std::pair<iterator, bool> insert(const value_type& val)
    {
        return m_container.insert(val);
    }

    template<typename KeyTypeRef, typename MappedTypeRef>
    std::pair<iterator, bool> emplace(KeyTypeRef&& key, MappedTypeRef&& mapped)
    {
        const auto iterRange = m_container.equal_range(key);
        for (auto it = iterRange.first; it != iterRange.second; ++it)
        {
            if (it->first.right == key.right)
                return std::make_pair(it, false);
        }

        const auto it = m_container.emplace_hint(
            iterRange.first,
            std::forward<KeyTypeRef>(key),
            std::forward<MappedTypeRef>(mapped));
        return std::make_pair(it, true);
    }

private:
    InternalContainerType m_container;
};

//-------------------------------------------------------------------------------------------------

template<typename Key>
class RangeConverter
{
public:
    using Range = typename detail::Range<Key>;
    using KeyType = Key;
    using SearchValueType = Key;

    static Range convertToKey(const std::string_view& valueStr)
    {
        const auto [values, valueCount] = split_n<2>(valueStr, '-');
        if (valueCount == 0)
            return Range();

        Range range;
        if (!nx::reflect::fromString(values[0], &range.left))
            return Range();

        if (valueCount >= 2)
        {
            if (!nx::reflect::fromString(values[1], &range.right))
                return Range();
        }
        else
        {
            range.right = range.left;
        }

        return range;
    }

    static Key convertToSearchValue(const std::string_view& str)
    {
        Key key;
        if (!nx::reflect::fromString(str, &key))
            return {};
        return key;
    }
};

} // namespace nx::utils::stree
