#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <vector>

#ifndef QN_NO_QT
#include <QtCore/QString>
#include "nx_ec/ec_api.h"
#endif

#include "nx/fusion/serialization/binary.h"
#include "nx/fusion/serialization/json.h"
#include "nx/fusion/serialization/xml.h"
#include "nx/fusion/serialization/csv.h"
#include "nx/fusion/serialization/ubjson.h"
#include "common/common_module.h"

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
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(2, tranSyncResponse, QnTranStateResponse, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(3, lockRequest, ApiLockData, \
                       false, /* persistent*/ \
                       true, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(4, lockResponse, ApiLockData, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(5, unlockRequest, ApiLockData, \
                       false, /* persistent*/ \
                       true, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(6, peerAliveInfo, ApiPeerAliveData, \
                       false, /* persistent*/ \
                       true, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(7, tranSyncDone, ApiTranSyncDoneData, \
                       false, /* persistent*/ \
                       true,  /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(100, testConnection, ApiLoginData, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(101, connect, ApiLoginData, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
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
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(201, removeResource, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(202, setResourceStatus, ApiResourceStatusData, \
                       true,  /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdRfc4122Helper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(213, removeResourceStatus, ApiIdData, /* Remove records from vms_resource_status by resource id */ \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(204, setResourceParams, ApiResourceParamWithRefDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceParamAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceParamAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceParamAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(203, getResourceParams, ApiResourceParamWithRefDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceParamAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceParamAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceParamAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(205, getResourceTypes, ApiResourceTypeDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
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
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(208, setResourceParam, ApiResourceParamWithRefData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashForResourceParamWithRefDataHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       ModifyResourceParamAccess(false), /* save permission checker */ \
                       ReadResourceParamAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceParamAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(209, removeResourceParam, ApiResourceParamWithRefData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashForResourceParamWithRefDataHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       ModifyResourceParamAccess(true), /* save permission checker */ \
                       ReadResourceParamAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(210, removeResourceParams, ApiResourceParamWithRefDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       ResourceNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceParamAccess>(true), /* Filter save func */ \
                       FilterListByAccess<ReadResourceParamAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(211, getStatusList, ApiResourceStatusDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(212, removeResources, ApiIdDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       &apiIdDataListTriggerNotificationHelper, \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(true), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(300, getCameras, ApiCameraDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(301, saveCamera, ApiCameraData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       ModifyResourceAccess(false), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(302, saveCameras, ApiCameraDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       [] (const QnTransaction<ApiCameraDataList> &tran, const NotificationParams &notificationParams) \
                         { \
                            notificationParams.cameraNotificationManager->triggerNotification(tran); \
                         }, \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(303, removeCamera, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(304, getCameraHistoryItems, ApiServerFootageDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyFootageDataAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadFootageDataAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadFootageDataAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(305, addCameraHistoryItem, ApiServerFootageData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForServerFootageDataHelper, /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       ModifyFootageDataAccess(), /* save permission checker */ \
                       ReadFootageDataAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadFootageDataAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(309, removeCameraHistoryItem, ApiServerFootageData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForServerFootageDataHelper, /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       ModifyFootageDataAccess(), /* save permission checker */ \
                       ReadFootageDataAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(310, saveCameraUserAttributes, ApiCameraAttributesData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForApiCameraAttributesDataHelper, /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       ModifyCameraAttributesAccess(), /* save permission checker */ \
                       ReadCameraAttributesAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadCameraAttributesAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(311, saveCameraUserAttributesList, ApiCameraAttributesDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       ModifyCameraAttributesListAccess(), /* Filter save func */ \
                       FilterListByAccess<ReadCameraAttributesAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadCameraAttributesAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(312, getCameraUserAttributes, ApiCameraAttributesDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       CameraNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       ModifyCameraAttributesListAccess(), /* Filter save func */ \
                       FilterListByAccess<ReadCameraAttributesAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadCameraAttributesAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(313, getCamerasEx, ApiCameraDataExList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(314, removeCameraUserAttributes, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(400, getMediaServers, ApiMediaServerDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(401, saveMediaServer, ApiMediaServerData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       ModifyResourceAccess(false), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(402, removeMediaServer, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(403, saveServerUserAttributes, ApiMediaServerUserAttributesData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForApiMediaServerUserAttributesDataHelper, /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       ModifyServerAttributesAccess(), /* save permission checker */ \
                       ReadServerAttributesAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadServerAttributesAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(404, saveServerUserAttributesList, ApiMediaServerUserAttributesDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyServerAttributesAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadServerAttributesAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadServerAttributesAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(405, getServerUserAttributes, ApiMediaServerUserAttributesDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyServerAttributesAccess>(), /* Filter save func */ \
                       FilterListByAccess<ReadServerAttributesAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadServerAttributesAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(406, removeServerUserAttributes, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(407, saveStorage, ApiStorageData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       ModifyResourceAccess(false), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(408, saveStorages, ApiStorageDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(409, removeStorage, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(410, removeStorages, ApiIdDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       &apiIdDataListTriggerNotificationHelper, \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(true), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(411, getMediaServersEx, ApiMediaServerDataExList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(412, getStorages, ApiStorageDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       MediaServerNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(500, getUsers, ApiUserDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(501, saveUser, ApiUserData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       UserNotificationManagerHelper(), \
                       ModifyResourceAccess(false), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(502, removeUser, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(503, getAccessRights, ApiAccessRightsDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(504, setAccessRights, ApiAccessRightsData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       &createHashForApiAccessRightsDataHelper, /* getHash*/ \
                       UserNotificationManagerHelper(), /* trigger notification*/ \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(509, removeAccessRights, ApiIdData, /* Remove records from vms_access_rights by resource id */ \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(505, getUserGroups, ApiUserGroupDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(506, saveUserGroup, ApiUserGroupData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       UserNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(507, removeUserGroup, ApiIdData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       RemoveUserGroupAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(508, getPredefinedRoles, ApiPredefinedRoleDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(600, getLayouts, ApiLayoutDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(601, saveLayout, ApiLayoutData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       LayoutNotificationManagerHelper(), /* trigger notification*/ \
                       ModifyResourceAccess(false), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(602, saveLayouts, ApiLayoutDataList, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       LayoutNotificationManagerHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(603, removeLayout, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       &apiIdDataTriggerNotificationHelper, /* trigger notification*/ \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(700, getVideowalls, ApiVideowallDataList, \
                       false, /* persistent*/ \
                       false, /* system*/ \
                       InvalidGetHashHelper(), /* getHash*/ \
                       InvalidTriggerNotificationHelper(), /* trigger notification*/ \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(701, saveVideowall, ApiVideowallData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), /* getHash*/ \
                       VideowallNotificationManagerHelper(), /* trigger notification*/ \
                       ModifyResourceAccess(false), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(702, removeVideowall, ApiIdData, \
                       true, /* persistent*/ \
                       false, /* system*/ \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(703, videowallControl, ApiVideowallControlMessageData, \
                       false, /* persistent*/\
                       false, /* system*/ \
                       InvalidGetHashHelper(), \
                       VideowallNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(800, getBusinessRules, ApiBusinessRuleDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(801, saveBusinessRule, ApiBusinessRuleData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       BusinessEventNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(802, removeBusinessRule, ApiIdData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(803, resetBusinessRules, ApiResetBusinessRuleData, \
                       true, \
                       false, \
                       [] (const ApiResetBusinessRuleData&) { return QnTransactionLog::makeHash("reset_brule", ADD_HASH_DATA); }, \
                       BusinessEventNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(804, broadcastBusinessAction, ApiBusinessActionData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       BusinessEventNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(805, execBusinessAction, ApiBusinessActionData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       BusinessEventNotificationManagerHelper(), \
                       UserInputAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(904, removeStoredFile, ApiStoredFilePath, \
                       true, \
                       false, \
                       [] (const ApiStoredFilePath &params) { return QnTransactionLog::makeHash(params.path.toUtf8()); }, \
                       [] (const QnTransaction<ApiStoredFilePath> &tran, const NotificationParams &notificationParams) \
                        { \
                            NX_ASSERT(tran.command == ApiCommand::removeStoredFile); \
                            notificationParams.storedFileNotificationManager->triggerNotification(tran); \
                        }, \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(901, getStoredFile, ApiStoredFileData, \
                       false, \
                       false, \
                       &createHashForApiStoredFileDataHelper, \
                       StoredFileNotificationManagerHelper(), \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(902, addStoredFile, ApiStoredFileData, \
                       true, \
                       false, \
                       &createHashForApiStoredFileDataHelper, \
                       StoredFileNotificationManagerHelper(), \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(903, updateStoredFile, ApiStoredFileData, \
                       true, \
                       false, \
                       &createHashForApiStoredFileDataHelper, \
                       StoredFileNotificationManagerHelper(), \
                       AllowForAllAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(900, listDirectory, ApiStoredDirContents, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(905, getStoredFiles, ApiStoredFileDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1000, getLicenses, ApiLicenseDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       LicenseNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1001, addLicense, ApiLicenseData, \
                       true, \
                       false, \
                       &createHashForApiLicenseDataHelper, \
                       LicenseNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1002, addLicenses, ApiLicenseDataList, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       LicenseNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1003, removeLicense, ApiLicenseData, \
                       true, \
                       false, \
                       &createHashForApiLicenseDataHelper, \
                       LicenseNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1200, uploadUpdate, ApiUpdateUploadData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       UpdateNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1201, uploadUpdateResponce, ApiUpdateUploadResponceData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       UpdateNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1202, installUpdate, ApiUpdateInstallData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       UpdateNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1301, discoveredServerChanged, ApiDiscoveredServerData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       DiscoveryNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1302, discoveredServersList, ApiDiscoveredServerDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       DiscoveryNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1401, discoverPeer, ApiDiscoverPeerData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       DiscoveryNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1402, addDiscoveryInformation, ApiDiscoveryData, \
                       true, \
                       false, \
                       &createHashForApiDiscoveryDataHelper, \
                       DiscoveryNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1403, removeDiscoveryInformation, ApiDiscoveryData, \
                       true, \
                       false, \
                       &createHashForApiDiscoveryDataHelper, \
                       DiscoveryNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1404, getDiscoveryData, ApiDiscoveryDataList, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       DiscoveryNotificationManagerHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1500, getWebPages, ApiWebPageDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<ModifyResourceAccess>(false), /* Filter save func */ \
                       FilterListByAccess<ReadResourceAccess>(), /* Filter read func */ \
                       ReadListAccessOut<ReadResourceAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1501, saveWebPage, ApiWebPageData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       WebPageNotificationManagerHelper(), \
                       ModifyResourceAccess(false), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       ReadResourceAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(1502, removeWebPage, ApiIdData, \
                       true, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       ModifyResourceAccess(true), /* save permission checker */ \
                       ReadResourceAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(2001, forcePrimaryTimeServer, ApiIdData, \
                       false, \
                       false, \
                       CreateHashByIdHelper(), \
                       &apiIdDataTriggerNotificationHelper, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(2002, broadcastPeerSystemTime, ApiPeerSystemTimeData, \
                       false, \
                       true, \
                       InvalidGetHashHelper(), \
                       EmptyNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(2003, getCurrentTime, ApiTimeData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(2004, changeSystemName, ApiSystemNameData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       [] (const QnTransaction<ApiSystemNameData> &tran, const NotificationParams &notificationParams) \
                        { return notificationParams.miscNotificationManager->triggerNotification(tran); }, \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(2005, getKnownPeersSystemTime, ApiPeerSystemTimeDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(2006, markLicenseOverflow, ApiLicenseOverflowData, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       ResourceNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(2007, getSettings, ApiResourceParamDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(2008, rebuildTransactionLog, ApiRebuildTransactionLogData, \
                       true, \
                       false, \
                       InvalidGetHashHelper(), \
                       ResourceNotificationManagerHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(4001, getClientInfos, ApiClientInfoDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AdminOnlyAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \
APPLY(4002, saveClientInfo, ApiClientInfoData, \
                       true, \
                       false, \
                       CreateHashByIdRfc4122Helper(), \
                       EmptyNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AllowForAllAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(5001, getStatisticsReport, ApiSystemStatistics, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(5002, triggerStatisticsReport, ApiStatisticsServerInfo, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
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
                       AllowForAllAccessOut()) /* Check remote peer rights for outgoing transaction */ \
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
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
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
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(9009, updatePersistentSequence, ApiUpdateSequenceData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       EmptyNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(9010, dumpDatabaseToFile, ApiDatabaseDumpToFileData, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       EmptyNotificationHelper(), \
                       AdminOnlyAccess(), /* save permission checker */ \
                       AdminOnlyAccess(), /* read permission checker */ \
                       InvalidFilterFunc(), /* Filter save func */ \
                       InvalidFilterFunc(), /* Filter read func */ \
                       AdminOnlyAccessOut()) /* Check remote peer rights for outgoing transaction */ \
APPLY(10000, getTransactionLog, ApiTransactionDataList, \
                       false, \
                       false, \
                       InvalidGetHashHelper(), \
                       InvalidTriggerNotificationHelper(), \
                       InvalidAccess(), /* save permission checker */ \
                       InvalidAccess(), /* read permission checker */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter save func */ \
                       FilterListByAccess<AllowForAllAccess>(), /* Filter read func */ \
                       ReadListAccessOut<AllowForAllAccess>()) /* Check remote peer rights for outgoing transaction */ \


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
#define ApiTransactionData_Fields (tranGuid)(tran)
QN_FUSION_DECLARE_FUNCTIONS(ApiTransactionData, (json)(ubjson)(xml)(csv_record))


    int generateRequestID();
} /* namespace ec2*/

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ec2::ApiCommand::Value), (metatype)(numeric))

#ifndef QN_NO_QT
Q_DECLARE_METATYPE(ec2::QnAbstractTransaction)
#endif

#endif  /*EC2_TRANSACTION_H*/
