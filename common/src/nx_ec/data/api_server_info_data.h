#ifndef QN_SERVER_INFO_I_H
#define QN_SERVER_INFO_I_H

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

    struct ApiServerInfoData: ApiData
    {
        QList<QByteArray> mainHardwareIds;
        QList<QByteArray> compatibleHardwareIds;

        QString publicIp;
        QString systemName;
        QString sessionKey;

        QString armBox;
        bool allowCameraChanges;
    };
#define ApiServerInfoData_Fields (mainHardwareIds)(compatibleHardwareIds)(publicIp)(systemName)(sessionKey)(armBox)(allowCameraChanges)

} // namespace ec2

#endif // QN_SERVER_INFO_I_H
