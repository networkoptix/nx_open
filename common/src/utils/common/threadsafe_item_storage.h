#pragma once

#include <nx/utils/thread/mutex.h>

#include <utils/common/warnings.h>

template <class T> class QnThreadsafeItemStorage;

/**
 * @brief The QnThreadsafeItemStorageNotifier class       Notification interface for the QnThreadsafeItemStorage class.
 */
template <class T>
class QnThreadsafeItemStorageNotifier
{
protected:
    void setItemsUnderLockInternal(QnThreadsafeItemStorage<T>* target, QnThreadsafeItemStorage<T>* items)
    {
        target->setItemsUnderLock(items);
    }
protected:
    virtual void storedItemAdded(const T &item) = 0;
    virtual void storedItemRemoved(const T &item) = 0;
    virtual void storedItemChanged(const T &item) = 0;

private:
    friend class QnThreadsafeItemStorage<T>;
};

/**
 * @brief The QnThreadsafeItemStorage class               Utility class for storing and handling set of custom items.
 *                                                        Item class T should have "uuid" field and it must be unique.
 */
template <class T>
class QnThreadsafeItemStorage
{
    friend class QnThreadsafeItemStorageNotifier<T>;
public:
    typedef QList<T> ItemList;
    typedef QHash<QnUuid, T> ItemMap;

    QnThreadsafeItemStorage(QnMutex *mutex, QnThreadsafeItemStorageNotifier<T>* notifier):
        m_mutex(mutex),
        m_notifier(notifier)
    {
    }

    void setItems(const ItemList &items)
    {
        ItemMap map;
        foreach(const T &item, items)
            map[item.uuid] = item;
        setItems(map);
    }

    void setItems(const ItemMap &items)
    {
        QnMutexLocker locker(m_mutex);
        setItemsUnderLock(items);
    }

    ItemMap getItems() const
    {
        QnMutexLocker locker(m_mutex);
        return m_itemByUuid;
    }

    T getItem(const QnUuid &uuid) const
    {
        QnMutexLocker locker(m_mutex);
        return m_itemByUuid.value(uuid);
    }

    bool hasItem(const QnUuid &uuid) const
    {
        QnMutexLocker locker(m_mutex);
        return m_itemByUuid.contains(uuid);
    }

    void addItem(const T &item)
    {
        QnMutexLocker locker(m_mutex);
        addItemUnderLock(item);
    }

    void removeItem(const T &item)
    {
        removeItem(item.uuid);
    }

    void removeItem(const QnUuid &uuid)
    {
        QnMutexLocker locker(m_mutex);
        removeItemUnderLock(uuid);
    }

    void updateItem(const T &item)
    {
        QnMutexLocker locker(m_mutex);
        updateItemUnderLock(item);
    }

    void addOrUpdateItem(const T &item)
    {
        QnMutexLocker locker(m_mutex);
        if (m_itemByUuid.contains(item.uuid))
            updateItemUnderLock(item);
        else
            addItemUnderLock(item);
    }

private:
    void setItemsUnderLock(QnThreadsafeItemStorage<T>* storage)
    {
        setItemsUnderLock(storage->m_itemByUuid);
    }

    void setItemsUnderLock(const ItemMap &items)
    {
        /* Here we must copy the list. */
        for (const T &item: m_itemByUuid.values())
            if (!items.contains(item.uuid))
                removeItemUnderLock(item.uuid);

        for (const T &item: items)
            if (m_itemByUuid.contains(item.uuid))
                updateItemUnderLock(item);
            else
                addItemUnderLock(item);
    }

    void addItemUnderLock(const T &item)
    {
        if (m_itemByUuid.contains(item.uuid))
        {
            qnWarning("Item with UUID %1 is already present.", item.uuid.toString());
            return;
        }

        m_itemByUuid[item.uuid] = item;

        if (!m_notifier)
            return;

        m_mutex->unlock();
        m_notifier->storedItemAdded(item);
        m_mutex->lock();
    }

    void updateItemUnderLock(const T &item)
    {
        typename ItemMap::iterator pos = m_itemByUuid.find(item.uuid);
        if (pos == m_itemByUuid.end())
        {
            qnWarning("There is no item with UUID %1.", item.uuid.toString());
            return;
        }

        if (*pos == item)
            return;

        *pos = item;

        if (!m_notifier)
            return;
        m_mutex->unlock();
        m_notifier->storedItemChanged(item);
        m_mutex->lock();
    }

    void removeItemUnderLock(const QnUuid &uuid)
    {
        typename ItemMap::iterator pos = m_itemByUuid.find(uuid);
        if (pos == m_itemByUuid.end())
            return;

        T item = *pos;
        m_itemByUuid.erase(pos);

        if (!m_notifier)
            return;

        m_mutex->unlock();
        m_notifier->storedItemRemoved(item);
        m_mutex->lock();
    }

private:
    ItemMap m_itemByUuid;
    QnMutex* m_mutex;
    QnThreadsafeItemStorageNotifier<T> *m_notifier;
};
