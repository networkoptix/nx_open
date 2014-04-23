#ifndef QN_SERVER_INFO_I_H
#define QN_SERVER_INFO_I_H

#include "api_types_i.h"

namespace ec2 {

    struct ServerInfo
    {
        ByteArrayList mainHardwareIds;
        ByteArrayList compatibleHardwareIds;

        QString publicIp;
        QString systemName;
        QString sessionKey;

        QString armBox;
        bool allowCameraChanges;
    };

} // namespace ec2

#endif // QN_SERVER_INFO_I_H
