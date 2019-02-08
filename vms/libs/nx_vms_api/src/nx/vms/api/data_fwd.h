#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace api {

struct Data;
class SoftwareVersion;
class SystemInformation;

struct ParentId;

#define DECLARE_LIST(Type) \
    using Type##List = std::vector<Type>;

#define DECLARE_STRUCT_EX(Type, Functions) struct Type; \
    QN_FUSION_DECLARE_FUNCTIONS(Type, Functions, NX_VMS_API)

#define DECLARE_STRUCT(Type) \
    DECLARE_STRUCT_EX(Type, (eq)(ubjson)(json)(xml)(sql_record)(csv_record))

#define DECLARE_STRUCT_AND_LIST_EX(Type, Functions) DECLARE_STRUCT_EX(Type, Functions) \
    DECLARE_LIST(Type)

#define DECLARE_STRUCT_AND_LIST(Type) DECLARE_STRUCT(Type) \
    DECLARE_LIST(Type)

DECLARE_STRUCT_AND_LIST(IdData)
DECLARE_STRUCT(DataWithVersion)
DECLARE_STRUCT(DatabaseDumpData)
DECLARE_STRUCT(DatabaseDumpToFileData)
DECLARE_STRUCT(SyncMarkerRecordData)
DECLARE_STRUCT(UpdateSequenceData)

DECLARE_STRUCT_AND_LIST(EventRuleData)
DECLARE_STRUCT_AND_LIST(EventActionData)
DECLARE_STRUCT(ResetEventRulesData)

DECLARE_STRUCT_AND_LIST(ResourceData)
DECLARE_STRUCT_AND_LIST(ResourceParamData)
DECLARE_STRUCT_AND_LIST(ResourceParamWithRefData)
DECLARE_STRUCT_AND_LIST(ResourceStatusData)

DECLARE_STRUCT(ScheduleTaskData)
DECLARE_STRUCT(ScheduleTaskWithRefData)

DECLARE_STRUCT_AND_LIST(CameraData)
DECLARE_STRUCT_AND_LIST(CameraDataEx)
DECLARE_STRUCT_AND_LIST(CameraAttributesData)
DECLARE_STRUCT_AND_LIST(CameraHistoryItemData)
DECLARE_STRUCT_AND_LIST(CameraHistoryData)
DECLARE_STRUCT_AND_LIST(ServerFootageData)

DECLARE_STRUCT(EmailSettingsData)

DECLARE_STRUCT_AND_LIST(StoredFileData)
DECLARE_STRUCT_AND_LIST(StoredFilePath)

DECLARE_STRUCT_AND_LIST(PropertyTypeData)
DECLARE_STRUCT_AND_LIST(ResourceTypeData)

DECLARE_STRUCT_AND_LIST(WebPageData)

DECLARE_STRUCT_EX(DewarpingData, (eq)(ubjson)(json)(xml)(csv_record))
DECLARE_STRUCT_EX(ImageCorrectionData, (eq)(ubjson)(json)(xml)(csv_record))
DECLARE_STRUCT_AND_LIST(LayoutItemData)
DECLARE_STRUCT_AND_LIST(LayoutData)

DECLARE_STRUCT_AND_LIST(ClientInfoData)
DECLARE_STRUCT_AND_LIST(ConnectionData)

DECLARE_STRUCT_AND_LIST(LayoutTourItemData)
DECLARE_STRUCT_AND_LIST(LayoutTourData)
DECLARE_STRUCT(LayoutTourSettings)

DECLARE_STRUCT(LockData)

DECLARE_STRUCT(UpdateInstallData)
DECLARE_STRUCT(UpdateUploadData)
DECLARE_STRUCT_AND_LIST(UpdateUploadResponseData)

DECLARE_STRUCT_AND_LIST(VideowallItemData)
DECLARE_STRUCT_AND_LIST(VideowallScreenData)
DECLARE_STRUCT_AND_LIST(VideowallMatrixItemData)
DECLARE_STRUCT_AND_LIST(VideowallMatrixData)
DECLARE_STRUCT_AND_LIST(VideowallData)
DECLARE_STRUCT(VideowallControlMessageData)

DECLARE_STRUCT_AND_LIST(AccessRightsData)
DECLARE_STRUCT_AND_LIST(CloudSystemData)

DECLARE_STRUCT_AND_LIST(LicenseData)
DECLARE_STRUCT_AND_LIST(DetailedLicenseData)
DECLARE_STRUCT(LicenseOverflowData)

DECLARE_STRUCT(CleanupDatabaseData)
DECLARE_STRUCT(P2pStatisticsData)

DECLARE_STRUCT(PersistentIdData)
DECLARE_STRUCT(PeerData)
DECLARE_STRUCT(PeerDataEx)
DECLARE_STRUCT(PeerAliveData)
DECLARE_STRUCT(TranState)
DECLARE_STRUCT(TranStateResponse)
DECLARE_STRUCT(TranSyncDoneData)
DECLARE_STRUCT(SyncRequestData)

DECLARE_STRUCT_AND_LIST(UserData)
DECLARE_STRUCT_AND_LIST(UserRoleData)
DECLARE_STRUCT_AND_LIST(PredefinedRoleData)
DECLARE_STRUCT(UserDataEx)

DECLARE_STRUCT(PeerSyncTimeData)

DECLARE_STRUCT(ReverseConnectionData)
DECLARE_STRUCT_EX(RuntimeData, (ubjson)(json)(xml))
DECLARE_STRUCT(SystemIdData)

DECLARE_STRUCT_AND_LIST(StorageData)
DECLARE_STRUCT_AND_LIST(MediaServerData)
DECLARE_STRUCT_AND_LIST(MediaServerDataEx)
DECLARE_STRUCT_AND_LIST(MediaServerUserAttributesData)

DECLARE_STRUCT_AND_LIST(SystemMergeHistoryRecord)
DECLARE_STRUCT_AND_LIST(KeyValueData)
DECLARE_STRUCT_AND_LIST(MiscData)

DECLARE_STRUCT_EX(DiscoverPeerData, (ubjson)(json)(xml)(csv_record))
DECLARE_STRUCT_AND_LIST_EX(DiscoveryData, (ubjson)(json)(xml)(csv_record)(sql_record))
DECLARE_STRUCT_AND_LIST_EX(DiscoveredServerData, (ubjson)(json)(xml)(csv_record))

DECLARE_STRUCT_EX(FullInfoData, (ubjson)(json)(xml)(csv_record))

DECLARE_STRUCT_EX(ModuleInformation, (eq)(ubjson)(json)(xml)(csv_record))
DECLARE_STRUCT_EX(ModuleInformationWithAddresses, (eq)(ubjson)(json)(xml)(csv_record))

DECLARE_STRUCT_AND_LIST(AnalyticsPluginData)
DECLARE_STRUCT_AND_LIST(AnalyticsEngineData)

#undef DECLARE_STRUCT
#undef DECLARE_STRUCT_EX
#undef DECLARE_STRUCT_AND_LIST
#undef DECLARE_STRUCT_AND_LIST_EX

} // namespace api
} // namespace vms
} // namespace nx
