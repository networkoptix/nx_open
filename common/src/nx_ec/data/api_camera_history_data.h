#pragma once

#include <nx_ec/data/api_fwd.h>

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

/** List of cameras that have footage on the given server. */
struct ApiServerFootageData: ApiData
{
    QnUuid serverGuid;
    std::vector<QnUuid> archivedCameras;

    ApiServerFootageData() = default;

    ApiServerFootageData(const QnUuid& serverGuid, const std::vector<QnUuid>& archivedCameras):
        serverGuid(serverGuid), archivedCameras(archivedCameras)
    {
    }
};
#define ApiServerFootageData_Fields (serverGuid)(archivedCameras)

/**
 * History item of camera movement from server to server. Server and timestamp when the camera
 * moved to it.
 */
struct ApiCameraHistoryItemData: ApiData
{
    ApiCameraHistoryItemData() = default;

    ApiCameraHistoryItemData(const QnUuid& serverGuid, const qint64& timestampMs):
        serverGuid(serverGuid), timestampMs(timestampMs)
    {
    }

    QnUuid serverGuid;
    qint64 timestampMs = 0;
};
#define ApiCameraHistoryItemData_Fields (serverGuid)(timestampMs)

/** Full history of the movement for the given camera. */
struct ApiCameraHistoryData
{
    QnUuid cameraId;
    ApiCameraHistoryItemDataList items;
};
#define ApiCameraHistoryData_Fields (cameraId)(items)

} // namesapce ec2

Q_DECLARE_METATYPE(ec2::ApiServerFootageData)
Q_DECLARE_METATYPE(ec2::ApiCameraHistoryItemData)
Q_DECLARE_METATYPE(ec2::ApiCameraHistoryData)
