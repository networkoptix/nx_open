#pragma once

#include <vector>
#include <QtCore/QString>

#include <nx_ec/ec_api.h>
#include <nx_ec/transaction_timestamp.h>
#include <nx/fusion/serialization/binary.h>
#include <nx/fusion/serialization/csv.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/fusion/serialization/xml.h>

/**
 * This class describes all possible transactions and defines various access righs check for them.
 *
 * Couple of examples:
 *
 * APPLY(                   -- macro header
 * 604,                     -- integer enum id
 * getLayoutTours,          -- transaction name
 * ApiLayoutTourDataList,   -- passed data structure
 * false,                   -- transaction is not persistent (does not save anything to database)
 * false,                   -- transaction is not system (handled common way)
 * InvalidGetHashHelper(),  -- Calculates hash for persistent transaction.
 *                             MUST yield the same result for corresponing setXXX and removeXXX
 *                             transactions. Actual for persistent transactions only.
 * InvalidTriggerNotificationHelper(),
 *                          -- actual mostly for persistent transactions. This callable SHOULD
 *                             implement second stage of transaction processing "in memory, non db"
 *                             logic (work with resource pool for example). It's quite possible
 *                             that non persistent transaction tiriggers some notifications (for
 *                             example dumpDataBase).
 * InvalidAccess(),         -- actual only for persistent transactions with one element.
 *                             Warning below MUST be fullfilled.
 * InvalidAccess(),         -- actual only for read transactions for one element.
 *                             Warning below MUST be fullfilled.
 * InvalidFilterFunc(),     -- actual only for persistent transactions with element list.
 *                             Warning below MUST be fullfilled.
 * FilterListByAccess<LayoutTourAccess>(), -- filtering requested list by the passed checker.
 *                             Warning below MUST be fullfilled.
 * AllowForAllAccessOut(),  -- Actual only for persistent transactions
 *                             Desides if remote peer has enough rights to receive this transaction
 *                             while the proxy transaction stage.
 * RegularTransactionType() -- transaction is common, regular, without any magic
 * )
 *
 * APPLY(                   -- macro header
 * 605,                     -- integer enum id
 * saveLayoutTour,          -- transaction name
 * ApiLayoutTourData,       -- passed data structure
 * true,                    -- transaction is persistent
 * false,                   -- transaction is not system (handled common way)
 * CreateHashByIdHelper(),  -- id is enough to generate hash
 * LayoutTourNotificationManagerHelper(), -- notify other users that we have changed the tour
 * LayoutTourAccess(),      -- check access to save
 * LayoutTourAccess(),      -- check access to read
 * InvalidFilterFunc(),     -- actual only for list transactions
 * InvalidFilterFunc(),     -- actual only for list transactions
 * AccessOut<LayoutTourAccess>(),  -- resending persistent transactions
 * RegularTransactionType() -- transaction is common, regular, without any magic
 * )
 *
 *                                      --WARNING--
 * all transaction descriptors for the same api data structures should have the same Access Rights
 * checker functions. For example setResourceParam and getResourceParam have the same checker for
 * read access - ReadResourceParamAccess.
 */


