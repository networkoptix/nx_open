#pragma once

#include <map>

#include "nx/utils/log/assert.h"
#include "nx/utils/thread/mutex.h"

namespace nx {
namespace utils {

template<typename Key, typename Mapped, typename Comp = std::less<Key>>
class SafeMap
{
    typedef std::map<Key, Mapped, Comp> InternalMap;

public:
    typedef typename InternalMap::const_iterator const_iterator;
    typedef typename InternalMap::iterator iterator;
    typedef typename InternalMap::value_type value_type;

    /**
     * Erases element with specified key from dictionary and moves its value to the caller.
     * NOTE: Does not test whether requested element actually present in the dictionary.
     */
    Mapped take(const Key& key)
    {
        QnMutexLocker lk(&m_mutex);
        auto it = m_data.find(key);
        NX_ASSERT(it != m_data.end());
        auto result = std::move(it->second);
        m_data.erase(it);
        return result;
    }

    template<typename ValueType, typename OnNewElementFunc>
    std::pair<iterator, bool> insert(ValueType&& value, OnNewElementFunc onNewElementFunc)
    {
        QnMutexLocker lk(&m_mutex);

        const auto iteratorAndInsertionFlag = m_data.insert(std::forward<ValueType>(value));
        if (iteratorAndInsertionFlag.second)
            onNewElementFunc(iteratorAndInsertionFlag.first);
        return iteratorAndInsertionFlag;
    }

    template<typename ValueType>
    std::pair<iterator, bool> insert(ValueType&& value)
    {
        QnMutexLocker lk(&m_mutex);
        return m_data.insert(std::forward<ValueType>(value));
    }

    template<typename KeyType, typename ValueType, typename OnNewElementFunc>
    std::pair<iterator, bool> emplace(
        KeyType&& key,
        ValueType&& value,
        OnNewElementFunc onNewElementFunc)
    {
        QnMutexLocker lk(&m_mutex);

        const auto iteratorAndInsertionFlag = m_data.emplace(
            std::forward<KeyType>(key),
            std::forward<ValueType>(value));
        if (iteratorAndInsertionFlag.second)
            onNewElementFunc(iteratorAndInsertionFlag.first);
        return iteratorAndInsertionFlag;
    }

    template<typename KeyType, typename ValueType>
    std::pair<iterator, bool> emplace(KeyType&& key, ValueType&& value)
    {
        QnMutexLocker lk(&m_mutex);
        return m_data.emplace(std::forward<KeyType>(key), std::forward<ValueType>(value));
    }

private:
    mutable QnMutex m_mutex;
    std::map<Key, Mapped, Comp> m_data;
};

} // namespace nx
} // namespace utils
