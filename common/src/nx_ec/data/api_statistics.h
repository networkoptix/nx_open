#ifndef API_STATISTICS_H
#define API_STATISTICS_H

#include <set>

#include "api_globals.h"
#include "api_resource_data.h"
#include "api_media_server_data.h"
#include "api_camera_data_ex.h"
#include "api_client_info_data.h"
#include "api_license_data.h"
#include "api_business_rule_data.h"
#include "api_layout_data.h"
#include "api_user_data.h"

#include <nx/fusion/model_functions_fwd.h>

// NOTE: structs with suffix 'Statistics' are only used to tell fusion which
//       fields should be serialized for statistics (to cut out private data
//       and keep report 100% anonimous)

namespace std
{
    template<> struct hash<QString>
    {
        std::size_t operator()(const QString& s) const
        {
            return qHash(s);
        }
    };
}

namespace ec2 {

    struct ApiCameraDataStatistics
		: ApiCameraDataEx
    {
        ApiCameraDataStatistics();
        ApiCameraDataStatistics(ApiCameraDataEx&& data);

		const static std::set<QString> EXCEPT_PARAMS;
        const static std::set<QString> RESOURCE_PARAMS;
    };
#define ApiCameraDataStatistics_Fields (id)(parentId)(status)(addParams) \
    (manuallyAdded)(model)(statusFlags)(vendor) \
    (scheduleEnabled)(motionType)(motionMask)(scheduleTasks)(audioEnabled)(disableDualStreaming) \
        (controlEnabled)(dewarpingParams)(minArchiveDays)(maxArchiveDays)(preferredServerId)(backupType)

    struct ApiStorageDataStatistics
        : ApiStorageData
    {
        ApiStorageDataStatistics();
        ApiStorageDataStatistics(ApiStorageData&& data);
    };
#define ApiStorageDataStatistics_Fields (id)(parentId)(spaceLimit)(usedForWriting) \
    (storageType)(isBackup)(addParams)

    struct ApiMediaServerDataStatistics
		: ApiMediaServerDataEx
    {
        ApiMediaServerDataStatistics();
        ApiMediaServerDataStatistics(ApiMediaServerDataEx&& data);

        // overrides ApiMediaServerDataEx::storages for fusion
        std::vector<ApiStorageDataStatistics> storages;
    };
#define ApiMediaServerDataStatistics_Fields (id)(parentId)(status)(storages)(addParams) \
    (flags)(version)(systemInfo)(maxCameras)(allowAutoRedundancy) \
    (backupType)(backupDaysOfTheWeek)(backupStart)(backupDuration)(backupBitrate)

	struct ApiLicenseStatistics
	{
        ApiLicenseStatistics();
        ApiLicenseStatistics(const ApiLicenseData& data);

        QString name, key, licenseType, version, brand, expiration, validation;
        qint64 cameraCount;
	};
#define ApiLicenseStatistics_Fields (name)(key)(cameraCount)(licenseType)(version)(brand)(expiration)(validation)

	struct ApiBusinessRuleStatistics
		: ApiBusinessRuleData
	{
        ApiBusinessRuleStatistics();
        ApiBusinessRuleStatistics(ApiBusinessRuleData&& data);
	};
#define ApiBusinessRuleStatistics_Fields (id)(eventType)(eventResourceIds)(eventCondition)(eventState) \
	(actionType)(actionResourceIds)(actionParams)(aggregationPeriod)(disabled)(schedule)(system)

    struct ApiUserDataStatistics
        : ApiUserData
    {
        ApiUserDataStatistics();
        ApiUserDataStatistics(ApiUserData&& data);
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
        std::vector<ApiLayoutData> layouts;
        std::vector<ApiUserDataStatistics> users;
    };
#define ApiSystemStatistics_Fields (systemId) \
    (mediaservers)(cameras)(licenses)(businessRules)(layouts)(users) \
    (reportInfo)

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
    (ApiStatisticsServerInfo)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES( \
    API_STATISTICS_DATA_TYPES, (ubjson)(xml)(json)(csv_record));

} // namespace ec2

#endif // API_STATISTICS_H
