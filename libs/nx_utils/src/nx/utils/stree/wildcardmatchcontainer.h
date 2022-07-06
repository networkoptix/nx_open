// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/utils/match/wildcard.h>

namespace nx::utils::stree {

/**
 * Associative dictionary. Element key interpreted as wildcard mask.
 * Find methods look up the first element with key satisfying supplied string.
 * NOTE: Performs validation to mask by sequentially checking all elements.
 * So do not use it when high performance is needed.
 * NOTE: Elements are tested in decreasing mask length order.
 */
template<class KeyType, class MappedType>
// requires String<KeyType>
class WildcardMatchContainer
{
    template<typename T>
    struct LongestStringFirstComp
    {
        bool operator()(const T& l, const T& r) const
        {
            return l.size() == r.size() ? l < r : l.size() > r.size();
        }
    };

    using InternalContainerType = std::map<KeyType, MappedType, LongestStringFirstComp<KeyType>>;

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

    void erase(const iterator& pos)
    {
        m_container.erase(pos);
    }

    iterator find(const key_type& key)
    {
        for (typename InternalContainerType::iterator
            it = m_container.begin();
            it != m_container.end();
            ++it)
        {
            if (wildcardMatch(it->first, key))
                return it;
        }

        return m_container.end();
    }

    const_iterator find(const key_type& key) const
    {
        for (typename InternalContainerType::const_iterator
            it = m_container.begin();
            it != m_container.end();
            ++it)
        {
            if (wildcardMatch(it->first, key))
                return it;
        }

        return m_container.end();
    }

    std::pair<iterator, bool> insert(const value_type& val)
    {
        return m_container.insert(val);
    }

    template<typename KeyTypeRef, typename MappedTypeRef>
    std::pair<iterator, bool> emplace(KeyTypeRef&& key, MappedTypeRef&& mapped)
    {
        return m_container.emplace(
            std::forward<KeyTypeRef>(key),
            std::forward<MappedTypeRef>(mapped));
    }

private:
    InternalContainerType m_container;
};

} // namespace nx::utils::stree
