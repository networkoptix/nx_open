/**********************************************************
* 12 feb 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_RUNTIME_INFO_H
#define EC2_RUNTIME_INFO_H

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiRuntimeData: ApiData
    {
        QString publicIp;
        QByteArray sessionKey;
        bool allowCameraChanges;
    };
#define ApiRuntimeData_Fields (publicIp)(sessionKey)(allowCameraChanges)

} // namespace ec2

#endif  //EC2_RUNTIME_INFO_H
