#include "transaction.h"

#include <atomic>

#include "common/common_module.h"
#include "database/db_manager.h"
#include "utils/common/synctime.h"

#include "utils/common/model_functions.h"


namespace ec2
{

    QAtomicInt qn_abstractTransaction_sequence(1);

    namespace ApiCommand
    {
        QString toString( Value val )
        {
            switch( val )
            {
                case tranSyncRequest:
                    return "tranSyncRequest";
                case tranSyncResponse:
                    return "tranSyncResponse";
                case lockRequest:
                    return "lockRequest";
                case lockResponse:
                    return "lockResponse";
                case unlockRequest:
                    return "unlockRequest";
                case peerAliveInfo:
                    return "peerAliveInfo";

                case testConnection:
                    return "testConnection";
                case connect:
                    return "connect";

                case getResourceTypes:
                    return "getResourceTypes";
                case setResourceStatus:
                    return "setResourceStatus";
                //case setResourceDisabled:
                //    return "setResourceDisabled";
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
                case getFullInfo:
                    return "getFullInfo";

                case saveCamera:
                    return "saveCamera";
                case saveCameras:
                    return "updateCameras";
                case removeCamera:
                    return "removeCamera";
                case getCameras:
                    return "getCameras";
                case getCameraHistoryItems:
                    return "getCameraHistoryItems";
                case addCameraHistoryItem:
                    return "addCameraHistoryItem";

                case getMediaServers:
                    return "getMediaServers";
                case saveMediaServer:
                    return "saveMediaServer";
                case removeMediaServer:
                    return "removeMediaServer";

                case saveUser:
                    return "saveUser";
                case getUsers:
                    return "getUsers";
                case removeUser:
                    return "removeUser";

                case saveBusinessRule:
                    return "saveBusinessRule";
                case removeBusinessRule:
                    return "removeBusinessRule";
                case broadcastBusinessAction:
                    return "broadcastBusinessAction";
                case execBusinessAction:
                    return "execBusinessAction";
                case getBusinessRules:
                    return "getBusinessRules";
                case resetBusinessRules:
                    return "resetBusinessRules";

                case saveLayout:
                    return "saveLayout";
                case saveLayouts:
                    return "addOrUpdateLayouts";
                case getLayouts:
                    return "getLayouts";
                case removeLayout:
                    return "removeLayout";

                case saveVideowall:
                    return "saveVideowall";
                case getVideowalls:
                    return "getVideowalls";
                case removeVideowall:
                    return "removeVideowall";
                case videowallControl:
                    return "videowallControl";

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

                case addLicenses:
                    return "addLicenses";
                case addLicense:
                    return "addLicense";
                case getLicenses:
                    return "getLicenses";

                case getCurrentTime:
                    return "getCurrentTime";

                case getSettings:
                    return "getSettings";
                case saveSettings:
                    return "saveSettings";

                case uploadUpdate:
                    return "uploadUpdate";
                case uploadUpdateResponce:
                    return "uploadUpdateResponce";
                case installUpdate:
                    return "installUpdate";

                case addCameraBookmarkTags:
                    return "addCameraBookmarkTags";
                case getCameraBookmarkTags:
                    return "getCameraBookmarkTags";
                case removeCameraBookmarkTags:
                    return "removeCameraBookmarkTags";

                case runtimeInfoChanged:
                    return "runtimeInfoChanged";

                default:
                    return "unknown " + QString::number((int)val);
            }
        }

        bool isSystem( Value val )
        {
            return  val == lockRequest   ||
                    val == lockResponse  ||
                    val == unlockRequest ||
                    val == tranSyncRequest ||
                    val == tranSyncResponse ||
                    val == runtimeInfoChanged ||
                    val == peerAliveInfo;
        }

    }

    QnAbstractTransaction::QnAbstractTransaction(ApiCommand::Value _command, bool _persistent)
    {
        command = _command;
        persistent = _persistent;
        id.peerID = qnCommon->moduleGUID();
        id.sequence = 0;
        timestamp = 0;
        if (QnTransactionLog::instance()) {
            timestamp = QnTransactionLog::instance()->getTimeStamp();
            id.dbID = QnDbManager::instance()->getID();
        }
        isLocal = false;
    }

    void QnAbstractTransaction::setStartSequence(int value)
    {
        qn_abstractTransaction_sequence = value;
    }

    void QnAbstractTransaction::fillSequence()
    {
        id.sequence = qn_abstractTransaction_sequence.fetchAndAddAcquire(1);
        if (!timestamp)
            timestamp = QnTransactionLog::instance() ? QnTransactionLog::instance()->getTimeStamp() : 0;
    }

    int generateRequestID()
    {
        static std::atomic<int> requestID;
        return ++requestID;
    }

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAbstractTransaction::ID,    (binary)(json),   (peerID)(dbID)(sequence))
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAbstractTransaction,        (binary)(json),   (command)(id)(persistent)(timestamp))
} // namespace ec2