namespace ec2
{

struct NotDefinedApiData {};

#define TRANSACTION_DESCRIPTOR_LIST(APPLY)  \
APPLY(1, tranSyncRequest, ApiSyncRequestData, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(2, tranSyncResponse, QnTranStateResponse, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(3, lockRequest, ApiLockData, \
                       false, /* persistent*/ \
                       true, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(4, lockResponse, ApiLockData, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(5, unlockRequest, ApiLockData, \
                       false, /* persistent*/ \
                       true, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(6, peerAliveInfo, ApiPeerAliveData, \
                       false, /* persistent*/ \
                       true, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(7, tranSyncDone, ApiTranSyncDoneData, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(100, testConnection, ApiLoginData, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(101, connect, ApiLoginData, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(102, openReverseConnection, ApiReverseConnectionData, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       [](const QnTransaction<ApiReverseConnectionData> &tran, const NotificationParams &notificationParams) \
                       { \
                            NX_ASSERT(tran.command == ApiCommand::openReverseConnection); \
                            emit notificationParams.ecConnection->reverseConnectionRequested(tran.params); \
                       },  /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(201, removeResource, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(202, setResourceStatus, ApiResourceStatusData, \
                       true,  /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdRfc4122Helper("status"), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(),                     \
                       SetStatusTransactionType()) /* Check remote peer rights for outgoing transaction */ \
APPLY(213, removeResourceStatus, ApiIdData, /* Remove records from vms_resource_status by resource id */ \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdRfc4122Helper("status"), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(),                     \
                       RegularTransactionType()) /* local transaction type is set manually in  server query processor via removeResourceStatusHelper call*/ \
APPLY(204, setResourceParams, ApiResourceParamWithRefDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceParamAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceParamAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceParamAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(203, getResourceParams, ApiResourceParamWithRefDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceParamAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceParamAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceParamAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(205, getResourceTypes, ApiResourceTypeDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(206, getFullInfo, ApiFullInfoData, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       [] (const QnTransaction<ApiFullInfoData> & tran, const NotificationParams &notificationParams) \
                       { \
                           emit notificationParams.ecConnection->initNotification(tran.params); \
                           for(const ApiDiscoveryData& data: tran.params.discoveryData) \
                               notificationParams.discoveryNotificationManager->triggerNotification(data); \
                       }, \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(208, setResourceParam, ApiResourceParamWithRefData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashForResourceParamWithRefDataHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       ModifyResourceParamAccess(false), /* save permission checker */ \
                       ReadResourceParamAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceParamAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       SetResourceParamTransactionType()) /* regular transaction type */ \
APPLY(209, removeResourceParam, ApiResourceParamWithRefData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashForResourceParamWithRefDataHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       ModifyResourceParamAccess(true), /* save permission checker */ \
                       ReadResourceParamAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(210, removeResourceParams, ApiResourceParamWithRefDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceParamAccess>(true), /* Filter save func */ \
                       FilterListByAccess<ReadResourceParamAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(211, getStatusList, ApiResourceStatusDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(212, removeResources, ApiIdDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       &apiIdDataListTriggerNotificationHelper, \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(true), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(300, getCameras, ApiCameraDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(301, saveCamera, ApiCameraData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       ModifyCameraDataAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(302, saveCameras, ApiCameraDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       [] (const QnTransaction<ApiCameraDataList> &tran, const NotificationParams &notificationParams) \
                         { \
                            notificationParams.cameraNotificationManager->triggerNotification(tran, notificationParams.source); \
                         }, \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(303, removeCamera, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(304, getCameraHistoryItems, ApiServerFootageDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyFootageDataAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadFootageDataAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadFootageDataAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(305, addCameraHistoryItem, ApiServerFootageData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForServerFootageDataHelper, /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       ModifyFootageDataAccess(), /* save permission checker */ \
                       ReadFootageDataAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadFootageDataAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(309, removeCameraHistoryItem, ApiServerFootageData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForServerFootageDataHelper, /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       ModifyFootageDataAccess(), /* save permission checker */ \
                       ReadFootageDataAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(310, saveCameraUserAttributes, ApiCameraAttributesData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForApiCameraAttributesDataHelper, /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       ModifyCameraAttributesAccess(), /* save permission checker */ \
                       ReadCameraAttributesAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadCameraAttributesAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(311, saveCameraUserAttributesList, ApiCameraAttributesDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       ModifyCameraAttributesListAccess(), /* Filter save func */ \
                       FilterListByAccess<ReadCameraAttributesAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadCameraAttributesAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(312, getCameraUserAttributesList, ApiCameraAttributesDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       ModifyCameraAttributesListAccess(), /* Filter save func */ \
                       FilterListByAccess<ReadCameraAttributesAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadCameraAttributesAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(313, getCamerasEx, ApiCameraDataExList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(314, removeCameraUserAttributes, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdRfc4122Helper("camera_attributes"), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(400, getMediaServers, ApiMediaServerDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(401, saveMediaServer, ApiMediaServerData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       AdminOnlyAccess(), \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(402, removeMediaServer, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(403, saveMediaServerUserAttributes, ApiMediaServerUserAttributesData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForApiMediaServerUserAttributesDataHelper, /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       ModifyServerAttributesAccess(), /* save permission checker */ \
                       ReadServerAttributesAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadServerAttributesAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(404, saveMediaServerUserAttributesList, ApiMediaServerUserAttributesDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyServerAttributesAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadServerAttributesAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadServerAttributesAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(405, getMediaServerUserAttributesList, ApiMediaServerUserAttributesDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyServerAttributesAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadServerAttributesAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadServerAttributesAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(406, removeServerUserAttributes, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdRfc4122Helper("server_attributes"), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(407, saveStorage, ApiStorageData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       ModifyResourceAccess(false), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(408, saveStorages, ApiStorageDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(409, removeStorage, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(410, removeStorages, ApiIdDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       &apiIdDataListTriggerNotificationHelper, \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(true), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(411, getMediaServersEx, ApiMediaServerDataExList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(412, getStorages, ApiStorageDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(500, getUsers, ApiUserDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(501, saveUser, ApiUserData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       UserNotificationManagerHelper(), \
                       ModifyResourceAccess(false), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       SaveUserTransactionType()) /* regular transaction type */ \
APPLY(502, removeUser, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RemoveUserTransactionType()) /* regular transaction type */ \
APPLY(503, getAccessRights, ApiAccessRightsDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(504, setAccessRights, ApiAccessRightsData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForApiAccessRightsDataHelper, /* getHash*/ \
                       UserNotificationManagerHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(),                     \
                       RegularTransactionType()) /* Check remote peer rights for outgoing transaction */ \
APPLY(509, removeAccessRights, ApiIdData, /* Remove records from vms_access_rights by resource id */ \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdRfc4122Helper("access_rights"), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(),                     \
                       RegularTransactionType()) /* Check remote peer rights for outgoing transaction */ \
APPLY(505, getUserRoles, ApiUserRoleDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(506, saveUserRole, ApiUserRoleData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       UserNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(507, removeUserRole, ApiIdData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       RemoveUserRoleAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(508, getPredefinedRoles, ApiPredefinedRoleDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(510, saveUsers,  ApiUserDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       UserNotificationManagerHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(600, getLayouts, ApiLayoutDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(601, saveLayout, ApiLayoutData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       LayoutNotificationManagerHelper(), /* trigger notification*/ \
                       ModifyResourceAccess(false), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(602, saveLayouts, ApiLayoutDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       LayoutNotificationManagerHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(603, removeLayout, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(604, getLayoutTours, ApiLayoutTourDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), \
                       FilterListByAccess<LayoutTourAccess>(), \
                       AllowForAllAccessOut(), /* not actual for non-persistent */ \
                       RegularTransactionType()) \
APPLY(605, saveLayoutTour, ApiLayoutTourData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       LayoutTourNotificationManagerHelper(), /* trigger notification*/ \
                       LayoutTourAccess(), /* save permission checker */ \
                       LayoutTourAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AccessOut<LayoutTourAccess>(), \
                       RegularTransactionType()) \
APPLY(606, removeLayoutTour, ApiIdData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       LayoutTourAccessById(), /* save permission checker */ \
                       LayoutTourAccessById(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(700, getVideowalls, ApiVideowallDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(701, saveVideowall, ApiVideowallData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       VideowallNotificationManagerHelper(), /* trigger notification*/ \
                       ModifyResourceAccess(false), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(702, removeVideowall, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(703, videowallControl, ApiVideowallControlMessageData, \
                       false, /* persistent*/\
                       false, /* system*/ \
                       InvalidGetHashHelper(), \
                       VideowallNotificationManagerHelper(), \
                       VideoWallControlAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(800, getEventRules, ApiBusinessRuleDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(801, saveEventRule, ApiBusinessRuleData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       BusinessEventNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(802, removeEventRule, ApiIdData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(803, resetEventRules, ApiResetBusinessRuleData, \
                       true, \
                       false, \
                       [] (const ApiResetBusinessRuleData&) { return QnTransactionLog::makeHash("reset_brule", ADD_HASH_DATA); }, \
                       BusinessEventNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(804, broadcastAction, ApiBusinessActionData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       BusinessEventNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(805, execAction, ApiBusinessActionData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       BusinessEventNotificationManagerHelper(), \
                       UserInputAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(904, removeStoredFile, ApiStoredFilePath, \
                       true, \
                       false, \
                       [] (const ApiStoredFilePath &params) { return QnTransactionLog::makeHash(params.path.toUtf8()); }, \
                       [] (const QnTransaction<ApiStoredFilePath> &tran, const NotificationParams &notificationParams) \
                        { \
                            NX_ASSERT(tran.command == ApiCommand::removeStoredFile); \
                            notificationParams.storedFileNotificationManager->triggerNotification(tran, notificationParams.source); \
                        }, \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(901, getStoredFile, ApiStoredFileData, \
                       false, \
                       false, \
                       &createHashForApiStoredFileDataHelper, \
                       StoredFileNotificationManagerHelper(), \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(902, addStoredFile, ApiStoredFileData, \
                       true, \
                       false, \
                       &createHashForApiStoredFileDataHelper, \
                       StoredFileNotificationManagerHelper(), \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(903, updateStoredFile, ApiStoredFileData, \
                       true, \
                       false, \
                       &createHashForApiStoredFileDataHelper, \
                       StoredFileNotificationManagerHelper(), \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(900, listDirectory, ApiStoredDirContents, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(905, getStoredFiles, ApiStoredFileDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1000, getLicenses, ApiLicenseDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       LicenseNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1001, addLicense, ApiLicenseData, \
                       true, \
                       false, \
                       &createHashForApiLicenseDataHelper, \
                       LicenseNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1002, addLicenses, ApiLicenseDataList, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       LicenseNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1003, removeLicense, ApiLicenseData, \
                       true, \
                       false, \
                       &createHashForApiLicenseDataHelper, \
                       LicenseNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1200, uploadUpdate, ApiUpdateUploadData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       UpdateNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1201, uploadUpdateResponce, ApiUpdateUploadResponceData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       UpdateNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1202, installUpdate, ApiUpdateInstallData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       UpdateNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1301, discoveredServerChanged, ApiDiscoveredServerData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       DiscoveryNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       LocalTransactionType()) /* local transaction type */ \
APPLY(1302, discoveredServersList, ApiDiscoveredServerDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       DiscoveryNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1401, discoverPeer, ApiDiscoverPeerData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       DiscoveryNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1402, addDiscoveryInformation, ApiDiscoveryData, \
                       true, \
                       false, \
                       &createHashForApiDiscoveryDataHelper, \
                       DiscoveryNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1403, removeDiscoveryInformation, ApiDiscoveryData, \
                       true, \
                       false, \
                       &createHashForApiDiscoveryDataHelper, \
                       DiscoveryNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1404, getDiscoveryData, ApiDiscoveryDataList, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       DiscoveryNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1500, getWebPages, ApiWebPageDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1501, saveWebPage, ApiWebPageData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       WebPageNotificationManagerHelper(), \
                       ModifyResourceAccess(false), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1502, removeWebPage, ApiIdData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(2001, forcePrimaryTimeServer, ApiIdData, \
                       false, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(2002, broadcastPeerSystemTime, ApiPeerSystemTimeData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       EmptyNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(2003, getCurrentTime, ApiTimeData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(2004, changeSystemId, ApiSystemIdData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<ApiSystemIdData> &tran, const NotificationParams &notificationParams) \
                        { return notificationParams.miscNotificationManager->triggerNotification(tran, notificationParams.source); }, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(2005, getKnownPeersSystemTime, ApiPeerSystemTimeDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(2006, markLicenseOverflow, ApiLicenseOverflowData, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       ResourceNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       LocalTransactionType()) /* local transaction type */ \
APPLY(2007, getSettings, ApiResourceParamDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceParamAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(),  \
                       RegularTransactionType()) /* Check remote peer rights for outgoing transaction */ \
APPLY(2008, cleanupDatabase, ApiCleanupDatabaseData, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       ResourceNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(),      \
                       LocalTransactionType()) /* Check remote peer rights for outgoing transaction */ \
APPLY(2009, broadcastPeerSyncTime, ApiPeerSyncTimeData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       EmptyNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
/* Transaction 4001 (getClientInfoList) removed, code is forbidden. */ \
/* Transaction 4002 (saveClientInfo) removed, code is forbidden. */ \
APPLY(5001, getStatisticsReport, ApiSystemStatistics, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(5002, triggerStatisticsReport, ApiStatisticsServerInfo, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(9004, runtimeInfoChanged, ApiRuntimeData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<ApiRuntimeData> & tran, const NotificationParams &notificationParams) \
                        { \
                            NX_ASSERT(tran.command == ApiCommand::runtimeInfoChanged); \
                            emit notificationParams.ecConnection->runtimeInfoChanged(tran.params); \
                        }, \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(9005, dumpDatabase, ApiDatabaseDumpData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<ApiDatabaseDumpData> &, const NotificationParams &notificationParams) \
                        { \
                            emit notificationParams.ecConnection->databaseDumped(); \
                        }, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       LocalTransactionType()) /* local transaction type */ \
APPLY(9006, restoreDatabase, ApiDatabaseDumpData, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<ApiDatabaseDumpData> &, const NotificationParams &notificationParams) \
                        { \
                            emit notificationParams.ecConnection->databaseDumped(); \
                        }, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       LocalTransactionType()) /* local transaction type */ \
APPLY(9009, updatePersistentSequence, ApiUpdateSequenceData, \
                       true, /* persistent*/ \
                       false,  /* system*/ \
                       InvalidGetHashHelper(), \
                       EmptyNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(9010, dumpDatabaseToFile, ApiDatabaseDumpToFileData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       EmptyNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(10000, getTransactionLog, ApiTransactionDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(10100, saveMiscParam, ApiMiscData, \
                       true, /* persistent*/ \
                       false,  /* system*/ \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<ApiMiscData> &tran, const NotificationParams &notificationParams) \
                        { return notificationParams.miscNotificationManager->triggerNotification(tran); }, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       LocalTransactionType()) /* local transaction type */ \
APPLY(10101, getMiscParam, ApiMiscData, \
                       true, /* persistent*/ \
                       false,  /* system*/ \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       LocalTransactionType()) /* regular transaction type */ \

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

    namespace TransactionType
    {
        enum Value
        {
            Unknown = -1,
            Regular = 0,
            Local = 1, //< do not propagate transactions to other server peers or cloud
            Cloud = 2,  //< sync transaction to cloud
        };
        QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Value)
    }

    struct HistoryAttributes
    {
        /** Id of user or entity who created transaction. */
        QnUuid author;

        bool operator==(const HistoryAttributes& rhs) const
        {
            return author == rhs.author;
        }
    };

    #define HistoryAttributes_Fields (author)
    QN_FUSION_DECLARE_FUNCTIONS(HistoryAttributes, (json)(ubjson)(xml)(csv_record))

    class QnAbstractTransaction
    {
    public:
        typedef Timestamp TimestampType;

        /**
         * Sets \a QnAbstractTransaction::peerID to \a commonModule()->moduleGUID().
         */
        QnAbstractTransaction():
            command(ApiCommand::NotDefined),
            transactionType(TransactionType::Regular)
        {
        }
        QnAbstractTransaction(QnUuid _peerID):
            command(ApiCommand::NotDefined),
            peerID(std::move(_peerID)),
            transactionType(TransactionType::Regular)
        {
        }
        QnAbstractTransaction(ApiCommand::Value value, QnUuid _peerID):
            command(value),
            peerID(std::move(_peerID)),
            transactionType(TransactionType::Regular)
        {
        }

        struct PersistentInfo
        {
            PersistentInfo(): sequence(0), timestamp(Timestamp::fromInteger(0)) {}
            bool isNull() const { return dbID.isNull(); }

            QnUuid dbID;
            qint32 sequence;
            Timestamp timestamp;

            friend uint qHash(const ec2::QnAbstractTransaction::PersistentInfo &id) {
                return ::qHash(QByteArray(id.dbID.toRfc4122()).append((const char*)&id.timestamp, sizeof(id.timestamp)), id.sequence);
            }

            bool operator==(const PersistentInfo &other) const {
                return dbID == other.dbID && sequence == other.sequence && timestamp == other.timestamp;
            }
        };

        ApiCommand::Value command;
        /** Id of peer that generated transaction. */
        QnUuid peerID;
        PersistentInfo persistentInfo;

        TransactionType::Value transactionType;
        HistoryAttributes historyAttributes;

        bool operator==(const QnAbstractTransaction& rhs) const
        {
            return command == rhs.command
                && peerID == rhs.peerID
                && persistentInfo == rhs.persistentInfo
                && transactionType == rhs.transactionType
                && historyAttributes == rhs.historyAttributes;
        }

        QString toString() const;
        bool isLocal() const { return transactionType == TransactionType::Local; }
        bool isCloud() const { return transactionType == TransactionType::Cloud; }
    };


    typedef std::vector<ec2::QnAbstractTransaction> QnAbstractTransactionList;
    typedef QnAbstractTransaction::PersistentInfo QnAbstractTransaction_PERSISTENT;
#define QnAbstractTransaction_PERSISTENT_Fields (dbID)(sequence)(timestamp)
#define QnAbstractTransaction_Fields (command)(peerID)(persistentInfo)(transactionType)(historyAttributes)

    template <class T>
    class QnTransaction: public QnAbstractTransaction
    {
    public:
        QnTransaction(): QnAbstractTransaction() {}
        QnTransaction(QnUuid _peerID):
            QnAbstractTransaction(std::move(_peerID))
        {
        }
        QnTransaction(const QnAbstractTransaction& abstractTran):
            QnAbstractTransaction(abstractTran)
        {
        }
        QnTransaction(
            ApiCommand::Value command,
            QnUuid _peerID,
            const T& params = T())
            :
            QnAbstractTransaction(command, std::move(_peerID)),
            params(params)
        {
        }

        template<typename U>
        QnTransaction(const QnTransaction<U>& /*otherTran*/)
        {
            NX_ASSERT(0, "Constructing from transaction with another Params type is disallowed");
        }

        T params;

        bool operator==(const QnTransaction& rhs) const
        {
            return QnAbstractTransaction::operator==(rhs)
                && params == rhs.params;
        }

        bool operator!=(const QnTransaction& rhs) const
        {
            return !(*this == rhs);
        }
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

    struct ApiTranLogFilter
    {
        bool cloudOnly;

        ApiTranLogFilter(): cloudOnly(false) {}
    };

    struct ApiTransactionData: public ApiData
    {
        QnUuid tranGuid;
        QnAbstractTransaction tran;
        int dataSize = 0;

        ApiTransactionData() {}
        ApiTransactionData(const QnUuid& peerGuid): tran(peerGuid) {}

        ApiTransactionData(const ApiTransactionData&) = default;
        ApiTransactionData& operator=(const ApiTransactionData&) = default;
        ApiTransactionData(ApiTransactionData&&) = default;
        ApiTransactionData& operator=(ApiTransactionData&&) = default;

        bool operator==(const ApiTransactionData& right) const
        {
            return tranGuid == right.tranGuid
                && tran == right.tran
                && dataSize == right.dataSize;
        }
    };
#define ApiTransactionData_Fields (tranGuid)(tran)(dataSize)
QN_FUSION_DECLARE_FUNCTIONS(ApiTransactionData, (json)(ubjson)(xml)(csv_record))

    int generateRequestID();
} /* namespace ec2*/

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ec2::ApiCommand::Value)(ec2::TransactionType::Value), (metatype)(numeric))

Q_DECLARE_METATYPE(ec2::QnAbstractTransaction)
