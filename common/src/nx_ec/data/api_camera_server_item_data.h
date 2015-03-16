#ifndef __CAMERA_SERVER_ITEM_DATA_H__
#define __CAMERA_SERVER_ITEM_DATA_H__

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    /** List of cameras that have footage on the given server. */
    struct ApiCameraHistoryData: ApiData
    {
        QnUuid serverGuid;
        std::vector<QnUuid> archivedCameras;
    };
    #define ApiCameraHistoryData_Fields (serverGuid)(archivedCameras)
    typedef std::vector<ApiCameraHistoryData> ApiCameraHistoryDataList;

    /** History item of camera movement from server to server. Server and timestamp when the camera moved to it. */
    struct ApiCameraHistoryMoveData: ApiData
    {
        ApiCameraHistoryMoveData(): timestampMs(0) {}
        ApiCameraHistoryMoveData(const QnUuid& serverGuid, const qint64& timestampMs): serverGuid(serverGuid), timestampMs(timestampMs) {}
        QnUuid serverGuid;
        qint64 timestampMs;
    };
    #define ApiCameraHistoryMoveData_Fields (serverGuid)(timestampMs)
    typedef std::vector<ApiCameraHistoryMoveData> ApiCameraHistoryMoveDataList;


    /** Full history of the movement for the given camera. */
    struct ApiCameraHistoryDetailData {
        QnUuid cameraId;
        ApiCameraHistoryMoveDataList moveHistory;
    };
    #define ApiCameraHistoryDetailData_Fields (cameraId)(moveHistory)
    typedef std::vector<ApiCameraHistoryDetailData> ApiCameraHistoryDetailDataList;
}

#endif // __CAMERA_SERVER_ITEM_DATA_H__
