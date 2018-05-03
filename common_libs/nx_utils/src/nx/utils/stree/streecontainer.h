#pragma once

#include <map>

namespace nx {
namespace utils {
namespace stree {

template <class KeyType, class MappedType>
class EqualMatchContainer: public std::map<KeyType, MappedType>
{
};

/**
 * find method selects element with min key greater than searched one.
 */
template<class KeyType, class MappedType, class Predicate>
class ConditionMatchContainer
{
    using InternalContainerType = std::map<KeyType, MappedType, Predicate>;

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
        return m_container.upper_bound(key);
    }

    const_iterator find(const key_type& key) const
    {
        return m_container.upper_bound(key);
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

template <class KeyType, class MappedType>
class MinGreaterMatchContainer:
    public ConditionMatchContainer<KeyType, MappedType, std::less<KeyType>>
{
};

template <class KeyType, class MappedType>
class MaxLesserMatchContainer:
    public ConditionMatchContainer<KeyType, MappedType, std::greater<KeyType>>
{
};

} // namespace stree
} // namespace utils
} // namespace nx
