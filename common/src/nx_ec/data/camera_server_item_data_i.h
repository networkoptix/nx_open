#ifndef QN_CAMERA_SERVER_ITEM_DATA_I_H
#define QN_CAMERA_SERVER_ITEM_DATA_I_H

#include "api_data_i.h"

namespace ec2 {
    struct ApiCameraServerItemData : ApiData {
        ApiCameraServerItemData(): timestamp(0) {}

        QString  physicalId;
        QString  serverGuid;
        qint64   timestamp;
    };

#define ApiCameraServerItemData_Fields (physicalId)(serverGuid)(timestamp)

} // namespace ec2


#endif // QN_CAMERA_SERVER_ITEM_DATA_I_H