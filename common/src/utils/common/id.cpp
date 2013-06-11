#include "id.h"

#include <QtCore/QMutex>

namespace {
    static const int firstCustomId = INT_MAX / 2;
    static int lastCustomId = firstCustomId;
    Q_GLOBAL_STATIC(QMutex, mutex)
}

QnId QnId::generateSpecialId()
{
    QMutexLocker locker(mutex());
    return lastCustomId < INT_MAX ? ++lastCustomId : 0;
}

bool QnId::isSpecial() const {
    return m_id >= firstCustomId;
}
