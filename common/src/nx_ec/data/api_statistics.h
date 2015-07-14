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
    };
#define ApiCameraDataStatistics_Fields (id)(parentId)(status)(addParams) \
    (manuallyAdded)(model)(statusFlags)(vendor) \
    (scheduleEnabled)(motionType)(motionMask)(scheduleTasks)(audioEnabled)(secondaryStreamQuality) \
        (controlEnabled)(dewarpingParams)(minArchiveDays)(maxArchiveDays)(preferedServerId)

    struct ApiStorageDataStatistics
        : ApiStorageData
    {
        ApiStorageDataStatistics();
        ApiStorageDataStatistics(ApiStorageData&& data);
    };
#define ApiStorageDataStatistics_Fields (id)(parentId)(spaceLimit)(usedForWriting)(storageType)(addParams)

    struct ApiMediaServerDataStatistics
		: ApiMediaServerDataEx
    {
        ApiMediaServerDataStatistics();
        ApiMediaServerDataStatistics(ApiMediaServerDataEx&& data);

        ApiStorageDataStatisticsList storages;
    };
#define ApiMediaServerDataStatistics_Fields (id)(parentId)(status)(storages)(addParams) \
    (flags)(not_used)(version)(systemInfo)(maxCameras)(allowAutoRedundancy)

	struct ApiLicenseStatistics
	{
        ApiLicenseStatistics();
        ApiLicenseStatistics(const ApiLicenseData& data);

        QString name, key, licenseType, version, brand, expiration;
        qint64 cameraCount;
	};
#define ApiLicenseStatistics_Fields (name)(key)(cameraCount)(licenseType)(version)(brand)(expiration)

	struct ApiBusinessRuleStatistics
		: ApiBusinessRuleData
	{
        ApiBusinessRuleStatistics();
        ApiBusinessRuleStatistics(ApiBusinessRuleData&& data);
	};
#define ApiBusinessRuleStatistics_Fields (id)(eventType)(eventResourceIds)(eventCondition)(eventState) \
	(actionType)(actionResourceIds)(actionParams)(aggregationPeriod)(disabled)(schedule)(system)

    struct ApiSystemStatistics
    {
        QnUuid systemId;
		
        ApiBusinessRuleStatisticsList       businessRules;
        ApiCameraDataStatisticsList         cameras;
        ApiClientInfoDataList               clients;
        ApiLicenseStatisticsList            licenses;
        ApiMediaServerDataStatisticsList    mediaservers;
    };
#define ApiSystemStatistics_Fields (systemId)(mediaservers)(cameras)(clients)(licenses)(businessRules)

    struct ApiStatisticsServerInfo
    {
        QnUuid  systemId;
        QString url;
        QString status;
    };
#define ApiStatisticsServerInfo_Fields (systemId)(url)(status)

} // namespace ec2

#endif // API_STATISTICS_H
