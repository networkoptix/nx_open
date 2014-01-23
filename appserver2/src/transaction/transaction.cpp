#include "transaction.h"

namespace ec2
{

QUuid QnAbstractTransaction::m_staticPeerGUID;
qint64 QnAbstractTransaction::m_staticNumber;
QMutex QnAbstractTransaction::m_mutex;

void QnAbstractTransaction::setPeerGuid(const QUuid& value)
{
    m_staticPeerGUID = value;
}

void QnAbstractTransaction::setStartNumber(const qint64& value)
{
    m_staticNumber = value;
}

void QnAbstractTransaction::createNewID()
{
    id.peerGUID = m_staticPeerGUID;
    QMutexLocker lock(&m_mutex);
    id.number = ++m_staticNumber;
}

}
