// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_HASH_H
#define QN_HASH_H

#include <typeindex>

#include <QtCore/QHash>
#include <QtCore/QPoint>
#include <QtGui/QColor>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/uuid.h>

inline size_t qHash(const QColor &color, size_t seed = 0) noexcept
{
    using ::qHash;

    if(color.isValid()) {
        return qHash(color.rgba(), seed);
    } else {
        return 0;
    }
}

inline size_t qHash(const std::type_index &index, size_t seed = 0) noexcept
{
    return qHash(index.hash_code(), seed);
}

inline size_t qHash(const QAuthenticator& auth, size_t seed = 0) noexcept
{
    using ::qHash;
    return qHash(auth.user() + auth.password(), seed);
}

#endif // QN_HASH_H
