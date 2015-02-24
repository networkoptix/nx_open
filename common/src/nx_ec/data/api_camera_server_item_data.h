#ifndef __CAMERA_SERVER_ITEM_DATA_H__
#define __CAMERA_SERVER_ITEM_DATA_H__

#include "api_globals.h"
#include "api_data.h"

#include <utils/common/latin1_array.h>

namespace ec2
{
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
#define ApiCameraServerItemDataOld_Fields (cameraUniqueId)(serverId)(timestamp)
#define ApiCameraServerItemData_Fields (cameraUniqueId)(serverGuid)(timestamp)

}

#endif // __CAMERA_SERVER_ITEM_DATA_H__
