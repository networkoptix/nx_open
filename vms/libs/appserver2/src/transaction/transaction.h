#pragma once

#include <vector>
#include <QtCore/QString>

#include <nx_ec/ec_api.h>
#include <nx/fusion/serialization/binary.h>
#include <nx/fusion/serialization/csv.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/fusion/serialization/xml.h>
#include <nx/vms/api/data/timestamp.h>
#include <utils/crypt/symmetrical.h>
#include <nx/vms/api/data/full_info_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/resource_type_data.h>

/**
 * This class describes all possible transactions and defines various access righs check for them.
 *
 * Couple of examples:
 *
 * APPLY(                   -- macro header
 * 604,                     -- integer enum id
 * getLayoutTours,          -- transaction name
 * LayoutTourDataList,      -- passed data structure
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
 * LayoutTourData,          -- passed data structure
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

static const char ADD_HASH_DATA[] = "$$_HASH_$$";

struct NotDefinedApiData {};

#define TRANSACTION_DESCRIPTOR_LIST(APPLY)  \
APPLY(1, tranSyncRequest, nx::vms::api::SyncRequestData, \
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
APPLY(2, tranSyncResponse, nx::vms::api::TranStateResponse, \
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
APPLY(3, lockRequest, nx::vms::api::LockData, \
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
APPLY(4, lockResponse, nx::vms::api::LockData, \
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
APPLY(5, unlockRequest, nx::vms::api::LockData, \
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
APPLY(6, peerAliveInfo, nx::vms::api::PeerAliveData, \
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
APPLY(7, tranSyncDone, nx::vms::api::TranSyncDoneData, \
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
APPLY(100, testConnection, nx::vms::api::ConnectionData, \
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
APPLY(101, connect, nx::vms::api::ConnectionData, \
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
APPLY(102, openReverseConnection, nx::vms::api::ReverseConnectionData, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       [](const QnTransaction<nx::vms::api::ReverseConnectionData>& tran, \
                            const NotificationParams& notificationParams) \
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
APPLY(201, removeResource, nx::vms::api::IdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       RemoveResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(202, setResourceStatus, nx::vms::api::ResourceStatusData, \
                       true,  /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdRfc4122Helper("status"), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(),                     \
                       SetStatusTransactionType<nx::vms::api::ResourceStatusData>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(213, removeResourceStatus, nx::vms::api::IdData, /* Remove records from vms_resource_status by resource id */ \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdRfc4122Helper("status"), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       RemoveResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(),                     \
                       SetStatusTransactionType<nx::vms::api::IdData>()) \
