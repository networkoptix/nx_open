// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QString>

#include <nx/fusion/serialization/csv.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/fusion/serialization/xml.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/crypt/symmetrical.h>
#include <nx/vms/api/data/access_rights_data_deprecated.h>
#include <nx/vms/api/data/database_dump_to_file_data.h>
#include <nx/vms/api/data/full_info_data.h>
#include <nx/vms/api/data/hardware_id_mapping.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/api/data/peer_alive_data.h>
#include <nx/vms/api/data/resource_type_data.h>
#include <nx/vms/api/data/server_runtime_event_data.h>
#include <nx/vms/api/data/system_merge_history_record.h>
#include <nx/vms/api/data/timestamp.h>
#include <nx/vms/api/data/update_sequence_data.h>
#include <nx/vms/api/data/user_data_deprecated.h>
#include <nx_ec/abstract_ec_connection.h>

namespace ec2 {

static const char ADD_HASH_DATA[] = "$$_HASH_$$";

struct NotDefinedApiData
{
};

/**
 * This table describes all possible transactions and defines various access right checks for them.
 *
 * Examples:
 *
 * APPLY(
 *     604, //< Integer transaction id.
 *     getShowreels, //< Transaction name.
 *     ShowreelDataList, //< Passed data structure.
 *     false, //< The transaction is not persistent (does not save anything to the database).
 *     false, //< The transaction is not system (handled in a common way).
 *
 *     // Calculates hash for a persistent transaction. MUST yield the same result for corresponding
 *     // setXXX and removeXXX transactions. Actual for persistent transactions only.
 *     InvalidGetHashHelper(), 
 *
 *     // Actual mostly for persistent transactions. This callable should implement the second
 *     // stage of transaction processing "in memory, non-db" logic (e.g. work with the Resource
 *     // Pool). It's quite possible that a non-persistent transaction triggers some notifications
 *     // (for example, dumpDatabase).
 *     InvalidTriggerNotificationHelper(),
 *
 *     // Actual only for persistent transactions with one element. The warning below MUST be
 *     // fulfilled.
 *     InvalidAccess(), 
 *
 *     // Actual only for read transactions with one element. The warning below MUST be fulfilled.
 *     InvalidAccess(), 
 *
 *     // Actual only for persistent transactions with an element list. The warning below MUST be
 *     // fulfilled.
 *     InvalidFilterFunc(), 
 *
 *     // Filter the requested list by the specified checker. The warning below MUST be fulfilled.
 *     FilterListByAccess<ShowreelAccess>(),
 *
 *     // Actual only for persistent transactions. Desides if the remote peer has enough rights to
 *     // receive this transaction during the proxy transaction stage.
 *     AllowForAllAccessOut(),
 *
 *     RegularTransactionType() //< The transaction is regular, without any magic.
 * )
 *
 * APPLY(
 *     605, //< Integer transaction id.
 *     saveShowreel, //< Transaction name.
 *     ShowreelData, //< Passed data structure.
 *     true, //< The transaction is persistent.
 *     false, //< The transaction is not system (handled in a common way).
 *     CreateHashByIdHelper(), //< The id is enough to generate a hash.
 *     ShowreelNotificationManagerHelper(), //< Notify other users that the Showreel was changed.
 *     ShowreelAccess(), //< Check the access to save.
 *     ShowreelAccess(), //< Check the access to read.
 *     InvalidFilterFunc(), //< Actual only for the list transactions.
 *     InvalidFilterFunc(), //< Actual only for the list transactions.
 *     AccessOut<ShowreelAccess>(), //< Resending the persistent transactions.
 *     RegularTransactionType() //< The transaction is regular, without any magic.
 * )
 *
 * WARNING:
 *
 * All transaction descriptors for the same API data structure should have the same Access Rights
 * checker functions. For example, setResourceParam and getResourceParam have the same checker for
 * read access - ReadResourceParamAccess.
 */
#define TRANSACTION_DESCRIPTOR_LIST(APPLY) \
APPLY(1, tranSyncRequest, nx::vms::api::SyncRequestData, \
    false, /*< isPersistent */ \
    true, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), /*< trigger notification */ \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    PowerUserAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(2, tranSyncResponse, nx::vms::api::TranStateResponse, \
    false, /*< isPersistent */ \
    true, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), /*< trigger notification */ \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    PowerUserAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(3, lockRequest, nx::vms::api::LockData, \
    false, /*< isPersistent */ \
    true, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), /*< trigger notification */ \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    PowerUserAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(4, lockResponse, nx::vms::api::LockData, \
    false, /*< isPersistent */ \
    true, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), /*< trigger notification */ \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    PowerUserAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(5, unlockRequest, nx::vms::api::LockData, \
    false, /*< isPersistent */ \
    true, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), /*< trigger notification */ \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    PowerUserAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(6, peerAliveInfo, nx::vms::api::PeerAliveData, \
    false, /*< isPersistent */ \
    true, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), /*< trigger notification */ \
    AllowForAllAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(7, tranSyncDone, nx::vms::api::TranSyncDoneData, \
    false, /*< isPersistent */ \
    true, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), /*< trigger notification */ \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    PowerUserAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(102, openReverseConnection, nx::vms::api::ReverseConnectionData, \
    false, /*< isPersistent */ \
    true, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    [](const QnTransaction<nx::vms::api::ReverseConnectionData>& tran, \
         const NotificationParams& notificationParams) \
    { \
         NX_ASSERT(tran.command == ApiCommand::openReverseConnection); \
         emit notificationParams.ecConnection->reverseConnectionRequested(tran.params); \
    }, /*< trigger notification */ \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    PowerUserAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(201, removeResource, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    &apiIdDataTriggerNotificationHelper, /*< trigger notification */ \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(202, setResourceStatus, nx::vms::api::ResourceStatusData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdRfc4122Helper("status"), /*< getHash */ \
    ResourceNotificationManagerHelper(), \
    AllowForAllAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), \
    SetStatusTransactionType<nx::vms::api::ResourceStatusData>()) /*< check remote peer rights for outgoing transaction */ \
/**%apidoc */ \
APPLY(213, removeResourceStatus, nx::vms::api::IdData, /*< Remove records from vms_resource_status by resource id */ \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdRfc4122Helper("status"), /*< getHash */ \
    &apiIdDataTriggerNotificationHelper, /*< trigger notification */ \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), \
    SetStatusTransactionType<nx::vms::api::IdData>()) \
/**%apidoc */ \
APPLY(204, setResourceParams, nx::vms::api::ResourceParamWithRefDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    ResourceNotificationManagerHelper(), \
    nullptr, /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceParamAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceParamAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(203, getResourceParams, nx::vms::api::ResourceParamWithRefDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    ResourceNotificationManagerHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceParamAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceParamAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(205, getResourceTypes, nx::vms::api::ResourceTypeDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<AllowForAllAccess>(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(206, getFullInfo, nx::vms::api::FullInfoData, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    [](const QnTransaction<nx::vms::api::FullInfoData>& tran, \
        const NotificationParams& notificationParams) \
    { \
        emit notificationParams.ecConnection->initNotification(tran.params); \
        for (const auto& data: tran.params.discoveryData) \
            notificationParams.discoveryNotificationManager->triggerNotification(data); \
    }, \
    AllowForAllAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(208, setResourceParam, nx::vms::api::ResourceParamWithRefData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashForResourceParamWithRefDataHelper(), /*< getHash */ \
    ResourceNotificationManagerHelper(), \
    ModifyResourceParamAccess(false), /*< save permission checker */ \
    ReadResourceParamAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadResourceParamAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) \
/**%apidoc */ \
APPLY(209, removeResourceParam, nx::vms::api::ResourceParamWithRefData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashForResourceParamWithRefDataHelper(), /*< getHash */ \
    ResourceNotificationManagerHelper(), \
    ModifyResourceParamAccess(true), /*< save permission checker */ \
    ReadResourceParamAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(210, removeResourceParams, nx::vms::api::ResourceParamWithRefDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    ResourceNotificationManagerHelper(), \
    nullptr, /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceParamAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(211, getStatusList, nx::vms::api::ResourceStatusDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(212, removeResources, nx::vms::api::IdDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    &apiIdDataListTriggerNotificationHelper, \
    nullptr, /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(300, getCameras, nx::vms::api::CameraDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    CameraNotificationManagerHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(301, saveCamera, nx::vms::api::CameraData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    CameraNotificationManagerHelper(), \
    ModifyCameraDataAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadResourceAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(302, saveCameras, nx::vms::api::CameraDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    [](const QnTransaction<nx::vms::api::CameraDataList>& tran, \
        const NotificationParams& notificationParams) \
    { \
        notificationParams.cameraNotificationManager->triggerNotification( \
            tran, notificationParams.source); \
    }, \
    nullptr, /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(303, removeCamera, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    &apiIdDataTriggerNotificationHelper, \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(304, getCameraHistoryItems, nx::vms::api::ServerFootageDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadFootageDataAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadFootageDataAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(305, addCameraHistoryItem, nx::vms::api::ServerFootageData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    &createHashForServerFootageDataHelper, /*< getHash */ \
    CameraNotificationManagerHelper(), \
    ModifyFootageDataAccess(), /*< save permission checker */ \
    ReadFootageDataAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadFootageDataAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(310, saveCameraUserAttributes, nx::vms::api::CameraAttributesData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    &createHashForApiCameraAttributesDataHelper, /*< getHash */ \
    CameraNotificationManagerHelper(), \
    ModifyCameraAttributesAccess(), /*< save permission checker */ \
    ReadCameraAttributesAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadCameraAttributesAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(311, saveCameraUserAttributesList, nx::vms::api::CameraAttributesDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    CameraNotificationManagerHelper(), \
    ModifyCameraAttributesListAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadCameraAttributesAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadCameraAttributesAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(312, getCameraUserAttributesList, nx::vms::api::CameraAttributesDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    CameraNotificationManagerHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadCameraAttributesAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadCameraAttributesAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(313, getCamerasEx, nx::vms::api::CameraDataExList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(314, removeCameraUserAttributes, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdRfc4122Helper("camera_attributes"), /*< getHash */ \
    &apiIdDataTriggerNotificationHelper, \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(315, addHardwareIdMapping, nx::vms::api::HardwareIdMapping, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    HardwareIdMappingHashHelper(), /*< getHash */ \
    CameraNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(316, removeHardwareIdMapping, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdRfc4122Helper("hardwareid_mapping"), /*< getHash */ \
    CameraNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(317, getHardwareIdMappings, nx::vms::api::HardwareIdMappingList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<PowerUserAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(400, getMediaServers, nx::vms::api::MediaServerDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(401, saveMediaServer, nx::vms::api::MediaServerData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    MediaServerNotificationManagerHelper(), \
    PowerUserAccess(), \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadResourceAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(402, removeMediaServer, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    &apiIdDataTriggerNotificationHelper, \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(403, saveMediaServerUserAttributes, nx::vms::api::MediaServerUserAttributesData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    &createHashForApiMediaServerUserAttributesDataHelper, /*< getHash */ \
    MediaServerNotificationManagerHelper(), \
    ModifyServerAttributesAccess(), /*< save permission checker */ \
    ReadServerAttributesAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadServerAttributesAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(404, saveMediaServerUserAttributesList, nx::vms::api::MediaServerUserAttributesDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    MediaServerNotificationManagerHelper(), \
    nullptr, /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadServerAttributesAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadServerAttributesAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(405, getMediaServerUserAttributesList, nx::vms::api::MediaServerUserAttributesDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    MediaServerNotificationManagerHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadServerAttributesAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadServerAttributesAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(406, removeServerUserAttributes, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdRfc4122Helper("server_attributes"), /*< getHash */ \
    &apiIdDataTriggerNotificationHelper, \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(407, saveStorage, nx::vms::api::StorageData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    MediaServerNotificationManagerHelper(), \
    ModifyStorageAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadResourceAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(408, saveStorages, nx::vms::api::StorageDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    MediaServerNotificationManagerHelper(), \
    nullptr, /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(409, removeStorage, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    &apiIdDataTriggerNotificationHelper, \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(410, removeStorages, nx::vms::api::IdDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    &apiIdDataListTriggerNotificationHelper, \
    nullptr, /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(411, getMediaServersEx, nx::vms::api::MediaServerDataExList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(412, getStorages, nx::vms::api::StorageDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    MediaServerNotificationManagerHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(500, getUsers, nx::vms::api::UserDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(501, saveUserDeprecated, nx::vms::api::UserDataDeprecated, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    UserNotificationManagerHelper(), \
    SaveUserAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadResourceAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(502, removeUser, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    &apiIdDataTriggerNotificationHelper, /*< trigger notification */ \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/* Transaction 503 (getAccessRights) removed, the code is forbidden. */ \
/**%apidoc */ \
APPLY(504, setAccessRightsDeprecated, nx::vms::api::AccessRightsDataDeprecated, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    UserNotificationManagerHelper(), /*< trigger notification */ \
    nullptr, /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), \
    RegularTransactionType()) /*< check remote peer rights for outgoing transaction */ \
APPLY(505, getUserGroups, nx::vms::api::UserGroupDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<AllowForAllAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(506, saveUserGroup, nx::vms::api::UserGroupData, \
    true, \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    UserNotificationManagerHelper(), \
    SaveUserRoleAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(507, removeUserGroup, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    &apiIdDataTriggerNotificationHelper, \
    RemoveUserRoleAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/* Transaction 509 (removeAccessRights) removed, the code is forbidden. */ \
/**%apidoc */ \
APPLY(510, saveUsers, nx::vms::api::UserDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    UserNotificationManagerHelper(), /*< trigger notification */ \
    nullptr, /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(511, removeUserGroups, nx::vms::api::IdDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    UserNotificationManagerHelper(), /*< trigger notification */ \
    nullptr, /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(600, getLayouts, nx::vms::api::LayoutDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), /*< trigger notification */ \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(601, saveLayout, nx::vms::api::LayoutData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    LayoutNotificationManagerHelper(), /*< trigger notification */ \
    ModifyResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadResourceAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(602, saveLayouts, nx::vms::api::LayoutDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    LayoutNotificationManagerHelper(), /*< trigger notification */ \
    nullptr, /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(603, removeLayout, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    &apiIdDataTriggerNotificationHelper, /*< trigger notification */ \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(604, getShowreels, nx::vms::api::ShowreelDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ShowreelAccess>(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< not actual for non-persistent */ \
    RegularTransactionType()) \
/**%apidoc */ \
APPLY(605, saveShowreel, nx::vms::api::ShowreelData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    ShowreelNotificationManagerHelper(), /*< trigger notification */ \
    ShowreelAccess(), /*< save permission checker */ \
    ShowreelAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AccessOut<ShowreelAccess>(), \
    RegularTransactionType()) \
/**%apidoc */ \
APPLY(606, removeShowreel, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    &apiIdDataTriggerNotificationHelper, \
    ShowreelAccessById(), /*< save permission checker */ \
    ShowreelAccessById(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(700, getVideowalls, nx::vms::api::VideowallDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), /*< trigger notification */ \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(701, saveVideowall, nx::vms::api::VideowallData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    VideowallNotificationManagerHelper(), /*< trigger notification */ \
    ModifyResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadResourceAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(702, removeVideowall, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    &apiIdDataTriggerNotificationHelper, \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(703, videowallControl, nx::vms::api::VideowallControlMessageData, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    VideowallNotificationManagerHelper(), \
    VideoWallControlAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(800, getEventRules, nx::vms::api::EventRuleDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<AllowForAllAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(801, saveEventRule, nx::vms::api::EventRuleData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    BusinessEventNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(802, removeEventRule, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    &apiIdDataTriggerNotificationHelper, \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(803, resetEventRules, nx::vms::api::ResetEventRulesData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    [](const nx::vms::api::ResetEventRulesData&) \
    { \
        return QnAbstractTransaction::makeHash("reset_brule", ADD_HASH_DATA); \
    }, \
    BusinessEventNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(804, broadcastAction, nx::vms::api::EventActionData, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    BusinessEventNotificationManagerHelper(), \
    AllowForAllAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(805, execAction, nx::vms::api::EventActionData, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    BusinessEventNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(810, getVmsRules, nx::vms::api::rules::RuleList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<AllowForAllAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(811, saveVmsRule, nx::vms::api::rules::Rule, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    VmsRulesNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(812, removeVmsRule, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    VmsRulesNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(813, resetVmsRules, nx::vms::api::rules::ResetRules, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    [](const nx::vms::api::rules::ResetRules&) \
    { \
        return QnAbstractTransaction::makeHash("reset_vms_rules", ADD_HASH_DATA); \
    }, \
    VmsRulesNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(814, broadcastVmsEvent, nx::vms::api::rules::EventInfo, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    VmsRulesNotificationManagerHelper(), \
    AllowForAllAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(815, transmitVmsEvent, nx::vms::api::rules::EventInfo, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    VmsRulesNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(904, removeStoredFile, nx::vms::api::StoredFilePath, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    [](const nx::vms::api::StoredFilePath& params) \
    { \
        return QnAbstractTransaction::makeHash(params.path.toUtf8()); \
    }, \
    [](const QnTransaction<nx::vms::api::StoredFilePath>& tran, \
        const NotificationParams& notificationParams) \
    { \
        NX_ASSERT(tran.command == ApiCommand::removeStoredFile); \
        notificationParams.storedFileNotificationManager->triggerNotification( \
            tran, notificationParams.source); \
    }, \
    AllowForAllAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(901, getStoredFile, nx::vms::api::StoredFileData, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    &createHashForApiStoredFileDataHelper, \
    StoredFileNotificationManagerHelper(), \
    AllowForAllAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(902, addStoredFile, nx::vms::api::StoredFileData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    &createHashForApiStoredFileDataHelper, \
    StoredFileNotificationManagerHelper(), \
    AllowForAllAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(903, updateStoredFile, nx::vms::api::StoredFileData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    &createHashForApiStoredFileDataHelper, \
    StoredFileNotificationManagerHelper(), \
    AllowForAllAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(900, listDirectory, nx::vms::api::StoredFilePathList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<AllowForAllAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(905, getStoredFiles, nx::vms::api::StoredFileDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<AllowForAllAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(1000, getLicenses, nx::vms::api::LicenseDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    LicenseNotificationManagerHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<AllowForAllAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(1001, addLicense, nx::vms::api::LicenseData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    &createHashForApiLicenseDataHelper, \
    LicenseNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(1002, addLicenses, nx::vms::api::LicenseDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    LicenseNotificationManagerHelper(), \
    nullptr, /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<AllowForAllAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(1003, removeLicense, nx::vms::api::LicenseData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    &createHashForApiLicenseDataHelper, \
    LicenseNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(1301, discoveredServerChanged, nx::vms::api::DiscoveredServerData, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    DiscoveryNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    LocalTransactionType()) /*< local transaction type */ \
/**%apidoc */ \
APPLY(1302, discoveredServersList, nx::vms::api::DiscoveredServerDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    DiscoveryNotificationManagerHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<AllowForAllAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(1401, discoverPeer, nx::vms::api::DiscoverPeerData, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    DiscoveryNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(1402, addDiscoveryInformation, nx::vms::api::DiscoveryData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    &createHashForApiDiscoveryDataHelper, \
    DiscoveryNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(1403, removeDiscoveryInformation, nx::vms::api::DiscoveryData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    &createHashForApiDiscoveryDataHelper, \
    DiscoveryNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(1404, getDiscoveryData, nx::vms::api::DiscoveryDataList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    DiscoveryNotificationManagerHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<AllowForAllAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(1500, getWebPages, nx::vms::api::WebPageDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(1501, saveWebPage, nx::vms::api::WebPageData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    WebPageNotificationManagerHelper(), \
    ModifyResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadResourceAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(1502, removeWebPage, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    &apiIdDataTriggerNotificationHelper, \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(2004, changeSystemId, nx::vms::api::SystemIdData, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    [](const QnTransaction<nx::vms::api::SystemIdData>& tran, \
        const NotificationParams& notificationParams) \
    { \
        return notificationParams.miscNotificationManager->triggerNotification( \
            tran, notificationParams.source); \
    }, \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(2006, markLicenseOverflow, nx::vms::api::LicenseOverflowData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    ResourceNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    LocalTransactionType()) /*< local transaction type */ \
APPLY(2007, getSettings, nx::vms::api::ResourceParamDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceParamAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), \
    RegularTransactionType()) /*< check remote peer rights for outgoing transaction */ \
/**%apidoc */ \
APPLY(2008, cleanupDatabase, nx::vms::api::CleanupDatabaseData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    ResourceNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), \
    LocalTransactionType()) /*< check remote peer rights for outgoing transaction */ \
APPLY(2009, broadcastPeerSyncTime, nx::vms::api::PeerSyncTimeData, \
    false, /*< isPersistent */ \
    true, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    TimeNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(2010, markVideoWallLicenseOverflow, nx::vms::api::VideoWallLicenseOverflowData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    ResourceNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    LocalTransactionType()) /*< local transaction type */ \
/* Transaction 4001 (getClientInfoList) removed, the code is forbidden. */ \
/* Transaction 4002 (saveClientInfo) removed, the code is forbidden. */ \
/* Transaction 5001 (getStatisticsReport) removed, the code is forbidden. */ \
/* Transaction 5002 (triggerStatisticsReport) removed, the code is forbidden. */ \
APPLY(9004, runtimeInfoChanged, nx::vms::api::RuntimeData, \
    false, /*< isPersistent */ \
    true, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    [](const QnTransaction<nx::vms::api::RuntimeData>& tran, \
        const NotificationParams& notificationParams) \
    { \
        NX_ASSERT(tran.command == ApiCommand::runtimeInfoChanged); \
        emit notificationParams.ecConnection->runtimeInfoChanged(tran.params); \
    }, \
    AllowForAllAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(9005, dumpDatabase, nx::vms::api::DatabaseDumpData, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    [](const QnTransaction<nx::vms::api::DatabaseDumpData>&, \
        const NotificationParams& notificationParams) \
    { \
        emit notificationParams.ecConnection->databaseDumped(); \
    }, \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    PowerUserAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    LocalTransactionType()) /*< local transaction type */ \
/**%apidoc */ \
APPLY(9006, restoreDatabase, nx::vms::api::DatabaseDumpData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    [](const QnTransaction<nx::vms::api::DatabaseDumpData>&, \
        const NotificationParams& notificationParams) \
    { \
        emit notificationParams.ecConnection->databaseDumped(); \
    }, \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    PowerUserAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    LocalTransactionType()) /*< local transaction type */ \
/**%apidoc */ \
APPLY(9009, updatePersistentSequence, nx::vms::api::UpdateSequenceData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    EmptyNotificationHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    PowerUserAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(9010, dumpDatabaseToFile, nx::vms::api::DatabaseDumpToFileData, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    EmptyNotificationHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    PowerUserAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(9011, runtimeInfoRemoved, nx::vms::api::IdData, false, true, \
    true, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    [](const QnTransaction<nx::vms::api::IdData>& tran, \
        const NotificationParams& notificationParams) \
    { \
        NX_ASSERT(tran.command == ApiCommand::runtimeInfoRemoved); \
        emit notificationParams.ecConnection->runtimeInfoRemoved(tran.params); \
    }, \
    AllowForAllAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(10000, getTransactionLog, ApiTransactionDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<AllowForAllAccess>(), /*< filter read func */ \
    ReadListAccessOut<AllowForAllAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(10100, saveMiscParam, nx::vms::api::MiscData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    [](const QnTransaction<nx::vms::api::MiscData>& tran, \
        const NotificationParams& notificationParams) \
    { \
        return notificationParams.miscNotificationManager->triggerNotification(tran); \
    }, \
    PowerUserAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    LocalTransactionType()) /*< local transaction type */ \
APPLY(10101, getMiscParam, nx::vms::api::MiscData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    LocalTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(10200, saveSystemMergeHistoryRecord, nx::vms::api::SystemMergeHistoryRecord, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    makeCreateHashFromCustomFieldHelper(&nx::vms::api::SystemMergeHistoryRecord::mergedSystemLocalId), /*< getHash */ \
    EmptyNotificationHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) \
APPLY(10201, getSystemMergeHistory, nx::vms::api::SystemMergeHistoryRecordList, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), /*< getHash */ \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    PowerUserAccess(), /*< read permission checker */ \
    FilterListByAccess<PowerUserAccess>(), /*< filter read func */ \
    PowerUserAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(10300, getAnalyticsPlugins, nx::vms::api::AnalyticsPluginDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(10301, saveAnalyticsPlugin, nx::vms::api::AnalyticsPluginData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    AnalyticsNotificationManagerHelper(), \
    ModifyResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadResourceAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(10302, removeAnalyticsPlugin, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    &apiIdDataTriggerNotificationHelper, \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(10400, getAnalyticsEngines, nx::vms::api::AnalyticsEngineDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<ReadResourceAccess>(), /*< filter read func */ \
    ReadListAccessOut<ReadResourceAccess>(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(10401, saveAnalyticsEngine, nx::vms::api::AnalyticsEngineData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    AnalyticsNotificationManagerHelper(), \
    ModifyResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadResourceAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
/**%apidoc */ \
APPLY(10402, removeAnalyticsEngine, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    &apiIdDataTriggerNotificationHelper, \
    RemoveResourceAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(10500, serverRuntimeEvent, nx::vms::api::ServerRuntimeEventData, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    [](const QnTransaction<nx::vms::api::ServerRuntimeEventData>& tran, \
         const NotificationParams& notificationParams) \
    { \
         NX_ASSERT(tran.command == ApiCommand::serverRuntimeEvent); \
         emit notificationParams.ecConnection->serverRuntimeEventOccurred(tran.params); \
    }, \
    AllowForAllAccess(), /*< Save permission checker */ \
    AllowForAllAccess(), /*< Read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(10501, saveUser, nx::vms::api::UserData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< getHash */ \
    UserNotificationManagerHelper(), \
    SaveUserAccess(), /*< save permission checker */ \
    ReadResourceAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    ReadResourceAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) /*< regular transaction type */ \
APPLY(10600, getLookupLists, nx::vms::api::LookupListDataList, \
    false, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    InvalidGetHashHelper(), \
    InvalidTriggerNotificationHelper(), \
    InvalidAccess(), /*< save permission checker */ \
    InvalidAccess(), /*< read permission checker */ \
    FilterListByAccess<AllowForAllAccess>(), /*< filter read func */ \
    AllowForAllAccessOut(),  /*< outgoing check is not actual for non-persistent transactions */ \
    RegularTransactionType()) \
APPLY(10601, saveLookupList, nx::vms::api::LookupListData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    false, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), /*< Id is enough to generate the hash. */ \
    LookupListNotificationManagerHelper(), \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) \
APPLY(10602, removeLookupList, nx::vms::api::IdData, \
    true, /*< isPersistent */ \
    false, /*< isSystem */ \
    true, /*< isRemoveOperation */ \
    CreateHashByIdHelper(), \
    &apiIdDataTriggerNotificationHelper, \
    PowerUserAccess(), /*< save permission checker */ \
    AllowForAllAccess(), /*< read permission checker */ \
    InvalidFilterFunc(), /*< filter read func */ \
    AllowForAllAccessOut(), /*< check remote peer rights for outgoing transaction */ \
    RegularTransactionType()) \

//-------------------------------------------------------------------------------------------------

namespace ApiCommand
{
    enum Value
    {
        CompositeSave = -1,
        NotDefined = 0,
        #define TRANSACTION_ENUM_APPLY(value, name, ...) name = value,
        TRANSACTION_DESCRIPTOR_LIST(TRANSACTION_ENUM_APPLY)
        #undef TRANSACTION_ENUM_APPLY
    };

    template<typename Visitor>
    constexpr auto nxReflectVisitAllEnumItems(Value*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<Value>;
        return visitor(
            Item{CompositeSave, "CompositeSave"},
            Item{NotDefined, "NotDefined"}
            #define TRANSACTION_ENUM_APPLY(VALUE, NAME, ...) , Item{NAME, #NAME}
            NX_MSVC_EXPAND(TRANSACTION_DESCRIPTOR_LIST(TRANSACTION_ENUM_APPLY))
            #undef TRANSACTION_ENUM_APPLY
        );
    }

    QString toString(Value value);
    Value fromString(const QString& value);

    /**
     * Whether the transaction can be sent independently of the current synchronization state.
     * These transactions are rather commands than data transfers.
     */
    bool isSystem(Value value);

    /** Whether the transaction should be written to the database. */
    bool isPersistent(Value value);
}  // namespace ApiCommand

NX_REFLECTION_ENUM_CLASS(TransactionType,
    Unknown = -1,
    Regular = 0,
    Local = 1, //< Do not propagate transactions to other Server peers or Cloud.
    Cloud = 2 //< Sync transaction to Cloud.
)

struct HistoryAttributes
{
    /** Id of user or entity who created transaction. */
    QnUuid author;

    bool operator==(const HistoryAttributes& other) const
    {
        return author == other.author;
    }
};
#define HistoryAttributes_Fields (author)
QN_FUSION_DECLARE_FUNCTIONS(HistoryAttributes, (json)(ubjson)(xml)(csv_record))
NX_REFLECTION_INSTRUMENT(HistoryAttributes, HistoryAttributes_Fields)

struct TransactionPersistentInfo
{
    using TimestampType = nx::vms::api::Timestamp;

    TransactionPersistentInfo(): timestamp(TimestampType::fromInteger(0)) {}
    bool isNull() const { return dbID.isNull(); }

    QnUuid dbID;
    qint32 sequence = 0;
    nx::vms::api::Timestamp timestamp;

    friend size_t qHash(const TransactionPersistentInfo& id)
    {
        QByteArray idData(id.dbID.toRfc4122());
        idData.append((const char*) &id.timestamp, sizeof(id.timestamp));
        return ::qHash(idData, id.sequence);
    }

    bool operator==(const TransactionPersistentInfo& other) const
    {
        return dbID == other.dbID
            && sequence == other.sequence
            && timestamp == other.timestamp;
    }
};

struct QnAbstractTransaction
{
    static QnUuid makeHash(const QByteArray& data1, const QByteArray& data2 = QByteArray());
    static QnUuid makeHash(const QByteArray& extraData, const nx::vms::api::DiscoveryData& data);

    /**
     * Sets QnAbstractTransaction::peerID to commonModule()->peerId().
     */
    QnAbstractTransaction():
        command(ApiCommand::NotDefined),
        transactionType(TransactionType::Regular)
    {
    }

    explicit QnAbstractTransaction(QnUuid _peerID):
        command(ApiCommand::NotDefined),
        peerID(_peerID),
        transactionType(TransactionType::Regular)
    {
    }

    QnAbstractTransaction(ApiCommand::Value value, QnUuid _peerID):
        command(value),
        peerID(_peerID),
        transactionType(TransactionType::Regular)
    {
    }

    using PersistentInfo = TransactionPersistentInfo;

    ApiCommand::Value command;

    /** Id of peer that generated transaction. */
    QnUuid peerID;

    TransactionPersistentInfo persistentInfo;

    TransactionType transactionType;
    HistoryAttributes historyAttributes;

    bool operator==(const QnAbstractTransaction& other) const
    {
        return command == other.command
            && peerID == other.peerID
            && persistentInfo == other.persistentInfo
            && transactionType == other.transactionType
            && historyAttributes == other.historyAttributes;
    }

    QString toString() const;
    bool isLocal() const { return transactionType == TransactionType::Local; }
};
typedef std::vector<ec2::QnAbstractTransaction> QnAbstractTransactionList;
typedef QnAbstractTransaction::PersistentInfo QnAbstractTransaction_PERSISTENT;
#define QnAbstractTransaction_PERSISTENT_Fields (dbID)(sequence)(timestamp)
#define QnAbstractTransaction_Fields \
    (command)(peerID)(persistentInfo)(transactionType)(historyAttributes)

NX_REFLECTION_INSTRUMENT(QnAbstractTransaction::PersistentInfo, QnAbstractTransaction_PERSISTENT_Fields)
NX_REFLECTION_INSTRUMENT(QnAbstractTransaction, QnAbstractTransaction_Fields)

template <class T>
class QnTransaction: public QnAbstractTransaction
{
public:
    using base_type = QnAbstractTransaction;

    QnTransaction(): QnAbstractTransaction() {}
    QnTransaction(QnUuid _peerID):
        QnAbstractTransaction(_peerID)
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
        QnAbstractTransaction(command, _peerID),
        params(params)
    {
    }

    template<typename U>
    explicit QnTransaction(const QnTransaction<U>& /*otherTran*/)
    {
        NX_ASSERT(0, "Constructing from transaction with another Params type is disallowed");
    }

    T params;

    bool operator==(const QnTransaction& other) const
    {
        return QnAbstractTransaction::operator==(other)
            && params == other.params;
    }

    bool operator!=(const QnTransaction& other) const
    {
        return !(*this == other);
    }
};

QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransaction::PersistentInfo, (json)(ubjson)(xml)(csv_record))
QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransaction, (json)(ubjson)(xml)(csv_record))
NX_REFLECTION_INSTRUMENT_TEMPLATE(QnTransaction, (params))

/** Json formatting functions for QnTransaction<T>. */
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

/** QnUbjson formatting functions for QnTransaction<T>. */
template <class T, class Output>
void serialize(const QnTransaction<T> &transaction,  QnUbjsonWriter<Output>* stream)
{
    QnUbjson::serialize(static_cast<const QnAbstractTransaction &>(transaction), stream);
    QnUbjson::serialize(transaction.params, stream);
}

template <class T, class Input>
bool deserialize(QnUbjsonReader<Input>* stream,  QnTransaction<T>* transaction)
{
    return
        QnUbjson::deserialize(stream, static_cast<QnAbstractTransaction *>(transaction))
        && QnUbjson::deserialize(stream, &transaction->params);
}

struct ApiTranLogFilter
{
    bool cloudOnly = false;
    bool removeOnly = false;
};
#define ApiTranLogFilter_Fields (cloudOnly)(removeOnly)
QN_FUSION_DECLARE_FUNCTIONS(ApiTranLogFilter, (json)(ubjson)(xml)(csv_record))

struct ApiTransactionData
{
    QnUuid tranGuid;
    QnAbstractTransaction tran;
    int dataSize = 0;

    ApiTransactionData() = default;
    explicit ApiTransactionData(const QnUuid& peerGuid): tran(peerGuid) {}

    ApiTransactionData(const ApiTransactionData&) = default;
    ApiTransactionData& operator=(const ApiTransactionData&) = default;
    ApiTransactionData(ApiTransactionData&&) = default;
    ApiTransactionData& operator=(ApiTransactionData&&) = default;

    bool operator==(const ApiTransactionData& right) const = default;
};
#define ApiTransactionData_Fields (tranGuid)(tran)(dataSize)
QN_FUSION_DECLARE_FUNCTIONS(ApiTransactionData, (json)(ubjson)(xml)(csv_record))

NX_REFLECTION_INSTRUMENT(ApiTransactionData, ApiTransactionData_Fields)

struct TransactionModel
{
    QnUuid id;
    QnAbstractTransaction info;
    int binaryDataSizeB = 0;
    std::optional<QJsonValue> data;

    /**%apidoc Error description if this transaction is stored in the DB incorrectly. */
    std::optional<QString> _error;

    template<typename T>
    void setData(T&& data)
    {
        QJsonValue value;
        QnJsonContext context;
        context.setChronoSerializedAsDouble(true);
        context.setSerializeMapToObject(true);
        QJson::serialize(&context, data, &value);
        this->data = value;
    }
    void setError(const QString& message, const QByteArray& binaryData, bool withData);
};
#define TransactionModel_Fields (id)(info)(binaryDataSizeB)(data)(_error)
QN_FUSION_DECLARE_FUNCTIONS(TransactionModel, (json))

int generateRequestID();

} // namespace ec2
