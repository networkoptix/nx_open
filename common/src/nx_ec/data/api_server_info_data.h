#ifndef QN_SERVER_INFO_I_H
#define QN_SERVER_INFO_I_H

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

    struct ApiServerInfoData: ApiData
    {
        QList<QByteArray> mainHardwareIds;
        QList<QByteArray> compatibleHardwareIds;
        QMap<QnId, QList<QByteArray>> remoteHardwareIds;

        QString publicIp;
        QString systemName;
        QByteArray sessionKey;

        QString armBox; // TODO: #Elric #EC2 what is this?
        bool allowCameraChanges; // TODO: #Elric #EC2 use "enabled"
        qint64 prematureLicenseExperationDate; // Not enough license for recording. Some recording will be stopped in a nearest future
    };

#define ApiServerInfoData_Fields (mainHardwareIds)(compatibleHardwareIds)(remoteHardwareIds)(publicIp)(systemName)(sessionKey)(armBox)(allowCameraChanges)(prematureLicenseExperationDate)

} // namespace ec2

#endif // QN_SERVER_INFO_I_H
