#ifndef __CAMERA_SERVER_ITEM_DATA_H__
#define __CAMERA_SERVER_ITEM_DATA_H__

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiCameraServerItemData: ApiData {
        ApiCameraServerItemData(): timestamp(0) {}

        QUuid  cameraId;
        QUuid  serverId;
        qint64   timestamp;
    };
#define ApiCameraServerItemData_Fields (cameraId)(serverId)(timestamp)

}

#endif // __CAMERA_SERVER_ITEM_DATA_H__
