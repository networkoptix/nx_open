#pragma once

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

struct ApiClientInfoData: ApiIdData
{
    QnUuid parentId;

    QString skin;
    QString fullVersion;
    QString systemInfo;
    QString systemRuntime;

    QString cpuArchitecture, cpuModelName;
    qint64 physicalMemory;
    QString openGLVersion, openGLVendor, openGLRenderer;

    static DeprecatedFieldNames* getDeprecatedFieldNames()
    {
        static DeprecatedFieldNames kDeprecatedFieldNames{
            {lit("physicalMemory"), lit("phisicalMemory")}, //< up to v2.6
        };
        return &kDeprecatedFieldNames;
    }
};
#define ApiClientInfoData_Fields \
    ApiIdData_Fields \
    (parentId) \
    (skin) \
    (fullVersion) \
    (systemInfo) \
    (systemRuntime) \
    (cpuArchitecture)(cpuModelName) \
    (physicalMemory) \
    (openGLVersion)(openGLVendor)(openGLRenderer)

} // namespace ec2
