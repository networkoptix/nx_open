#include "transaction.h"

#include <atomic>


namespace ec2
{

    namespace ApiCommand
    {
        QString toString( Value val )
        {
            switch( val )
            {
                case testConnection:
                    return "testConnection";
                case connect:
                    return "connect";

                case getResourceTypes:
                    return "getResourceTypes";
                case setResourceStatus:
                    return "setResourceStatus";

                case addCamera:
                    return "addCamera";
                case updateCamera:
                    return "updateCamera";
                case removeCamera:
                    return "removeCamera";
                case getCameras:
                    return "getCameras";
                case getCameraHistoryList:
                    return "getCameraHistoryList";

                case getMediaServerList:
                    return "getMediaServerList";
                case addMediaServer:
                    return "addMediaServer";
                case updateMediaServer:
                    return "updateMediaServer";

                case getUserList:
                    return "getUserList";
                case getBusinessRuleList:
                    return "getBusinessRuleList";

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


    int generateRequestID()
    {
        static std::atomic<int> requestID;
        return ++requestID;
    }
}
