#pragma once

#include <nx/utils/log/assert.h>

/** QSet-semantics class that counters number of stored instances of key. */
template<typename T>
class QnCounterHash
{
public:
    QnCounterHash() {}

    using key_iterator = typename QHash<T, int>::key_iterator;

    key_iterator keyBegin() const
    {
        return m_hash.keyBegin();
    }

    key_iterator keyEnd() const
    {
        return m_hash.keyEnd();
    }

    /** Returns true if key was absent in hash, false otherwise. */
    bool insert(const T& key)
    {
        auto iter = m_hash.find(key);
        bool isNew = (iter == m_hash.end());
        if (isNew)
            m_hash.insert(key, 1);
        else
            ++iter.value();
        return isNew;
    }

    /** Returns true if key will be removed from hash, false otherwise. */
    bool remove(const T& key)
    {
        auto iter = m_hash.find(key);
        if (iter == m_hash.end())
            return false;

        NX_ASSERT(iter.value() > 0);
        const bool wasLast = (--iter.value() <= 0);
        if (wasLast)
            m_hash.erase(iter);
        return wasLast;
    }

    bool contains(const T& key) const
    {
        return m_hash.contains(key);
    }

    QList<T> keys() const
    {
        return m_hash.keys();
    }

    void clear()
    {
        m_hash.clear();
    }

private:
    QHash<T, int> m_hash;
};
