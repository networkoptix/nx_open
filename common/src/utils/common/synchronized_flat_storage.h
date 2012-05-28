#ifndef QN_PROTECTED_STORAGE_H
#define QN_PROTECTED_STORAGE_H

#include <QtCore/QReadWriteLock>
#include <QtCore/QReadLocker>
#include <QtCore/QWriteLocker>

#include "warnings.h"
#include "flat_storage.h"

/**
 * This is a thread-safe container for heap-allocated items that takes ownership
 * of the items inside it.
 */
template<class Key, class T>
class QnSynchronizedFlatStorage: private QnFlatStorage<Key, T> {
    typedef QnFlatStorage<Key, T> base_type;
public:
    QnSynchronizedFlatStorage() {}

    ~QnSynchronizedFlatStorage() {}

    T value(const Key &key) {
        QReadLocker guard(&m_lock);

        return base_type::value(key);
    }

    void insert(const Key &key, T value, bool claimOwnership = true) {
        QWriteLocker guard(&m_lock);

        base_type::insert(key, value, claimOwnership);
    }

    void clear() {
        QWriteLocker guard(&m_lock);

        base_type::clear();
    }

    bool empty() const {
        QReadLocker guard(&m_lock);

        return base_type::empty();
    }

private:
    Q_DISABLE_COPY(QnSynchronizedFlatStorage);

private:
    QReadWriteLock m_lock;
};

#endif // QN_PROTECTED_STORAGE_H
