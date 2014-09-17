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
                case tranSyncDone:
                    return "tranSyncDone";
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
                case removeLicense:
                    return "removeLicense";

                case testEmailSettings:
                    return "testEmailSettings";
                case sendEmail:
                    return "sendEmail";

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

                case moduleInfo:
                    return "moduleInfo";
                case moduleInfoList:
                    return "moduleInfoList";

                case discoverPeer:
                    return "discoverPeer";
                case addDiscoveryInformation:
                    return "addDiscoveryInformation";
                case removeDiscoveryInformation:
                    return "removeDiscoveryInformation";

                case forcePrimaryTimeServer:
                    return "forcePrimaryTimeServer";
                case broadcastPeerSystemTime:
                    return "broadcastPeerSystemTime";
                case getCurrentTime:
                    return "getCurrentTime";
                case changeSystemName:
                    return "changeSystemName";
                case getKnownPeersSystemTime:
                    return "getKnownPeersSystemTime";

                case runtimeInfoChanged:
                    return "runtimeInfoChanged";
                case dumpDatabase:
                    return "dumpDatabase";
                case restoreDatabase:
                    return "restoreDatabase";
                case updatePersistentSequence:
                    return "updatePersistentSequence";
                case markLicenseOverflow:
                    return "markLicenseOverflow";
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
                    val == peerAliveInfo ||
                    val == broadcastPeerSystemTime ||
                    val == tranSyncDone;
        }

        bool isPersistent( Value val )
        {
            return  val == saveResource   ||
                val == removeResource  ||
                val == setResourceStatus ||
                val == setResourceParams ||
                val == setPanicMode ||
                val == saveCamera ||
                val == saveCameras ||
                val == removeCamera ||
                val == addCameraHistoryItem ||
                val == addCameraBookmarkTags ||
                val == removeCameraBookmarkTags ||
                val == saveMediaServer ||
                val == removeMediaServer ||
                val == saveUser ||
                val == removeUser ||
                val == saveLayout ||
                val == saveLayouts ||
                val == removeLayout ||
                val == saveVideowall ||
                val == removeVideowall ||
                val == saveBusinessRule ||
                val == removeBusinessRule ||
                val == resetBusinessRules ||
                val == addStoredFile ||
                val == updateStoredFile ||
                val == removeStoredFile ||
                val == addDiscoveryInformation ||
                val == removeDiscoveryInformation ||
                val == addLicense ||
                val == addLicenses ||
                val == removeLicense || 
                val == restoreDatabase ||
                val == markLicenseOverflow;
        }

    }


    void QnAbstractTransaction::setStartSequence(int value)
    {
        qn_abstractTransaction_sequence = value;
    }

    void QnAbstractTransaction::fillPersistentInfo()
    {
        if (QnDbManager::instance() && persistentInfo.isNull()) {
            if (!isLocal)
                persistentInfo.sequence = qn_abstractTransaction_sequence.fetchAndAddAcquire(1);
            persistentInfo.dbID = QnDbManager::instance()->getID();
            persistentInfo.timestamp = QnTransactionLog::instance()->getTimeStamp();
        }
    }

    void QnAbstractTransaction::cancel()
    {
        if (persistentInfo.sequence)
            qn_abstractTransaction_sequence.fetchAndAddAcquire(-1);
    }

    int generateRequestID()
    {
        static std::atomic<int> requestID;
        return ++requestID;
    }

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAbstractTransaction::PersistentInfo,    (json)(ubjson),   QnAbstractTransaction_PERSISTENT_Fields)
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAbstractTransaction,                    (json)(ubjson),   QnAbstractTransaction_Fields)
	
} // namespace ec2

