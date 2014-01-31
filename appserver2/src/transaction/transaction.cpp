#include "transaction.h"

namespace ec2
{

    namespace ApiCommand
    {
        QString toString( Value val )
        {
            switch( val )
            {
                case getResourceTypes:
                    return "getResourceTypes";
                case addCamera:
                    return "addCamera";
                case updateCamera:
                    return "updateCamera";
                case removeCamera:
                    return "removeCamera";
                case getCameras:
                    return "getCameras";
                case saveMediaServer:
                    return "saveMediaServer";
                default:
                    return "unknown";
            }
        }
    }



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

