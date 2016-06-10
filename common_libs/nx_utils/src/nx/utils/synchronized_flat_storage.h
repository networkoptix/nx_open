#ifndef QN_PROTECTED_STORAGE_H
#define QN_PROTECTED_STORAGE_H

#include <mutex>

#include "flat_storage.h"

/**
 * This is a thread-safe container for heap-allocated items that takes ownership
 * of the items inside it.
 */
template<class Key, class T>
class QnSynchronizedFlatStorage {
    typedef QnFlatStorage<Key, T> base_type;
public:
    QnSynchronizedFlatStorage() {}

    ~QnSynchronizedFlatStorage() {}

    T value(const Key &key) const {
        std::unique_lock<std::mutex> guard(m_mutex);

        return m_storage.value(key);
    }

    QSet<T> values() const {
        std::unique_lock<std::mutex> guard(m_mutex);

        return m_storage.values();
    }

    void insert(const Key &key, T value, bool claimOwnership = true) {
        std::unique_lock<std::mutex> guard(m_mutex);

        m_storage.insert(key, value, claimOwnership);
    }

    void clear() {
        std::unique_lock<std::mutex> guard(m_mutex);

        m_storage.clear();
    }

    bool empty() const {
        std::unique_lock<std::mutex> guard(m_mutex);

        return m_storage.empty();
    }

private:
    Q_DISABLE_COPY(QnSynchronizedFlatStorage);

private:
    /** Mutex that protects all operations. Note that we're not using a 
     * shared/read-write mutex as it is more heavyweight than a regular mutex,
     * and the read operations are extremely fast anyway. */
    mutable std::mutex m_mutex;

    /** Underlying storage. */
    QnFlatStorage<Key, T> m_storage;
};

#endif // QN_PROTECTED_STORAGE_H
