#ifndef API_STATISTICS_H
#define API_STATISTICS_H

#include "api_globals.h"
#include "api_resource_data.h"
#include "api_media_server_data.h"
#include "api_camera_data_ex.h"

#include <unordered_set>

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

namespace ec2
{
    struct ApiCameraDataStatistics
    :
        ApiCameraDataEx
    {
        ApiCameraDataStatistics();
        ApiCameraDataStatistics(const ApiCameraDataEx& data);

		const static std::unordered_set<QString> ADD_PARAMS;

        ApiResourceParamDataList addParams;
    };
#define ApiCameraDataStatistics_Fields (id)(parentId)(status)(addParams) \
    (manuallyAdded)(model)(statusFlags)(vendor) \
    (scheduleEnabled)(motionType)(motionMask)(scheduleTasks)(audioEnabled)(secondaryStreamQuality) \
        (controlEnabled)(dewarpingParams)(minArchiveDays)(maxArchiveDays)(preferedServerId)

    struct ApiClientDataStatistics
    :
        ApiCameraDataEx
    {
        ApiClientDataStatistics();
        ApiClientDataStatistics(const ApiCameraDataEx& data);

		const static std::unordered_set<QString> ADD_PARAMS;

        ApiResourceParamDataList addParams;
    };
#define ApiClientDataStatistics_Fields (id)(parentId)(addParams)

    struct ApiStorageDataStatistics
    :
        ApiStorageData
    {
        ApiStorageDataStatistics();
        ApiStorageDataStatistics(const ApiStorageData& data);
    };
#define ApiStorageDataStatistics_Fields (id)(parentId)(spaceLimit)(usedForWriting)

    struct ApiMediaServerDataStatistics
    :
        ApiMediaServerDataEx
    {
        ApiMediaServerDataStatistics();
        ApiMediaServerDataStatistics(const ApiMediaServerDataEx& data);

		const static std::unordered_set<QString> ADD_PARAMS;

        ApiStorageDataStatisticsList    storages;
        ApiResourceParamDataList        addParams;
    };
#define ApiMediaServerDataStatistics_Fields (id)(parentId)(status)(storages)(addParams) \
    (flags)(not_used)(version)(systemInfo)(maxCameras)(allowAutoRedundancy)

    struct ApiSystemStatistics
    {
        QnUuid systemId;

        ApiMediaServerDataStatisticsList    mediaservers;
        ApiCameraDataStatisticsList         cameras;
        ApiClientDataStatisticsList         clients;
    };
#define ApiSystemStatistics_Fields (systemId)(mediaservers)(cameras)(clients)

    struct ApiStatisticsServerInfo
    {
        QnUuid systemId;
        QString url;
        QString status;
    };
#define ApiStatisticsServerInfo_Fields (systemId)(url)(status)
}

#endif // API_STATISTICS_H
