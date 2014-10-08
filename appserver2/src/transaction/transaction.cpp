#include "transaction.h"

#include <atomic>

#include "common/common_module.h"
#include "database/db_manager.h"
#include "utils/common/synctime.h"

#include "utils/common/model_functions.h"


namespace ec2
{

    namespace ApiCommand
    {
        struct ApiCommandName
        {
            Value command;
            const char* name;
        };

#define REGISTER_COMMAND(x) {x, #x}

        static ApiCommandName COMMAND_NAMES[] =
        {
            REGISTER_COMMAND(tranSyncRequest),
            REGISTER_COMMAND(tranSyncResponse),
            REGISTER_COMMAND(tranSyncDone),
            REGISTER_COMMAND(lockRequest),
            REGISTER_COMMAND(lockResponse),
            REGISTER_COMMAND(unlockRequest),
            REGISTER_COMMAND(peerAliveInfo),

            REGISTER_COMMAND(testConnection),
            REGISTER_COMMAND(connect),

            REGISTER_COMMAND(getResourceTypes),
            REGISTER_COMMAND(setResourceStatus),
            REGISTER_COMMAND(setResourceParams),
            REGISTER_COMMAND(setResourceParam),
            REGISTER_COMMAND(removeResourceParam),
            REGISTER_COMMAND(removeResourceParams),
            REGISTER_COMMAND(getResourceParams),
            REGISTER_COMMAND(saveResource),
            REGISTER_COMMAND(removeResource),
            REGISTER_COMMAND(setPanicMode),
            REGISTER_COMMAND(getFullInfo),

            REGISTER_COMMAND(saveCamera),
            REGISTER_COMMAND(saveCameras),
            REGISTER_COMMAND(removeCamera),
            REGISTER_COMMAND(getCameras),
            REGISTER_COMMAND(getCameraHistoryItems),
            REGISTER_COMMAND(addCameraHistoryItem),
            REGISTER_COMMAND(saveCameraUserAttributes),
            REGISTER_COMMAND(saveCameraUserAttributesList),
            REGISTER_COMMAND(getCameraUserAttributes),
            REGISTER_COMMAND(getCamerasEx),

            REGISTER_COMMAND(addCameraBookmarkTags),
            REGISTER_COMMAND(getCameraBookmarkTags),
            REGISTER_COMMAND(removeCameraBookmarkTags),
            REGISTER_COMMAND(removeCameraHistoryItem),

            REGISTER_COMMAND(getMediaServers),
            REGISTER_COMMAND(saveMediaServer),
            REGISTER_COMMAND(removeMediaServer),
            REGISTER_COMMAND(saveServerUserAttributes),
            REGISTER_COMMAND(saveServerUserAttributesList),
            REGISTER_COMMAND(getServerUserAttributes),
            REGISTER_COMMAND(saveStorage),
            REGISTER_COMMAND(saveStorages),
            REGISTER_COMMAND(removeStorage),
            REGISTER_COMMAND(removeStorages),

            REGISTER_COMMAND(saveUser),
            REGISTER_COMMAND(getUsers),
            REGISTER_COMMAND(removeUser),

            REGISTER_COMMAND(saveBusinessRule),
            REGISTER_COMMAND(removeBusinessRule),
            REGISTER_COMMAND(broadcastBusinessAction),
            REGISTER_COMMAND(execBusinessAction),
            REGISTER_COMMAND(getBusinessRules),
            REGISTER_COMMAND(resetBusinessRules),

            REGISTER_COMMAND(saveLayout),
            REGISTER_COMMAND(saveLayouts),
            REGISTER_COMMAND(getLayouts),
            REGISTER_COMMAND(removeLayout),

            REGISTER_COMMAND(saveVideowall),
            REGISTER_COMMAND(getVideowalls),
            REGISTER_COMMAND(removeVideowall),
            REGISTER_COMMAND(videowallControl),

            REGISTER_COMMAND(listDirectory),
            REGISTER_COMMAND(getStoredFile),
            REGISTER_COMMAND(addStoredFile),
            REGISTER_COMMAND(updateStoredFile),
            REGISTER_COMMAND(removeStoredFile),

            REGISTER_COMMAND(addLicenses),
            REGISTER_COMMAND(addLicense),
            REGISTER_COMMAND(getLicenses),
            REGISTER_COMMAND(removeLicense),

            REGISTER_COMMAND(testEmailSettings),
            REGISTER_COMMAND(sendEmail),

            REGISTER_COMMAND(uploadUpdate),
            REGISTER_COMMAND(uploadUpdateResponce),
            REGISTER_COMMAND(installUpdate),

            REGISTER_COMMAND(addCameraBookmarkTags),
            REGISTER_COMMAND(getCameraBookmarkTags),
            REGISTER_COMMAND(removeCameraBookmarkTags),

            REGISTER_COMMAND(moduleInfo),
            REGISTER_COMMAND(moduleInfoList),

            REGISTER_COMMAND(discoverPeer),
            REGISTER_COMMAND(addDiscoveryInformation),
            REGISTER_COMMAND(removeDiscoveryInformation),

            REGISTER_COMMAND(forcePrimaryTimeServer),
            REGISTER_COMMAND(broadcastPeerSystemTime),
            REGISTER_COMMAND(getCurrentTime),
            REGISTER_COMMAND(changeSystemName),
            REGISTER_COMMAND(getKnownPeersSystemTime),

            REGISTER_COMMAND(runtimeInfoChanged),
            REGISTER_COMMAND(dumpDatabase),
            REGISTER_COMMAND(restoreDatabase),
            REGISTER_COMMAND(updatePersistentSequence),
            REGISTER_COMMAND(markLicenseOverflow)
        };

        QString toString(Value val) 
        {
            for (int i = 0; i < sizeof(COMMAND_NAMES) / sizeof(ApiCommandName); ++i)
            {
                if (COMMAND_NAMES[i].command == val)
                    return QString::fromUtf8(COMMAND_NAMES[i].name);
            }
            return "unknown " + QString::number((int)val);
        }

        Value fromString(const QString& val)
        {
            QByteArray data = val.toUtf8();
            for (int i = 0; i < sizeof(COMMAND_NAMES) / sizeof(ApiCommandName); ++i)
            {
                if (COMMAND_NAMES[i].name == data)
                    return COMMAND_NAMES[i].command;
            }
            return NotDefined;
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
                val == setResourceParam ||
                val == removeResourceParam ||
                val == removeResourceParams ||
                val == setPanicMode ||
                val == saveCamera ||
                val == saveCameraUserAttributes ||
                val == saveCameraUserAttributesList ||
                val == saveCameras ||
                val == removeCamera ||
                val == addCameraHistoryItem ||
                val == removeCameraHistoryItem ||
                val == addCameraBookmarkTags ||
                val == removeCameraBookmarkTags ||
                val == saveMediaServer ||
                val == saveServerUserAttributes ||
                val == saveServerUserAttributesList ||
                val == saveStorage ||
                val == saveStorages ||
                val == removeStorage ||
                val == removeStorages ||
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

    int generateRequestID()
    {
        static std::atomic<int> requestID;
        return ++requestID;
    }

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAbstractTransaction::PersistentInfo,    (json)(ubjson),   QnAbstractTransaction_PERSISTENT_Fields)
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAbstractTransaction,                    (json)(ubjson),   QnAbstractTransaction_Fields)
	
} // namespace ec2

