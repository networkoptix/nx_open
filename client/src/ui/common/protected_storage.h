#ifndef QN_PROTECTED_STORAGE_H
#define QN_PROTECTED_STORAGE_H

#include <QVector>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <utils/common/warnings.h>

/**
 * This is a thread-safe container for head-allocated items that takes ownership
 * of the items inside it.
 */
template<class T>
class ProtectedStorage {
public:
    ProtectedStorage() {}

    virtual ~ProtectedStorage() {
        foreach(T *value, m_storage)
            if(value != NULL)
                delete value;
        m_storage.clear();
    }

    T *get(int index) {
        QReadLocker guard(&m_lock);

        if(index < 0 || index >= m_storage.size())
            return NULL;

        return m_storage[index];
    }

    void set(int index, T *value) {
        if(index < 0) {
            qnWarning("Invalid index '%1'", index);
            return;
        }

        QWriteLocker guard(&m_lock);

        if(index >= m_storage.size())
            m_storage.resize(index + 1);

        T *oldValue = m_storage[index];
        if(oldValue != NULL)
            delete oldValue;

        m_storage[index] = value;
    }

private:
    Q_DISABLE_COPY(ProtectedStorage);

private:
    QReadWriteLock m_lock;
    QVector<T *> m_storage;
};

#endif // QN_PROTECTED_STORAGE_H
