#include "transaction.h"

namespace ec2
{

QnUuid QnAbstractTransaction::m_staticPeerGUID;
qint64 QnAbstractTransaction::m_staticNumber;
QMutex QnAbstractTransaction::m_mutex;

void QnAbstractTransaction::setPeerGuid(const QnUuid& value)
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


#if 1

#include "nx_ec/data/camera_data.h"
static void test()
{
    ec2::ApiCameraData data;
    BinaryStream<QByteArray> stream;
    data.serialize(stream);
    data.deserialize(stream);

    ec2::QnTransaction<ec2::ApiCameraData> tran;
    tran.deserialize(stream);
}
#endif

