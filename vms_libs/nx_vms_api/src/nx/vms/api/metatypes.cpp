#include "metatypes.h"

#include <QtCore/QList>
#include <QtCore/QMetaType>

#include "data/access_rights_data.h"
#include "data/camera_data.h"
#include "data/camera_attributes_data.h"
#include "data/camera_history_data.h"
#include "data/cleanup_db_data.h"
#include "data/cloud_system_data.h"
#include "data/database_dump_data.h"
#include "data/database_dump_to_file_data.h"
#include "data/discovery_data.h"
#include "data/event_rule_data.h"
#include "data/full_info_data.h"
#include "data/layout_data.h"
#include "data/layout_tour_data.h"
#include "data/license_data.h"
#include "data/license_overflow_data.h"
#include "data/lock_data.h"
#include "data/media_server_data.h"
#include "data/misc_data.h"
#include "data/module_information.h"
#include "data/p2p_statistics_data.h"
#include "data/peer_data.h"
#include "data/peer_alive_data.h"
#include "data/peer_sync_time_data.h"
#include "data/resource_data.h"
#include "data/resource_type_data.h"
#include "data/reverse_connection_data.h"
#include "data/runtime_data.h"
#include "data/software_version.h"
#include "data/system_id_data.h"
#include "data/system_merge_history_record.h"
#include "data/system_information.h"
#include "data/tran_state_data.h"
#include "data/timestamp.h"
#include "data/update_data.h"
#include "data/user_data.h"
#include "data/user_role_data.h"
#include "data/videowall_data.h"
#include "data/webpage_data.h"
#include "data/user_data_ex.h"
#include "data/analytics_data.h"

#include "types/access_rights_types.h"

namespace {

static bool nx_vms_api_metatypes_initialized = false;

} // namespace

namespace nx {
namespace vms {
namespace api {

void Metatypes::initialize()
{
    // Note that running the code twice is perfectly OK.
    if (nx_vms_api_metatypes_initialized)
        return;

    nx_vms_api_metatypes_initialized = true;

    // Fully qualified namespaces are not needed here but are mandatory in all signal declarations.

    qRegisterMetaType<GlobalPermission>();
    qRegisterMetaType<GlobalPermissions>();

    qRegisterMetaType<AccessRightsData>();
    qRegisterMetaType<AccessRightsDataList>();
    qRegisterMetaType<CameraAttributesData>();
    qRegisterMetaType<CameraData>();
    qRegisterMetaType<CameraHistoryData>();
    qRegisterMetaType<CameraHistoryDataList>();
    qRegisterMetaType<CameraHistoryItemData>();
    qRegisterMetaType<CameraHistoryItemDataList>();
    qRegisterMetaType<CleanupDatabaseData>();
    qRegisterMetaType<CloudSystemData>();
    qRegisterMetaType<CloudSystemDataList>();
    qRegisterMetaType<DatabaseDumpData>();
    qRegisterMetaType<DatabaseDumpToFileData>();
    qRegisterMetaType<DetailedLicenseData>();
    qRegisterMetaType<DiscoveryData>();
    qRegisterMetaType<DiscoveryDataList>();
    qRegisterMetaType<DiscoverPeerData>();
    qRegisterMetaType<DiscoveredServerData>();
    qRegisterMetaType<DiscoveredServerDataList>();
    qRegisterMetaType<EventRuleData>();
    qRegisterMetaType<EventRuleDataList>();
    qRegisterMetaType<FullInfoData>();
    qRegisterMetaType<LayoutData>();
    qRegisterMetaType<LayoutItemData>();
    qRegisterMetaType<LayoutTourData>();
    qRegisterMetaType<LicenseData>();
    qRegisterMetaType<LicenseOverflowData>();
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
    qRegisterMetaType<QList<ModuleInformation>>();
    qRegisterMetaType<QList<ModuleInformationWithAddresses>>();
    qRegisterMetaType<P2pStatisticsData>();
    qRegisterMetaType<PeerData>();
    qRegisterMetaType<PeerDataEx>();
    qRegisterMetaType<PeerAliveData>();
    qRegisterMetaType<PeerSyncTimeData>();
    qRegisterMetaType<PersistentIdData>();
    qRegisterMetaType<PredefinedRoleData>();
    qRegisterMetaType<ResourceData>();
    qRegisterMetaType<ResourceStatusData>();
    qRegisterMetaType<ResourceParamData>();
    qRegisterMetaType<ResourceParamDataList>();
    qRegisterMetaType<ResourceParamWithRefData>();
    qRegisterMetaType<ResourceParamWithRefDataList>();
    qRegisterMetaType<ReverseConnectionData>();
    qRegisterMetaType<RuntimeData>();
    qRegisterMetaType<ServerFootageData>();
    qRegisterMetaType<ServerFootageDataList>();
    qRegisterMetaType<SoftwareVersion>();
    qRegisterMetaTypeStreamOperators<SoftwareVersion>();
    qRegisterMetaType<StorageData>();
    qRegisterMetaType<StorageDataList>();
    qRegisterMetaType<SyncRequestData>();
    qRegisterMetaType<SystemIdData>();
    qRegisterMetaType<SystemMergeHistoryRecord>();
    qRegisterMetaType<SystemInformation>();
    qRegisterMetaTypeStreamOperators<SystemInformation>();
    qRegisterMetaType<Timestamp>();
    qRegisterMetaType<TranState>();
    qRegisterMetaType<TranStateResponse>();
    qRegisterMetaType<TranSyncDoneData>();
    qRegisterMetaType<UpdateInstallData>();
    qRegisterMetaType<UserData>();
    qRegisterMetaType<UserDataEx>();
    qRegisterMetaType<UserRoleData>();
    qRegisterMetaType<UpdateUploadResponseData>();
    qRegisterMetaType<VideowallData>();
    qRegisterMetaType<VideowallControlMessageData>();
    qRegisterMetaType<WebPageData>();
    qRegisterMetaType<AnalyticsPluginData>();
    qRegisterMetaType<AnalyticsEngineData>();
};

} // namespace api
} // namespace vms
} // namespace nx
