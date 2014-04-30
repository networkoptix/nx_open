#ifndef __CAMERA_SERVER_ITEM_DATA_H__
#define __CAMERA_SERVER_ITEM_DATA_H__

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiCameraServerItemData: ApiData {
        ApiCameraServerItemData(): timestamp(0) {}

        QString  physicalId;
        QString  serverGuid; // TODO: #Elric #EC2 is it server id?
        qint64   timestamp;
    };
#define ApiCameraServerItemData_Fields (physicalId)(serverGuid)(timestamp)

}

#endif // __CAMERA_SERVER_ITEM_DATA_H__
