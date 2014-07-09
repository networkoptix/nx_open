#ifndef THREADSAFE_ITEM_STORAGE_H
#define THREADSAFE_ITEM_STORAGE_H

#include <QtCore/QMutex>

#include <utils/common/warnings.h>

template <class T> class QnThreadsafeItemStorage;

/**
 * @brief The QnThreadsafeItemStorageNotifier class       Notification interface for the QnThreadsafeItemStorage class.
 */
template <class T>
class QnThreadsafeItemStorageNotifier {
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
class QnThreadsafeItemStorage {
public:
    typedef QList<T> ItemList;
    typedef QHash<QUuid, T> ItemMap;

    QnThreadsafeItemStorage(QMutex *mutex, QnThreadsafeItemStorageNotifier<T>* notifier): 
        m_mutex(mutex),
        m_notifier(notifier)
    {}

    void setItems(const ItemList &items) { 
        ItemMap map;
        foreach(const T &item, items)
            map[item.uuid] = item;
        setItems(map);
    }

    void setItems(const ItemMap &items) {
        QMutexLocker locker(m_mutex);
        setItemsUnderLock(items);
    }

    void setItemsUnderLock(QnThreadsafeItemStorage<T>* storage) {
        setItemsUnderLock(storage->m_itemByUuid);
    }

    ItemMap getItems() const { 
        QMutexLocker locker(m_mutex);
        return m_itemByUuid; 
    }

    T getItem(const QUuid &uuid) const {
        QMutexLocker locker(m_mutex);
        return m_itemByUuid.value(uuid);
    }

    bool hasItem(const QUuid &uuid) const {
        QMutexLocker locker(m_mutex);
        return m_itemByUuid.contains(uuid);
    }

    void addItem(const T &item) {
        QMutexLocker locker(m_mutex);
        addItemUnderLock(item);
    }

    void removeItem(const T &item) {
        removeItem(item.uuid);
    }

    void removeItem(const QUuid &uuid) {
        QMutexLocker locker(m_mutex);
        removeItemUnderLock(uuid);
    }

    void updateItem(const QUuid &uuid, const T &item) {
        QMutexLocker locker(m_mutex);
        updateItemUnderLock(uuid, item);
    }

private:
    void setItemsUnderLock(const ItemMap &items) {
        foreach(const T &item, m_itemByUuid)
            if(!items.contains(item.uuid))
                removeItemUnderLock(item.uuid);

        foreach(const T &item, items) {
            if(m_itemByUuid.contains(item.uuid)) {
                updateItemUnderLock(item.uuid, item);
            } else {
                addItemUnderLock(item);
            }
        }
    }

    void addItemUnderLock(const T &item) {
        if(m_itemByUuid.contains(item.uuid)) {
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

    void updateItemUnderLock(const QUuid &uuid, const T &item) {
        typename ItemMap::iterator pos = m_itemByUuid.find(uuid);
        if(pos == m_itemByUuid.end()) {
            qnWarning("There is no item with UUID %1.", uuid.toString());
            return;
        }

        if(*pos == item)
            return;

        *pos = item;

        if (!m_notifier)
            return;
        m_mutex->unlock();
        m_notifier->storedItemChanged(item);
        m_mutex->lock();
    }

    void removeItemUnderLock(const QUuid &uuid) {
        typename ItemMap::iterator pos = m_itemByUuid.find(uuid);
        if(pos == m_itemByUuid.end())
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
    QMutex* m_mutex;
    QnThreadsafeItemStorageNotifier<T> *m_notifier;
};

#endif // THREADSAFE_ITEM_STORAGE_H
