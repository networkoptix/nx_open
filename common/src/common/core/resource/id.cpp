#include "id.h"

#include <QtCore/QAtomicInt>

static QBasicAtomicInt theIdCounter = Q_BASIC_ATOMIC_INITIALIZER(1);

QnId::QnId()
{
    const int id = theIdCounter.fetchAndAddRelaxed(1); // generate an unique ID
    m_id = QString::number(id);
}
