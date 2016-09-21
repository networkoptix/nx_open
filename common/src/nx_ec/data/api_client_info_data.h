#ifndef EC2__CLIENT_INFO_DATA_H
#define EC2__CLIENT_INFO_DATA_H

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

    struct ApiClientInfoData
        : ApiData
    {
        QnUuid id;
        QnUuid parentId;

        QString skin;
        QString fullVersion;
        QString systemInfo;
        QString systemRuntime;

        QString cpuArchitecture, cpuModelName;
        qint64 physicalMemory;
        QString openGLVersion, openGLVendor, openGLRenderer;
    };
#define ApiClientInfoData_Fields (id)(parentId) \
    (skin)(fullVersion)(systemInfo)(systemRuntime) \
    (cpuArchitecture)(cpuModelName)(physicalMemory) \
    (openGLVersion)(openGLVendor)(openGLRenderer)

} // namespace ec2

#endif // EC2__CLIENT_INFO_DATA_H
