#include "transaction.h"

#include <atomic>
#include "common/common_module.h"


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
                case saveCamera:
                    return "saveCamera";
                case saveCameras:
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
                case saveMediaServer:
                    return "saveMediaServer";
                case removeMediaServer:
                    return "removeMediaServer";

                case saveUser:
                    return "saveUser";
                case getUserList:
                    return "getUserList";
                case removeUser:
                    return "removeUser";

                case saveBusinessRule:
                    return "saveBusinessRule";
                case removeBusinessRule:
                    return "removeBusinessRule";
                case broadcastBusinessAction:
                    return "broadcastBusinessAction";
                case getBusinessRuleList:
                    return "getBusinessRuleList";

                case saveLayouts:
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



    void QnAbstractTransaction::createNewID()
    {
        id.peerGUID = qnCommon->moduleGUID();
        id.tranGUID = QUuid::createUuid();
    }


    int generateRequestID()
    {
        static std::atomic<int> requestID;
        return ++requestID;
    }

}
