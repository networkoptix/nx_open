// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metatypes.h"

#include <atomic>

#include <QtCore/QList>

#include <nx/fusion/serialization/json_functions.h>

#include "data/analytics_data.h"
#include "data/camera_attributes_data.h"
#include "data/camera_data.h"
#include "data/camera_history_data.h"
#include "data/cleanup_db_data.h"
#include "data/cloud_system_data.h"
#include "data/credentials.h"
#include "data/database_dump_data.h"
#include "data/database_dump_to_file_data.h"
#include "data/dewarping_data.h"
#include "data/discovery_data.h"
#include "data/event_rule_data.h"
#include "data/full_info_data.h"
#include "data/image_correction_data.h"
#include "data/layout_data.h"
#include "data/ldap.h"
#include "data/license_data.h"
#include "data/license_overflow_data.h"
#include "data/lock_data.h"
#include "data/media_server_data.h"
#include "data/misc_data.h"
#include "data/module_information.h"
#include "data/os_information.h"
#include "data/p2p_statistics_data.h"
#include "data/peer_alive_data.h"
#include "data/peer_data.h"
#include "data/peer_sync_time_data.h"
#include "data/resource_data.h"
#include "data/resource_type_data.h"
#include "data/reverse_connection_data.h"
#include "data/runtime_data.h"
#include "data/server_runtime_event_data.h"
#include "data/showreel_data.h"
#include "data/system_id_data.h"
#include "data/system_merge_history_record.h"
#include "data/timestamp.h"
#include "data/tran_state_data.h"
#include "data/update_data.h"
#include "data/user_data.h"
#include "data/user_data_ex.h"
#include "data/user_group_data.h"
#include "data/videowall_data.h"
#include "data/webpage_data.h"
#include "rules/event_info.h"
#include "rules/rule.h"
#include "types/access_rights_types.h"
#include "types/motion_types.h"
#include "types/rtp_types.h"

namespace nx::vms::api {

void Metatypes::initialize()
{
    static std::atomic_bool initialized = false;

    if (initialized.exchange(true))
        return;

    // Fully qualified namespaces are not needed here but are mandatory in all signal declarations.

    qRegisterMetaType<GlobalPermission>();
    qRegisterMetaType<GlobalPermissions>();
    qRegisterMetaType<AccessRight>();
    qRegisterMetaType<AccessRights>();

    qRegisterMetaType<AnalyticsEngineData>();
    qRegisterMetaType<AnalyticsPluginData>();
    qRegisterMetaType<CameraAttributesData>();
    qRegisterMetaType<CameraData>();
    qRegisterMetaType<CameraHistoryData>();
    qRegisterMetaType<CameraHistoryDataList>();
    qRegisterMetaType<CameraHistoryItemData>();
    qRegisterMetaType<CameraHistoryItemDataList>();
    qRegisterMetaType<Credentials>();
    qRegisterMetaType<CleanupDatabaseData>();
    qRegisterMetaType<CloudSystemData>();
    qRegisterMetaType<CloudSystemDataList>();
    qRegisterMetaType<DatabaseDumpData>();
    qRegisterMetaType<DatabaseDumpToFileData>();
    qRegisterMetaType<DetailedLicenseData>();
    qRegisterMetaType<dewarping::ViewData>();
    qRegisterMetaType<dewarping::MediaData>();
    qRegisterMetaType<dewarping::CameraProjection>();
    qRegisterMetaType<dewarping::FisheyeCameraMount>();
    qRegisterMetaType<dewarping::ViewProjection>();
    qRegisterMetaType<DiscoveryData>();
    qRegisterMetaType<DiscoveryDataList>();
    qRegisterMetaType<DiscoverPeerData>();
    qRegisterMetaType<DiscoveredServerData>();
    qRegisterMetaType<DiscoveredServerDataList>();
    qRegisterMetaType<EventReason>();
    qRegisterMetaType<EventRuleData>();
    qRegisterMetaType<EventRuleDataList>();
    qRegisterMetaType<FullInfoData>();
    qRegisterMetaType<ImageCorrectionData>();
    qRegisterMetaType<LayoutData>();
    qRegisterMetaType<LayoutItemData>();
    qRegisterMetaType<ShowreelData>();
    qRegisterMetaType<LicenseData>();
    qRegisterMetaType<GracePeriodExpirationData>();
    qRegisterMetaType<LockData>();
    qRegisterMetaType<MediaServerData>();
    qRegisterMetaType<MediaServerDataList>();
    qRegisterMetaType<MediaServerDataEx>();
    qRegisterMetaType<MediaServerDataExList>();
    qRegisterMetaType<MediaServerUserAttributesData>();
    qRegisterMetaType<MediaServerUserAttributesDataList>();
    qRegisterMetaType<MiscData>();
    qRegisterMetaType<MiscDataList>();
    qRegisterMetaType<ModuleInformation>();
    qRegisterMetaType<ModuleInformationWithAddresses>();
    qRegisterMetaType<MotionType>();
    qRegisterMetaType<StreamIndex>();
    qRegisterMetaType<QList<ModuleInformation>>();
    qRegisterMetaType<QList<ModuleInformationWithAddresses>>();
    qRegisterMetaType<P2pStatisticsData>();
    qRegisterMetaType<PeerData>();
    qRegisterMetaType<PeerDataEx>();
    qRegisterMetaType<PeerAliveData>();
    qRegisterMetaType<PeerSyncTimeData>();
    qRegisterMetaType<PersistentIdData>();
    qRegisterMetaType<ResourceData>();
    qRegisterMetaType<ResourceStatus>();
    qRegisterMetaType<ResourceStatusData>();
    qRegisterMetaType<ResourceParamData>();
    qRegisterMetaType<ResourceParamDataList>();
    qRegisterMetaType<ResourceParamWithRefData>();
    qRegisterMetaType<ResourceParamWithRefDataList>();
    qRegisterMetaType<ReverseConnectionData>();
    qRegisterMetaType<RtpTransportType>();
    qRegisterMetaType<RuntimeData>();
    qRegisterMetaType<ServerFootageData>();
    qRegisterMetaType<ServerFootageDataList>();
    qRegisterMetaType<ServerRuntimeEventData>();
    qRegisterMetaType<StorageData>();
    qRegisterMetaType<StorageDataList>();
    qRegisterMetaType<SyncRequestData>();
    qRegisterMetaType<SystemIdData>();
    qRegisterMetaType<SystemMergeHistoryRecord>();
    qRegisterMetaType<OsInformation>();
    qRegisterMetaType<Timestamp>();
    qRegisterMetaType<TranState>();
    qRegisterMetaType<TranStateResponse>();
    qRegisterMetaType<TranSyncDoneData>();
    qRegisterMetaType<UpdateInstallData>();
    qRegisterMetaType<UserData>();
    qRegisterMetaType<UserDataEx>();
    qRegisterMetaType<UserGroupData>();
    qRegisterMetaType<UpdateUploadResponseData>();
    qRegisterMetaType<VideowallData>();
    qRegisterMetaType<VideowallControlMessageData>();
    qRegisterMetaType<WebPageData>();
    qRegisterMetaType<rules::EventInfo>();

    // Section for QnResourceDataJsonSerializer to work.
    QnJsonSerializer::registerSerializer<nx::vms::api::Credentials>();
    QnJsonSerializer::registerSerializer<QList<nx::vms::api::Credentials>>();
};

} // namespace nx::vms::api
