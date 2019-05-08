#pragma once

#include <set>

#include "api_globals.h"

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/client_info_data.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/videowall_data.h>

// NOTE: structs with suffix 'Statistics' are only used to tell fusion which
//       fields should be serialized for statistics (to cut out private data
//       and keep report 100% anonimous)

namespace std {

template<>
struct hash<QString>
{
    std::size_t operator()(const QString& s) const
    {
        return qHash(s);
    }
};

} // namespace std

namespace ec2 {

struct ApiCameraDataStatistics:
    nx::vms::api::CameraDataEx
{
    ApiCameraDataStatistics();
    ApiCameraDataStatistics(nx::vms::api::CameraDataEx&& data);
};
#define ApiCameraDataStatistics_Fields (id)(parentId)(status)(addParams) \
    (manuallyAdded)(model)(statusFlags)(vendor) \
    (scheduleEnabled)(motionType)(motionMask)(scheduleTasks)(audioEnabled)(disableDualStreaming) \
    (controlEnabled)(dewarpingParams)(minArchiveDays)(maxArchiveDays)(preferredServerId) \
    (backupType)

struct ApiStorageDataStatistics:
    nx::vms::api::StorageData
{
    ApiStorageDataStatistics();
    ApiStorageDataStatistics(nx::vms::api::StorageData&& data);
};
#define ApiStorageDataStatistics_Fields (id)(parentId)(spaceLimit)(usedForWriting) \
    (storageType)(isBackup)(addParams)

struct ApiMediaServerDataStatistics:
    nx::vms::api::MediaServerDataEx
{
    ApiMediaServerDataStatistics();
    ApiMediaServerDataStatistics(nx::vms::api::MediaServerDataEx&& data);

    // overrides ApiMediaServerDataEx::storages for fusion
    std::vector<ApiStorageDataStatistics> storages;
};
#define ApiMediaServerDataStatistics_Fields (id)(parentId)(status)(storages)(addParams) \
    (flags)(version)(systemInfo)(maxCameras)(allowAutoRedundancy) \
    (backupType)(backupDaysOfTheWeek)(backupStart)(backupDuration)(backupBitrate)

struct ApiLicenseStatistics
{
    ApiLicenseStatistics();
    ApiLicenseStatistics(const nx::vms::api::LicenseData& data);

    QString name, key, licenseType, version, brand, expiration, validation;
    qint64 cameraCount;
};
#define ApiLicenseStatistics_Fields \
    (name)(key)(cameraCount)(licenseType)(version)(brand)(expiration)(validation)

struct ApiBusinessRuleStatistics:
    nx::vms::api::EventRuleData
{
    ApiBusinessRuleStatistics();
    ApiBusinessRuleStatistics(nx::vms::api::EventRuleData&& data);
};
#define ApiBusinessRuleStatistics_Fields \
    (id)(eventType)(eventResourceIds)(eventCondition)(eventState)(actionType)(actionResourceIds) \
    (actionParams)(aggregationPeriod)(disabled)(schedule)(system)

struct ApiUserDataStatistics:
    nx::vms::api::UserData
{
    ApiUserDataStatistics();
    ApiUserDataStatistics(nx::vms::api::UserData&& data);
};
#define ApiUserDataStatistics_Fields (id)(isAdmin)(permissions)(isLdap)(isEnabled)

struct ApiStatisticsReportInfo
{
    QnUuid id;
    qint64 number;

    ApiStatisticsReportInfo();
};
#define ApiStatisticsReportInfo_Fields (id)(number)

struct ApiSystemStatistics
{
    QnUuid systemId;
    ApiStatisticsReportInfo reportInfo;

    std::vector<ApiBusinessRuleStatistics> businessRules;
    std::vector<ApiCameraDataStatistics> cameras;
    std::vector<ApiLicenseStatistics> licenses;
    std::vector<ApiMediaServerDataStatistics> mediaservers;
    nx::vms::api::LayoutDataList layouts;
    std::vector<ApiUserDataStatistics> users;
    nx::vms::api::VideowallDataList videowalls;
};
#define ApiSystemStatistics_Fields (systemId) \
    (mediaservers)(cameras)(licenses)(businessRules)(layouts)(users)(videowalls) \
    /* Note the order of fields. */ (reportInfo)

struct ApiStatisticsServerArguments
{
    bool randomSystemId = false;
};
#define ApiStatisticsServerArguments_Fields (randomSystemId)

struct ApiStatisticsServerInfo
{
    QnUuid  systemId;
    QString url;
    QString status;
};
#define ApiStatisticsServerInfo_Fields (systemId)(url)(status)

#define API_STATISTICS_DATA_TYPES \
    (ApiCameraDataStatistics) \
    (ApiStorageDataStatistics) \
    (ApiMediaServerDataStatistics) \
    (ApiLicenseStatistics) \
    (ApiBusinessRuleStatistics) \
    (ApiUserDataStatistics) \
    (ApiStatisticsReportInfo) \
    (ApiSystemStatistics) \
    (ApiStatisticsServerArguments) \
    (ApiStatisticsServerInfo)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES( \
    API_STATISTICS_DATA_TYPES, (ubjson)(xml)(json)(csv_record));

} // namespace ec2
