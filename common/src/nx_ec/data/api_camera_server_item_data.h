#ifndef __CAMERA_SERVER_ITEM_DATA_H__
#define __CAMERA_SERVER_ITEM_DATA_H__

#include "api_globals.h"
#include "api_data.h"

#include <utils/common/latin1_array.h>

namespace ec2
{
    /*
    struct ApiCameraServerItemDataOld: ApiData {
        ApiCameraServerItemDataOld(): timestamp(0) {}

        QString cameraUniqueId;
        QnLatin1Array serverId;
        qint64   timestamp;
    };

    struct ApiCameraServerItemData: ApiData {
        ApiCameraServerItemData(): timestamp(0) {}

        QString cameraUniqueId;
        QnUuid serverGuid;
        qint64   timestamp;
    };
    */

    struct ApiCameraHistoryData: ApiData
    {
        QnUuid serverGuid;
        std::vector<QnUuid> archivedCameras;
    };

    struct ApiCameraHistoryMoveData: ApiData
    {
        ApiCameraHistoryMoveData(): timestamp(0) {}
        ApiCameraHistoryMoveData(const QnUuid& serverGuid, const qint64& timestamp): serverGuid(serverGuid), timestamp(timestamp) {}
        QnUuid serverGuid;
        qint64 timestamp;
    };

    typedef std::vector<ApiCameraHistoryMoveData> ApiCameraHistoryMoveDataList;
    struct ApiCameraHistoryDetailData {
        QnUuid cameraId;
        ApiCameraHistoryMoveDataList moveHistory;
    };
    typedef std::vector<ApiCameraHistoryDetailData> ApiCameraHistoryDetailDataList;

#define ApiCameraHistoryData_Fields (serverGuid)(archivedCameras)
#define ApiCameraHistoryMoveData_Fields (serverGuid)(timestamp)
#define ApiCameraHistoryDetailData_Fields (cameraId)(moveHistory)

}

#endif // __CAMERA_SERVER_ITEM_DATA_H__
