#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace api {

struct Data;

#define DECLARE_STRUCT(Type) struct Type; \
    QN_FUSION_DECLARE_FUNCTIONS(Type, (eq)(ubjson)(xml)(json)(sql_record)(csv_record), NX_VMS_API) \
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

#undef DECLARE_STRUCT

} // namespace api
} // namespace vms
} // namespace nx
