#pragma once

#include "id_data.h"

#include <QtCore/QString>
#include <QtCore/QtGlobal>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API ClientInfoData: IdData
{
    QnUuid parentId;

    QString skin;
    QString fullVersion;
    QString systemInfo;
    QString systemRuntime;

    QString cpuArchitecture;
    QString cpuModelName;
    qint64 physicalMemory = 0;
    QString openGLVersion;
    QString openGLVendor;
    QString openGLRenderer;

    static DeprecatedFieldNames* getDeprecatedFieldNames()
    {
        static DeprecatedFieldNames kDeprecatedFieldNames{
            {QStringLiteral("physicalMemory"), QStringLiteral("phisicalMemory")}, //< up to v2.6
        };
        return &kDeprecatedFieldNames;
    }
};
#define ClientInfoData_Fields \
    IdData_Fields \
    (parentId) \
    (skin) \
    (fullVersion) \
    (systemInfo) \
    (systemRuntime) \
    (cpuArchitecture)(cpuModelName) \
    (physicalMemory) \
    (openGLVersion)(openGLVendor)(openGLRenderer)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::ClientInfoData)
Q_DECLARE_METATYPE(nx::vms::api::ClientInfoDataList)
