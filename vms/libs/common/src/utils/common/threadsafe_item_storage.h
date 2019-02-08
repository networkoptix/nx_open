#pragma once

#include <nx/utils/thread/mutex.h>

#include <utils/common/warnings.h>
#include <utils/common/functional.h>

template <class T> class QnThreadsafeItemStorage;

/**
 * @brief The QnThreadsafeItemStorageNotifier class       Notification interface for the QnThreadsafeItemStorage class.
 */
template <class T>
class QnThreadsafeItemStorageNotifier
{
protected:
    void setItemsUnderLockInternal(
        QnThreadsafeItemStorage<T>* target,
        QnThreadsafeItemStorage<T>* items,
        Qn::NotifierList& notifiers)
    {
        target->setItemsUnderLock(items, notifiers);
    }
protected:
    virtual Qn::Notifier storedItemAdded(const T &item) = 0;
    virtual Qn::Notifier storedItemRemoved(const T &item) = 0;
    virtual Qn::Notifier storedItemChanged(const T& /*item*/)
    {
        return Qn::Notifier();
    };
    virtual Qn::Notifier storedItemChanged(const T& /*item*/, const T& /*oldItem*/)
    {
        return Qn::Notifier();
    };

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
        for (const T &item : items)
            map[item.uuid] = item;
        setItems(map);
    }

    void setItems(const ItemMap &items)
    {
        Qn::NotifierList notifiers;
        {
            QnMutexLocker locker(m_mutex);
            setItemsUnderLock(items, notifiers);
        }
        notify(notifiers);
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
        Qn::NotifierList notifiers;
        {
            QnMutexLocker locker(m_mutex);
            addItemUnderLock(item, notifiers);
        }
        notify(notifiers);
    }


    void removeItem(const T &item)
    {
        removeItem(item.uuid);
    }

    void removeItem(const QnUuid &uuid)
    {
        Qn::NotifierList notifiers;
        {
            QnMutexLocker locker(m_mutex);
            removeItemUnderLock(uuid, notifiers);
        }
        notify(notifiers);
    }

    void updateItem(const T &item)
    {
        Qn::NotifierList notifiers;
        {
            QnMutexLocker locker(m_mutex);
            updateItemUnderLock(item, notifiers);
        }
        notify(notifiers);
    }

    void addOrUpdateItem(const T &item)
    {
        Qn::NotifierList notifiers;
        {
            QnMutexLocker locker(m_mutex);
            if (m_itemByUuid.contains(item.uuid))
                updateItemUnderLock(item, notifiers);
            else
                addItemUnderLock(item, notifiers);
        }
        notify(notifiers);
    }

private:
    void notify(const Qn::NotifierList& notifiers)
    {
        for (auto notifier : notifiers)
            notifier();
    }

    void setItemsUnderLock(QnThreadsafeItemStorage<T>* storage, Qn::NotifierList& notifiers)
    {
        setItemsUnderLock(storage->m_itemByUuid, notifiers);
    }

    void setItemsUnderLock(const ItemMap &items, Qn::NotifierList& notifiers)
    {
        /* Here we must copy the list. */
        for (const T &item : m_itemByUuid.values())
        {
            if (!items.contains(item.uuid))
                removeItemUnderLock(item.uuid, notifiers);
        }

        for (const T &item : items)
        {
            if (m_itemByUuid.contains(item.uuid))
                updateItemUnderLock(item, notifiers);
            else
                addItemUnderLock(item, notifiers);
        }
    }

    void addItemUnderLock(const T &item, Qn::NotifierList& notifiers)
    {
        if (m_itemByUuid.contains(item.uuid))
        {
            qnWarning("Item with UUID %1 is already present.", item.uuid.toString());
            return;
        }

        m_itemByUuid[item.uuid] = item;

        if (m_notifier)
            notifiers << m_notifier->storedItemAdded(item);
    }

    void updateItemUnderLock(const T &item, Qn::NotifierList& notifiers)
    {
        typename ItemMap::iterator pos = m_itemByUuid.find(item.uuid);
        if (pos == m_itemByUuid.end())
        {
            qnWarning("There is no item with UUID %1.", item.uuid.toString());
            return;
        }

        if (*pos == item)
            return;

        T oldItem = *pos;
        *pos = item;

        if (m_notifier)
        {
            auto simple = m_notifier->storedItemChanged(item);
            if (simple)
                notifiers << simple;
            auto advanced = m_notifier->storedItemChanged(item, oldItem);
            if (advanced)
                notifiers << advanced;
        }
    }

    void removeItemUnderLock(const QnUuid &uuid, Qn::NotifierList& notifiers)
    {
        typename ItemMap::iterator pos = m_itemByUuid.find(uuid);
        if (pos == m_itemByUuid.end())
            return;

        T item = *pos;
        m_itemByUuid.erase(pos);

        if (m_notifier)
            notifiers << m_notifier->storedItemRemoved(item);
    }

private:
    ItemMap m_itemByUuid;
    QnMutex* m_mutex;
    QnThreadsafeItemStorageNotifier<T>* m_notifier;
};
