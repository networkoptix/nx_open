#pragma once

#include <memory>
#include <set>

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/log/assert.h>

template<class KeyType, class MappedType>
class QnGeneralAttributePool
{
public:
    class ScopedLock
    {
    public:
        ScopedLock(QnGeneralAttributePool* pool, const KeyType& key):
            m_pool(pool),
            m_key(key),
            m_lockedElement(pool->lock(key))
        {
        }

        ~ScopedLock()
        {
            m_lockedElement = nullptr;
            m_pool->unlock(m_key);
        }

        MappedType* operator->() { return m_lockedElement; }
        const MappedType* operator->() const { return m_lockedElement; }

        MappedType& operator*() { return *m_lockedElement; }
        const MappedType& operator*() const { return *m_lockedElement; }

    private:
        QnGeneralAttributePool* m_pool;
        const KeyType m_key;
        MappedType* m_lockedElement;
    };

    QnGeneralAttributePool():
        m_lockAllowed(true)
    {
    }

    template<class T>
    void setElementInitializer(T handler)
    {
        m_customInitializer = std::move(handler);
    }

    MappedType get(const KeyType& key)
    {
        ScopedLock lk(this, key);
        return *lk;
    }

    /**
     * Blocks until all locked items have been unlocked.
     */
    void clear()
    {
        clear([]() {});
    }

protected:
    template<typename OnStartedWaitingForUnlock>
    void clear(OnStartedWaitingForUnlock onStartedWaitingForUnlock)
    {
        QnMutexLocker lk(&m_mutex);

        if (!m_lockedKeys.empty())
            onStartedWaitingForUnlock();

        while (!m_lockedKeys.empty())
        {
            m_lockAllowed = false;
            m_cond.wait(lk.mutex());
            m_lockAllowed = true;
        }

        m_elements.clear();
        m_cond.wakeAll();
    }

private:
    struct DataCtx
    {
        bool locked = false;
        MappedType mapped = MappedType();
    };

    std::map<KeyType, std::unique_ptr<DataCtx>> m_elements;
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    std::function<void(const KeyType&, MappedType&)> m_customInitializer;
    std::set<KeyType> m_lockedKeys;
    bool m_lockAllowed;

    /**
     * If element already locked, blocks till element has been unlocked.
     * @note if requested element does not exist, it is created.
     */
    MappedType* lock(const KeyType& key)
    {
        QnMutexLocker lk(&m_mutex);

        for (;;)
        {
            auto p = m_elements.emplace(std::make_pair(key, nullptr));
            if (p.second)
            {
                p.first->second.reset(new DataCtx());
                if (m_customInitializer)
                    m_customInitializer(key, p.first->second->mapped);
            }

            if (m_lockAllowed && !p.first->second->locked)
            {
                p.first->second->locked = true;
                m_lockedKeys.insert(key);
                return &p.first->second->mapped;
            }

            m_cond.wait(lk.mutex());
        }
    }

    void unlock(const KeyType& key)
    {
        QnMutexLocker lk(&m_mutex);
        auto it = m_elements.find(key);
        NX_ASSERT(it != m_elements.end());
        NX_ASSERT(it->second->locked);
        it->second->locked = false;
        m_lockedKeys.erase(key);
        m_cond.wakeAll();
    }
};

template<class ResourcePtrType>
QList<QnUuid> idListFromResList(const QList<ResourcePtrType>& resList)
{
    QList<QnUuid> idList;
    idList.reserve(resList.size());
    for (const ResourcePtrType& resPtr : resList)
        idList.push_back(resPtr->getId());
    return idList;
}
