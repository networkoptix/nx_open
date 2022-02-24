// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/utils/time.h>

#include "lru_cache.h"

namespace nx::utils {

/**
 * Cache which limits life-time of its elements.
 * Implemented on top of LruCache.
 * Expired elements are removed by all non-const methods(getValue, put, erase)
 * by removing fixed number of elements (10) at most.
 * Thus, the run-time complexity of those methods is not degraded.
 */
template<
    typename Key, typename Value,
    template<typename...> class Dict = std::unordered_map
>
class TimeOutCache
{
    struct Item
    {
        Value value;
        std::chrono::steady_clock::time_point lastAccessTime;
    };

    static constexpr int kMaxExpiredElementsToRemoveAtOnce = 10;

public:
    TimeOutCache(
        std::chrono::milliseconds expirationPeriod,
        std::size_t maxSize)
        :
        m_expirationPeriod(expirationPeriod),
        m_lruCache(maxSize)
    {
    }

    /**
     * Updates last access time of the element.
     */
    std::optional<std::reference_wrapper<Value>> getValue(const Key& key)
    {
        removeExpiredItems();

        auto item = m_lruCache.getValue(key);
        if (!item)
            return std::nullopt;

        ((Item&) *item).lastAccessTime = nx::utils::monotonicTime();
        return std::reference_wrapper<Value>(((Item&) *item).value);
    }

    std::optional<std::reference_wrapper<const Value>> peekValue(const Key& key) const
    {
        auto item = m_lruCache.peekValue(key);
        if (!item)
            return std::nullopt;
        return std::reference_wrapper<const Value>(((const Item&)*item).value);
    }

    template<class K, class V>
    void put(K&& key, V&& value)
    {
        removeExpiredItems();

        Item item{std::forward<V>(value), nx::utils::monotonicTime()};
        m_lruCache.put(std::forward<K>(key), std::move(item));
    }

    /**
     * NOTE: Last access time of the element is not updated.
     */
    bool contains(const Key& key) const
    {
        const auto& item = m_lruCache.peekValue(key);
        if (!item)
            return false;

        return !isExpired(*item);
    }

    void erase(const Key& key)
    {
        removeExpiredItems();

        m_lruCache.erase(key);
    }

    void clear() noexcept
    {
        m_lruCache.clear();
    }

private:
    bool isExpired(const Item& item) const
    {
        return item.lastAccessTime + m_expirationPeriod <= nx::utils::monotonicTime();
    }

    void removeExpiredItems()
    {
        for (int i = 0;
            i < kMaxExpiredElementsToRemoveAtOnce &&
                !m_lruCache.empty() && isExpired(m_lruCache.peekLru().second);
            ++i)
        {
            m_lruCache.erase(m_lruCache.peekLru().first);
        }
    }

private:
    const std::chrono::milliseconds m_expirationPeriod;
    detail::LruCacheBase<Key, Item, Dict> m_lruCache;
};

} // namespace nx::utils
