#ifndef QN_PROTECTED_STORAGE_H
#define QN_PROTECTED_STORAGE_H

#include <QtCore/QSet>
#include <QtCore/QVector>
#include <QtCore/QReadWriteLock>
#include <QtCore/QReadLocker>
#include <QtCore/QWriteLocker>

#include "warnings.h"
#include "flat_map.h"

/**
 * This is a thread-safe container for heap-allocated items that takes ownership
 * of the items inside it.
 */
template<class Key, class T>
class QnSynchronizedFlatStorage {
public:
    QnSynchronizedFlatStorage() {}

    virtual ~QnSynchronizedFlatStorage() {
        foreach(T *value, m_owned)
            delete value;
        m_storage.clear();
        m_owned.clear();
    }

    T *value(const Key &key) {
        QReadLocker guard(&m_lock);

        return m_storage.value(key);
    }

    void insert(const Key &key, T *value) {
        QWriteLocker guard(&m_lock);

        m_storage[key] = value;
        if(value != NULL)
            m_owned.insert(value);
    }

private:
    Q_DISABLE_COPY(QnSynchronizedFlatStorage);

private:
    QReadWriteLock m_lock;
    QnFlatMap<Key, T *> m_storage;
    QSet<T *> m_owned;
};

#endif // QN_PROTECTED_STORAGE_H
