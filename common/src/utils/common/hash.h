#ifndef QN_HASH_H
#define QN_HASH_H

#include <QPoint>
#include <QHash>

inline uint qHash(const QPoint &value) {
    using ::qHash;

    return qHash(qMakePair(value.x(), value.y()));
}

#endif // QN_HASH_H