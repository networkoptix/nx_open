#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <vector>

#ifndef QN_NO_QT
#include <QtCore/QString>
#include "nx_ec/ec_api.h"
#endif

#include "utils/serialization/binary.h"
#include "utils/serialization/json.h"
#include "utils/serialization/xml.h"
#include "utils/serialization/csv.h"
#include "utils/serialization/ubjson.h"
#include "common/common_module.h"

namespace ec2
{

struct NotDefinedApiData {};

#define TRANSACTION_DESCRIPTOR_LIST(APPLY)  \
APPLY(1, tranSyncRequest, ApiSyncRequestData, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) /* trigger notification*/ \
APPLY(2, tranSyncResponse, QnTranStateResponse, \
                       false, /* persistent*/ \
                       false,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) /* trigger notification*/ \
APPLY(3, lockRequest, ApiLockData, \
                       false, /* persistent*/ \
                       true, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) /* trigger notification*/ \
APPLY(4, lockResponse, ApiLockData, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) /* trigger notification*/ \
APPLY(5, unlockRequest, ApiLockData, \
                       false, /* persistent*/ \
                       true, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) /* trigger notification*/ \
APPLY(6, peerAliveInfo, ApiPeerAliveData, \
                       false, /* persistent*/ \
                       true, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) /* trigger notification*/ \
APPLY(7, tranSyncDone, ApiTranSyncDoneData, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) /* trigger notification*/ \
APPLY(100, testConnection, ApiLoginData, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) /* trigger notification*/ \
APPLY(101, connect, ApiLoginData, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) /* trigger notification*/ \
APPLY(102, openReverseConnection, ApiReverseConnectionData, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       [](const QnTransaction<ApiReverseConnectionData> &tran, const NotificationParams &notificationParams) \
                       { \
                            NX_ASSERT(tran.command == ApiCommand::openReverseConnection); \
                            emit notificationParams.ecConnection->reverseConnectionRequested(tran.params); \
                       })  /* trigger notification*/ \
APPLY(201, removeResource, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper) /* trigger notification*/ \
APPLY(202, setResourceStatus, ApiResourceStatusData, \
                       true,  /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdRfc4122Helper(), /* getHash*/ \
                       ResourceNotificationManagerHelper()) \
APPLY(204, setResourceParams, ApiResourceParamWithRefDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper()) \
APPLY(203, getResourceParams, ApiResourceParamWithRefDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper()) \
APPLY(205, getResourceTypes, ApiResourceTypeDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) \
APPLY(206, getFullInfo, ApiFullInfoData, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       [] (const QnTransaction<ApiFullInfoData> & tran, const NotificationParams &notificationParams) \
                       { \
                           emit notificationParams.ecConnection->initNotification(tran.params); \
                           for(const ApiDiscoveryData& data: tran.params.discoveryData) \
                               notificationParams.discoveryNotificationManager->triggerNotification(data); \
                       }) \
APPLY(208, setResourceParam, ApiResourceParamWithRefData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashForResourceParamWithRefDataHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper()) \
APPLY(209, removeResourceParam, ApiResourceParamWithRefData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashForResourceParamWithRefDataHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper()) \
APPLY(210, removeResourceParams, ApiResourceParamWithRefDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper()) \
APPLY(211, getStatusList, ApiResourceStatusDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) \
APPLY(212, removeResources, ApiIdDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       &apiIdDataListTriggerNotificationHelper) \
APPLY(300, getCameras, ApiCameraDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper()) \
APPLY(301, saveCamera, ApiCameraData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper()) \
APPLY(302, saveCameras, ApiCameraDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       [] (const QnTransaction<ApiCameraDataList> &tran, const NotificationParams &notificationParams) \
                         { \
                            notificationParams.cameraNotificationManager->triggerNotification(tran); \
                         }) \
APPLY(303, removeCamera, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper) \
APPLY(304, getCameraHistoryItems, ApiServerFootageDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) \
APPLY(305, addCameraHistoryItem, ApiServerFootageData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForServerFootageDataHelper, /* getHash*/ \
                       CameraNotificationManagerHelper()) \
APPLY(309, removeCameraHistoryItem, ApiServerFootageData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForServerFootageDataHelper, /* getHash*/ \
                       CameraNotificationManagerHelper()) \
APPLY(310, saveCameraUserAttributes, ApiCameraAttributesData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForApiCameraAttributesDataHelper, /* getHash*/ \
                       CameraNotificationManagerHelper()) \
APPLY(311, saveCameraUserAttributesList, ApiCameraAttributesDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper()) \
APPLY(312, getCameraUserAttributes, ApiCameraAttributesDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper()) \
APPLY(313, getCamerasEx, ApiCameraDataExList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) \
APPLY(400, getMediaServers, ApiMediaServerDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) \
APPLY(401, saveMediaServer, ApiMediaServerData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper()) \
APPLY(402, removeMediaServer, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper) \
APPLY(403, saveServerUserAttributes, ApiMediaServerUserAttributesData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForApiMediaServerUserAttributesDataHelper, /* getHash*/ \
                       MediaServerNotificationManagerHelper()) \
APPLY(404, saveServerUserAttributesList, ApiMediaServerUserAttributesDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper()) \
APPLY(405, getServerUserAttributes, ApiMediaServerUserAttributesDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper()) \
APPLY(406, removeServerUserAttributes, ApiIdData, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper) \
APPLY(407, saveStorage, ApiStorageData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper()) \
APPLY(408, saveStorages, ApiStorageDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper()) \
APPLY(409, removeStorage, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper) \
APPLY(410, removeStorages, ApiIdDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       &apiIdDataListTriggerNotificationHelper) \
APPLY(411, getMediaServersEx, ApiMediaServerDataExList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) \
APPLY(412, getStorages, ApiStorageDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper()) \
APPLY(500, getUsers, ApiUserDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) \
APPLY(501, saveUser, ApiUserData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       UserNotificationManagerHelper()) \
APPLY(502, removeUser, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper) /* trigger notification*/ \
APPLY(503, getAccessRights, ApiAccessRightsDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) /* trigger notification*/ \
APPLY(504, setAccessRights, ApiAccessRightsData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByUserIdHelper(), /* getHash*/ \
                       UserNotificationManagerHelper()) /* trigger notification*/ \
APPLY(505, getUserGroups, ApiUserGroupDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(506, saveUserGroup, ApiUserGroupData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       UserNotificationManagerHelper()) \
APPLY(507, removeUserGroup, ApiIdData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper) \
APPLY(600, getLayouts, ApiLayoutDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) /* trigger notification*/ \
APPLY(601, saveLayout, ApiLayoutData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       LayoutNotificationManagerHelper()) /* trigger notification*/ \
APPLY(602, saveLayouts, ApiLayoutDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       LayoutNotificationManagerHelper()) /* trigger notification*/ \
APPLY(603, removeLayout, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper) /* trigger notification*/ \
APPLY(700, getVideowalls, ApiVideowallDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper()) /* trigger notification*/ \
APPLY(701, saveVideowall, ApiVideowallData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       VideowallNotificationManagerHelper()) /* trigger notification*/ \
APPLY(702, removeVideowall, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper) \
APPLY(703, videowallControl, ApiVideowallControlMessageData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       VideowallNotificationManagerHelper()) \
APPLY(800, getBusinessRules, ApiBusinessRuleDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(801, saveBusinessRule, ApiBusinessRuleData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       BusinessEventNotificationManagerHelper()) \
APPLY(802, removeBusinessRule, ApiIdData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper) \
APPLY(803, resetBusinessRules, ApiResetBusinessRuleData, \
                       true, \
                       false, \
                       [] (const ApiResetBusinessRuleData&) { return QnTransactionLog::makeHash("reset_brule", ADD_HASH_DATA); }, \
                       BusinessEventNotificationManagerHelper()) \
APPLY(804, broadcastBusinessAction, ApiBusinessActionData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       BusinessEventNotificationManagerHelper()) \
APPLY(805, execBusinessAction, ApiBusinessActionData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       BusinessEventNotificationManagerHelper()) \
APPLY(904, removeStoredFile, ApiStoredFilePath, \
                       true, \
                       false, \
                       [] (const ApiStoredFilePath &params) { return QnTransactionLog::makeHash(params.path.toUtf8()); }, \
                       [] (const QnTransaction<ApiStoredFilePath> &tran, const NotificationParams &notificationParams) \
                        { \
                            NX_ASSERT(tran.command == ApiCommand::removeStoredFile); \
                            notificationParams.storedFileNotificationManager->triggerNotification(tran); \
                        }) \
APPLY(901, getStoredFile, ApiStoredFileData, \
                       false, \
                       false, \
                       &createHashForApiStoredFileDataHelper, \
                       StoredFileNotificationManagerHelper()) \
APPLY(902, addStoredFile, ApiStoredFileData, \
                       true, \
                       false, \
                       &createHashForApiStoredFileDataHelper, \
                       StoredFileNotificationManagerHelper()) \
APPLY(903, updateStoredFile, ApiStoredFileData, \
                       true, \
                       false, \
                       &createHashForApiStoredFileDataHelper, \
                       StoredFileNotificationManagerHelper()) \
APPLY(900, listDirectory, ApiStoredDirContents, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(905, getStoredFiles, ApiStoredFileDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(1000, getLicenses, ApiLicenseDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       LicenseNotificationManagerHelper()) \
APPLY(1001, addLicense, ApiLicenseData, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(1002, addLicenses, ApiLicenseDataList, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       LicenseNotificationManagerHelper()) \
APPLY(1003, removeLicense, ApiLicenseData, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(1200, uploadUpdate, ApiUpdateUploadData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       UpdateNotificationManagerHelper()) \
APPLY(1201, uploadUpdateResponce, ApiUpdateUploadResponceData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       UpdateNotificationManagerHelper()) \
APPLY(1202, installUpdate, ApiUpdateInstallData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       UpdateNotificationManagerHelper()) \
APPLY(1301, discoveredServerChanged, ApiDiscoveredServerData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       DiscoveryNotificationManagerHelper()) \
APPLY(1302, discoveredServersList, ApiDiscoveredServerDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       DiscoveryNotificationManagerHelper()) \
APPLY(1401, discoverPeer, ApiDiscoverPeerData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       DiscoveryNotificationManagerHelper()) \
APPLY(1402, addDiscoveryInformation, ApiDiscoveryData, \
                       true, \
                       false, \
                       &createHashForApiDiscoveryDataHelper, \
                       DiscoveryNotificationManagerHelper()) \
APPLY(1403, removeDiscoveryInformation, ApiDiscoveryData, \
                       true, \
                       false, \
                       &createHashForApiDiscoveryDataHelper, \
                       DiscoveryNotificationManagerHelper()) \
APPLY(1404, getDiscoveryData, ApiDiscoveryDataList, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       DiscoveryNotificationManagerHelper()) \
APPLY(1500, getWebPages, ApiWebPageDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(1501, saveWebPage, ApiWebPageData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       WebPageNotificationManagerHelper()) \
APPLY(1502, removeWebPage, ApiIdData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper) \
APPLY(2001, forcePrimaryTimeServer, ApiIdData, \
                       false, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper) \
APPLY(2002, broadcastPeerSystemTime, ApiPeerSystemTimeData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       EmptyNotificationHelper()) \
APPLY(2003, getCurrentTime, ApiTimeData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(2004, changeSystemName, ApiSystemNameData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<ApiSystemNameData> &tran, const NotificationParams &notificationParams) \
                        { return notificationParams.miscNotificationManager->triggerNotification(tran); }) \
APPLY(2005, getKnownPeersSystemTime, ApiPeerSystemTimeDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(2006, markLicenseOverflow, ApiLicenseOverflowData, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       ResourceNotificationManagerHelper()) \
APPLY(2007, getSettings, ApiResourceParamDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(4001, getClientInfos, ApiClientInfoDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(4002, saveClientInfo, ApiClientInfoData, \
                       true, \
                       false, \
                       CreateHashByIdRfc4122Helper(), \
                       EmptyNotificationHelper()) \
APPLY(5001, getStatisticsReport, ApiSystemStatistics, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(5002, triggerStatisticsReport, ApiStatisticsServerInfo, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper()) \
APPLY(9004, runtimeInfoChanged, ApiRuntimeData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<ApiRuntimeData> & tran, const NotificationParams &notificationParams) \
                        { \
                            NX_ASSERT(tran.command == ApiCommand::runtimeInfoChanged); \
                            emit notificationParams.ecConnection->runtimeInfoChanged(tran.params); \
                        }) \
APPLY(9005, dumpDatabase, ApiDatabaseDumpData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<ApiDatabaseDumpData> &, const NotificationParams &notificationParams) \
                        { \
                            emit notificationParams.ecConnection->databaseDumped(); \
                        }) \
APPLY(9006, restoreDatabase, ApiDatabaseDumpData, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<ApiDatabaseDumpData> &, const NotificationParams &notificationParams) \
                        { \
                            emit notificationParams.ecConnection->databaseDumped(); \
                        }) \
APPLY(9009, updatePersistentSequence, ApiUpdateSequenceData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       EmptyNotificationHelper()) \
APPLY(9010, dumpDatabaseToFile, ApiDatabaseDumpToFileData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       EmptyNotificationHelper()) \
APPLY(10000, getTransactionLog, ApiTransactionDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper())


#define TRANSACTION_ENUM_APPLY(value, name, ...) name = value,

    namespace ApiCommand
    {
        /* TODO: #Elric #enum*/
        enum Value
        {
            NotDefined = 0,
            TRANSACTION_DESCRIPTOR_LIST(TRANSACTION_ENUM_APPLY)
            testLdapSettings = 11001,
            maxTransactionValue         = 65535
        };
#undef TRANSACTION_ENUM_APPLY

        QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Value)

        QString toString( Value val );
        Value fromString(const QString& val);

        /** Check if transaction can be sent independently of current syncronisation state.
         *  These transactions are rather commands then data transfers. */
        bool isSystem( Value val );

        /** Check if transaction should be written to data base. */
        bool isPersistent( Value val );

    }


    /*!Contains information on how transaction has been delivered*/
    class QnTranDeliveryInformation
    {
    public:
        enum TranOriginatorType
        {
            localServer,
            remoteServer,
            client
        };

        TranOriginatorType originatorType;

        QnTranDeliveryInformation()
        :
            originatorType( TranOriginatorType::localServer )
        {
        }
    };

    class QnAbstractTransaction
    {
    public:
        QnAbstractTransaction(): command(ApiCommand::NotDefined), isLocal(false) { peerID = qnCommon->moduleGUID(); }
        QnAbstractTransaction(ApiCommand::Value value): command(value), isLocal(false) { peerID = qnCommon->moduleGUID(); }

        struct PersistentInfo
        {
            PersistentInfo(): sequence(0), timestamp(0) {}
            bool isNull() const { return dbID.isNull(); }

            QnUuid dbID;
            qint32 sequence;
            qint64 timestamp;

#ifndef QN_NO_QT
            friend uint qHash(const ec2::QnAbstractTransaction::PersistentInfo &id) {
                return ::qHash(QByteArray(id.dbID.toRfc4122()).append((const char*)&id.timestamp, sizeof(id.timestamp)), id.sequence);
            }
#endif

            bool operator==(const PersistentInfo &other) const {
                return dbID == other.dbID && sequence == other.sequence && timestamp == other.timestamp;
            }
        };

        ApiCommand::Value command;
        QnUuid peerID;
        PersistentInfo persistentInfo;
        QnTranDeliveryInformation deliveryInfo;

        bool isLocal; /* do not propagate transactions to other server peers*/

        QString toString() const;
    };


    typedef std::vector<ec2::QnAbstractTransaction> QnAbstractTransactionList;
    typedef QnAbstractTransaction::PersistentInfo QnAbstractTransaction_PERSISTENT;
#define QnAbstractTransaction_PERSISTENT_Fields (dbID)(sequence)(timestamp)
#define QnAbstractTransaction_Fields (command)(peerID)(persistentInfo)(isLocal)

    template <class T>
    class QnTransaction: public QnAbstractTransaction
    {
    public:
        QnTransaction(): QnAbstractTransaction() {}
        QnTransaction(const QnAbstractTransaction& abstractTran): QnAbstractTransaction(abstractTran) {}
        QnTransaction(ApiCommand::Value command, const T& params = T()): QnAbstractTransaction(command), params(params) {}

        template<typename U>
        QnTransaction(const QnTransaction<U>&)
        {
            NX_ASSERT(0, "Constructing from transaction with another Params type is disallowed");
        }

        T params;
    };

    QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransaction::PersistentInfo, (json)(ubjson)(xml)(csv_record))
    QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransaction, (json)(ubjson)(xml)(csv_record))

    /*Binary format functions for QnTransaction<T>*/
    /*template <class T, class Output>*/
    /*void serialize(const QnTransaction<T> &transaction,  QnOutputBinaryStream<Output> *stream)*/
    /*{*/
    /*    QnBinary::serialize(static_cast<const QnAbstractTransaction &>(transaction), stream);*/
    /*    QnBinary::serialize(transaction.params, stream);*/
    /*}*/

    /*template <class T, class Input>*/
    /*bool deserialize(QnInputBinaryStream<Input>* stream,  QnTransaction<T> *transaction)*/
    /*{*/
    /*    return*/
    /*        QnBinary::deserialize(stream,  static_cast<QnAbstractTransaction *>(transaction)) &&*/
    /*        QnBinary::deserialize(stream, &transaction->params);*/
    /*}*/


    /*Json format functions for QnTransaction<T>*/
    template<class T>
    void serialize(QnJsonContext* ctx, const QnTransaction<T>& tran, QJsonValue* target)
    {
        QJson::serialize(ctx, static_cast<const QnAbstractTransaction&>(tran), target);
        QJsonObject localTarget = target->toObject();
        QJson::serialize(ctx, tran.params, "params", &localTarget);
        *target = localTarget;
    }

    template<class T>
    bool deserialize(QnJsonContext* ctx, const QJsonValue& value, QnTransaction<T>* target)
    {
        return
            QJson::deserialize(ctx, value, static_cast<QnAbstractTransaction*>(target)) &&
            QJson::deserialize(ctx, value.toObject(), "params", &target->params);
    }

    /*QnUbjson format functions for QnTransaction<T>*/
    template <class T, class Output>
    void serialize(const QnTransaction<T> &transaction,  QnUbjsonWriter<Output> *stream)
    {
        QnUbjson::serialize(static_cast<const QnAbstractTransaction &>(transaction), stream);
        QnUbjson::serialize(transaction.params, stream);
    }

    template <class T, class Input>
    bool deserialize(QnUbjsonReader<Input>* stream,  QnTransaction<T> *transaction)
    {
        return
            QnUbjson::deserialize(stream,  static_cast<QnAbstractTransaction *>(transaction)) &&
            QnUbjson::deserialize(stream, &transaction->params);
    }

    struct ApiTransactionData: public ApiData
    {
        QnUuid tranGuid;
        QnAbstractTransaction tran;
    };
#define ApiTransactionDataFields (tranGuid)(tran)
QN_FUSION_DECLARE_FUNCTIONS(ApiTransactionData, (json)(ubjson)(xml)(csv_record))


    int generateRequestID();
} /* namespace ec2*/

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ec2::ApiCommand::Value), (metatype)(numeric))

#ifndef QN_NO_QT
Q_DECLARE_METATYPE(ec2::QnAbstractTransaction)
#endif

#endif  /*EC2_TRANSACTION_H*/