APPLY(204, setResourceParams, nx::vms::api::ResourceParamWithRefDataList, \
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
APPLY(203, getResourceParams, nx::vms::api::ResourceParamWithRefDataList, \
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
APPLY(205, getResourceTypes, nx::vms::api::ResourceTypeDataList, \
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
APPLY(206, getFullInfo, nx::vms::api::FullInfoData, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       [] (const QnTransaction<nx::vms::api::FullInfoData> & tran, const NotificationParams &notificationParams) \
                       { \
                           emit notificationParams.ecConnection->initNotification(tran.params); \
                           for (const auto& data: tran.params.discoveryData) \
                               notificationParams.discoveryNotificationManager->triggerNotification(data); \
                       }, \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(208, setResourceParam, nx::vms::api::ResourceParamWithRefData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashForResourceParamWithRefDataHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       ModifyResourceParamAccess(false), /* save permission checker */ \
                       ReadResourceParamAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceParamAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       SetResourceParamTransactionType()) \
APPLY(209, removeResourceParam, nx::vms::api::ResourceParamWithRefData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashForResourceParamWithRefDataHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       ModifyResourceParamAccess(true), /* save permission checker */ \
                       ReadResourceParamAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       SetResourceParamTransactionType()) /* regular transaction type */ \
APPLY(210, removeResourceParams, nx::vms::api::ResourceParamWithRefDataList, \
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
APPLY(211, getStatusList, nx::vms::api::ResourceStatusDataList, \
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
APPLY(212, removeResources, nx::vms::api::IdDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       &apiIdDataListTriggerNotificationHelper, \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<RemoveResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(300, getCameras, nx::vms::api::CameraDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(301, saveCamera, nx::vms::api::CameraData, \
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
APPLY(302, saveCameras, nx::vms::api::CameraDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       [] (const QnTransaction<nx::vms::api::CameraDataList> &tran, const NotificationParams &notificationParams) \
                         { \
                            notificationParams.cameraNotificationManager->triggerNotification(tran, notificationParams.source); \
                         }, \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(303, removeCamera, nx::vms::api::IdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       RemoveResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(304, getCameraHistoryItems, nx::vms::api::ServerFootageDataList, \
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
APPLY(305, addCameraHistoryItem, nx::vms::api::ServerFootageData, \
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
APPLY(309, removeCameraHistoryItem, nx::vms::api::ServerFootageData, \
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
APPLY(310, saveCameraUserAttributes, nx::vms::api::CameraAttributesData, \
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
APPLY(311, saveCameraUserAttributesList, nx::vms::api::CameraAttributesDataList, \
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
APPLY(312, getCameraUserAttributesList, nx::vms::api::CameraAttributesDataList, \
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
APPLY(313, getCamerasEx, nx::vms::api::CameraDataExList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(314, removeCameraUserAttributes, nx::vms::api::IdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdRfc4122Helper("camera_attributes"), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       RemoveResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(400, getMediaServers, nx::vms::api::MediaServerDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(401, saveMediaServer, nx::vms::api::MediaServerData, \
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
APPLY(402, removeMediaServer, nx::vms::api::IdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       RemoveResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(403, saveMediaServerUserAttributes, nx::vms::api::MediaServerUserAttributesData, \
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
APPLY(404, saveMediaServerUserAttributesList, nx::vms::api::MediaServerUserAttributesDataList, \
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
APPLY(405, getMediaServerUserAttributesList, nx::vms::api::MediaServerUserAttributesDataList, \
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
APPLY(406, removeServerUserAttributes, nx::vms::api::IdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdRfc4122Helper("server_attributes"), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       RemoveResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(407, saveStorage, nx::vms::api::StorageData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       ModifyResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(408, saveStorages, nx::vms::api::StorageDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(409, removeStorage, nx::vms::api::IdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       RemoveResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(410, removeStorages, nx::vms::api::IdDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       &apiIdDataListTriggerNotificationHelper, \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<RemoveResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(411, getMediaServersEx, nx::vms::api::MediaServerDataExList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(412, getStorages, nx::vms::api::StorageDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(500, getUsers, nx::vms::api::UserDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(501, saveUser, nx::vms::api::UserData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       UserNotificationManagerHelper(), \
                       SaveUserAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       SaveUserTransactionType()) /* regular transaction type */ \
APPLY(502, removeUser, nx::vms::api::IdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       RemoveResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RemoveUserTransactionType()) /* regular transaction type */ \
APPLY(503, getAccessRights, nx::vms::api::AccessRightsDataList, \
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
APPLY(504, setAccessRights, nx::vms::api::AccessRightsData, \
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
APPLY(509, removeAccessRights, nx::vms::api::IdData, /* Remove records from vms_access_rights by resource id */ \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdRfc4122Helper("access_rights"), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       RemoveResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(),                     \
                       RegularTransactionType()) /* Check remote peer rights for outgoing transaction */ \
APPLY(505, getUserRoles, nx::vms::api::UserRoleDataList, \
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
APPLY(506, saveUserRole, nx::vms::api::UserRoleData, \
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
APPLY(507, removeUserRole, nx::vms::api::IdData, \
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
APPLY(508, getPredefinedRoles, nx::vms::api::PredefinedRoleDataList, \
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
APPLY(510, saveUsers,  nx::vms::api::UserDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       UserNotificationManagerHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(600, getLayouts, nx::vms::api::LayoutDataList, \
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
APPLY(601, saveLayout, nx::vms::api::LayoutData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       LayoutNotificationManagerHelper(), /* trigger notification*/ \
                       ModifyResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(602, saveLayouts, nx::vms::api::LayoutDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       LayoutNotificationManagerHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(603, removeLayout, nx::vms::api::IdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       RemoveResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(604, getLayoutTours, nx::vms::api::LayoutTourDataList, \
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
APPLY(605, saveLayoutTour, nx::vms::api::LayoutTourData, \
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
APPLY(606, removeLayoutTour, nx::vms::api::IdData, \
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
APPLY(700, getVideowalls, nx::vms::api::VideowallDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(701, saveVideowall, nx::vms::api::VideowallData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       VideowallNotificationManagerHelper(), /* trigger notification*/ \
                       ModifyResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(702, removeVideowall, nx::vms::api::IdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       RemoveResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(703, videowallControl, nx::vms::api::VideowallControlMessageData, \
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
APPLY(800, getEventRules, nx::vms::api::EventRuleDataList, \
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
APPLY(801, saveEventRule, nx::vms::api::EventRuleData, \
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
APPLY(802, removeEventRule, nx::vms::api::IdData, \
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
APPLY(803, resetEventRules, nx::vms::api::ResetEventRulesData, \
                       true, \
                       false, \
                       [] (const nx::vms::api::ResetEventRulesData&) { return QnAbstractTransaction::makeHash("reset_brule", ADD_HASH_DATA); }, \
                       BusinessEventNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(804, broadcastAction, nx::vms::api::EventActionData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       BusinessEventNotificationManagerHelper(), \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(805, execAction, nx::vms::api::EventActionData, \
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
APPLY(904, removeStoredFile, nx::vms::api::StoredFilePath, \
                       true, \
                       false, \
                       [] (const nx::vms::api::StoredFilePath &params) { return QnAbstractTransaction::makeHash(params.path.toUtf8()); }, \
                       [] (const QnTransaction<nx::vms::api::StoredFilePath> &tran, const NotificationParams &notificationParams) \
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
APPLY(901, getStoredFile, nx::vms::api::StoredFileData, \
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
APPLY(902, addStoredFile, nx::vms::api::StoredFileData, \
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
APPLY(903, updateStoredFile, nx::vms::api::StoredFileData, \
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
APPLY(900, listDirectory, nx::vms::api::StoredFilePathList, \
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
APPLY(905, getStoredFiles, nx::vms::api::StoredFileDataList, \
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
APPLY(1000, getLicenses, nx::vms::api::LicenseDataList, \
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
APPLY(1001, addLicense, nx::vms::api::LicenseData, \
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
APPLY(1002, addLicenses, nx::vms::api::LicenseDataList, \
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
APPLY(1003, removeLicense, nx::vms::api::LicenseData, \
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
APPLY(1200, uploadUpdate, nx::vms::api::UpdateUploadData, \
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
APPLY(1201, uploadUpdateResponce, nx::vms::api::UpdateUploadResponseData, \
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
APPLY(1202, installUpdate, nx::vms::api::UpdateInstallData, \
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
APPLY(1301, discoveredServerChanged, nx::vms::api::DiscoveredServerData, \
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
APPLY(1302, discoveredServersList, nx::vms::api::DiscoveredServerDataList, \
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
APPLY(1401, discoverPeer, nx::vms::api::DiscoverPeerData, \
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
APPLY(1402, addDiscoveryInformation, nx::vms::api::DiscoveryData, \
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
APPLY(1403, removeDiscoveryInformation, nx::vms::api::DiscoveryData, \
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
APPLY(1404, getDiscoveryData, nx::vms::api::DiscoveryDataList, \
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
APPLY(1500, getWebPages, nx::vms::api::WebPageDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1501, saveWebPage, nx::vms::api::WebPageData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       WebPageNotificationManagerHelper(), \
                       ModifyResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(1502, removeWebPage, nx::vms::api::IdData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       RemoveResourceAccess(), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(2004, changeSystemId, nx::vms::api::SystemIdData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<nx::vms::api::SystemIdData>& tran, const NotificationParams& notificationParams) \
                        { return notificationParams.miscNotificationManager->triggerNotification(tran, notificationParams.source); }, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(2006, markLicenseOverflow, nx::vms::api::LicenseOverflowData, \
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
APPLY(2007, getSettings, nx::vms::api::ResourceParamDataList, \
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
APPLY(2008, cleanupDatabase, nx::vms::api::CleanupDatabaseData, \
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
APPLY(2009, broadcastPeerSyncTime, nx::vms::api::PeerSyncTimeData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       TimeNotificationManagerHelper(), \
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
APPLY(9004, runtimeInfoChanged, nx::vms::api::RuntimeData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<nx::vms::api::RuntimeData> & tran, const NotificationParams &notificationParams) \
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
APPLY(9005, dumpDatabase, nx::vms::api::DatabaseDumpData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<nx::vms::api::DatabaseDumpData> &, const NotificationParams &notificationParams) \
                        { \
                            emit notificationParams.ecConnection->databaseDumped(); \
                        }, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       LocalTransactionType()) /* local transaction type */ \
APPLY(9006, restoreDatabase, nx::vms::api::DatabaseDumpData, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<nx::vms::api::DatabaseDumpData> &, const NotificationParams &notificationParams) \
                        { \
                            emit notificationParams.ecConnection->databaseDumped(); \
                        }, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       LocalTransactionType()) /* local transaction type */ \
APPLY(9009, updatePersistentSequence, nx::vms::api::UpdateSequenceData, \
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
APPLY(9010, dumpDatabaseToFile, nx::vms::api::DatabaseDumpToFileData, \
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
APPLY(9011, runtimeInfoRemoved, nx::vms::api::IdData, false, true,                                                \
                        InvalidGetHashHelper(),                                                                   \
                        [](const QnTransaction<nx::vms::api::IdData>& tran,                                       \
                            const NotificationParams& notificationParams) {                                       \
                            NX_ASSERT(tran.command == ApiCommand::runtimeInfoRemoved);                            \
                            emit notificationParams.ecConnection->runtimeInfoRemoved(tran.params);                \
                        },                                                                                        \
                        AllowForAllAccess(),      /* save permission checker */                                   \
                        AllowForAllAccess(),      /* read permission checker */                                   \
                        InvalidFilterFunc(),      /* Filter save func */                                          \
                        InvalidFilterFunc(),      /* Filter read func */                                          \
                        AllowForAllAccessOut(),   /* Check remote peer rights for outgoing transaction */         \
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
APPLY(10100, saveMiscParam, nx::vms::api::MiscData, \
                       true, /* persistent*/ \
                       false,  /* system*/ \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<nx::vms::api::MiscData>& tran, const NotificationParams& notificationParams) \
                        { return notificationParams.miscNotificationManager->triggerNotification(tran); }, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       LocalTransactionType()) /* local transaction type */ \
APPLY(10101, getMiscParam, nx::vms::api::MiscData, \
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
APPLY(10200, saveSystemMergeHistoryRecord, nx::vms::api::SystemMergeHistoryRecord, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       makeCreateHashFromCustomFieldHelper(&nx::vms::api::SystemMergeHistoryRecord::mergedSystemLocalId), /* getHash*/ \
                       EmptyNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       CloudTransactionType()) \
APPLY(10201, getSystemMergeHistory, nx::vms::api::SystemMergeHistoryRecordList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter read func */ \
                       AdminOnlyAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                       RegularTransactionType()) /* regular transaction type */ \
APPLY(10300, getAnalyticsPlugins, nx::vms::api::AnalyticsPluginDataList, \
                        false, \
                        false, \
                        InvalidGetHashHelper(), \
                        InvalidTriggerNotificationHelper(), \
                        InvalidAccess(), /* save permission checker */ \
                        InvalidAccess(), /* read permission checker */ \
                        FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                        FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                        ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                        RegularTransactionType()) /* regular transaction type */ \
APPLY(10301, saveAnalyticsPlugin, nx::vms::api::AnalyticsPluginData, \
                        true, \
                        false, \
                        CreateHashByIdHelper(), \
                        AnalyticsNotificationManagerHelper(), \
                        ModifyResourceAccess(), /* save permission checker */ \
                        ReadResourceAccess(), /* read permission checker */ \
                        InvalidFilterFunc(), /* Filter save func */ \
                        InvalidFilterFunc(), /* Filter read func */ \
                        ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                        RegularTransactionType()) /* regular transaction type */ \
APPLY(10302, removeAnalyticsPlugin, nx::vms::api::IdData, \
                        true, \
                        false, \
                        CreateHashByIdHelper(), \
                        &apiIdDataTriggerNotificationHelper, \
                        RemoveResourceAccess(), /* save permission checker */ \
                        ReadResourceAccess(), /* read permission checker */ \
                        InvalidFilterFunc(), /* Filter save func */ \
                        InvalidFilterFunc(), /* Filter read func */ \
                        AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                        RegularTransactionType()) /* regular transaction type */ \
APPLY(10400, getAnalyticsEngines, nx::vms::api::AnalyticsEngineDataList, \
                        false, \
                        false, \
                        InvalidGetHashHelper(), \
                        InvalidTriggerNotificationHelper(), \
                        InvalidAccess(), /* save permission checker */ \
                        InvalidAccess(), /* read permission checker */ \
                        FilterListByAccess<ModifyResourceAccess>(), /* Filter save func */ \
                        FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                        ReadListAccessOut<ReadResourceAccess>(), /* Check remote peer rights for outgoing transaction */ \
                        RegularTransactionType()) /* regular transaction type */ \
APPLY(10401, saveAnalyticsEngine, nx::vms::api::AnalyticsEngineData, \
                        true, \
                        false, \
                        CreateHashByIdHelper(), \
                        AnalyticsNotificationManagerHelper(), \
                        ModifyResourceAccess(), /* save permission checker */ \
                        ReadResourceAccess(), /* read permission checker */ \
                        InvalidFilterFunc(), /* Filter save func */ \
                        InvalidFilterFunc(), /* Filter read func */ \
                        ReadResourceAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                        RegularTransactionType()) /* regular transaction type */ \
APPLY(10402, removeAnalyticsEngine, nx::vms::api::IdData, \
                        true, \
                        false, \
                        CreateHashByIdHelper(), \
                        &apiIdDataTriggerNotificationHelper, \
                        RemoveResourceAccess(), /* save permission checker */ \
                        ReadResourceAccess(), /* read permission checker */ \
                        InvalidFilterFunc(), /* Filter save func */ \
                        InvalidFilterFunc(), /* Filter read func */ \
                        AllowForAllAccessOut(), /* Check remote peer rights for outgoing transaction */ \
                        RegularTransactionType()) /* regular transaction type */ \

#define TRANSACTION_ENUM_APPLY(value, name, ...) name = value,

    namespace ApiCommand
    {
        /* TODO: #Elric #enum*/
        enum Value
        {
            NotDefined = 0,
            TRANSACTION_DESCRIPTOR_LIST(TRANSACTION_ENUM_APPLY)
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
        using TimestampType = nx::vms::api::Timestamp;

        static QnUuid makeHash(const QByteArray& data1, const QByteArray& data2 = QByteArray());
        static QnUuid makeHash(const QByteArray &extraData, const nx::vms::api::DiscoveryData& data);

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
            PersistentInfo(): sequence(0), timestamp(TimestampType::fromInteger(0)) {}
            bool isNull() const { return dbID.isNull(); }

            QnUuid dbID;
            qint32 sequence;
            TimestampType timestamp;

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
        QJson::serialize(ctx, tran.params, QLatin1String("params"), &localTarget);
        *target = localTarget;
    }

    template<class T>
    bool deserialize(QnJsonContext* ctx, const QJsonValue& value, QnTransaction<T>* target)
    {
        return
            QJson::deserialize(ctx, value, static_cast<QnAbstractTransaction*>(target)) &&
            QJson::deserialize(ctx, value.toObject(), QLatin1String("params"), &target->params);
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

    struct ApiTransactionData: public nx::vms::api::Data
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

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ec2::ApiCommand::Value)(ec2::TransactionType::Value), (lexical))

Q_DECLARE_METATYPE(ec2::QnAbstractTransaction)
