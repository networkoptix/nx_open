#ifndef QN_HASH_H
#define QN_HASH_H

#include <QtCore/QPoint>
#include <QtCore/QHash>
#include <QtCore/QUuid>
#include <QtGui/QColor>

inline uint qHash(const QPoint &value) {
    using ::qHash;

    return qHash(qMakePair(value.x(), value.y()));
}

inline uint qHash(const QUuid &uuid) {
#if defined(_MSC_VER) && _MSC_VER >= 1600
    static_assert(sizeof(QUuid) == 4 * sizeof(uint), "Size of QUuid is expected to be four times the size of uint.");
#endif

    const uint *u = reinterpret_cast<const uint *>(&uuid);
    return u[0] ^ u[1] ^ u[2] ^ u[3];
}

inline uint qHash(const QColor &color) {
    using ::qHash;

    if(color.isValid()) {
        return qHash(color.rgba());
    } else {
        return 0;
    }
}

#endif // QN_HASH_H

