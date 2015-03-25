#ifndef API_STATISTICS_H
#define API_STATISTICS_H

#include "api_globals.h"
#include "api_resource_data.h"
#include "api_media_server_data.h"
#include "api_camera_data_ex.h"

namespace ec2
{

    struct ApiCameraDataStatistics
    :
        ApiCameraDataEx
    {
        ApiCameraDataStatistics();
        ApiCameraDataStatistics(const ApiCameraDataEx& data);

        ApiResourceParamDataList extraParams;
    };
#define ApiCameraDataStatistics_Fields (id)(parentId)(status)(extraParams) \
    (manuallyAdded)(model)(statusFlags)(vendor) \
    (scheduleEnabled)(motionType)(motionMask)(scheduleTasks)(audioEnabled)(secondaryStreamQuality) \
        (controlEnabled)(dewarpingParams)(minArchiveDays)(maxArchiveDays)(preferedServerId)

    struct ApiClientDataStatistics
    :
        ApiCameraDataEx
    {
        ApiClientDataStatistics();
        ApiClientDataStatistics(const ApiCameraDataEx& data);

        ApiResourceParamDataList extraParams;
    };
#define ApiClientDataStatistics_Fields (id)(parentId)(extraParams)

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

        ApiResourceParamDataList extraParams;
    };
#define ApiMediaServerDataStatistics_Fields (id)(parentId)(status)(extraParams) \
    (flags)(not_used)(version)(systemInfo)(maxCameras)(allowAutoRedundancy)

    struct ApiSystemStatistics
    {
        QnUuid systemId;

        ApiMediaServerDataStatisticsList    mediaservers;
        ApiCameraDataStatisticsList         cameras;
        ApiClientDataStatisticsList         clients;
        ApiStorageDataStatisticsList        storages;
    };
#define ApiSystemStatistics_Fields (systemId)(mediaservers)(cameras)(clients)(storages)

    struct ApiStatisticsServerInfo
    {
        QnUuid systemId;
        QString url;
        QString status;
    };
#define ApiStatisticsServerInfo_Fields (systemId)(url)(status)
}

#endif // API_STATISTICS_H
