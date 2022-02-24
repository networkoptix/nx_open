// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits> /* For std::is_pointer. */

#include <QtCore/QSet>

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
        for(T value: m_owned)
            delete value;
    }

    using base_type::empty;
    using base_type::clear;
    using base_type::value;

    const QSet<T> &values() const {
        return m_owned;
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
