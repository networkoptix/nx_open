#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace api {

struct Data;

#define DECLARE_STRUCT(Type) struct Type; \
    QN_FUSION_DECLARE_FUNCTIONS(Type, (eq)(ubjson)(xml)(json)(sql_record)(csv_record), NX_VMS_API)

DECLARE_STRUCT(IdData)
using IdDataList = std::vector<IdData>;

DECLARE_STRUCT(DataWithVersion)
DECLARE_STRUCT(DatabaseDumpData)
DECLARE_STRUCT(DatabaseDumpToFileData)
DECLARE_STRUCT(SyncMarkerRecordData)
DECLARE_STRUCT(UpdateSequenceData)

DECLARE_STRUCT(EventRuleData)
DECLARE_STRUCT(EventActionData)
using EventRuleDataList = std::vector<EventRuleData>;

DECLARE_STRUCT(ResetEventRulesData)

DECLARE_STRUCT(ResourceData)
using ResourceDataList = std::vector<ResourceData>;
DECLARE_STRUCT(ResourceParamData)
using ResourceParamDataList = std::vector<ResourceParamData>;
DECLARE_STRUCT(ResourceParamWithRefData)
using ResourceParamWithRefDataList = std::vector<ResourceParamWithRefData>;
DECLARE_STRUCT(ResourceStatusData)
using ResourceStatusDataList = std::vector<ResourceStatusData>;

DECLARE_STRUCT(CameraData)
using CameraDataList = std::vector<CameraData>;

DECLARE_STRUCT(ScheduleTaskData)
DECLARE_STRUCT(ScheduleTaskWithRefData)
DECLARE_STRUCT(CameraAttributesData)
using CameraAttributesDataList = std::vector<CameraAttributesData>;

DECLARE_STRUCT(CameraDataEx)
using CameraDataExList = std::vector<CameraDataEx>;

DECLARE_STRUCT(ServerFootageData)
using ServerFootageDataList = std::vector<ServerFootageData>;
DECLARE_STRUCT(CameraHistoryItemData)
using CameraHistoryItemDataList = std::vector<CameraHistoryItemData>;
DECLARE_STRUCT(CameraHistoryData)
using CameraHistoryDataList = std::vector<CameraHistoryData>;

DECLARE_STRUCT(EmailSettingsData)

#undef DECLARE_STRUCT

} // namespace api
} // namespace vms
} // namespace nx
