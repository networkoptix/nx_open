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
#define ApiCameraDataStatistics_Fields (id)(parentId)(model)(statusFlags)(vendor)(audioEnabled)(secondaryStreamQuality)(extraParams)

    struct ApiClientDataStatistics
    :
        ApiCameraDataEx
    {
        ApiClientDataStatistics();
        ApiClientDataStatistics(const ApiCameraDataEx& data);

        ApiResourceParamDataList extraParams;
    };
#define ApiClientDataStatistics_Fields (id)(parentId)(extraParams)

    struct ApiMediaServerDataStatistics
    :
        ApiMediaServerDataEx
    {
        ApiMediaServerDataStatistics();
        ApiMediaServerDataStatistics(const ApiMediaServerDataEx& data);

        ApiResourceParamDataList extraParams;
    };
#define ApiMediaServerDataStatistics_Fields (id)(parentId)(extraParams)

    struct ApiSystemStatistics
    {
        ApiMediaServerDataStatisticsList    mediaservers;
        ApiCameraDataStatisticsList         cameras;
        ApiClientDataStatisticsList         clients;
    };
#define ApiSystemStatistics_Fields (mediaservers)(cameras)(clients)

    struct ApiStatisticsServerInfo
    {
        QString url;
    };
#define ApiStatisticsServerInfo_Fields (url)
}

#endif // API_STATISTICS_H
