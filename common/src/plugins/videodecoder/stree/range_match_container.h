/**********************************************************
* Aug 18, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <map>

#include <QtCore/QString>


namespace stree {

namespace detail {
template<typename BoundaryType>
class Range
{
public:
    BoundaryType left;
    BoundaryType right;

    Range()
    :
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
};
}   // namespace detail

/** Finds range that contains given value.
    If multiple ranges contain given value, then any matching range is returned.
    Both boundaries of a range are inclusive.

    \param KeyType Type of range boundary
*/
template<class KeyType, class MappedType>
class RangeMatchContainer
{
    typedef typename detail::Range<KeyType> Range;
    typedef std::multimap<Range, MappedType, std::greater<Range>> InternalContainerType;

public:
    typedef typename InternalContainerType::iterator iterator;
    typedef typename InternalContainerType::const_iterator const_iterator;
    typedef typename InternalContainerType::key_type key_type;
    typedef typename InternalContainerType::mapped_type mapped_type;
    typedef typename InternalContainerType::value_type value_type;

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

    //void erase(const iterator& pos)
    //{
    //    m_container.erase(pos);
    //}

    //iterator find(const key_type& key)
    //{
    //    return m_container.upper_bound(key);
    //}

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
        const auto it = m_container.emplace(
            std::forward<KeyTypeRef>(key),
            std::forward<MappedTypeRef>(mapped));
        return std::make_pair(it, true);
    }

private:
    InternalContainerType m_container;
};

template<typename Key>
class RangeConverter
{
public:
    typedef typename detail::Range<Key> Range;
    typedef Key KeyType;
    typedef Key SearchValueType;

    static Range convertToKey(const QVariant& value)
    {
        const auto& values = value.toString().split(QLatin1Char('-'));
        if (values.empty())
            return Range();
        Range range;
        range.left = QVariant(values[0]).value<Key>();
        if (values.size() >= 2)
            range.right = QVariant(values[1]).value<Key>();
        else
            range.right = range.left;
        return range;
    }

    static Key convertToSearchValue(const QVariant& value)
    {
        return value.value<Key>();
    }
};

template<typename Key>
QString toPrintable(const typename detail::Range<Key>& val)
{
    return QString::fromLatin1("%1-%2").arg(val.left).arg(val.right);
}

}   // namespace stree
