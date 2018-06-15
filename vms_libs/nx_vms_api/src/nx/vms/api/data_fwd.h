#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace api {

struct Data;

#define DECLARE_STRUCT_NO_LIST(Type) struct Type; \
    QN_FUSION_DECLARE_FUNCTIONS(Type, \
        (eq)(ubjson)(json)(xml)(sql_record)(csv_record), \
        NX_VMS_API)

#define DECLARE_STRUCT(Type) DECLARE_STRUCT_NO_LIST(Type) \
    using Type##List = std::vector<Type>;

DECLARE_STRUCT(IdData)
DECLARE_STRUCT(DataWithVersion)
DECLARE_STRUCT(DatabaseDumpData)
DECLARE_STRUCT(DatabaseDumpToFileData)
DECLARE_STRUCT(SyncMarkerRecordData)
DECLARE_STRUCT(UpdateSequenceData)

DECLARE_STRUCT(EventRuleData)
DECLARE_STRUCT(EventActionData)
DECLARE_STRUCT(ResetEventRulesData)

DECLARE_STRUCT(ResourceData)
DECLARE_STRUCT(ResourceParamData)
DECLARE_STRUCT(ResourceParamWithRefData)
DECLARE_STRUCT(ResourceStatusData)

DECLARE_STRUCT(CameraData)

DECLARE_STRUCT(ScheduleTaskData)
DECLARE_STRUCT(ScheduleTaskWithRefData)
DECLARE_STRUCT(CameraAttributesData)

DECLARE_STRUCT(CameraDataEx)

DECLARE_STRUCT(ServerFootageData)
DECLARE_STRUCT(CameraHistoryItemData)
DECLARE_STRUCT(CameraHistoryData)

DECLARE_STRUCT(EmailSettingsData)

DECLARE_STRUCT(StoredFileData)
DECLARE_STRUCT(StoredFilePath)

DECLARE_STRUCT(PropertyTypeData)
DECLARE_STRUCT(ResourceTypeData)

DECLARE_STRUCT(WebPageData)

DECLARE_STRUCT(LayoutItemData)
DECLARE_STRUCT(LayoutData)

DECLARE_STRUCT(ClientInfoData)
DECLARE_STRUCT(ConnectionData)

DECLARE_STRUCT(LayoutTourItemData)
DECLARE_STRUCT(LayoutTourSettings)
DECLARE_STRUCT(LayoutTourData)

DECLARE_STRUCT(LockData)

DECLARE_STRUCT_NO_LIST(UpdateInstallData)
DECLARE_STRUCT_NO_LIST(UpdateUploadData)
DECLARE_STRUCT(UpdateUploadResponseData)

DECLARE_STRUCT(VideowallItemData)
DECLARE_STRUCT(VideowallScreenData)
DECLARE_STRUCT(VideowallMatrixItemData)
DECLARE_STRUCT(VideowallMatrixData)
DECLARE_STRUCT(VideowallData)
DECLARE_STRUCT(VideowallControlMessageData)

DECLARE_STRUCT(AccessRightsData)
DECLARE_STRUCT(CloudSystemData)

DECLARE_STRUCT(LicenseData)
DECLARE_STRUCT(DetailedLicenseData)
DECLARE_STRUCT_NO_LIST(LicenseOverflowData)

DECLARE_STRUCT_NO_LIST(CleanupDatabaseData)
DECLARE_STRUCT_NO_LIST(P2pStatisticsData)

DECLARE_STRUCT_NO_LIST(PersistentIdData)
DECLARE_STRUCT_NO_LIST(PeerData)
DECLARE_STRUCT_NO_LIST(PeerDataEx)

DECLARE_STRUCT_NO_LIST(TranState)
DECLARE_STRUCT_NO_LIST(TranStateResponse)
DECLARE_STRUCT_NO_LIST(TranSyncDoneData)
DECLARE_STRUCT_NO_LIST(SyncRequestData)

#undef DECLARE_STRUCT
#undef DECLARE_STRUCT_NO_LIST

} // namespace api
} // namespace vms
} // namespace nx
