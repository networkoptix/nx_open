#pragma once

#include <algorithm>
#include <map>

#include <boost/optional.hpp>

#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cdb {

/**
 * Data cache to be used by managers to reduce number of data fetch requests to DB.
 * Should support:
 * - data update events.
 * - custom indexes?
 */
template<class KeyType, class CachedType>
class Cache
{
public:
    typedef typename std::map<KeyType, CachedType>::value_type value_type;

    /**
     * @return false if already exists
     */
    bool insert(KeyType key, CachedType value)
    {
        QnMutexLocker lk(&m_mutex);
        return m_data.emplace(std::move(key), std::move(value)).second;
    }

    boost::optional<CachedType> find(const KeyType& key) const
    {
        QnMutexLocker lk(&m_mutex);

        auto it = m_data.find(key);
        if( it == m_data.end() )
            return boost::none;
        return it->second;
    }

    /**
     * @return true if erased something.
     */
    bool erase(const KeyType& key)
    {
        QnMutexLocker lk(&m_mutex);
        return m_data.erase(key) > 0;
    }

    /**
     * Executes updateFunc on item with key.
     * @return True if item found and updated. False otherwise.
     * WARNING: updateFunc is executed with internal mutex locked, so it MUST NOT BLOCK!
     */
    template<class Func>
    bool atomicUpdate(
        const KeyType& key,
        Func updateFunc)
    {
        QnMutexLocker lk(&m_mutex);

        auto it = m_data.find(key);
        if( it == m_data.end() )
            return false;
        updateFunc(it->second);
        return true;
    }

    /**
     * Linear search through the cache.
     */
    template<class Func>
    boost::optional<CachedType> findIf(const Func& func) const
    {
        QnMutexLocker lk(&m_mutex);
        auto it = std::find_if(m_data.begin(), m_data.end(), func);
        if( it == m_data.end() )
            return boost::none;
        return it->second;
    }

    template<class Func, class OutputIterator>
    void copyIf(const Func& func, OutputIterator outIt) const
    {
        QnMutexLocker lk(&m_mutex);
        std::copy_if(m_data.begin(), m_data.end(), outIt, func);
    }

    template<class Func>
    void forEach(const Func& func) const
    {
        QnMutexLocker lk(&m_mutex);
        std::for_each(m_data.begin(), m_data.end(), func);
    }

private:
    mutable QnMutex m_mutex;
    std::map<KeyType, CachedType> m_data;
};

} // namespace cdb
} // namespace nx
