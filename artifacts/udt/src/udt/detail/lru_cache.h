// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <list>
#include <memory>
#include <optional>
#include <unordered_map>

namespace udt::detail {

template<class Key, class Value, template<typename...> class Dict>
class LruCacheBase;

/**
 * NOTE: The complexity of get/put/erase operations is the same as that of the Dict type:
 * O(1) if using std::unordered_map, O(logN) when using std::map.
 */
template<
    class Key, class Value,
    template<typename...> class Dict = std::unordered_map
>
class LruCache
{
public:
    LruCache(size_t size): m_impl(size) {}

    /**
     * Returns value with the given key and makes it the most recently used one.
     */
    std::optional<std::reference_wrapper<Value>> getValue(const Key& key)
    {
        return m_impl.getValue(key);
    }

    /**
     * Inserts or replaces value with the given key while removing the least recently used value
     * if the cache if filled.
     * The new/updated value is considered the most recently used one.
     */
    template<class K, class V>
    void put(K&& key, V&& value)
    {
        m_impl.put(std::forward<K>(key), std::forward<V>(value));
    }

    bool contains(const Key& key) const
    {
        return m_impl.contains(key);
    }

    bool empty() const
    {
        return m_impl.empty();
    }

    void erase(const Key& key)
    {
        m_impl.erase(key);
    }

private:
    detail::LruCacheBase<Key, Value, Dict> m_impl;
};

//-------------------------------------------------------------------------------------------------

template<class Key, class Value, template<typename...> class Dict>
class LruCacheBase
{
public:
    LruCacheBase(size_t size): m_maxCacheSize(size) {}

    std::optional<std::reference_wrapper<Value>> getValue(const Key& key)
    {
        auto it = m_cacheMap.find(key);
        if (it == m_cacheMap.end())
            return std::nullopt;

        m_cacheList.splice(m_cacheList.begin(), m_cacheList, it->second);
        return it->second->second;
    }

    /**
     * Returns value (if exists) without making it the most recently used one.
     */
    std::optional<std::reference_wrapper<const Value>> peekValue(const Key& key) const
    {
        auto it = m_cacheMap.find(key);
        if (it == m_cacheMap.end())
            return std::nullopt;
        return it->second->second;
    }

    bool empty() const
    {
        return m_cacheMap.empty();
    }

    /**
     * Returns the LRU element without making it the most recently used one.
     * Warning: behavior is undefined if the cache is empty.
     */
    std::pair<Key, Value>& peekLru()
    {
        return m_cacheList.back();
    }

    template<class K, class V>
    void put(K&& key, V&& value)
    {
        auto it = m_cacheMap.find(key);
        if (it != m_cacheMap.end())
        {
            m_cacheList.splice(m_cacheList.begin(), m_cacheList, it->second);
            // Do not require Value type to provide the move operator.
            std::destroy_at(&m_cacheList.begin()->second);
            // TODO: #akolesnikov replace the following with
            // std::construct_at(&m_cacheList.begin()->second, std::forward<V>(value)); when c++20
            // is fully available here.
            std::allocator<Value> alloc;
            std::allocator_traits<std::allocator<Value>>::construct(
                alloc, &m_cacheList.begin()->second, std::forward<V>(value));
            m_cacheMap[key] = m_cacheList.begin();
        }
        else
        {
            auto& elementIt = m_cacheMap[key];
            m_cacheList.emplace_front(std::forward<K>(key), std::forward<V>(value));
            elementIt = m_cacheList.begin();
        }

        if (m_cacheMap.size() > m_maxCacheSize)
        {
            m_cacheMap.erase(m_cacheList.rbegin()->first);
            m_cacheList.pop_back();
        }
    }

    bool contains(const Key& key) const
    {
        return m_cacheMap.count(key);
    }

    void erase(const Key& key)
    {
        if (!m_cacheMap.count(key))
            return;

        // NOTE: key may be a ref into m_cacheList.
        const IterT listIter = m_cacheMap[key];
        m_cacheMap.erase(key);
        m_cacheList.erase(listIter);
    }

    void clear() noexcept
    {
        m_cacheMap.clear();
        m_cacheList.clear();
    }

private:
    using IterT = typename std::list<std::pair<Key, Value>>::iterator;

    std::list<std::pair<Key, Value>> m_cacheList;
    Dict<Key, IterT> m_cacheMap;
    const size_t m_maxCacheSize;
};

} // namespace udt::detail
