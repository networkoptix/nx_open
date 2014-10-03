#ifndef __API_CAMERA_DATA_H_
#define __API_CAMERA_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "api_resource_data.h"

namespace ec2
{
    struct ApiCameraData: ApiResourceData
    {
        ApiCameraData(): manuallyAdded(false), statusFlags(0) {}

        QnLatin1Array       mac;
        QString             login;
        QString             password;
        QString             physicalId;
        bool                manuallyAdded;
        QString             model;
        QString             groupId;
        QString             groupName;
        Qn::CameraStatusFlags   statusFlags;
        QString             vendor;
    };
#define ApiCameraData_Fields ApiResourceData_Fields (mac)(login)(password)(physicalId)(manuallyAdded)(model) \
                            (groupId)(groupName)(statusFlags)(vendor)

} // namespace ec2

#endif // __API_CAMERA_DATA_H_
