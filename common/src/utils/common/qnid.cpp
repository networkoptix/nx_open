#include "qnid.h"

#include <QtCore/QMutex>

static const int FIRST_CUSTOM_ID = INT_MAX / 2;

Q_GLOBAL_STATIC(QMutex, mutex)

QnId QnId::generateSpecialId()
{
    static int m_lastCustomId = FIRST_CUSTOM_ID;

    QMutexLocker locker(mutex());
    return m_lastCustomId < INT_MAX ? ++m_lastCustomId : 0;
}

bool QnId::isSpecial() const
{
    return m_id >= FIRST_CUSTOM_ID;
}
