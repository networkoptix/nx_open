#ifndef QN_HASH_H
#define QN_HASH_H

#include <typeindex>

#include <QtCore/QPoint>
#include <QtCore/QHash>
#include <utils/common/uuid.h>
#include <QtGui/QColor>

inline uint qHash(const QPoint &value, uint seed = 0) {
    using ::qHash;

    return qHash(qMakePair(value.x(), value.y()), seed);
}

inline uint qHash(const QColor &color, uint seed = 0) {
    using ::qHash;

    if(color.isValid()) {
        return qHash(color.rgba(), seed);
    } else {
        return 0;
    }
}

inline uint qHash(const std::type_index &index, uint seed = 0) {
    return qHash(index.hash_code(), seed);
}

#endif // QN_HASH_H

