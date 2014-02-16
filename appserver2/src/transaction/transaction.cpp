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
                case getResources:
                    return "getResources";
                case getResource:
                    return "getResource";
                case setResourceStatus:
                    return "setResourceStatus";
                case setResourceDisabled:
                    return "setResourceDisabled";
                case setResourceParams:
                    return "setResourceParams";
                case getResourceParams:
                    return "getResourceParams";
                case saveResource:
                    return "saveResource";
                case removeResource:
                    return "removeResource";
                case setPanicMode:
                    return "setPanicMode";
                case getAllDataList:
                    return "getResourceList";

                case addCamera:
                    return "addCamera";
                case updateCamera:
                    return "updateCamera";
                case updateCameras:
                    return "updateCameras";
                case removeCamera:
                    return "removeCamera";
                case getCameras:
                    return "getCameras";
                case getCameraHistoryList:
                    return "getCameraHistoryList";
                case addCameraHistoryItem:
                    return "addCameraHistoryItem";

                case getMediaServerList:
                    return "getMediaServerList";
                case addMediaServer:
                    return "addMediaServer";
                case updateMediaServer:
                    return "updateMediaServer";
                case removeMediaServer:
                    return "removeMediaServer";

                case addUser:
                    return "addUser";
                case updateUser:
                    return "updateUser";
                case getUserList:
                    return "getUserList";
                case removeUser:
                    return "removeUser";

                case addBusinessRule:
                    return "addBusinessRule";
                case updateBusinessRule:
                    return "updateBusinessRule";
                case removeBusinessRule:
                    return "removeBusinessRule";
                case getBusinessRuleList:
                    return "getBusinessRuleList";

                case addOrUpdateLayouts:
                    return "addOrUpdateLayouts";
                case getLayoutList:
                    return "getLayoutList";
                case removeLayout:
                    return "removeLayout";

                case listDirectory:
                    return "listDirectory";
                case getStoredFile:
                    return "getStoredFile";
                case addStoredFile:
                    return "addStoredFile";
                case updateStoredFile:
                    return "updateStoredFile";
                case removeStoredFile:
                    return "removeStoredFile";

                case getCurrentTime:
                    return "getCurrentTime";

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

    int generateUniqueID()
    {
        return QDateTime::currentMSecsSinceEpoch() - rand();
    }
}
