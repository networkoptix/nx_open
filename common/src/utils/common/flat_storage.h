#ifndef QN_FLAT_STORAGE_H
#define QN_FLAT_STORAGE_H

#include <type_traits> /* For std::is_pointer. */

#include <QtCore/QSet>
#include <QtCore/QtAlgorithms> /* For qDeleteAll. */

#include "flat_map.h"

/**
 * Map for heap-allocated items that takes ownership of the items inside it.
 * Uses integer keys that allows it to use arrays for faster lookup.
 */
template<class Key, class T>
class QnFlatStorage: private QnFlatMap<Key, T> {
    typedef QnFlatMap<Key, T> base_type;

    static_assert(std::is_pointer<T>::value, "Stored type must be a pointer.");

public:
    QnFlatStorage() {}

    ~QnFlatStorage() {
        qDeleteAll(m_owned);
    }

    using base_type::empty;
    using base_type::clear;
    using base_type::value;

    QList<T> values() const {
        return m_owned.toList();
    }

    void insert(const Key &key, T value, bool claimOwnership = true) {
        base_type::insert(key, value);
        if(value != NULL && claimOwnership)
            m_owned.insert(value);
    }

private:
    Q_DISABLE_COPY(QnFlatStorage);

private:
    QSet<T> m_owned;
};

#endif // QN_FLAT_STORAGE_H
