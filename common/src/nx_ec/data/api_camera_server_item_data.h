#ifndef __CAMERA_SERVER_ITEM_DATA_H__
#define __CAMERA_SERVER_ITEM_DATA_H__

#include "api_globals.h"
#include "api_data.h"

#include <nx_ec/data/api_fwd.h>

namespace ec2
{
    /** List of cameras that have footage on the given server. */
    struct ApiServerFootageData: ApiData
    {
        QnUuid serverGuid;
        std::vector<QnUuid> archivedCameras;
    };
    #define ApiServerFootageData_Fields (serverGuid)(archivedCameras)

    /** History item of camera movement from server to server. Server and timestamp when the camera moved to it. */
    struct ApiCameraHistoryMoveData: ApiData
    {
        ApiCameraHistoryMoveData(): timestampMs(0) {}
        ApiCameraHistoryMoveData(const QnUuid& serverGuid, const qint64& timestampMs): serverGuid(serverGuid), timestampMs(timestampMs) {}
        QnUuid serverGuid;
        qint64 timestampMs;
    };
    #define ApiCameraHistoryMoveData_Fields (serverGuid)(timestampMs)

    /** Full history of the movement for the given camera. */
    struct ApiCameraHistoryDetailData {
        QnUuid cameraId;
        ApiCameraHistoryMoveDataList moveHistory;
    };
    #define ApiCameraHistoryDetailData_Fields (cameraId)(moveHistory)
}

Q_DECLARE_METATYPE(ec2::ApiServerFootageData)
Q_DECLARE_METATYPE(ec2::ApiCameraHistoryMoveData)
Q_DECLARE_METATYPE(ec2::ApiCameraHistoryDetailData)

#endif // __CAMERA_SERVER_ITEM_DATA_H__
